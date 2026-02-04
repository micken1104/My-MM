#include "OrderBook.h"
#include "ScanMarket.h"
#include <fmt/core.h>
#include <thread>
#include <chrono>
#include <map>
#include <string>


int main() {
    std::map<std::string, MarketState> history;
    std::vector<TradeData> pending_checks;

    while (true) {
        scan_market(history, pending_checks);
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }
    return 0;
}