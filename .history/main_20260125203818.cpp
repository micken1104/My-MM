#include "OrderBook.h"
#include "ScanMarket.h"
#include <fmt/core.h>
#include <thread>
#include <chrono>

#include <map>
#include <string>

struct MarketState {
    double imbalance;
    std::chrono::steady_clock::time_point timestamp;
};

int main() {
    fmt::print("Multi-file Project Started!\n");
    while (true) {
        //update_order_book();
        //update_order_book_depth();
        std::map<std::string, MarketState> history;
        scan_market(history);
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }
    return 0;
}