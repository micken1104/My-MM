#include <fmt/color.h>
#include <chrono>
#include <fmt/color.h>
#include "ScanMarket.h"

static auto last_trade_time = std::chrono::steady_clock::now();
static double virtual_balance = 10000.0; // 仮想的な現在の残高


// 仮想的な注文ロジックの例
void execute_trade(double expectancy, double current_price, std::string symbol, 
                   std::vector<TradeData>& pending_trades, double local_risk) {
    
    // --- 資金管理 (Money Management) ---
    double edge = expectancy / 100.0;
    double safety_factor = 0.1; // 1/10ケリー程度がより安全
    double leverage = 5.0;     // 実効レバレッジの上限
    
    double lot_size_usd = virtual_balance * std::abs(edge) * leverage * safety_factor;
    lot_size_usd = std::min(lot_size_usd, virtual_balance * 0.2); // 1トレード最大20%まで

    // --- エントリー判断 ---
    double base_threshold = 0.15; // 基本0.15%以上の期待値でエントリー
    double dynamic_entry_threshold = base_threshold + (local_risk * 1.5);

    if(std::abs(expectancy) > dynamic_entry_threshold) {
        // すでにこの銘柄のポジションがあるかチェック（銘柄ごとに制限する場合）
        for(const auto& t : pending_trades) if(t.symbol == symbol) return;

        TradeData order;
        order.symbol = symbol;
        order.side = (expectancy > 0) ? "LONG" : "SHORT";
        order.entry_price = current_price;
        order.lot_size = lot_size_usd;

        // --- 損切り幅の最適化 ---
        double sl_percent = (local_risk / 100.0) * 2.5; // 少し余裕を持たせる
        sl_percent = std::clamp(sl_percent, 0.001, 0.01); // 0.1% ~ 1.0% の範囲に収める

        order.dynamic_sl = -sl_percent;
        order.entry_time = std::chrono::steady_clock::now();
        
        pending_trades.push_back(order);
        
        fmt::print(fg(fmt::color::yellow), "🚀 [ORDER] {} {} | Price: ${:.2f} | SL: {:.2f}%\n", 
                   order.side, symbol, current_price, sl_percent * 100.0);
    }
}