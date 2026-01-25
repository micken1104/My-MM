#include <httplib.h>
#include <nlohmann/json.hpp>
#include <fmt/core.h>

using json = nlohmann::json;

int main() {
    fmt::print("1. Program started...\n");

    // HTTPS接続のクライアント作成
    httplib::Client cli("https://api.binance.com");
    
    // タイムアウトを少し長めの10秒に設定
    cli.set_connection_timeout(10); 
    cli.set_read_timeout(10);

    fmt::print("2. Sending request to Binance...\n");

    auto res = cli.Get("/api/v3/ticker/price?symbol=BTCUSDT");

    if (res) {
        fmt::print("3. Response received! Status: {}\n", res->status);
        if (res->status == 200) {
            json data = json::parse(res->body);
            fmt::print("4. BTC Price: ${}\n", data["price"].get<std::string>());
        } else {
            fmt::print("Error: Server returned status {}\n", res->status);
        }
    } else {
        // ここが重要：なぜ失敗したかエラーコードを出す
        auto err = res.error();
        fmt::print("3. Request FAILED. Error code: {}\n", (int)err);
        
        if ((int)err == 1) fmt::print("-> Hint: SSL connection error. OpenSSL might be missing.\n");
        if ((int)err == 5) fmt::print("-> Hint: Connection timeout. Check your internet.\n");
    }

    fmt::print("5. Program finished.\n");
    return 0;
}