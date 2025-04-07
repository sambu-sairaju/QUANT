#pragma once

#include <chrono>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <algorithm>
#include <numeric>
#include <spdlog/spdlog.h>

namespace goquant {

class PerformanceMonitor {
public:
    struct LatencyStats {
        double min = 0;
        double max = 0;
        double avg = 0;
        double p95 = 0;
        size_t sample_count = 0;
    };

    static PerformanceMonitor& getInstance() {
        static PerformanceMonitor instance;
        return instance;
    }

    // Start timing a specific operation
    void startOperation(const std::string& operation_name) {
        auto now = std::chrono::high_resolution_clock::now();
        std::lock_guard<std::mutex> lock(mutex_);
        start_times_[operation_name] = now;
    }

    // End timing and record latency
    void endOperation(const std::string& operation_name) {
        auto end = std::chrono::high_resolution_clock::now();
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (start_times_.find(operation_name) != start_times_.end()) {
            auto start = start_times_[operation_name];
            double duration = std::chrono::duration<double, std::milli>(end - start).count();
            latencies_[operation_name].push_back(duration);
        }
    }

    // Get statistics for an operation
    LatencyStats getStats(const std::string& operation_name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        LatencyStats stats;
        
        auto it = latencies_.find(operation_name);
        if (it == latencies_.end() || it->second.empty()) {
            return stats;
        }

        const auto& samples = it->second;
        stats.sample_count = samples.size();
        
        // Calculate min, max, avg
        stats.min = *std::min_element(samples.begin(), samples.end());
        stats.max = *std::max_element(samples.begin(), samples.end());
        stats.avg = std::accumulate(samples.begin(), samples.end(), 0.0) / samples.size();
        
        // Calculate P95
        std::vector<double> sorted_samples = samples;
        std::sort(sorted_samples.begin(), sorted_samples.end());
        size_t p95_index = static_cast<size_t>(samples.size() * 0.95);
        stats.p95 = sorted_samples[p95_index];
        
        return stats;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        start_times_.clear();
        latencies_.clear();
    }

    // Memory tracking
    void recordMemoryUsage(size_t bytes_used) {
        std::lock_guard<std::mutex> lock(mutex_);
        memory_samples_.push_back(bytes_used);
    }

    // Thread metrics
    void recordThreadMetrics(const std::string& thread_name, size_t cpu_usage) {
        std::lock_guard<std::mutex> lock(mutex_);
        thread_metrics_[thread_name].cpu_usage = cpu_usage;
    }

private:
    PerformanceMonitor() = default;
    
    mutable std::mutex mutex_;
    std::map<std::string, std::chrono::high_resolution_clock::time_point> start_times_;
    std::map<std::string, std::vector<double>> latencies_;
    std::vector<size_t> memory_samples_;
    
    struct ThreadMetrics {
        size_t cpu_usage;
    };
    std::map<std::string, ThreadMetrics> thread_metrics_;
};

} // namespace goquant 