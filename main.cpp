#include "ScanMarket.h"
#include "SOMEvaluator.h"
#include "ExecuteTrade.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <map>
#include <string>
#include <nlohmann/json.hpp>
#include <ixwebsocket/IXWebSocket.h>
#include <vector>
#include <algorithm>
#include <mutex>
#include <filesystem>
#include <fstream>
#include <iomanip>

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#endif

using json = nlohmann::json;

int main() {
    // windows socketの初期化
    #ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "WSAStartup failed." << std::endl;
            return 1;
        }
    #endif

    std::filesystem::create_directories("data");
    std::filesystem::create_directories("models");

    // 銘柄リスト
    std::vector<std::string> symbols = {"ATOMUSDT", "ETHUSDT", "SOLUSDT", "BTCUSDT"};
    std::string btc_symbol = "BTCUSDT";
    
    // 銘柄の相場情報
    std::map<std::string, double> prices;
    std::map<std::string, MarketState> market_state;
    
    // 仮想トレード管理
    std::vector<TradeData> active_trades;
    
    std::mutex price_mutex;
    
    // SOMモデル読み込み
    std::map<std::string, SOMEvaluator> som_models;
    for (const auto& symbol : symbols) {
        std::string prefix = "models/" + symbol + "_";
        som_models[symbol].loadModel(
            prefix + "map_weights.csv",
            prefix + "expectancy.csv",
            prefix + "scaling_params.csv",
            prefix + "risk_map.csv"
        );
    }
    
    // 30分ごとにSOM再学習
    std::thread training_thread([&symbols, &som_models, &price_mutex]() {
        while (true) {        
            for (const auto& symbol : symbols) {
                std::string cmd = "C:\\Users\\MichihikoKubota\\Documents\\My-MM\\.venv\\Scripts\\python.exe train_som.py " + symbol;
                int result = std::system(cmd.c_str());
                if (result == 0){
                    std::lock_guard<std::mutex> lock(price_mutex); // 推論中に読み替えないようロック
                    std::string prefix = "models/" + symbol + "_";
                    bool success = som_models[symbol].loadModel(
                        prefix + "map_weights.csv",
                        prefix + "expectancy.csv",  
                        prefix + "scaling_params.csv",
                        prefix + "risk_map.csv"
                    );
                    if (success) {
                        std::cout << "Model reloaded for " << symbol << std::endl;
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::minutes(30));
        }
    });
    training_thread.detach();
    
    // WebSocket 接続
    ix::WebSocket webSocket;
    std::string url = "wss://stream.binance.com/stream?streams="; // ポート9443を外し、/streamを明示
    
    for (size_t i = 0; i < symbols.size(); ++i) {
        std::string lower_symbol = symbols[i];
        std::transform(lower_symbol.begin(), lower_symbol.end(), lower_symbol.begin(), ::tolower);
        url += lower_symbol + "@bookTicker";
        if (i < symbols.size() - 1) {
            url += "/";
        }
    }
    
    webSocket.setUrl(url);
    
    // WebSocket メッセージ受信
    webSocket.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            try {
                auto root = json::parse(msg->str);
                auto data = root.count("data") ? root["data"] : root;
                
                if (!data.contains("s")) {
                    return;
                }
                
                std::string symbol = data["s"];
                double bid_price = std::stod(data["b"].get<std::string>());
                double ask_price = std::stod(data["a"].get<std::string>());
                double bid_volume = std::stod(data["B"].get<std::string>());
                double ask_volume = std::stod(data["A"].get<std::string>());
                
                double mid_price = (bid_price + ask_price) / 2.0;
                double imbalance = (bid_volume - ask_volume) / (bid_volume + ask_volume);
                
                {
                    std::lock_guard<std::mutex> lock(price_mutex);
                    
                    prices[symbol] = mid_price;
                    
                    double previous_imbalance = market_state[symbol].imbalance;
                    double imbalance_change = imbalance - previous_imbalance;
                    
                    process_ws_data(symbol, imbalance, imbalance_change, market_state);
                    market_state[symbol].last_price = mid_price;
                    
                    if (prices.count(symbol) && prices.count(btc_symbol)) {
                        double symbol_imbalance = market_state[symbol].imbalance;
                        double symbol_imbalance_change = market_state[symbol].diff;  // diff = imbalance_change
                        double btc_imbalance = market_state[btc_symbol].imbalance;
                        double btc_imbalance_change = market_state[btc_symbol].diff;  // diff = imbalance_change
                        
                        // 新しいフォーマット: 4つの特徴量
                        std::vector<double> features = {
                            symbol_imbalance,
                            symbol_imbalance_change,
                            btc_imbalance,
                            btc_imbalance_change
                        };
                        
                        SOMResult result = som_models[symbol].getPrediction(features);
                        execute_trade(result.expectancy, mid_price, symbol, active_trades, symbol_imbalance);
                    }
                }
            } catch (const std::exception& e) {
                // Error
            }
        }
    });
    
    webSocket.start();
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    // メインループ
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        {
            std::lock_guard<std::mutex> lock(price_mutex);
            
            auto now = std::chrono::steady_clock::now();
            
            for (auto it = active_trades.begin(); it != active_trades.end(); ) {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->entry_time).count();
                
                // 60秒後に決済
                if (elapsed >= 60) {
                    double exit_price = prices[it->symbol];
                    double pnl_pct = (exit_price - it->entry_price) / it->entry_price * 100.0;
                    
                    // CSV保存
                    std::string filename = "data/" + it->symbol + "_trades.csv";
                    bool file_exists = std::filesystem::exists(filename);
                    std::ofstream file(filename, std::ios::app);
                    
                    if (!file_exists) {
                        file << "entry_time,symbol,entry_price,exit_price,pnl_pct,entry_imbalance\n";
                    }
                    
                    long long entry_ts = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
                    
                    file << entry_ts << ","
                         << it->symbol << ","
                         << it->entry_price << ","
                         << exit_price << ","
                         << std::fixed << std::setprecision(4) << pnl_pct << ","
                         << std::setprecision(6) << it->entry_imbalance << "\n";
                    file.close();
                    
                    it = active_trades.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }
    
    #ifdef _WIN32
        WSACleanup();
    #endif

    return 0;
}
