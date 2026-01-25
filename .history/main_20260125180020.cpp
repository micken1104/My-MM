#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <fmt/core.h>
#include <fmt/color.h> //for color
#include <iostream>
#include <thread>  //for sleep
#include <chrono>

using json = nlohmann::json;
double show(double last_price){
    httplib::Client cli("https://api.binance.com");
    cli.set_connection_timeout(5, 0); // 5 seconds
    cli.set_read_timeout(5, 0);
    std::string price="error!";

    auto res = cli.Get("/api/v3/ticker/price?symbol=BTCUSDT");

    if (res && res->status == 200) {
        json data = json::parse(res->body);
        double current_price = std::stod(data["price"].get<std::string>());
        
        // nlohmann::json から値を取得。fmtなら型を自動判別してくれます
        price = data["price"];
        std::string symbol = data["symbol"];
        if(last<=double_price){
            fmt::print(fg(fmt::color::green), "Price: ${}\n", price);
        }else{
            fmt::print(fg(fmt::color::red), "Price: ${}\n", price);
        }
        return current_price;
    } else {
        auto err = res.error();
        fmt::print(stderr, "Network Error: {}\n", (int)err);
        return last_price;
    }
}

int main() {
    fmt::print("Program Started...\n");
    std::fflush(stdout);
    
    double current=0;
    for(int i=0;i<100;i++){
        current=show(current);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    return 0;
}