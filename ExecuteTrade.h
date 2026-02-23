#ifndef EXECUTETRADE_H
#define EXECUTETRADE_H

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <map>

struct TradingConstraints {
    double tp_rate;
    double sl_rate;
    int max_hold_sec;
    double default_risk;
};

struct TradeData {
    std::string symbol;
    std::string side;
    double entry_price;
    double lot_size;
    double dynamic_sl;
    double entry_imbalance;
    double entry_diff;
    double entry_volume;
    long long entry_timestamp;
    std::chrono::steady_clock::time_point entry_time;
};

void execute_trade(double expectancy, double current_price, std::string symbol, 
                   std::vector<TradeData>& pending_trades, double local_risk);
void check_and_close_trades(std::vector<TradeData>& active_trades, 
                            std::map<std::string, double>& current_prices);

#endif
