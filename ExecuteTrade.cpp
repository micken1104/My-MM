#include "ExecuteTrade.h"
#include "ScanMarket.h" // MarketStateを参照するために必要
#include <fstream>
#include <iostream>
#include <map>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <ctime>
#include <filesystem>
#include <vector>

static double total_pnl_pct = 0.0; // 通算損益（％）
static int win_count = 0;
static int loss_count = 0;
std::map<std::string, std::chrono::steady_clock::time_point> last_exit_times;

namespace {
constexpr double kFeePerSidePct = 0.04;
constexpr double kRoundTripFeePct = kFeePerSidePct * 2.0;
constexpr int kMaxHoldSec = 180;

int current_utc_hour() {
    std::time_t now_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm utc_tm{};
#ifdef _WIN32
    gmtime_s(&utc_tm, &now_time);
#else
    gmtime_r(&now_time, &utc_tm);
#endif
    return utc_tm.tm_hour;
}

bool is_allowed_entry_hour(const std::string& symbol, int hour_utc) {
    static const std::map<std::string, std::vector<int>> kAllowedHours = {
        {"ATOMUSDT", {2, 13, 15}},
        {"BTCUSDT", {9, 16, 20}},
        {"ETHUSDT", {7, 15, 19}},
        {"SOLUSDT", {0, 8, 9}},
    };

    auto it = kAllowedHours.find(symbol);
    if (it == kAllowedHours.end()) {
        return true;
    }
    const auto& hours = it->second;
    return std::find(hours.begin(), hours.end(), hour_utc) != hours.end();
}
} // namespace

bool is_market_crashing(const MarketState& state) {
    // BTCの相関が高く、かつBTCに対して負の方向への勢いが強い場合を「地合い悪化」とみなす
    // 強い相関があり、かつ「負の方向」への勢いが強いかチェック
    // state.diff がマイナス（下落方向）かつ ボラが高い場合
    if (state.btc_corr > 0.7 && state.diff < -0.0005 && state.volatility > 0.001) {
        return true; 
    }
    return false;
}
void execute_trade(double expectancy, double current_price, std::string symbol, 
                   std::vector<TradeData>& pending_trades, double local_risk,
                   const MarketState& state) {
    // クールダウンチェック（決済から30秒間はエントリー禁止）
    auto now = std::chrono::steady_clock::now();
    if (last_exit_times.count(symbol)) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_exit_times[symbol]).count();
        if (elapsed < 30) return; // 30秒以内ならスキップ
    }
    
    int hour_utc = current_utc_hour();
    if (!is_allowed_entry_hour(symbol, hour_utc)) {
        return;
    }

    // ボラ高ではやや閾値を緩め、ボラ低では厳しめにする
    double dynamic_threshold = (state.volatility > 0.001) ? 0.15 : 0.2;
    // 地合いフィルターの適用
    if (is_market_crashing(state)) {
        dynamic_threshold += 0.15; // クラッシュ時は 0.3 ~ 0.35 までハードルを上げる
        // もしくは return; で完全に止めても良い
    }

    // SHORTエントリー閾値（要求仕様）
    const double short_threshold = (state.volatility > 0.001) ? -0.30 : -0.45;
    const bool should_long = expectancy > dynamic_threshold;
    const bool should_short = expectancy < short_threshold;
    if (!should_long && !should_short) {
        return;
    }


    // 板の厚みフィルター
    // 厚みが極端に薄いときは、インバランスの数値が嘘をつきやすいので避ける
    if (state.total_depth * current_price < 1000.0) { // USDT換算で1000未満は薄いとみなす
        // std::cout << symbol << " Skip: Too Thin (Depth: " << state.total_depth << ")" << std::endl;
        return;
    }

    // ボラティリティフィルター
    // 凪の状態でのインバランスは、価格が動かずタイムアップになりやすい
    double vol_threshold = (symbol == "BTCUSDT") ? 0.00003 : 0.00008; // 銘柄ごとにボラの出やすさが違う
    if(state.volatility < vol_threshold) {
        // std::cout << symbol << " Skip: No Volatility (Vol: " << state.volatility << ")" << std::endl;
        return;
    }
    
    // この銘柄でポジションが既にあるかチェック
    for (const auto& trade : pending_trades) {
        if (trade.symbol == symbol) {
            return;  // ポジション既存、スキップ
        }
    }
    
    // 新しいトレードを作成
    TradeData new_trade;
    new_trade.symbol = symbol;
    new_trade.side = should_short ? "SHORT" : "LONG";
    new_trade.entry_price = current_price;
    new_trade.tp_rate = std::clamp(state.volatility * 2.0, 0.0010, 0.0050);
    new_trade.sl_rate = std::clamp(state.volatility * 1.5, 0.0008, 0.0035);
    new_trade.entry_imbalance = local_risk;
    new_trade.entry_time = now;
    
    pending_trades.push_back(new_trade);
    std::cout << "OPEN " << new_trade.side << " " << symbol
              << " at " << current_price
              << " | TP=" << std::fixed << std::setprecision(3) << (new_trade.tp_rate * 100.0) << "%"
              << " | SL=" << (new_trade.sl_rate * 100.0) << "%"
              << std::endl;
}

