#include "OrderBook.h"
#include <fmt/core.h>
#include <thread>
#include <chrono>

int main() {
    fmt::print("Multi-file Project Started!\n");
    while (true) {
        //update_order_book();
        //update_order_book_depth();
        scan_market(history);
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }
    return 0;
}