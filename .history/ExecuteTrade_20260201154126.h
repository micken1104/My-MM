#ifndef EXECUTE_TRADE_H
#define EXECUTE_TRADE_H

#include <string>
#include <vector>
#include "ScanMarket.h" // TradeDataの定義が必要な場合

void execute_trade(double expectancy, double current_price, std::string symbol, 
                   std::vector<TradeData>& pending_trades, double local_risk)
#endif