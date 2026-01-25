#include "OrderBook.h"
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <fmt/core.h>
#include <fmt/color.h>

using json = nlohmann::json;

void update_order_book() {
    httplib::Client cli("https://api.binance.com");
    auto res = cli.Get("/api/v3/depth?symbol=BTCUSDT&limit=5");
    //get five datas of each transit at BTCUSDT

    if (res && res->status == 200) {
        json data = json::parse(res->body);
        fmt::print("\n--- BTC/USDT Order Book ---\n");

        auto asks = data["asks"];
        fmt::print(fg(fmt::color::indian_red), " [ Asks ]\n");
        for (int i = 4; i >= 0; i--) {
            fmt::print("  {}: {}\n", asks[i][0].get<std::string>(), asks[i][1].get<std::string>());
        }

        auto bids = data["bids"];
        fmt::print(fg(fmt::color::light_green), " [ Bids ]\n");
        for (int i = 0; i < 5; i++) {
            fmt::print("  {}: {}\n", bids[i][0].get<std::string>(), bids[i][1].get<std::string>());
        }
    }
}