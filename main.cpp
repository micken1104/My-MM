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
    std::thread training_thread([&symbols]() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::minutes(30));
            
            for (const auto& symbol : symbols) {
                std::string cmd = "C:\\Users\\MichihikoKubota\\Documents\\My-MM\\.venv\\Scripts\\python.exe train_som.py " + symbol;
                std::cout << "Training " << symbol << std::endl;
                std::system(cmd.c_str());
            }
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
    
    std::cout << "Connecting: " << url << std::endl;
    webSocket.setUrl(url);
    
    // WebSocket メッセージ受信
    webSocket.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
        static int msg_count = 0;
        msg_count++;
        
        if (msg_count <= 5 || msg_count % 100 == 0) {
            std::cout << "Message #" << msg_count << " Type: " << (int)msg->type << std::endl;
        }
        if (msg->type == ix::WebSocketMessageType::Error) {
            std::cout << "Error: " << msg->errorInfo.reason << std::endl;
            std::cout << "HTTP Status: " << msg->errorInfo.http_status << std::endl;
        }
        if (msg->type == ix::WebSocketMessageType::Message) {
            try {
                std::string msg_str = msg->str;
                if (msg_count <= 2) {
                    std::cout << "Raw message (first 200 chars): " << msg_str.substr(0, 200) << std::endl;
                }
                
                auto root = json::parse(msg_str);
                auto data = root.count("data") ? root["data"] : root;
                
                if (msg_count <= 2) {
                    std::cout << "Parsed JSON keys: ";
                    for (auto& [key, val] : root.items()) {
                        std::cout << key << " ";
                    }
                    std::cout << std::endl;
                }
                
                if (!data.contains("s")) {
                    if (msg_count <= 2) {
                        std::cout << "WARNING: No 's' field in data" << std::endl;
                    }
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
                    
                    // Python学習用のデータを保存
                    process_ws_data(symbol, imbalance, imbalance_change, market_state);
                    
                    market_state[symbol].last_price = mid_price;
                    
                    // SOM評価
                    if (prices.count(symbol) && prices.count(btc_symbol)) {
                        double symbol_imbalance = market_state[symbol].imbalance;
                        double symbol_diff = market_state[symbol].diff;
                        double btc_imbalance = market_state[btc_symbol].imbalance;
                        double btc_diff = market_state[btc_symbol].diff;
                        
                        std::vector<double> features = {
                            symbol_imbalance,
                            symbol_diff,
                            0.0,
                            btc_imbalance,
                            btc_diff,
                            0.0
                        };
                        
                        SOMResult result = som_models[symbol].getPrediction(features);
                        
                        // トレード実行判定をexecute_tradeに委譲
                        execute_trade(result.expectancy, mid_price, symbol, active_trades, symbol_imbalance);
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "Parse error at message #" << msg_count << ": " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "Unknown error at message #" << msg_count << std::endl;
            }
        } else {
            std::cout << "Non-Message type received: " << (int)msg->type << std::endl;
        }
    });
    
    webSocket.start();
    std::cout << "Running..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    // メインループ
    while (true) {
        static int loop_count = 0;
        loop_count++;
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        {
            std::lock_guard<std::mutex> lock(price_mutex);
            
            if (loop_count % 10 == 0) {
                std::cout << "Loop #" << loop_count << " - prices=" << prices.size() 
                          << " trades=" << active_trades.size() << std::endl;
            }
            
            auto now = std::chrono::steady_clock::now();
            
            // トレード管理
            for (auto it = active_trades.begin(); it != active_trades.end(); ) {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->entry_time).count();
                
                // 60秒後に決済
                if (elapsed >= 60) {
                    double exit_price = prices[it->symbol];
                    double pnl_pct = (exit_price - it->entry_price) / it->entry_price * 100.0;
                    
                    std::cout << "SELL " << it->symbol << " at " << exit_price << " pnl=" << pnl_pct << "%" << std::endl;
                    
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
