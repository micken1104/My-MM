#include <iostream>
#include <fstream>
#include <iomanip>
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
    double imbalance = 0.0;
    double diff = 0.0;     // 追加
    double volume = 0.0;   // 追加（現在は0固定のようですが、器を作っておきます）
    std::chrono::steady_clock::time_point timestamp;
};

// 1分後の結果を待つためのデータ構造
struct TradeData {
    std::string symbol;
    double entry_price;
    double entry_imbalance;
    double entry_diff;
    double entry_volume;
    std::chrono::steady_clock::time_point entry_time;
};


void save_to_csv(const TradeData& data, double exit_price) {
    std::ofstream file("market_data.csv", std::ios::app);
    double price_change = (exit_price - data.entry_price) / data.entry_price * 100.0;
    file << data.symbol << "," << data.entry_imbalance << "," << data.entry_diff << "," 
         << data.entry_volume << "," << price_change << "\n";
}

void check_pending_trades(const std::map<std::string, double>& current_prices, std::vector<TradeData>& pending_checks) {
    auto now = std::chrono::steady_clock::now();
    for (auto it = pending_checks.begin(); it != pending_checks.end(); ) {
        if (now - it->entry_time >= std::chrono::seconds(60)) {
            if (current_prices.count(it->symbol)) {
                save_to_csv(*it, current_prices.at(it->symbol));
                fmt::print(fg(fmt::color::magenta), "📊 [DATA SAVED] {} (Change: {:.2f}%)\n", it->symbol, (current_prices.at(it->symbol) - it->entry_price)/it->entry_price*100);
            }
            it = pending_checks.erase(it);
        } else {
            ++it;
        }
    }
}


void scan_market(std::map<std::string, MarketState>& history, std::vector<TradeData>& pending_checks) {
    httplib::Client cli("https://api.binance.com");
    cli.set_follow_location(true);
    // タイムアウト設定を追加（これでフリーズを防ぐ）
    cli.set_connection_timeout(5); 
    cli.set_read_timeout(5);
    auto res = cli.Get("/api/v3/ticker/bookTicker");

    if (res && res->status == 200) {
        json data = json::parse(res->body);
        auto now = std::chrono::steady_clock::now();
        std::vector<std::string> targets = {"BTCUSDT", "ETHUSDT", "SOLUSDT", "BNBUSDT", "XRPUSDT"};
        std::map<std::string, double> current_prices;

        for (auto& symbol_data : data) {
            std::string symbol = symbol_data["symbol"];
            if (std::find(targets.begin(), targets.end(), symbol) == targets.end()) continue;

            double bid_p = std::stod(symbol_data["bidPrice"].get<std::string>());
            double ask_p = std::stod(symbol_data["askPrice"].get<std::string>());
            double mid_p = (bid_p + ask_p) / 2.0;
            current_prices[symbol] = mid_p;

            // インバランス計算
            double bid_vol = std::stod(symbol_data["bidQty"].get<std::string>());
            double ask_vol = std::stod(symbol_data["askQty"].get<std::string>());
            if (bid_vol + ask_vol == 0) continue;

            double current_imbalance = (bid_vol - ask_vol) / (bid_vol + ask_vol);
            double diff = current_imbalance - history[symbol].imbalance;
            double vol = 0.0; // 現在のコードでは 0.0 固定

            pending_checks.push_back({
                symbol,
                mid_p,
                current_imbalance,
                diff,
                vol,
                now
            });
            history[symbol].imbalance = current_imbalance;
            history[symbol].diff = diff;
            history[symbol].volume = vol;
            history[symbol].timestamp = now;
        }
        check_pending_trades(current_prices, pending_checks);
    }
}

