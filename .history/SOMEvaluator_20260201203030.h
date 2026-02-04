#ifndef SOMEVALUATOR_H
#define SOMEVALUATOR_H

#include <vector>
#include <string>
#include <mutex> // 追加

class SOMEvaluator {
public:
    SOMEvaluator() {}
    
    // ロード関数（risk_csvを追加）
    bool loadModel(const std::string& weights_csv, 
                   const std::string& signals_csv, 
                   const std::string& params_csv,
                   const std::string& risk_csv); // リスクマップも読み込むように拡張
    
    double predict(const std::vector<double>& raw_data);
    double getRisk(const std::vector<double>& raw_data); // 修正

private:
    mutable std::mutex mtx; // スレッド間の競合を防ぐための重し
    
    std::vector<std::vector<double>> map_weights;
    std::vector<double> expectancy_map;
    std::vector<double> risk_map; 
    std::vector<double> mins, maxs;

    double calculateDist(const std::vector<double>& a, const std::vector<double>& b);
};

#endif