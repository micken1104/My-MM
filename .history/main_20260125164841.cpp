#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <fmt/core.h>
#include <iostream>

using json = nlohmann::json;

int main() {
    fmt::print("Program Started...\n");
    httplib::Client cli("https://api.binance.com");

    fmt::print("Fetching BTC price from Binance...\n");

    auto res = cli.Get("/api/v3/ticker/price?symbol=BTCUSDT");

    if (res && res->status == 200) {
        json data = json::parse(res->body);
        
        // nlohmann::json から値を取得。fmtなら型を自動判別してくれます
        std::string price = data["price"];
        std::string symbol = data["symbol"];
        
        fmt::print("-------------------------------\n");
        fmt::print("Symbol: {}\n", symbol);
        fmt::print("Price : ${}\n", price);
        fmt::print("-------------------------------\n");
    } else {
        fmt::print(stderr, "Error: Failed to fetch data. Status: {}\n", (res ? res->status : -1));
    }

    return 0;
}