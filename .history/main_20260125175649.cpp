#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <fmt/core.h>
#include <iostream>
#include <thread>  //for sleep
#include <chrono>

using json = nlohmann::json;
int show(int last){
    httplib::Client cli("https://api.binance.com");
    cli.set_connection_timeout(5, 0); // 5 seconds
    cli.set_read_timeout(5, 0);
    std::string price="error!";

    auto res = cli.Get("/api/v3/ticker/price?symbol=BTCUSDT");

    if (res && res->status == 200) {
        json data = json::parse(res->body);
        
        // nlohmann::json から値を取得。fmtなら型を自動判別してくれます
        price = data["price"];
        std::string symbol = data["symbol"];
        
        fmt::print("{} Price : ${}\n", symbol, price);
    } else {
        auto err = res.error();
        fmt::print(stderr, "Network Error: {}\n", (int)err);
    }
    return std::stoi(price);
}

int main() {
    fmt::print("Program Started...\n");
    std::fflush(stdout);
    
    for(int i=0;i<100;i++){
        int current=0;
        current=show(current);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    return 0;
}