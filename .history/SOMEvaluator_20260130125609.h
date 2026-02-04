#ifndef SOMEVALUATOR_H
#define SOMEVALUATOR_H

#include <vector>
#include <string>

class SOMEvaluator {
public:
    SOMEvaluator() {}
    // Pythonが吐き出した3つのCSVを読み込む
    bool loadModel(const std::string& weights_csv, const std::string& signals_csv, const std::string& params_csv);
    
    // 生の板情報を受け取ってを返す
    double predict(const std::vector<double>& raw_data);

private:
    std::vector<std::vector<double>> map_weights; // 400x6 の地図
    std::vector<int> signal_map;                  // 400個の売買指示
    std::vector<double> mins, maxs;               // スケーリング用定数

    // 距離計算（L1ノルム）
    double calculateDist(const std::vector<double>& a, const std::vector<double>& b);
};

#endif