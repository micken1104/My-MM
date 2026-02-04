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
    double last_price = 0.0; // 追加しておくと便利
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

// ScanMarket.cpp に追加
void process_ws_data(const nlohmann::json& j, std::map<std::string, MarketState>& history, std::vector<TradeData>& pending_checks) {
    // 通貨ごとの「最後に保存した時刻」を記録するマップ
    static std::map<std::string, std::chrono::steady_clock::time_point> last_save_times;
    auto now = std::chrono::steady_clock::now();

    std::string symbol = j["s"]; // "BTCUSDT" など
    double bid_p = std::stod(j["b"].get<std::string>());
    double ask_p = std::stod(j["a"].get<std::string>());
    double bid_v = std::stod(j["B"].get<std::string>());
    double ask_v = std::stod(j["A"].get<std::string>());
    double mid_p = (bid_p + ask_p) / 2.0;


    double current_imbalance = (bid_v - ask_v) / (bid_v + ask_v);
    double diff = current_imbalance - history[symbol].imbalance;

    history[symbol].imbalance = current_imbalance;
    history[symbol].diff = diff;
    history[symbol].volume = 0.0; // 必要に応じて j["v"] などから取得
    history[symbol].last_price = mid_p;

    // --- 間引き処理（CSV学習データ用） ---
    // その通貨で1秒（1000ms）以上経過していたら pending_checks に追加
    //当初は通信負荷やディスク容量を考慮して5秒間隔でサンプリングしていましたが、市場の微細な変化（マイクロストラクチャー）を捉えるためには解像度が不足していると判断し、1秒間隔のサンプリングへ最適化しました。これにより、学習データの密度が向上し、SOMの収束速度と予測精度のバランスを改善しました。」
    if (now - last_save_times[symbol] >= std::chrono::milliseconds(1000)) {
        pending_checks.push_back({
            symbol,
            mid_p,
            current_imbalance,
            diff,
            0.0, // volume
            now
        });
        last_save_times[symbol] = now; // 保存時刻を更新
    }
}

void save_to_csv(const TradeData& data, double exit_price) {
    std::string filename = "market_data.csv";
    
    // ファイルが空（新しく作られる）場合、ヘッダーを書き込む
    std::ifstream check_file(filename);
    bool is_empty = check_file.peek() == std::ifstream::traits_type::eof();
    check_file.close();

    std::ofstream file(filename, std::ios::app);
    if (is_empty) {
        file << "symbol,imbalance,diff,volume,target\n";
    }

    // 計算
    double price_change = (exit_price - data.entry_price) / data.entry_price * 100.0;
    
    // 書き込み（精度を固定するとPythonで扱いやすい）
    file << data.symbol << "," 
         << std::fixed << std::setprecision(6) << data.entry_imbalance << "," 
         << data.entry_diff << "," 
         << data.entry_volume << "," 
         << data.entry_price << "," // エントリー価格自体も残しておくと後で検証しやすい
         << exit_price << ","      // エグジット価格も
         << price_change << "\n";
}

void check_pending_trades(const std::map<std::string, double>& current_prices, std::vector<TradeData>& pending_checks) {
    auto now = std::chrono::steady_clock::now();
    for (auto it = pending_checks.begin(); it != pending_checks.end(); ) {
        if (now - it->entry_time >= std::chrono::seconds(30)) {
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
    } else if (!res) {
        auto err = res.error();
        fmt::print(fg(fmt::color::red), "❌ Connection Error: {}\n", (int)err);
        return; // ここで安全に抜ける
    } else if (res->status != 200) {
        fmt::print(fg(fmt::color::yellow), "⚠️ API Status Error: {}\n", res->status);
        return;
    }
}

