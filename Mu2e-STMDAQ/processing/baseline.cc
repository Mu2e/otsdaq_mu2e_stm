// Baseline code
#include "Mu2e-STMDAQ/processing/baseline.hh"

// Constructor
Baseline::Baseline(Config& cfg_,
                   const std::shared_ptr<AsyncLogger>& logger_,
                   const std::shared_ptr<STMdata>& stm_) :
  cfg(cfg_), logger(logger_), stm(stm_),
  nbins(stm->baseline_config.hist_bin_num),
  min_adc(stm->baseline_config.hist_min_adc),
  max_adc(stm->baseline_config.hist_max_adc),
  width(stm->baseline_config.hist_bin_width),
  invw(stm->baseline_config.hist_inv_bin_width),
  window_size_buffers(stm->baseline_config.hist_window_buffers),
  hist_counts_window(nbins, 0),
  hist_counts_all(nbins, 0),
  hist_bin_centres(stm->baseline_config.hist_bin_centres),
  per_buffer_hists(window_size_buffers, std::vector<uint64_t>(nbins, 0)),
  max_iters(stm->baseline_config.EM_max_iters),
  tol(stm->baseline_config.EM_LL_tol),
  var_floor(stm->baseline_config.EM_var_floor) {

  // Register operations for OperationManager
  register_operation("calc_baseline", [this](auto& b){ calc_baseline(b); });
  
}

// Calculate baselne by fitting histogrammed ADC data
void Baseline::calc_baseline(std::shared_ptr<DataStruct>& buffer){

  // The adc length
  size_t n = buffer->raw_len;
  
  // Define a pointer to the raw data buffer
  const int16_t* data_ptr = buffer->raw.data();

  // Check histogram sizes and throw error
  if (buffer->hist_counts_window.size() != nbins){
      logger->log("Baseline: Error! buffer->hist_window has " +
                  std::to_string(buffer->hist_counts_window.size()) +
                  " bins. Config bin num = " +
                  std::to_string(nbins) + ". Exiting...",0);
      std::this_thread::sleep_for(std::chrono::seconds(3));
      return;
  }
  if (buffer->hist_counts_all.size() != nbins){
      logger->log("Baseline: Error! buffer->hist_all has " +
                  std::to_string(buffer->hist_counts_all.size()) +
                  " bins. Config bin num = " +
                  std::to_string(nbins) + ". Exiting...",0);
      std::this_thread::sleep_for(std::chrono::seconds(3));
      return;
  }

  // Remove the old buffer from the window histogram
  //  auto &old_hist = per_buffer_hists[window_index];
  for (int b = 0; b < nbins; ++b) {
    hist_counts_window[b] -= per_buffer_hists[window_index][b];
    total_window -= per_buffer_hists[window_index][b];
    per_buffer_hists[window_index][b] = 0; // reset this buffer histogram
  }
  
  // Fill new buffer histogram
  for (std::size_t i = 0; i < n; ++i) {

    // Get sample
    const int16_t sample = data_ptr[i];

    // Underflow
    if (sample < min_adc){
      // Ignore sample
      continue;
    }
    
    // Overflow
    if (sample > max_adc){
      // Warning
      logger->log("Baseline: Warning! ADC data has value " +
                  std::to_string(sample) +
                  " which is larger than the config max ADC value of " +
                  std::to_string(int(max_adc)) + ".",2);
      //std::this_thread::sleep_for(std::chrono::seconds(3));
      return;
    }

    // Find bin for sample
    int bin = int((sample - min_adc) * invw);

    // Add to histograms
    ++per_buffer_hists[window_index][bin];
    ++hist_counts_window[bin];
    ++hist_counts_all[bin];
    // Add to counts
    ++total_window;
    ++total_all;
    
  }

  // Update histgrams in buffer
  std::memcpy(buffer->hist_counts_window.data(),
              hist_counts_window.data(),
              nbins * sizeof(uint64_t));
  buffer->total_window = total_window;
  std::memcpy(buffer->hist_counts_all.data(),
              hist_counts_all.data(),
              nbins * sizeof(uint64_t));
  buffer->total_all = total_all;
  
  // Advance circular index
  ++window_index;
  if (window_index == window_size_buffers) window_index = 0;

  // Fit sliding window histogram
  buffer->baseline_window = EM_algorithm(buffer->hist_counts_window,
                                         buffer->total_window);
  // Fit all data histogram
  buffer->baseline_all = EM_algorithm(buffer->hist_counts_all,
                                      buffer->total_all);
  // Store global results
  mu0_all = buffer->baseline_all.mu0;
  sigma0_all = buffer->baseline_all.sigma0;

  // Increment buffer count
  ++buffer_count;
  
  // If calculated first full window
  //if (buffer_count % window_size_buffers == 0){
  if (buffer_count == window_size_buffers){
    logger->log("Baseline: Last " + std::to_string(stm->baseline_config.hist_window_period) +
                " s window has baseline value: " + std::to_string(mu0_all) +
                " ± " + std::to_string(sigma0_all) + " ADCs.",1);
  }

  
}

