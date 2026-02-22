#include "ExecuteTrade.h"

void execute_trade(double expectancy, double current_price, std::string symbol, 
                   std::vector<TradeData>& pending_trades, double local_risk) {
    if (expectancy <= 0.15) {
        return;
    }
    
    for (const auto& trade : pending_trades) {
        if (trade.symbol == symbol) {
            return;
        }
    }
    
    TradeData new_trade;
    new_trade.symbol = symbol;
    new_trade.entry_price = current_price;
    new_trade.entry_imbalance = local_risk;
    new_trade.entry_time = std::chrono::steady_clock::now();
    
    pending_trades.push_back(new_trade);
}
