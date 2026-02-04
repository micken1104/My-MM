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

void update_order_book_with_depth() {
    httplib::Client cli("https://api.binance.com");
    // 深さを知るためにlimitを少し増やしてもOK（例: 20）
    auto res = cli.Get("/api/v3/depth?symbol=BTCUSDT&limit=20");

    if (res && res->status == 200) {
        json data = json::parse(res->body);

        double total_bid_vol = 0.0;
        double total_ask_vol = 0.0;

        // 買い板(bids)の合計数量を計算
        for (const auto& bid : data["bids"]) {
            total_bid_vol += std::stod(bid[1].get<std::string>());
        }

        // 売り板(asks)の合計数量を計算
        for (const auto& ask : data["asks"]) {
            total_ask_vol += std::stod(ask[1].get<std::string>());
        }

        // 不均衡（Imbalance）の計算: (Bid - Ask) / (Bid + Ask)
        // 1.0に近いほど買いが厚く、-1.0に近いほど売りが厚い
        double imbalance = (total_bid_vol - total_ask_vol) / (total_bid_vol + total_ask_vol);

        fmt::print("\n--- Order Book Depth Analysis ---\n");
        fmt::print("Total Bid Volume (Buy): {:.3f} BTC\n", total_bid_vol);
        fmt::print("Total Ask Volume (Sell): {:.3f} BTC\n", total_ask_vol);
        
        if (imbalance > 0.2) {
            fmt::print(fg(fmt::color::green), "Sentiment: BULLISH (Buy walls detected)\n");
        } else if (imbalance < -0.2) {
            fmt::print(fg(fmt::color::red), "Sentiment: BEARISH (Sell walls detected)\n");
        } else {
            fmt::print("Sentiment: NEUTRAL\n");
        }
        fmt::print("Imbalance Score: {:.2f}\n", imbalance);
    }
}