#include "ExecuteTrade.h"
#include "ScanMarket.h" // MarketStateを参照するために必要
#include <fstream>
#include <iostream>
#include <map>
#include <iomanip>
#include <chrono>

static double total_pnl_pct = 0.0; // 通算損益（％）
static int win_count = 0;
static int loss_count = 0;
std::map<std::string, std::chrono::steady_clock::time_point> last_exit_times;

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
    
    // 期待値が高い場合のみトレード
    // ボラティリティが高い時だけ、期待値のハードルを下げる（チャンスが多いので）
    double dynamic_threshold = (state.volatility > 0.001) ? 0.15 : 0.2;
    // 地合いフィルターの適用
    if (is_market_crashing(state)) {
        dynamic_threshold += 0.15; // クラッシュ時は 0.3 ~ 0.35 までハードルを上げる
        // もしくは return; で完全に止めても良い
    }
    if (expectancy <= dynamic_threshold) {
        // std::cout << symbol << " Skip: Expectancy low (" << expectancy << ")" << std::endl;
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
    new_trade.entry_price = current_price;
    new_trade.entry_imbalance = local_risk;
    new_trade.entry_time = std::chrono::steady_clock::now();
    
    pending_trades.push_back(new_trade);
    std::cout << "BUY " << symbol << " at " << current_price << std::endl;
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
        double pnl_ratio = (current_price - it->entry_price) / it->entry_price;

        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->entry_time).count();
        
        // --- 判定フラグ ---
        bool should_close = false;
        std::string reason = "";

        if (pnl_ratio >= 0.0012) {        // 0.12% 利益で利確
            should_close = true;
            reason = "TP (Take Profit)";
        } else if (pnl_ratio <= -0.0015) { // 0.15% 損失で損切
            should_close = true;
            reason = "SL (Stop Loss)";
        } else if (elapsed >= 45) {       // 45秒経過でタイムアップ,30秒たっても離隔できないならそのインバランスはもうすでに解消か予測外れ
            should_close = true;
            reason = "Time Up";
        }

        if (should_close) {
            double pnl_pct = pnl_ratio * 100.0;
            total_pnl_pct += pnl_pct; // 合計に加算
            if (pnl_pct > 0) win_count++;
            else if (pnl_pct < 0) loss_count++;

            // コンソールに決済ログを表示
            std::cout << "SELL [" << reason << "] " << it->symbol 
                    << " at " << current_price 
                    << " | PnL: " << std::fixed << std::setprecision(3) << pnl_pct << "%" 
                    << " | Hold: " << elapsed << "s" << std::endl;
            // 画面に「現在の全成績」を表示
            std::cout << "========== WALLET STATS ==========" << std::endl;
            std::cout << " Total PnL: " << total_pnl_pct << "%" << std::endl;
            std::cout << " Win/Loss: " << win_count << "/" << loss_count << std::endl;
            std::cout << "==================================" << std::endl;
            // CSVに保存
            // 銘柄別CSVの下に、共通ログも追記する
            std::ofstream all_file("data/all_trades_history.csv", std::ios::app);
            std::string filename = "data/" + it->symbol + "_trades.csv";
            std::ofstream file(filename, std::ios::app);
            if (file.is_open()) {
                long long ts = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                file << ts << "," << it->symbol << "," << it->entry_price << "," 
                    << current_price << "," << pnl_pct << "," << reason << "\n";
                file.close();
                all_file << ts << "," << it->symbol << "," << pnl_pct << "," << total_pnl_pct << "\n";
                all_file.close();
            }

            last_exit_times[it->symbol] = std::chrono::steady_clock::now(); // 決済時刻を記録
            it = active_trades.erase(it); // ポジション削除
        } else {
            ++it;
        }
    }
}