// Expectation–Maximization (EM) Algorithm
// Gaussian (baseline) + one-sided exponential tail (pulses)
baseline_fit Baseline::EM_algorithm(const std::vector<uint64_t>& hist_counts,
                                    const uint64_t total_hist_counts) {

  // Histogram Mode
  const int mode_bin = get_hist_mode_idx(hist_counts); // Mode
  // ADC value at mode (baseline mean guess)
  const double mu0_init = hist_bin_centres[mode_bin];
  // Baseline mean + 1*sigma (clean side)
  //const double p84= hist_percentile(hist_counts,total_hist_counts,0.84);
  // Baseline sigma guess
  //double sigma0_init = std::max(1e-3, p84 - mu0_init);
  // Ensure sigma is > 0
  //if (!(sigma0_init > 0.0)){
  //    sigma0_init = std::max(1.0, (max_adc - min_adc) / (2.0 * nbins));
  //}

  // Find ADC value when the gaussian side has dropped to 
  // 60.7% of the mode height (e^-1/2)
  const double frac_of_mode = mode_fraction(hist_counts, mode_bin, 0.607);
  // Baseline sigma guess
  double sigma0_init = frac_of_mode - mu0_init;
    
  // Guess mixture weights 
  double w0 = 0.6; // baseline weight
  double w1 = 0.4; // tail weight

  // Baseline Gaussian parameters
  double mu0  = mu0_init;
  double var0 = sigma0_init * sigma0_init;

  // One-sided exponential params
  // Cutoff t anchored at the baseline mean - re-anchored each iteration.
  double t = mu0_init;

  // Initial lambda (1/scale). Rough guess from tail reach.
  double lambda = 1.0 / std::max(50.0, (t - min_adc) / 6.0);

  // Parameter floors / bounds for numerical stability
  const double var_floor = this->var_floor; // existing floor
  const double lambda_min = 1e-6; // ~ very long tail
  const double lambda_max = 1e+1; // ~ very short tail

  // Previous log likeliehood
  double prev_ll = -std::numeric_limits<double>::infinity();
  // Convergence bool
  bool convergence = false;

  // Get total counts
  const double total = total_hist_counts;

  // Loop over number of EM iterations
  for (int iter = 0; iter < max_iters; ++iter) {
    
    // Stats
    // Effective counts for gaussian / exponential
    double n0 = 0.0, n1 = 0.0;       
    double sum0 = 0.0; // ∑ r0 c x
    double sumxx0 = 0.0; // ∑ r0 c x^2
    double sum_x_minus_t = 0.0; // ∑ r1 c (x - t) 
    double ll = 0.0; // log-likelihood

    // Expectation step

    // Loop over histogram bins
    for (int i = 0; i < nbins; ++i) {

      // Get counts in bins
      const double c = hist_counts[i];

      // If no counts, continue
      if (c == 0.0) continue;

      // Get bin centre
      const double x = hist_bin_centres[i];

      // Baseline Gaussian likelihood (with variance floor)
      const double p0 = w0 * gaussian_pdf(x, mu0, std::max(var0, var_floor));

      // One-sided exponential likelihood; zero for x > t
      double p1 = 0.0;
      if (x <= t) {
        // f_exp(x|lambda,t) = lambda * exp(lambda * (x - t)), x <= t
        // Multiply by mixture weight
        p1 = w1 * lambda * std::exp(lambda * (x - t));
      }

      // Total mixture density (+tiny epsilon for log/ratio safety)
      const double den = p0 + p1 + 1e-300;

      // Responsibilities 
      const double r0 = p0 / den;
      const double r1 = 1.0 - r0;

      // Convert to effective counts
      const double cr0 = c * r0;
      const double cr1 = c * r1;

      // Accumulate stats
      n0 += cr0;
      n1 += cr1;
      sum0   += cr0 * x;
      sumxx0 += cr0 * x * x;

      // If (x - t) ≤ 0
      if (x <= t) {
        // Sum num is negative --> will produce positive lambda below.
        sum_x_minus_t += cr1 * (x - t);
      }

      // Log-likelihood contribution (histogram-weighted)
      ll += c * std::log(den);
      
    } // End loop over bins

    // Maxmimisationstep
    
    // Guard against degeneracy
    if (n0 < 1e-12 || n1 < 1e-12) {
      n0 = std::max(n0, 1e-12);
      n1 = std::max(n1, 1e-12);
    }

    // Update mixture weights
    w0 = n0 / total;
    w1 = n1 / total;

    // Update Gaussian mean / variance
    mu0  = sum0 / n0;
    var0 = std::max(var_floor, (sumxx0 / n0) - mu0 * mu0);

    // Update exponential rate 
    // lambda = - N1 / Σ r1 c (x - t), with Σ r1 c (x - t) < 0
    {
      const double denom = sum_x_minus_t; // negative or ~0
      if (denom < -1e-12) {
        lambda = - n1 / denom;
      }
      // Avoid runaway
      if (!(lambda > 0.0)) lambda = 1.0 / 300.0;
      lambda = std::min(std::max(lambda, lambda_min), lambda_max);
    }

    // Re-anchor exponential cutoff to the current baseline
    t = mu0;

    // Convergence check 
    if (std::abs(ll - prev_ll) < std::max(1.0, std::abs(ll)) * tol) {
      // If converged, break
      convergence = true;
      break;
    }

    // Store last LL
    prev_ll = ll;
    
  }

  // If not converged, hard exit
  if (!convergence) {
    logger->log("[Baseline] EM (Gauss+Exp) did not converge in " +
                std::to_string(max_iters) + " iterations.",0);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    baseline_fit result = {0,0,0,0,0,0};
    return result;
  }

  // Convert exponential parameters to (mean, rms)
  // For f(x|lambda,t) on (-inf, t]: mean  = t - 1/lambda, sigma = 1/lambda
  const double sigma0 = std::sqrt(var0);
  const double mu_exp = t - 1.0 / lambda;
  const double sig_exp = 1.0 / lambda;

  // Baseline fit result
  baseline_fit result = {w0, mu0, sigma0, w1, t, lambda};
  
  // Return outputs
  return result;
  
}


