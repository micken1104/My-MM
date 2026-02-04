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
    std::string target = "ATOMUSDT";   // 取引したいメイン銘柄
    std::string support = "BTCUSDT";  // 相関を見るための銘柄（常にBTCにするのがおすすめ）
    // ==========================================

    // 小文字変換用のラムダ関数
    auto to_lower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
        return s;
    };

    SOMEvaluator som;
    // モデルの読み込み
    if (!som.loadModel("som_map_weights.csv", "som_expectancy.csv", "som_scaling_params.csv", "som_risk_map.csv")) {
        fmt::print(fg(fmt::color::red), "❌ Failed to load SOM model files.\n");
        return -1;
    }
    fmt::print(fg(fmt::color::green), "✅ SOM Model Loaded Successfully!\n");
    // ... 初回のロード ...

    // 🔄 ホット・リロード用スレッドを開始
    std::thread updater([&som]() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::minutes(30));
            // Pythonを実行
            if (std::system("python train_som.py") == 0) {
                // クラス内のloadModelを呼ぶだけで、内部でmutexロックされる
                if (som.loadModel("som_map_weights.csv", "som_expectancy.csv", "som_scaling_params.csv", "som_risk_map.csv")) {
                    fmt::print(fg(fmt::color::green), "🔄 [MODEL] Hot-reload complete.\n");
                }
            }
        }
    });
    updater.detach(); // メインスレッドから切り離してバックグラウンドで実行

    std::map<std::string, MarketState> history;
    std::vector<TradeData> pending_samples; // CSV保存用（1秒間隔）
    std::vector<TradeData> pending_trades;  // 損益計算用（注文時のみ）

    ix::WebSocket webSocket;
    // URL生成
    std::string url = fmt::format("wss://stream.binance.com:9443/ws/{}@bookTicker/{}@bookTicker", 
                                  to_lower(target), 
                                  to_lower(support));
    webSocket.setUrl(url);

    webSocket.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            try {
                auto j = nlohmann::json::parse(msg->str);
                process_ws_data(j, history, pending_samples);

                // 変数 target と support を使ってチェック
                if (history.count(target) && history.count(support)) {
                    auto& main_data = history[target];
                    auto& sub_data = history[support];

                    // 入力ベクトルも自動で対応
                    // [main_imb, main_diff, main_vol, sub_imb, sub_diff, sub_vol]
                    std::vector<double> input = {
                        main_data.imbalance, main_data.diff, main_data.volume,
                        sub_data.imbalance, sub_data.diff, sub_data.volume
                    };

                    double expectancy = som.predict(input); 
                    double current_price = main_data.last_price;
                    double risk = som.getRisk(input);
                    execute_trade(expectancy, current_price, target, pending_trades, risk);
                }
            } catch (...) {}
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