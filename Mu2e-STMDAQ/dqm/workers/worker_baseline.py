import numpy as np
import math
import psutil
import queue
import os
import time
import dash
import plotly.graph_objects as go
from scipy.stats import norm
from datetime import datetime, timedelta
from utils.shared_memory import SharedMemoryReader
from utils.config import get_xml_node_value

# For noise data
fADC = float(get_xml_node_value("fw/fADC")) # ADC frequency (MHz)
#noise_period = float(get_xml_node_value("dqm/baseline/noise_length")) # Noise period (us)
noise_size = float(get_xml_node_value("buffers/baseline_size")) # Noise size (bytes)
noise_len = int(noise_size/2) # Noise length (ADCs)
noise_period = noise_len/fADC
x_times = [i / fADC for i in range(noise_len)]  # µs

# For baseline hist
bin_num = int(get_xml_node_value("baseline/hist/bin_num"))
min_adc = int(get_xml_node_value("baseline/hist/min_adc"))
max_adc = int(get_xml_node_value("baseline/hist/max_adc"))
window_s = float(get_xml_node_value("baseline/hist/window_period"))
bin_width = (max_adc - min_adc)/bin_num
bin_edges = np.linspace(min_adc,max_adc,bin_num)
bin_centres = np.linspace(min_adc+bin_width/2,max_adc-bin_width/2,bin_num)

# Define the expected shared memory layout
# <Q Q d*6 Q => (uint64_t, uint64_t, 6 doubles, data, uint64_t)
#STRUCT_FORMAT = f"<Q Q dddddd {noise_len}h Q"
HEADER_FORMAT = "<QQ"+("d"*10)+"QQ"

# Shared memory block reader for ADC baseline
reader = SharedMemoryReader("/dqm_adc_baseline", HEADER_FORMAT)

def run_worker(task_queue, result_queue, core_id):
    # Pin worker to CPU core
    p = psutil.Process(os.getpid())
    try:
        p.cpu_affinity([core_id])
        print(f"DQM baseline worker pinned to core {core_id}")
    except Exception as e:
        print(f"DQM baseline worker could no set affinity: {e}")

    while True:
        try:
            task = task_queue.get(timeout=0.5)
        except queue.Empty:
            continue

        if task is None:
            print(f"DQM baseline worker received shutdown signal. Exiting...")
        
        history = task
        try:
            status, time_plot, hist_plot, noise_plot, noise_fft, updated_history = draw_baseline(history)
            result_queue.put((status, time_plot, hist_plot, noise_plot, noise_fft, history))
        except Exception as e:
            print(f"Error in baseline worker: {e}")
            result_queue.put((None, None, None, None, None, history))


