#include "ScanMarket.h"
#include "SOMEvaluator.h"
#include "ExecuteTrade.h"
#include <fmt/core.h>
#include <fmt/color.h>
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

using json = nlohmann::json;

int main() {
    // フォルダがなければ作成
    std::filesystem::create_directories("data");
    std::filesystem::create_directories("models");

    // 1. 設定の初期化
    std::string support_symbol = "BTCUSDT"; 
    
    auto to_lower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
        return s;
    };

    // 2. 銘柄ごとにSOMモデルを管理
    std::map<std::string, SOMEvaluator> som_map;
    
    for (const auto& symbol : targets) {
        std::string prefix = std::string("models/") + symbol + "_";
        if (!som_map[symbol].loadModel(prefix + "map_weights.csv", 
                                    prefix + "expectancy.csv", 
                                    prefix + "scaling_params.csv", 
                                    prefix + "risk_map.csv")) {
            fmt::print(fg(fmt::color::yellow), "⚠️ {} のモデル読み込み失敗。学習を待機します。\n", symbol);
        }
    }

    // 3. 学習スレッド（定期的にモデルを更新）
    std::thread updater([&som_map]() {
        while (true) {
            // 起動直後はデータが溜まるまで少し待機（任意）
            std::this_thread::sleep_for(std::chrono::minutes(5)); 

            for (const auto& symbol : targets) {
                fmt::print("🔄 Training SOM for {}...\n", symbol);
                std::string cmd = "python train_som.py " + symbol;
                
                int result = std::system(cmd.c_str()); 
                
                if(result == 0) {
                    std::string prefix = "models/" + symbol + "_";
                    som_map[symbol].loadModel(prefix + "map_weights.csv", 
                                              prefix + "expectancy.csv", 
                                              prefix + "scaling_params.csv", 
                                              prefix + "risk_map.csv");
                    fmt::print(fg(fmt::color::green), "✅ {} Model Updated.\n", symbol);
                }
            }
            // 次の学習まで30分休む
            std::this_thread::sleep_for(std::chrono::minutes(30));
        }
    });
    updater.detach();

    std::map<std::string, MarketState> history;
    std::vector<TradeData> pending_samples; 
    std::vector<TradeData> pending_trades;  

    // 4. WebSocket設定
    ix::WebSocket webSocket;
    std::string url = "wss://stream.binance.com:9443/stream?streams=";
    for (size_t i = 0; i < targets.size(); ++i) {
        url += to_lower(targets[i]) + "@bookTicker/";
    }
    url += to_lower(support_symbol) + "@bookTicker";
    
    webSocket.setUrl(url);
    std::mutex mtx_history;
    std::mutex mtx_trades;

    // 5. メッセージ受信コールバック
    webSocket.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            try {
                auto root = nlohmann::json::parse(msg->str);
                auto j = root.count("data") ? root["data"] : root;
                std::string symbol = j["s"];

                {
                    std::lock_guard<std::mutex> lock(mtx_history);
                    process_ws_data(j, history, pending_samples);
                    
                    // 予測とトレード判断
                    if (som_map.count(symbol) && history.count(support_symbol)) {
                        auto& main_data = history[symbol];
                        auto& btc_data = history[support_symbol];

                        std::vector<double> features = {
                            main_data.imbalance, main_data.diff, main_data.volume,
                            btc_data.imbalance, btc_data.diff, btc_data.volume
                        };

                        // 推論実行
                        SOMResult res = som_map[symbol].getPrediction(features);
                        
                        // トレード実行（別のミューテックスで保護）
                        if (res.expectancy != 0.0) {
                            std::lock_guard<std::mutex> lock_t(mtx_trades);
                            execute_trade(res.expectancy, main_data.last_price, symbol, pending_trades, res.risk);
                        }
                    }
                }
            } catch (const std::exception& e) {
                // パースエラー等のハンドリング
            }
        }
    });
    
    webSocket.start();
    fmt::print(fg(fmt::color::cyan), "📡 Trading bot is running on {} markets...\n", targets.size());

    // 6. メインループ（管理タスク）
    while (true) {
        std::map<std::string, double> current_prices;
        {
            std::lock_guard<std::mutex> lock(mtx_history);
            for (auto const& [sym, state] : history) {
                if (state.last_price > 0) {
                    current_prices[sym] = state.last_price;
                }
            }
        }

        {
            std::lock_guard<std::mutex> lock(mtx_history);
            check_pending_trades(current_prices, pending_samples);
        }

        {
            std::lock_guard<std::mutex> lock_t(mtx_trades);
            update_real_pnl(current_prices, pending_trades, symbol_settings);
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}