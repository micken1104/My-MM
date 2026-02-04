#include "OrderBook.h"
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <fmt/core.h>
#include <fmt/color.h>
#include <string>

using json = nlohmann::json;

void update_order_book() {
    httplib::Client cli("https://api.binance.com");
    auto res = cli.Get("/api/v3/depth?symbol=BTCUSDT&limit=20");
    //get 20 datas of each transit at BTCUSDT

    if (res && res->status == 200) {
        json data = json::parse(res->body);

        // calc thickness
        double total_bid_vol = 0.0;
        double total_ask_vol = 0.0;
        for (const auto& bid : data["bids"]) {
            total_bid_vol += std::stod(bid[1].get<std::string>());
        }
        for (const auto& ask : data["asks"]) {
            total_ask_vol += std::stod(ask[1].get<std::string>());
        }


        // extract best data(on Binance 0 is best)
        auto best_ask = std::stod(data["asks"][0][0].get<std::string>());
        auto best_bid = std::stod(data["bids"][0][0].get<std::string>());

        //calcrate spread
        double spread = best_ask - best_bid;
        double spread_pct = (spread / best_bid) * 100.0;

        // show
        fmt::print("\n====================================\n");
        fmt::print("Best Ask (Sell): {:>10.2f}\n", best_ask);
        fmt::print(fg(fmt::color::yellow), "SPREAD         : ${:>10.2f} ({:.4f}%)\n", spread, spread_pct);
        fmt::print("Best Bid (Buy) : {:>10.2f}\n", best_bid);
        fmt::print("====================================\n");

        // quantity
        double ask_vol = std::stod(data["asks"][0][1].get<std::string>());
        double bid_vol = std::stod(data["bids"][0][1].get<std::string>());
        fmt::print("Volume - Ask: {:.3f} | Bid: {:.3f}\n", ask_vol, bid_vol);
    } else {
        fmt::print(stderr, "Error: Could not fetch depth data.\n");
    }
}