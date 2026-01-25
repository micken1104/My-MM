#include <iostream>
#include <httplib.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

int main() {
    // 1. BinanceのAPIサーバーに接続（SSL/HTTPSを使用）
    httplib::Client cli("https://api.binance.com");

    std::cout << "Fetching BTC price from Binance..." << std::endl;

    // 2. BTC/USDTの最新価格エンドポイントを叩く
    auto res = cli.Get("/api/v3/ticker/price?symbol=BTCUSDT");

    if (res && res->status == 200) {
        // 3. 取得したJSONデータを解析
        json data = json::parse(res->body);
        std::string price = data["price"];
        std::string symbol = data["symbol"];
        
        std::cout << "-------------------------------" << std::endl;
        std::cout << "Symbol: " << symbol << std::endl;
        std::cout << "Price : $" << price << std::endl;
        std::cout << "-------------------------------" << std::endl;
    } else {
        std::cerr << "Error: Failed to fetch data. Status: " << (res ? res->status : -1) << std::endl;
    }

    return 0;
}