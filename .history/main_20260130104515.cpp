#include "OrderBook.h"
#include "ScanMarket.h"
#include <fmt/core.h>
#include <thread>
#include <chrono>

#include <map>
#include <string>


int main() {
    std::map<std::string, MarketState> history;
    while (true) {
        //update_order_book();
        //update_order_book_depth();
        std::map<std::string, MarketState> history;
    std::vector<TradeData> pending_checks;
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }
    return 0;
}