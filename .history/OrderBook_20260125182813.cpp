#include "OrderBook.h"
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <fmt/core.h>
#include <fmt/color.h>
#include <string>

using json = nlohmann::json;

void update_order_book() {
    httplib::Client cli("https://api.binance.com");
    auto res = cli.Get("/api/v3/depth?symbol=BTCUSDT&limit=5");
    //get 5 datas of each transit at BTCUSDT

    if (res && res->status == 200) {
        json data = json::parse(res->body);

        // extract best data(on Binance 0 is best)
        auto best_ask = std::stod(data["asks"][0][0].get<std::string>());
        auto best_bid = std::stod(data["bids"][0][0].get<std::string>());

        //calcrate spread
        double spread = best_ask - best_bid;
        double spread_pct = (spread / best_bid) * 100.0;

        // show
        fmt::print("\n====================================\n");
        fmt::print("Best Ask (Sell): {:>10.2f}\n", best_ask);
        fmt::print(fg(fmt::color::yellow), "SPREAD         : ${:>10.2f} ({:.4f}%)\n", spread, spread_pct);
        fmt::print("Best Bid (Buy) : {:>10.2f}\n", best_bid);
        fmt::print("====================================\n");
        for (int i = 4; i >= 0; i--) {
            fmt::print("  {}: {}\n", asks[i][0].get<std::string>(), asks[i][1].get<std::string>());
        }

        auto bids = data["bids"];
        fmt::print(fg(fmt::color::light_green), " [ Bids ]\n");
        for (int i = 0; i < 5; i++) {
            fmt::print("  {}: {}\n", bids[i][0].get<std::string>(), bids[i][1].get<std::string>());
        }
    }
}