void check_and_close_trades(std::vector<TradeData>& active_trades, 
                            std::map<std::string, double>& current_prices) {
    auto now = std::chrono::steady_clock::now();
            
    for (auto it = active_trades.begin(); it != active_trades.end(); ) {
        // 現在の価格が取得できない場合はスキップ
        if (current_prices.find(it->symbol) == current_prices.end()) {
            ++it; continue;
        }
        
        double current_price = current_prices[it->symbol];

        const bool is_short = (it->side == "SHORT");
        double gross_pnl_pct = is_short
            ? ((it->entry_price - current_price) / it->entry_price) * 100.0
            : ((current_price - it->entry_price) / it->entry_price) * 100.0;
        double pnl_ratio = gross_pnl_pct / 100.0;

        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->entry_time).count();
        
        // --- 判定フラグ ---
        bool should_close = false;
        std::string reason = "";

        if (pnl_ratio >= it->tp_rate) {
            should_close = true;
            reason = "TP (Take Profit)";
        } else if (pnl_ratio <= -it->sl_rate) {
            should_close = true;
            reason = "SL (Stop Loss)";
        } else if (elapsed >= kMaxHoldSec) {
            should_close = true;
            reason = "Time Up";
        }

        if (should_close) {
            double net_pnl_pct = gross_pnl_pct - kRoundTripFeePct;
            total_pnl_pct += net_pnl_pct;
            if (net_pnl_pct > 0) win_count++;
            else if (net_pnl_pct < 0) loss_count++;

            // コンソールに決済ログを表示
            std::cout << "CLOSE " << it->side << " [" << reason << "] " << it->symbol 
                    << " at " << current_price 
                    << " | Gross: " << std::fixed << std::setprecision(3) << gross_pnl_pct << "%"
                    << " | Net: " << net_pnl_pct << "%"
                    << " | Hold: " << elapsed << "s" << std::endl;
            // 画面に「現在の全成績」を表示
            std::cout << "========== WALLET STATS ==========" << std::endl;
            std::cout << " Total PnL: " << total_pnl_pct << "%" << std::endl;
            std::cout << " Win/Loss: " << win_count << "/" << loss_count << std::endl;
            std::cout << "==================================" << std::endl;
            // CSVに保存
            // 銘柄別CSVの下に、共通ログも追記する
            const std::string all_filename = "data/current/all_trades_history.csv";
            bool all_exists = std::filesystem::exists(all_filename);
            std::filesystem::create_directories("data/current");
            std::ofstream all_file(all_filename, std::ios::app);
            std::string filename = "data/current/" + it->symbol + "_trades.csv";
            bool symbol_exists = std::filesystem::exists(filename);
            std::ofstream file(filename, std::ios::app);
            if (file.is_open() && all_file.is_open()) {
                long long ts = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                if (!symbol_exists || std::filesystem::file_size(filename) == 0) {
                    file << "ts,symbol,entry,exit,pnl,reason,side,gross_pnl,net_pnl,hold_sec,tp_rate,sl_rate\n";
                }
                if (!all_exists || std::filesystem::file_size(all_filename) == 0) {
                    all_file << "ts,symbol,side,entry,exit,gross_pnl,net_pnl,total_net_pnl,reason,hold_sec\n";
                }

                file << ts << "," << it->symbol << "," << it->entry_price << "," 
                    << current_price << "," << net_pnl_pct << "," << reason << ","
                    << it->side << "," << gross_pnl_pct << "," << net_pnl_pct << ","
                    << elapsed << "," << it->tp_rate << "," << it->sl_rate << "\n";
                file.close();
                all_file << ts << "," << it->symbol << "," << it->side << "," << it->entry_price << ","
                         << current_price << "," << gross_pnl_pct << "," << net_pnl_pct << ","
                         << total_pnl_pct << "," << reason << "," << elapsed << "\n";
                all_file.close();
            }

            last_exit_times[it->symbol] = std::chrono::steady_clock::now(); // 決済時刻を記録
            it = active_trades.erase(it); // ポジション削除
        } else {
            ++it;
        }
    }
}