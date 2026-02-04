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

// 仮想的な注文ロジックの例
void execute_trade(double expectancy) {
    double balance = 10000.0;     // 現在の口座残高（例: 10,000ドル）
    double risk_free_rate = 2.0;  // 資金の何倍までリスクを取るか（レバレッジ係数）
    
    // expectancy は Python からきた「平均変化率(%)」
    // 例: 0.15 (%) -> 0.0015 (小数)
    double edge = expectancy / 100.0;

    // ケリー基準の超簡易版: ポジションサイズ = 残高 * (エッジ / リスク)
    // 期待値がプラスなら買い、マイナスなら売り
    double lot_size_usd = balance * std::abs(edge) * risk_free_rate;

    // あまりに大きな注文にならないよう上限を設定（安全策）
    double max_lot = balance * 0.5; 
    lot_size_usd = std::min(lot_size_usd, max_lot);

    if (lot_size_usd > 10.0) { // 最小注文単位以上の時だけ実行
        if (expectancy > 0) {
            fmt::print(fg(fmt::color::green), "📈 Long: ${:.2f} (Expectancy: {:.3f}%)\n", lot_size_usd, expectancy);
        } else {
            fmt::print(fg(fmt::color::red), "📉 Short: ${:.2f} (Expectancy: {:.3f}%)\n", lot_size_usd, expectancy);
        }
    }
}

int main() {
    SOMEvaluator som;
    // モデルの読み込み
    if (!som.loadModel("som_map_weights.csv", "som_expectancy.csv", "som_scaling_params.csv")) {
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

                    static int last_signal = 0;
                    if (signal != last_signal) {
                        // BUY -> SELL に変わった瞬間だけ表示
                        fmt::print("Signal Changed: {}\n", signal);
                        last_signal = signal;
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