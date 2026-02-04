#ifndef SCAN_MARKET_H
#define SCAN_MARKET_H

#include <map>
#include <string>
#include <chrono>

struct MarketState {
    double imbalance;
    std::chrono::steady_clock::time_point timestamp;
};

// 引数に履歴のmapを追加する
void scan_market(std::map<std::string, MarketState>& history);

#endif