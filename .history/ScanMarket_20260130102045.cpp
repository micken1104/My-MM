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

            if (symbol == "BNBBTC") {
                double diff = current_imbalance - history[symbol].imbalance;
                fmt::print("DEBUG: {:<10} | Imb: {:>5.2f} | Diff: {:>+5.2f}\n", 
                        symbol, current_imbalance, diff);
            }

            // 前回のデータがあれば比較
            if (history.count(symbol)) {
                double diff = current_imbalance - history[symbol].imbalance;

                // インバランスが0.5以上急激にプラスに振れた銘柄を表示
                if (diff > 0.5) { 
                    fmt::print(fg(fmt::color::green) | fmt::emphasis::bold, 
                               "🔥 [PUMP ALERT] {:<10} | Imbalance: {:>5.2f} (Shift: {:>+5.2f})\n", 
                               symbol, current_imbalance, diff);
                }
            }

            // 履歴を更新
            history[symbol].imbalance = current_imbalance;
        }
    }
}


// 1分後の結果を待つためのデータ構造
struct TradeData {
    std::string symbol;
    double entry_price;
    double entry_imbalance;
    double entry_diff;
    double entry_volume;
    std::chrono::steady_clock::time_point entry_time;
};

std::vector<TradeData> pending_checks;

void save_to_csv(const TradeData& data, double exit_price) {
    std::ofstream file("market_data.csv", std::ios::app); // 追記モード
    double price_change = (exit_price - data.entry_price) / data.entry_price * 100.0;
    
    // CSVのヘッダー：Symbol, Imb, Diff, Volume, PriceChange%
    file << data.symbol << ","
         << data.entry_imbalance << ","
         << data.entry_diff << ","
         << data.entry_volume << ","
         << price_change << "\n";
}

// scan_market関数の中で呼び出す
void check_pending_trades(const std::map<std::string, double>& current_prices) {
    auto now = std::chrono::steady_clock::now();
    for (auto it = pending_checks.begin(); it != pending_checks.end(); ) {
        // 60秒経過したかチェック
        if (now - it->entry_time >= std::chrono::seconds(60)) {
            if (current_prices.count(it->symbol)) {
                save_to_csv(*it, current_prices.at(it->symbol));
                fmt::print(fg(fmt::color::magenta), "📊 [DATA SAVED] {}\n", it->symbol);
            }
            it = pending_checks.erase(it); // リストから削除
        } else {
            ++it;
        }
    }
}