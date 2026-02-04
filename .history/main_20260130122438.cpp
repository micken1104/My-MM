#include "ScanMarket.h"
#include "SOMEvaluator.h"
#include <fmt/core.h>
#include <fmt/color.h>
#include <thread>
#include <chrono>
#include <map>
#include <string>
#include <ixwebsocket/IXWebSocket.h>

ix::WebSocket webSocket;
std::string url = "wss://stream.binance.com:9443/ws/btcusdt@bookTicker/ethusdt@bookTicker";


int main() {
    SOMEvaluator som;
    if(!som.loadModel("som_map_weights.csv", "som_signals.csv", "som_scaling_params.csv")) {
        fmt::print("Failed to load SOM model files.\n");
        return -1;
    }
    fmt::print("✅ SOM Model Loaded Successfully!\n");

    std::map<std::string, MarketState> history;
    std::vector<TradeData> pending_checks;

    webSocket.setUrl(url);
    webSocket.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg)) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            auto j = json::parse(msg->str);
        // 市場データのスキャン
        fmt::print("🔍 Scanning market...\n");
        scan_market(history, pending_checks);

        // BTCとETHの両方のデータがhistoryに揃っているか確認
        if (history.count("BTCUSDT") && history.count("ETHUSDT")) {
            auto& btc = history["BTCUSDT"];
            auto& eth = history["ETHUSDT"];

            // Python側で作ったfeaturesの順番 [btc_imb, btc_diff, btc_vol, eth_imb, eth_diff, eth_vol]
            std::vector<double> input = {
                btc.imbalance, btc.diff, btc.volume,
                eth.imbalance, eth.diff, eth.volume
            };

            int signal = som.predict(input);
            fmt::print("📊 Data received. SOM Signal: {}\n", signal); // 判定が動いているか確認

            if (signal == 1) {
                fmt::print("🚀 SOM Signal: BUY! Target: BTCUSDT\n");
                // ここで発注関数を呼ぶ
            } else if (signal == -1) {
                fmt::print("🔻 SOM Signal: SELL! Target: BTCUSDT\n");
                // ここで発注関数を呼ぶ
            }
        } else {
            fmt::print("⏳ Waiting for both BTC and ETH data...\n");
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }
    return 0;
}