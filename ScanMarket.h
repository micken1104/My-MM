#ifndef SCANMARKET_H
#define SCANMARKET_H

#include <map>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <chrono>
#include "ExecuteTrade.h"

struct MarketState {
    double imbalance = 0.0;
    double diff = 0.0; // imbalance_change
    double last_price = 0.0;
    double total_depth = 0.0;
    double volatility = 0.0;
    double btc_corr = 0.0;      
};

// 板情報をCSVに保存（Python学習用）
void save_market_data_to_csv(const std::string& symbol, double imbalance, double imbalance_change);

// 板情報を処理（マーケット状態更新・CSV保存）
void process_ws_data(const std::string& symbol, double imbalance, double imbalance_change,
                     double total_depth, double current_price, double btc_price,
                     std::map<std::string, MarketState>& market_state);

#endif
