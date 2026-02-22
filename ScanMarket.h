#ifndef SCANMARKET_H
#define SCANMARKET_H

#include <map>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <chrono>
#include "ExecuteTrade.h"

struct MarketState {
    double last_price = 0;
    double imbalance = 0;
    double diff = 0;
    double volume = 0;
};

// 板情報をCSVに保存（Python学習用）
void save_market_data_to_csv(const std::string& symbol, double imbalance, double imbalance_change);

// 板情報を処理（マーケット状態更新・CSV保存）
void process_ws_data(const std::string& symbol, double imbalance, double imbalance_change,
                     std::map<std::string, MarketState>& market_state);

#endif
