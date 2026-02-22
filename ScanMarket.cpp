#include "ScanMarket.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <filesystem>

// Python学習用の板情報をCSVに保存
void save_market_data_to_csv(const std::string& symbol, double imbalance, double imbalance_change) {
    std::string filename = "data/" + symbol + "_market_data.csv";
    
    bool file_exists = std::filesystem::exists(filename);
    std::ofstream file(filename, std::ios::app);
    
    if (!file.is_open()) {
        return;
    }
    
    if (!file_exists) {
        file << "timestamp,symbol,imbalance,imbalance_change\n";
    }
    
    long long timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    file << timestamp << ","
         << symbol << ","
         << std::fixed << std::setprecision(6) << imbalance << ","
         << imbalance_change << "\n";
    
    file.flush();
    file.close();
}

// 板情報を処理（30秒に1回CSVに保存）
void process_ws_data(const std::string& symbol, double imbalance, double imbalance_change,
                     std::map<std::string, MarketState>& market_state) {
    static std::map<std::string, std::chrono::steady_clock::time_point> last_save_times;
    
    auto now = std::chrono::steady_clock::now();
    
    // マーケット状態を更新
    market_state[symbol].imbalance = imbalance;
    market_state[symbol].diff = imbalance_change;
    
    // 30秒ごとにCSVに保存
    if (!last_save_times.count(symbol) || 
        now - last_save_times[symbol] >= std::chrono::seconds(30)) {
        save_market_data_to_csv(symbol, imbalance, imbalance_change);
        last_save_times[symbol] = now;
    }
}
