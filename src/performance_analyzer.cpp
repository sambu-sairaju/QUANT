#include "performance_monitor.hpp"
#include <iostream>
#include <iomanip>

namespace goquant {

class PerformanceAnalyzer {
public:
    static void printLatencyReport() {
        auto& monitor = PerformanceMonitor::getInstance();
        
        std::cout << "\n╔═══════════════════════════════════════════╗" << std::endl;
        std::cout << "║        PERFORMANCE ANALYSIS REPORT         ║" << std::endl;
        std::cout << "╠═══════════════════════════════════════════╣" << std::endl;
        
        printMetric("Order Placement", monitor.getStats("order_placement"));
        printMetric("Market Data Processing", monitor.getStats("market_data"));
        printMetric("WebSocket Message", monitor.getStats("websocket_message"));
        printMetric("Trading Loop", monitor.getStats("trading_loop"));
        
        std::cout << "╚═══════════════════════════════════════════╝" << std::endl;
    }

private:
    static void printMetric(const std::string& name, const PerformanceMonitor::LatencyStats& stats) {
        std::cout << "║ " << std::left << std::setw(20) << name << "                    ║" << std::endl;
        std::cout << "║ ---------------------------------------- ║" << std::endl;
        std::cout << "║   Min: " << std::right << std::setw(10) << std::fixed << std::setprecision(3) 
                 << stats.min << " ms                    ║" << std::endl;
        std::cout << "║   Max: " << std::right << std::setw(10) << stats.max << " ms                    ║" << std::endl;
        std::cout << "║   Avg: " << std::right << std::setw(10) << stats.avg << " ms                    ║" << std::endl;
        std::cout << "║   P95: " << std::right << std::setw(10) << stats.p95 << " ms                    ║" << std::endl;
        std::cout << "║   Samples: " << std::right << std::setw(6) << stats.sample_count 
                 << "                          ║" << std::endl;
        std::cout << "╠═══════════════════════════════════════════╣" << std::endl;
    }
};

} // namespace goquant 