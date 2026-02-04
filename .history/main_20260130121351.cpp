#include "ScanMarket.h"
#include "SOMEvaluator.h"
#include <fmt/core.h>
#include <fmt/color.h>
#include <thread>
#include <chrono>
#include <map>
#include <string>


int main() {
    SOMEvaluator som;
    if(!som.loadModel("som_map_weights.csv", "som_signals.csv", "som_scaling_params.csv")) {
        fmt::print("Failed to load SOM model files.\n");
        return -1;
    }
    fmt::print("✅ SOM Model Loaded Successfully!\n");

    std::map<std::string, MarketState> history;
    std::vector<TradeData> pending_checks;

    while (true) {
        // 市場データのスキャン
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

            if (signal == 1) {
                fmt::print("🚀 SOM Signal: BUY! Target: BTCUSDT\n");
                // ここで発注関数を呼ぶ
            } else if (signal == -1) {
                fmt::print("🔻 SOM Signal: SELL! Target: BTCUSDT\n");
                // ここで発注関数を呼ぶ
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }
    return 0;
}