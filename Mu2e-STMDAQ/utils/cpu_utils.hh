#ifndef CPU_UTILS_HH
#define CPU_UTILS_HH

#include <pthread.h>
#include <sched.h>

#include <atomic>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

// Config code
#include "Mu2e-STMDAQ/config/config.hh"

// Forward declaration
class AsyncLogger;

// NUMA socket identifier
enum class Socket {
  Socket0,
  Socket1
};

// Application role used to select the appropriate CPU affinity configuration from the shared XML
enum class CpuRole {
  Standalone,
  ArtDAQ
};

// Singleton utility class for managing CPU thread pinning
class cpu_utils {

private:

  // Private constructor; only accessible by getInstance()
  explicit cpu_utils(const Config& cfg_, CpuRole role_);

  // Low-level function to pin thread to a specified core
  void pin_thread_to_core(size_t core_id, const std::string& name);

  // Store reference to the Config instance
  const Config& cfg;

  // Application type (Standalone or ArtDAQ) Used to determine which CPU affinity settings should be loaded from the XML configuration
  CpuRole role;

  // First logical core to assign from within the configured socket
  size_t starting_core_id;

  // Total number of available hardware cores reported by the operating system
  size_t max_cores;

  // Atomic counter for round-robin assignment of logical cores within the selected socket
  std::atomic<size_t> next_core_id;

  // Mutex to protect access to used_cores set
  std::mutex tracking_mutex;

  // Set of all physical CPU cores that have been assigned so far
  std::unordered_set<size_t> used_cores;

  // NUMA socket assigned to this application
  //Socket socket_id;
  int socket_id;

  // Physical CPU IDs available to the configured NUMA socket
  std::vector<int> allowed_cores;

  // Get logger with mutex to protect setting/appending
  std::mutex log_mutex;
  std::shared_ptr<AsyncLogger> logger;
  std::vector<std::pair<std::string,int>> pending_logs;

  // Singleton instance pointer
  static std::shared_ptr<cpu_utils> instance;

  // Flag to ensure singleton is initialized only once
  static std::once_flag init_flag;

  // Verified physical CPU mappings for NUMA socket 0
  static const std::vector<int> socket0_cores;

  // Verified physical CPU mappings for NUMA socket 1
  static const std::vector<int> socket1_cores;

  void log(const std::string& msg, int level);

public:

  // Delete copy and move constructors/operators to enforce singleton pattern
  cpu_utils(const cpu_utils&) = delete;
  cpu_utils& operator=(const cpu_utils&) = delete;
  cpu_utils(cpu_utils&&) = delete;
  cpu_utils& operator=(cpu_utils&&) = delete;

  // Static method to access the singleton instance and initialise the CPU affinity manager for the specified application role
  static std::shared_ptr<cpu_utils> getInstance(const Config& cfg, CpuRole role);

  // Assigns the calling thread to the next available CPU core within the configured NUMA socket and returns the physical CPU core ID
  size_t get_next_core(const std::string& name);

  void set_logger(std::shared_ptr<AsyncLogger> l);
  void clear_logger() {
    std::lock_guard<std::mutex> lock(log_mutex);
    logger = nullptr;
    pending_logs.clear();
  }

  void reset() {
    std::lock_guard<std::mutex> lock(tracking_mutex);
    next_core_id.store(0);
    used_cores.clear();
  }

  // Destructor
  ~cpu_utils() {
    log("CPU utils shutting down. Cores assigned: " +
        std::to_string(next_core_id.load()) +
        ", unique cores used: " +
        std::to_string(used_cores.size()),1);

    //std::cout << "cpu_utils destructor called.\n";
  }

};

#endif // CPU_UTILS_HH
