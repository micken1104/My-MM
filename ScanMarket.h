#ifndef SCANMARKET_H
#define SCANMARKET_H

#include <map>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "ExecuteTrade.h" // これをインクルードして構造体を利用する

struct MarketState {
    double last_price = 0;
    double imbalance = 0;
    double diff = 0;
    double volume = 0;
};

// 他のファイル(main.cpp)から見えるように extern 宣言を追加
extern std::vector<std::string> targets; 
extern std::map<std::string, TradingConstraints> symbol_settings;

void process_ws_data(const nlohmann::json& j, std::map<std::string, MarketState>& history, std::vector<TradeData>& pending_checks);
void update_real_pnl(const std::map<std::string, double>& current_prices, std::vector<TradeData>& pending_trades, const std::map<std::string, TradingConstraints>& settings);
void check_pending_trades(const std::map<std::string, double>& current_prices, std::vector<TradeData>& pending_checks);

#endif