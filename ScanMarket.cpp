#include "ScanMarket.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <filesystem>
#include <vector>
#include <numeric>
#include <cmath>
#include <deque>
#include <chrono>
#include <map>

// --- データ保存用の構造体 ---
struct MarketMetrics {
    std::deque<double> price_history;
    std::deque<double> btc_price_history;
    const size_t max_history = 60; // 60秒分
};

// 銘柄ごとの履歴を保持（staticで値を保持し続ける）
static std::map<std::string, MarketMetrics> market_history;

// ボラティリティ（価格の荒れ具合）を計算
// 過去60秒間の価格から標準偏差（ばらつき）を計算
// ボラが高い: トレンドが発生するチャンス
// ボラが低い: 今買っても手数料負けするだけ
// 今は動かないから手を出さないでおこうという判断ができるようになる
double calculate_volatility(const std::deque<double>& prices) {
    if (prices.size() < 2) return 0.0;
    double sum = std::accumulate(prices.begin(), prices.end(), 0.0);
    double mean = sum / prices.size();
    double sq_sum = 0;
    for (double p : prices) sq_sum += (p - mean) * (p - mean);
    return std::sqrt(sq_sum / prices.size()) / mean; 
}

// BTCとの相関を計算
// アルトコインの騰落率/BTCの騰落率
// 値が 1.0 に近い: BTCが1%上がったから、アルトも1%上がった。
// 値が 5.0 などの大きな値: 個別の強い買いが入った。
// 値がマイナス: BTCは上がっているのに、このコインだけ下がっている（逆行、危険）。
double calculate_btc_correlation(const std::deque<double>& prices, const std::deque<double>& btc_prices) {
    if (prices.size() < 10 || btc_prices.size() < 10) return 0.0;
    double p_change = (prices.back() - prices.front()) / prices.front();
    double b_change = (btc_prices.back() - btc_prices.front()) / btc_prices.front();
    if (std::abs(b_change) < 0.000001) return 1.0; // BTCが動いてなければ1.0
    return p_change / b_change;
}

// --- メインの保存処理 ---

void save_market_data_to_csv(const std::string& symbol, double imbalance, double imbalance_change, 
                             double total_depth, double current_price, double btc_price) {
    
    std::string filename = "data/" + symbol + "_market_data.csv";
    bool file_exists = std::filesystem::exists(filename);
    std::ofstream file(filename, std::ios::app);
    
    if (!file.is_open()) return;

    // 1. 履歴を更新
    auto& metrics = market_history[symbol];
    metrics.price_history.push_back(current_price);
    metrics.btc_price_history.push_back(btc_price);
    if (metrics.price_history.size() > metrics.max_history) {
        metrics.price_history.pop_front();
        metrics.btc_price_history.pop_front();
    }

    // 2. 指標を計算
    double vol = calculate_volatility(metrics.price_history);
    double corr = calculate_btc_correlation(metrics.price_history, metrics.btc_price_history);

    // 3. ヘッダー（最初だけ）
    if (!file_exists) {
        file << "timestamp,symbol,imbalance,imbalance_change,total_depth,price,btc_price,volatility,btc_corr\n";
    }

    // 4. 書き込み
    long long ts = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    file << ts << "," << symbol << ","
         << std::fixed << std::setprecision(6) << imbalance << ","
         << imbalance_change << ","
         << total_depth << ","
         << current_price << ","
         << btc_price << ","
         << vol << ","
         << corr << "\n";
}

// データが届くたびに呼ばれる関数
void process_ws_data(const std::string& symbol, double imbalance, double imbalance_change,
                    double total_depth, double current_price, double btc_price,
                    std::map<std::string, MarketState>& market_state) {
    
    static std::map<std::string, std::chrono::steady_clock::time_point> last_save_times;
    auto now = std::chrono::steady_clock::now();

    // 内部状態を更新
    market_state[symbol].imbalance = imbalance;
    market_state[symbol].diff = imbalance_change;
    market_state[symbol].total_depth = total_depth;
    market_state[symbol].last_price = current_price;
    market_state[symbol].volatility = 0.0; // 一旦初期化（後で計算）
    market_state[symbol].btc_corr = 0.0;   // 一旦初期化（後で計算）

    // 1秒に1回だけ保存する（ここですべての引数を渡す）
    if (now - last_save_times[symbol] >= std::chrono::seconds(1)) {
        save_market_data_to_csv(symbol, imbalance, imbalance_change, total_depth, current_price, btc_price);
        last_save_times[symbol] = now;
    }
}