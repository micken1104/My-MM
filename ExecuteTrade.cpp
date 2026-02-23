#include "ExecuteTrade.h"
#include <iostream>

void execute_trade(double expectancy, double current_price, std::string symbol, 
                   std::vector<TradeData>& pending_trades, double local_risk) {
    // 期待値が高い場合のみトレード
    if (expectancy <= 0.05) {
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