def initialise_plots():
    # Empty layout figures with titles and axes but no data

    empty_time_series = go.Figure(layout=dict(
        title=dict(
            text="Average of ADC baseline noise over run",
            font=dict(size=20, family="Arial", color="black"),
            x=0.5,  # Center title (optional)
            xanchor="center"
        ),
        xaxis=dict(
            title=dict(
                text="Time",
                font=dict(size=18)
            ),
            tickformat="%H:%M:%S.%f",
        ),
        yaxis=dict(
            title=dict(
                text="ADC Counts",
                font=dict(size=18)
            )
        ),
        margin={"l": 40, "r": 10, "t": 100, "b": 40},
        legend=dict(
            orientation="h",
            yanchor="bottom",
            y=1.02,
            xanchor="center",
            x=0.4,
            font=dict(size=12)
        )
    ))
    
    empty_noise = go.Figure(layout=dict(
        title=dict(
            text=f"Noise Sample: {noise_period:.2f} us from",
            font=dict(size=20, family="Arial", color="black"),
            x=0.5,  # Center title (optional)
            xanchor="center"
        ),
        xaxis=dict(
            title=dict(
                text="Time (µs)",
                font=dict(size=18)
            ),
            tickformat="%H:%M:%S.%f",
            range=[min(x_times), max(x_times)]
        ),
        yaxis=dict(
            title=dict(
                text="ADC Counts",
                font=dict(size=18)
            ),
            range = [-300, 300]
        ),
        margin={"l": 40, "r": 10, "t": 100, "b": 40},
    ))

    empty_fft = go.Figure(layout=dict(
        title=dict(
            text=f"Noise FFT: {noise_period:.2f} us from",
            font=dict(size=20, family="Arial", color="black"),
            x=0.5,  # Center title (optional)
            xanchor="center"
        ),
        xaxis=dict(
            title=dict(
                text="FFT (Hz)",
                font=dict(size=18)  
            ),
            tickformat="%H:%M:%S.%f",
            range=[0, fADC/2]
        ),
        yaxis=dict(
                title=dict(
                text="FFT magnitude [a.u.]",
                font=dict(size=18)
            ),
            range = [0, 5000]
        ),
        margin={"l": 40, "r": 10, "t": 100, "b": 40},
    ))

    empty_hist = go.Figure(layout=dict(
        title=dict(
            #text=f"Baseline histogram and fit using {window_s:.1f} us window",
            text=f"Baseline histogram and fit since start of run",
            font=dict(size=20, family="Arial", color="black"),
            x=0.5,  # Center title (optional)
            xanchor="center"
        ),
        xaxis=dict(
            title=dict(
                text="ADC counts",
                font=dict(size=18)
            ),
            tickformat="%H:%M:%S.%f",
            range=[min_adc, max_adc]
        ),
        yaxis=dict(
                title=dict(
                text="Number per bin",
                font=dict(size=18),
            ),
            #range = [0,1e9]
            type="log",
            range = [0, 9],
            minor = dict(dtick="D1")
        ),
        margin={"l": 40, "r": 10, "t": 100, "b": 40},
        legend=dict(
            orientation="h",
            yanchor="bottom",
            y=1.02,
            xanchor="center",
            x=0.4,
            font=dict(size=16)
        )
    ))

    return empty_time_series, empty_hist, empty_noise, empty_fft

def normal(x, mu, sigma):
    if sigma == 0:
        norm = 0
    else:
        norm = 1 / (np.sqrt(2 * np.pi) * sigma)
    exp = np.exp( -(x-mu)**2 / (2 * sigma**2) )
    return norm*exp