// Estimate percentile position from histogram
double Baseline::hist_percentile(const std::vector<uint64_t>& hist_counts,
                                 const uint64_t total_hist_counts, const double p01) {

  // Total number of counts to accumulate to reach the desired percentile.
  const double target = p01 * total_hist_counts;

  // Cumulative count tracker
  double s = 0.0;                        

  // Loop through bins from low to high ADC values
  for (int i = 0; i < nbins; ++i) {

    // Cumulative count before this bin
    double prev = s;

    // Add this bin’s counts
    s += hist_counts[i];                  
 
    // Once the cumulative sum passes the target percentile count,
    // we've found the bin containing the percentile.
    if (s >= target) {
      // Fractional position within this bin between its start and end
      const double frac = (target - prev) /
                    std::max(1e-12, static_cast<double>(hist_counts[i]));
   
      // Compute corresponding ADC value:
      return hist_bin_centres[i] - 0.5 * width + frac * width;
    }
  }

  // If the loop never hits the target (e.g., due to rounding),
  // return the center of the last bin as a fallback.
  return hist_bin_centres.back();

}

double Baseline::mode_fraction(const std::vector<uint64_t>& hist_counts,
                                 const uint64_t mode_bin, const double frac) {

  // Get the target value for fraction of the mode
  const double target = hist_counts[mode_bin] * frac;

  // Hist counts
  double c_curr;
  double c_prev = static_cast<double>(hist_counts[mode_bin]);

  // Loop through bins from low to high ADC values
  for (int i = mode_bin+1; i < nbins; ++i) {

    // Get bin count 
    c_curr = static_cast<double>(hist_counts[i]);

    // If below target
    if (c_curr <= target) {

      // Linear interpolation
      double interp = (c_prev - target) / (c_prev - c_curr);

      return hist_bin_centres[i-1] + interp * width;
    } 
    
    // Store as prev
    c_prev = c_curr;
  }

  // Return the center of the last bin as a fallback.
  return hist_bin_centres.back();

}
