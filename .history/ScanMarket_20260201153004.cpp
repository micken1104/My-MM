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
    std::string filename = "market_data.csv";
    
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

void update_real_pnl(const std::map<std::string, double>& current_prices, std::vector<TradeData>& pending_trades) {
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = pending_trades.begin(); it != pending_trades.end(); ) {
        if (current_prices.count(it->symbol)) {
            double current_price = current_prices.at(it->symbol);
            // 損益率（%）の計算
            double change = (current_price - it->entry_price) / it->entry_price;
            double pnl_ratio = (it->side == "LONG") ? change : -change;


            // --- ★動的利確ロジックの追加 ---
            // 板情報からスプレッド率を算出 (スプレッドが 0.02% なら 0.0002)
            // historyに記録されている最新の mid_price と imbalance 等から逆算するか、
            // ws_dataから直接 spread を保持するように拡張するのがベストです。
            
            double min_tp = 0.0005; // 最低利確ライン (手数料負けしない 0.05%)
            double target_tp = 0.0010; // デフォルト 0.1%

            // もしボラティリティが極端に低い（スプレッドが狭い）なら、利確を早める
            // ※簡易的に 0.0006 程度に下げて回転数を上げる設定
            double dynamic_tp = (it->symbol == "ATOMUSDT") ? 0.0006 : 0.0010; 


            // 決済
            bool should_close = false;
            std::string reason = "";

            if (pnl_ratio >= dynamic_tp) {
                should_close = true;
                reason = "DYNAMIC_TP";
            }

            // 1. 利確 (+0.10% で即逃げ)
            if (pnl_ratio >= tp_threshold) {
                should_close = true;
                reason = "TAKE_PROFIT";
            }
            // 2. 損切 (-0.08% で即撤退)
            else if (pnl_ratio <= -0.0008) {
                should_close = true;
                reason = "STOP_LOSS";
            }
            // 3. タイムアウト (60秒経ったら強制決済)
            else if (std::chrono::duration_cast<std::chrono::seconds>(now - it->entry_time).count() >= 60) {
                should_close = true;
                reason = "TIME_OUT";
            }

            if (should_close) {
                // 手数料(往復0.05%)を差し引いた実損益
                double net_pnl = (it->lot_size * pnl_ratio) - (it->lot_size * 0.0005);

                real_balance += net_pnl;
                total_net_profit += net_pnl;

                // ログ出力（色分けで見やすく）
                auto color = (net_pnl >= 0) ? fmt::color::green_yellow : fmt::color::orange_red;
                fmt::print(fg(color), 
                    "🏁 [RESULT] {:<8} | {:<11} | PnL: ${:+.2f} | Balance: ${:.2f} (Total: ${:+.2f})\n",
                    it->symbol, reason, net_pnl, real_balance, total_net_profit);

                it = pending_trades.erase(it); // リストから削除して決済完了
                continue;
            }
        }
        ++it;
    }
}


void scan_market(std::map<std::string, MarketState>& history, std::vector<TradeData>& pending_checks) {
    httplib::Client cli("http://api.binance.com");
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

            TradeData data_node;
            data_node.symbol = symbol;
            data_node.entry_price = mid_p;
            data_node.entry_imbalance = current_imbalance;
            data_node.entry_diff = diff;
            data_node.entry_volume = 0.0;
            data_node.entry_time = now;
            data_node.entry_timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

            pending_checks.push_back(data_node);
            
            history[symbol].imbalance = current_imbalance;
            history[symbol].diff = diff;
            history[symbol].volume = vol;
            history[symbol].timestamp = now;
        }
        check_pending_trades(current_prices, pending_checks);
    } else if (!res) {
        auto err = res.error();
        fmt::print(fg(fmt::color::red), "❌ Connection Error: {}\n", (int)err);
        return; // ここで安全に抜ける
    } else if (res->status != 200) {
        fmt::print(fg(fmt::color::yellow), "⚠️ API Status Error: {}\n", res->status);
        return;
    }
}

