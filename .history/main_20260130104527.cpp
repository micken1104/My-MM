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
        while (true) {
        scan_market(history, pending_checks); // 参照渡しで更新し続ける
        std::this_thread::sleep_for(std::chrono::seconds(2)); // スキャン間隔
    }
    return 0;
    }
    return 0;
}