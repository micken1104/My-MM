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


using json = nlohmann::json;


int main() {
    // ==========================================
    std::string target = "SOLUSDT";   // 取引したいメイン銘柄
    std::string support = "BTCUSDT";  // 相関を見るための銘柄（常にBTCにするのがおすすめ）

    SOMEvaluator som;
    // モデルの読み込み
    if (!som.loadModel("som_map_weights.csv", "som_expectancy.csv", "som_scaling_params.csv")) {
        fmt::print(fg(fmt::color::red), "❌ Failed to load SOM model files.\n");
        return -1;
    }
    fmt::print(fg(fmt::color::green), "✅ SOM Model Loaded Successfully!\n");

    std::map<std::string, MarketState> history;
    std::vector<TradeData> pending_samples; // CSV保存用（1秒間隔）
    std::vector<TradeData> pending_trades;  // 損益計算用（注文時のみ）

    ix::WebSocket webSocket;
    // BTCとETHの両方のストリームを購読
    std::string url = "wss://stream.binance.com:9443/ws/btcusdt@bookTicker/ethusdt@bookTicker";
    webSocket.setUrl(url);

    webSocket.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            try {
                auto j = nlohmann::json::parse(msg->str);
                
                // WebSocketデータのパースと履歴更新
                process_ws_data(j, history, pending_samples); // 全データをサンプルとして蓄積


                // BTCとETHの両方のデータがhistoryに揃っているか確認
                if (history.count("BTCUSDT") && history.count("ETHUSDT")) {
                    auto& btc = history["BTCUSDT"];
                    auto& eth = history["ETHUSDT"];

                    // [btc_imb, btc_diff, btc_vol, eth_imb, eth_diff, eth_vol]
                    std::vector<double> input = {
                        btc.imbalance, btc.diff, btc.volume,
                        eth.imbalance, eth.diff, eth.volume
                    };

                    double expectancy = som.predict(input); 
                    // BTCを基準にする場合
                    double current_btc_price = history["BTCUSDT"].last_price;
                    std::string btc_symbol = "BTCUSDT";
                    execute_trade(expectancy, current_btc_price, btc_symbol, pending_trades);

                    //static int last_signal = 0;
                    //if (signal != last_signal) {
                        // BUY -> SELL に変わった瞬間だけ表示
                        //fmt::print("Signal Changed: {}\n", signal);
                        //last_signal = signal;
                    //}
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

        check_pending_trades(current_prices, pending_samples); // CSVへ保存（30秒後）
        update_real_pnl(current_prices, pending_trades);      // 損益を表示（60秒後）

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}