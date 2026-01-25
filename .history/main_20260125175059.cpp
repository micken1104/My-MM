#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <fmt/core.h>
#include <iostream>
#include <thread>  //for sleep
#include <chrono>

using json = nlohmann::json;
void show(){
    httplib::Client cli("https://api.binance.com");
    cli.set_connection_timeout(5, 0); // 5 seconds
    cli.set_read_timeout(5, 0);

    fmt::print("Fetching BTC price from Binance...\n");
    std::fflush(stdout);

    auto res = cli.Get("/api/v3/ticker/price?symbol=BTCUSDT");

    if (res && res->status == 200) {
        json data = json::parse(res->body);
        
        // nlohmann::json から値を取得。fmtなら型を自動判別してくれます
        std::string price = data["price"];
        std::string symbol = data["symbol"];
        
        fmt::print("-------------------------------\n");
        fmt::print("{} Price : ${}\n", symbol);
        fmt::print("Price : ${}\n", price);
        fmt::print("-------------------------------\n");
    } else {
        auto err = res.error();
        fmt::print(stderr, "Network Error: {}\n", (int)err);
    }
}

int main() {
    fmt::print("Program Started...\n");
    std::fflush(stdout);
    
    for(int i=0;i<100;i++){
        show();
        usleep(500000);
    }

    return 0;
}