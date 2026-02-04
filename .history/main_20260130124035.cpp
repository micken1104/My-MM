#include "ScanMarket.h"
#include "SOMEvaluator.h"
#include <fmt/core.h>
#include <fmt/color.h>
#include <thread>
#include <chrono>
#include <map>
#include <string>
#include <nlohmann/json.hpp>
#include <ixwebsocket/IXWebSocket.h>

using json = nlohmann::json;

int main() {
    SOMEvaluator som;
    // モデルの読み込み
    if (!som.loadModel("som_map_weights.csv", "som_signals.csv", "som_scaling_params.csv")) {
        fmt::print(fg(fmt::color::red), "❌ Failed to load SOM model files.\n");
        return -1;
    }
    fmt::print(fg(fmt::color::green), "✅ SOM Model Loaded Successfully!\n");

    std::map<std::string, MarketState> history;
    std::vector<TradeData> pending_checks;

    ix::WebSocket webSocket;
    // BTCとETHの両方のストリームを購読
    std::string url = "wss://stream.binance.com:9443/ws/btcusdt@bookTicker/ethusdt@bookTicker";
    webSocket.setUrl(url);

    webSocket.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            try {
                auto j = nlohmann::json::parse(msg->str);
                
                // WebSocketデータのパースと履歴更新
                process_ws_data(j, history, pending_checks);

                // 表示間隔を管理するタイマー
                static auto last_print_time = std::chrono::steady_clock::now();
                auto now = std::chrono::steady_clock::now();

                // BTCとETHの両方のデータがhistoryに揃っているか確認
                if (history.count("BTCUSDT") && history.count("ETHUSDT")) {
                    auto& btc = history["BTCUSDT"];
                    auto& eth = history["ETHUSDT"];

                    // [btc_imb, btc_diff, btc_vol, eth_imb, eth_diff, eth_vol]
                    std::vector<double> input = {
                        btc.imbalance, btc.diff, btc.volume,
                        eth.imbalance, eth.diff, eth.volume
                    };

                    int signal = som.predict(input);

                    // 前回の表示から500ms（0.5秒）以上経った時だけ画面に出す
                if (now - last_print_time >= std::chrono::milliseconds(500)) {
                    if (signal == 1) {
                        fmt::print(fg(fmt::color::green), "🚀 BUY  | {}\n", j["s"].get<std::string>());
                    } else if (signal == -1) {
                        fmt::print(fg(fmt::color::red), "🔻 SELL | {}\n", j["s"].get<std::string>());
                    } else {
                        // シグナルがない時も生存確認として出すならここ
                        // fmt::print(" . "); 
                    }
                    last_print_time = now;
                }
                }
            } catch (const std::exception& e) {
                // パースエラー等のハンドリング
            }
        }
    });

    webSocket.start();
    fmt::print(fg(fmt::color::cyan), "📡 WebSocket connected. Trading bot is running...\n");

    // メインループ: 答え合わせ(check_pending_trades)などを定期実行
    while (true) {
        // 価格チェック用のmapを作成（pending_checks用）
        std::map<std::string, double> current_prices;
        for (auto const& [symbol, state] : history) {
            if (state.last_price > 0) {
                current_prices[symbol] = state.last_price;
            }
        }

        // 1分経過したデータのCSV保存チェック
        check_pending_trades(current_prices, pending_checks);

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}