def draw_baseline(history):

    empty_time_series, empty_hist, empty_noise, empty_fft = initialise_plots()

    # Read the memory block and extract data
    data, status = reader.read_updated()
    if not data:
        return status, empty_time_series, empty_hist, empty_noise, empty_fft, history 
    elif len(data) != 15 and data == "no update":
        return status, *([dash.no_update] * 5)

    (   timestamp_ns,
        mean_prev,
        rms_prev,
        mean_avg,
        rms_avg,
        mean_curr,
        rms_curr,
        fit_mean_all,
        fit_sigma_all,
        fit_mean_window,
        fit_sigma_window,
        baseline_nbins,
        noise_len,
        baseline_hist,
        noise_samples
    ) = data

    # Convert timestamp to datetime object
    dt = datetime.fromtimestamp(timestamp_ns / 1e9)

    # Append new values to time series lists
    history["x"].append(dt)
    history["prev"].append(mean_prev)
    history["prev_rms"].append(rms_prev)
    #history["avg"].append(mean_avg)
    #history["avg_rms"].append(rms_avg)
    #history["curr"].append(mean_curr)
    #history["curr_rms"].append(rms_curr)
    history["avg"].append(fit_mean_all)
    history["avg_rms"].append(fit_sigma_all)
    history["curr"].append(fit_mean_window)
    history["curr_rms"].append(fit_sigma_window)
    history["last_timestamp"] = timestamp_ns

    # (Optional) Keep only last N points to limit graph load
    max_points = 100
    for key in history:
        if isinstance(history[key], list):
            history[key] = history[key][-max_points:]

    # Precompute bands
    hx = history["x"]
    prev_upper = [m + r for m, r in zip(history["prev"], history["prev_rms"])]
    prev_lower = [m - r for m, r in zip(history["prev"], history["prev_rms"])]

    # Get last values
    last_prev = history["prev"][-1]
    last_prev_rms = history["prev_rms"][-1]
    last_avg = history["avg"][-1]
    last_avg_rms = history["avg_rms"][-1]
    last_curr = history["curr"][-1]
    last_curr_rms = history["curr_rms"][-1]

    # Copy empty layout
    time_fig = go.Figure(empty_time_series)
    # Previous band
    time_fig.add_trace(go.Scattergl(x=hx, # upper line
                               y=prev_upper,
                               mode='lines',
                               line=dict(color='blue', dash='dash'),
                               name=f"Reference<br>= {last_prev:.2f} ± {last_prev_rms:.2f}\u00A0\u00A0\u00A0"))
    time_fig.add_trace(go.Scattergl(x=hx, # lower line
                               y=prev_lower,
                               mode='lines',
                               line=dict(color='blue', dash='dash'),
                               showlegend=False, name=""))
        
     # Average since start of run
    time_fig.add_trace(go.Scatter(x=hx,
                             y=history["avg"],
                             mode='lines+markers',
                             name=f"Rolling average<br>= {last_avg:.2f} ± {last_avg_rms:.2f}\u00A0\u00A0\u00A0",
                             line=dict(dash='dash'),
                             marker=dict(color='green', size=6),
                             error_y=dict(type='data', array=history["avg_rms"],
                                          visible=True)))

    # Current run points
    time_fig.add_trace(go.Scatter(x=hx,
                             y=history["curr"],
                             mode='markers',
                             name=f"{window_s:.1f} s window average<br>= {last_curr:.2f} ± {last_curr_rms:.2f}\u00A0\u00A0\u00A0",
                             marker=dict(color='red', size=6),
                             error_y=dict(type='data', array=history["curr_rms"],
                                          visible=True)))

    # --- Fill noise graph ---
    noise_figure = go.Figure(empty_noise)
    noise_figure.add_trace(go.Scatter(
        x=x_times,
        y=noise_samples,
        mode='lines',
        name='Noise samples'
    ))
    noise_figure.update_layout(
        title=dict(
            text=f"ADC Noise Samples: {noise_period:.2f} us from {dt.strftime('%H:%M:%S.')}{dt.microsecond // 10000:02d}",
        ),
        xaxis=dict(
            range=[min(x_times), max(x_times)]
        ),
        yaxis=dict(
            autorange = "min"
        )
    )

    # --- Fill FFT plot ---
    noise_fft = np.fft.fft(noise_samples)
    noise_fft_mag = np.absolute(noise_fft)
    noise_freqs = np.fft.fftfreq(noise_len, d=1/(fADC*1e6))
    noise_freqs_MHz = noise_freqs[:noise_len//2] * 1e-6

    fft_figure = go.Figure(empty_fft)
    fft_figure.update_layout(
            yaxis=dict(
                autorange=True
            )
    )
    fft_figure.add_trace(go.Scatter(
        x = noise_freqs_MHz,
        y = noise_fft_mag,
        mode = 'lines',
        name = 'FFT'
    ))

    # --- Draw baseline histogram and fit result ---
    gauss_x = np.arange(fit_mean_all - 3*fit_sigma_all, fit_mean_all + 4*fit_sigma_all, 20)

    # Gauss fit to entire run normalised to current window
    gauss_all = normal(gauss_x, fit_mean_all, fit_sigma_all)
    gauss_all_scaled = bin_width * np.sum(baseline_hist) * gauss_all

    # Gauss fit to window
    gauss_window = normal(gauss_x, fit_mean_window, fit_sigma_window)
    gauss_window_scaled = bin_width * np.sum(baseline_hist) * gauss_window

    baseline_figure = go.Figure(empty_hist)
    baseline_figure.add_trace(go.Scatter(
        x = gauss_x,
        y = gauss_all_scaled,
        mode = 'lines',
        name = 'Window fit'
    ))
    baseline_figure.add_trace(go.Bar(
        x = bin_centres,
        y = baseline_hist,
        name = "Window histogram"
    ))
    baseline_figure.update_layout(
            yaxis=dict(
                autorange=True
            )
    )
    
    return status, time_fig, baseline_figure, noise_figure, fft_figure, history


