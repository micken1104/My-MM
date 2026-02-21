#include "ScanMarket.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <httplib.h>
#include <fmt/core.h>
#include <fmt/color.h>

// 実体（グローバル変数）の定義
using json = nlohmann::json;

static double real_balance = 10000.0; // 真実の残高
static double total_net_profit = 0.0; // 増え方の累計

// 監視対象の銘柄リストを定義
std::vector<std::string> targets = {"ATOMUSDT", "ETHUSDT", "SOLUSDT", "BTCUSDT"};
// 銘柄ごとの設定値を定義
std::map<std::string, TradingConstraints> symbol_settings = {
    {"ATOMUSDT", {0.0025, -0.0008, 60, 0.02}},
    {"ETHUSDT",  {0.0035, -0.0010, 90, 0.03}},
    {"SOLUSDT",  {0.0050, -0.0015, 45, 0.05}}
};

void process_ws_data(const nlohmann::json& j, std::map<std::string, MarketState>& history, std::vector<TradeData>& pending_checks) {
    static std::map<std::string, std::chrono::steady_clock::time_point> last_save_times;
    auto now = std::chrono::steady_clock::now();

    std::string symbol = j["s"];
    double bid_p = std::stod(j["b"].get<std::string>());
    double ask_p = std::stod(j["a"].get<std::string>());
    double bid_v = std::stod(j["B"].get<std::string>());
    double ask_v = std::stod(j["A"].get<std::string>());
    double mid_p = (bid_p + ask_p) / 2.0;

    double current_imbalance = (bid_v - ask_v) / (bid_v + ask_v);
    double diff = current_imbalance - history[symbol].imbalance;

    history[symbol].imbalance = current_imbalance;
    history[symbol].diff = diff;
    history[symbol].volume = 0.0;
    history[symbol].last_price = mid_p;

    if (now - last_save_times[symbol] >= std::chrono::milliseconds(1000)) {
        // --- 修正箇所：個別に代入することで引数エラーを防ぐ ---
        TradeData data;
        data.symbol = symbol;
        data.entry_price = mid_p;
        data.entry_imbalance = current_imbalance;
        data.entry_diff = diff;
        data.entry_volume = 0.0;
        data.entry_time = now;
        data.entry_timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        pending_checks.push_back(data);
        last_save_times[symbol] = now;
    }
}

void save_to_csv(const TradeData& data, double exit_price) {
    // 銘柄ごとにファイル名を分ける
    std::string filename = "data/" + data.symbol + "_market_data.csv";
    
    // ファイルが空（新しく作られる）場合、ヘッダーを書き込む
    std::ifstream check_file(filename);
    bool is_empty = check_file.peek() == std::ifstream::traits_type::eof();
    check_file.close();

    std::ofstream file(filename, std::ios::app);
    // ... ヘッダー判定 ...
    if (is_empty) {
        // ヘッダーの先頭に timestamp を追加
        file << "timestamp,symbol,imbalance,diff,volume,entry_price,exit_price,price_change\n";
    }

    // 計算
    double price_change = (exit_price - data.entry_price) / data.entry_price * 100.0;
    
    // 書き込み（精度を固定するとPythonで扱いやすい）
    file << data.entry_timestamp << "," 
         << data.symbol << "," 
         << std::fixed << std::setprecision(6) << data.entry_imbalance << "," 
         << data.entry_diff << "," 
         << data.entry_volume << "," 
         << data.entry_price << "," 
         << exit_price << "," 
         << (exit_price - data.entry_price) / data.entry_price * 100.0 << "\n";
}

void check_pending_trades(const std::map<std::string, double>& current_prices, std::vector<TradeData>& pending_checks) {
    auto now = std::chrono::steady_clock::now();
    for (auto it = pending_checks.begin(); it != pending_checks.end(); ) {
        if (now - it->entry_time >= std::chrono::seconds(30)) {
            if (current_prices.count(it->symbol)) {
                save_to_csv(*it, current_prices.at(it->symbol));
                //fmt::print(fg(fmt::color::magenta), "📊 [DATA SAVED] {} (Change: {:.2f}%)\n", it->symbol, (current_prices.at(it->symbol) - it->entry_price)/it->entry_price*100);
            }
            it = pending_checks.erase(it);
        } else {
            ++it;
        }
    }
}
void update_real_pnl(const std::map<std::string, double>& current_prices, 
                     std::vector<TradeData>& pending_trades,
                     const std::map<std::string, TradingConstraints>& settings) {
    
    auto now = std::chrono::steady_clock::now();
    for (auto it = pending_trades.begin(); it != pending_trades.end(); ) {
        if (!current_prices.count(it->symbol)) { ++it; continue; }
        
        // その銘柄の設定を取得（なければデフォルト値を使用）
        TradingConstraints config = settings.count(it->symbol) ? settings.at(it->symbol) : settings.at("ATOMUSDT");

        double current_price = current_prices.at(it->symbol);
        double change = (current_price - it->entry_price) / it->entry_price;
        double pnl_ratio = (it->side == "LONG") ? change : -change;

        bool should_close = false;
        if (pnl_ratio >= config.tp_rate) should_close = true;        // 銘柄別TP
        else if (pnl_ratio <= config.sl_rate) should_close = true;  // 銘柄別SL
        else if (std::chrono::duration_cast<std::chrono::seconds>(now - it->entry_time).count() >= config.max_hold_sec) should_close = true; // 銘柄別タイムアウト

        if (should_close) {
            // ... 決済処理 ...
            it = pending_trades.erase(it);
        } else {
            ++it;
        }
    }
}