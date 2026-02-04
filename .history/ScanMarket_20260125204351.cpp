#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <fmt/core.h>
#include <fmt/color.h>

using json = nlohmann::json;

struct MarketState {
    double imbalance;
    std::chrono::steady_clock::time_point timestamp;
};

// 全銘柄の板情報を一括取得して、インバランスの変化を追う
void scan_market(std::map<std::string, MarketState>& history) {
    httplib::Client cli("https://api.binance.com");
    auto res = cli.Get("/api/v3/ticker/bookTicker"); // 全銘柄の最良気配を一括取得

    if (res && res->status == 200) {
        json data = json::parse(res->body);
        fmt::print("Scanned {} symbols...\n", data.size());
        auto now = std::chrono::steady_clock::now();

        for (auto& symbol_data : data) {
            std::string symbol = symbol_data["symbol"];
            

            double bid_vol = std::stod(symbol_data["bidQty"].get<std::string>());
            double ask_vol = std::stod(symbol_data["askQty"].get<std::string>());

            if (bid_vol + ask_vol == 0) continue;
            double current_imbalance = (bid_vol - ask_vol) / (bid_vol + ask_vol);

            // 前回のデータがあれば比較
            if (history.count(symbol)) {
                double diff = current_imbalance - history[symbol].imbalance;

                // インバランスが0.5以上急激にプラスに振れた銘柄を表示
                if (diff > 0.01) { 
                    fmt::print(fg(fmt::color::green) | fmt::emphasis::bold, 
                               "🔥 [PUMP ALERT] {:<10} | Imbalance: {:>5.2f} (Shift: {:>+5.2f})\n", 
                               symbol, current_imbalance, diff);
                }
            }

            // 履歴を更新
            history[symbol] = {current_imbalance, now};
        }
    }
}
