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


#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#endif

using json = nlohmann::json;

void initialize_files() {
    // 開発時時間がかかるのでいったんコメントアウト　必要に応じて再度有効化
    // data フォルダの CSV 削除
    //if (std::filesystem::exists("data")) {
    //    for (const auto& entry : std::filesystem::directory_iterator("data")) {
    //        if (entry.path().extension() == ".csv") std::filesystem::remove(entry.path());
    //    }
    //}
    // models フォルダの CSV 削除
    //if (std::filesystem::exists("models")) {
    //    for (const auto& entry : std::filesystem::directory_iterator("models")) {
    //        if (entry.path().extension() == ".csv") std::filesystem::remove(entry.path());
    //    }
    //}
    std::cout << "Logs and previous models cleared." << std::endl;
}
int count_csv_lines(std::string filename) {
    if (!std::filesystem::exists(filename)) return 0;
    try {
        std::ifstream file(filename);
        if (!file.is_open()) return 0;
        return std::count(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>(), '\n');
    } catch (...) {
        return 0; // ファイルが使用中の場合は 0 を返して次のループで再トライ
    }
}

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
    
    // .csv初期化
    initialize_files();
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
    // トレード開始フラグ
    bool trading_enabled = false;
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
                
                // 板の総厚みを計算
                double total_depth = bid_volume + ask_volume;

                {
                    std::lock_guard<std::mutex> lock(price_mutex);
    
                    prices[symbol] = mid_price;

                    // インバランスの変化を計算（初回は0.0）
                    double imbalance_change = market_state.count(symbol) ? imbalance - market_state[symbol].imbalance : 0.0;

                    // 計算とCSV保存を実行
                    double btc_price = prices.count(btc_symbol) ? prices[btc_symbol] : mid_price;
                    process_ws_data(symbol, imbalance, imbalance_change, total_depth, mid_price, btc_price, market_state);
                    
                    // トレード開始時刻を過ぎていたらトレード判定を行う
                    if (trading_enabled && prices.count(symbol) && prices.count(btc_symbol)) {
                        // SOMへの入力
                        std::vector<double> features = {
                            market_state[symbol].imbalance,
                            market_state[symbol].diff,
                            market_state[btc_symbol].imbalance,
                            market_state[btc_symbol].diff,
                            market_state[symbol].total_depth,
                            market_state[symbol].volatility,
                            market_state[symbol].btc_corr
                        };
                        
                        SOMResult result = som_models[symbol].getPrediction(features);
                        execute_trade(result.expectancy, mid_price, symbol, active_trades, market_state[symbol].imbalance, market_state[symbol]);
                    }
                }
            } catch (const std::exception& e) {
                // Error
            }
        }
    });
    
    webSocket.start();
    std::cout << "Waiting for data to reach 500 lines..." << std::endl;
    while (true) {
        bool all_ready = true;
        for (const auto& symbol : symbols) {
            std::string filename = "data/" + symbol + "_market_data.csv";
            int lines = count_csv_lines(filename);
            if (lines < 1500) {
                std::cout << "Waiting for " << symbol << ": " << lines << "/1500 lines collected." << std::endl;
                all_ready = false;
            }
        }
        if (all_ready) break;
        std::this_thread::sleep_for(std::chrono::seconds(60)); // 60秒おきにチェック
    }
    // 行数満たした後、初回の学習を実行
    std::cout << "Starting initial SOM training with collected data..." << std::endl;
    for (const auto& symbol : symbols) {
        std::cout << "Training " << symbol << "..." << std::endl;
        std::string cmd = "C:\\Users\\MichihikoKubota\\Documents\\My-MM\\.venv\\Scripts\\python.exe train_som.py " + symbol;
        int result = std::system(cmd.c_str());
        
        if (result == 0) {
            std::lock_guard<std::mutex> lock(price_mutex);
            std::string prefix = "models/" + symbol + "_";
            bool success = som_models[symbol].loadModel(
                prefix + "map_weights.csv",
                prefix + "expectancy.csv",
                prefix + "scaling_params.csv",
                prefix + "risk_map.csv"
            );
            if (success) std::cout << "Model loaded for " << symbol << std::endl;
        } else {
            std::cerr << "Initial training failed for " << symbol << std::endl;
        }
    }
    // 初期学習が終わったのでフラグをONにする
    {
        std::lock_guard<std::mutex> lock(price_mutex); // 全コアに対しメモリの同期
        trading_enabled = true;
        std::cout << "Warm-up complete. Trading enabled!" << std::endl;
    }
        
    // 30分ごとにSOM再学習
    std::thread training_thread([&symbols, &som_models, &price_mutex]() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::minutes(30)); 
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
                        // グラフのために学習完了ログを追記
                        std::ofstream train_log("data/training_events.csv", std::ios::app);
                        if (train_log.is_open()) {
                            long long ts = std::chrono::duration_cast<std::chrono::seconds>(
                                std::chrono::system_clock::now().time_since_epoch()).count();
                            train_log << ts << "," << symbol << "\n";
                            train_log.close();
                        }
                    }
                }
            }
        }
    });
    training_thread.detach();
    
    
    // メインループ
    while (true) {
        // 1500行ためるため、0.5秒おきに
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        {
            std::lock_guard<std::mutex> lock(price_mutex);
            check_and_close_trades(active_trades, prices);
        }
    }
    
    #ifdef _WIN32
        WSACleanup();
    #endif

    return 0;
}
