#include "ScanMarket.h"
#include "SOMEvaluator.h"
#include <fmt/core.h>
#include <fmt/color.h>
#include <ixwebsocket/IXWebSocket.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>

using json = nlohmann::json;

int main() {
    SOMEvaluator som;
    if(!som.loadModel("som_map_weights.csv", "som_signals.csv", "som_scaling_params.csv")) {
        fmt::print(fg(fmt::color::red), "Failed to load SOM model files.\n");
        return -1;
    }
    fmt::print(fg(fmt::color::green), "✅ SOM Model Loaded Successfully!\n");

    std::map<std::string, MarketState> history;
    std::vector<TradeData> pending_checks;

    ix::WebSocket webSocket;
    std::string url = "wss://stream.binance.com:9443/ws/btcusdt@bookTicker/ethusdt@bookTicker";
    webSocket.setUrl(url);

    // メッセージ受信時のコールバック
    webSocket.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            try {
                auto j = json::parse(msg->str);
                
                // WebSocketから届いた個別データを処理（ScanMarket.cppのロジックを利用）
                // ここで本来のscan_marketの「中身」を呼ぶか、WebSocket用にパース
                process_ws_data(j, history, pending_checks);

                if (history.count("BTCUSDT") && history.count("ETHUSDT")) {
                    auto& btc = history["BTCUSDT"];
                    auto& eth = history["ETHUSDT"];

                    std::vector<double> input = {
                        btc.imbalance, btc.diff, btc.volume,
                        eth.imbalance, eth.diff, eth.volume
                    };

                    int signal = som.predict(input);
                    
                    if (signal != 0) {
                        auto color = (signal == 1) ? fmt::color::green : fmt::color::red;
                        fmt::print(fg(color), "📊 SOM Signal: {} ({})\n", 
                                  (signal == 1 ? "BUY" : "SELL"), j["s"].get<std::string>());
                    }
                }
            } catch (const std::exception& e) {
                fmt::print("Error: {}\n", e.what());
            }
        } else if (msg->type == ix::WebSocketMessageType::Error) {
            fmt::print(fg(fmt::color::red), "WS Error: {}\n", msg->errorInfo.reason);
        }
    });

    webSocket.start(); // 通信開始（非同期）

    fmt::print("📡 WebSocket started. Listening for Binance streams...\n");

    // メインスレッドを維持
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}