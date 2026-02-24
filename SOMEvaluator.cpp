#include "SOMEvaluator.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm> // std::clamp用
#include <iostream>

bool SOMEvaluator::loadModel(const std::string& weights_csv, 
                             const std::string& expectancy_csv, 
                             const std::string& params_csv,
                             const std::string& risk_csv) {
    std::lock_guard<std::mutex> lock(mtx);
    
    map_weights.clear();
    expectancy_map.clear();
    risk_map.clear();
    mins.clear();
    maxs.clear();

    std::string line, val;

    // 1. 重みのロード
    std::ifstream wf(weights_csv);
    if (!wf.is_open()) return false;
    while (std::getline(wf, line)) {
        std::vector<double> row;
        std::stringstream ss(line);
        while (std::getline(ss, val, ',')) { row.push_back(std::stod(val)); }
        if (!row.empty()) map_weights.push_back(row);
    }

    // 2. 期待値(Expectancy)のロード
    std::ifstream sf(expectancy_csv);
    if (!sf.is_open()) return false;
    while (std::getline(sf, line)) {
        if (!line.empty()) expectancy_map.push_back(std::stod(line));
    }

    // 3. リスク(Risk)のロード ★修正：引数 risk_csv を使用するように変更
    std::ifstream rf(risk_csv);
    if (!rf.is_open()) {
        std::cerr << "Risk map not found, using default 0.05" << std::endl;
    } else {
        while (std::getline(rf, line)) {
            if (!line.empty()) risk_map.push_back(std::stod(line));
        }
    }

    // 4. スケーリングパラメータのロード
    std::ifstream pf(params_csv);
    if (!pf.is_open()) return false;
    std::getline(pf, line); // ヘッダーをスキップ
    while (std::getline(pf, line)) {
        std::stringstream ss(line);
        std::string feature_name, min_val, max_val;
        // フォーマット: feature_name,min,max
        if (std::getline(ss, feature_name, ',') && 
            std::getline(ss, min_val, ',') && 
            std::getline(ss, max_val, ',')) {
            mins.push_back(std::stod(min_val));
            maxs.push_back(std::stod(max_val));
        }
    }

    // バリデーション (7つの特徴量: imbalance, imbalance_change, btc_imbalance, btc_imbalance_change)
    if (map_weights.size() != 400 || expectancy_map.size() != 400 || mins.size() != 7) {
        return false;
    }
    return true; 
}

SOMResult SOMEvaluator::getPrediction(const std::vector<double>& raw_data) {
    std::lock_guard<std::mutex> lock(mtx);
    
    // 入力データのサイズバリデーションを追加
    if (mins.empty() || map_weights.empty() || raw_data.size() < mins.size()) {
        return {0.0, 0.05}; 
    }
    // A. スケーリング（計算は1回だけ）
    std::vector<double> scaled_data(raw_data.size());
    for(size_t i=0; i < raw_data.size(); ++i) {
        double range = maxs[i] - mins[i]; // 0の場合もある
        scaled_data[i] = (range < 1e-9) ? 0.5 : (raw_data[i] - mins[i]) / range;
    }

    // B. BMU探索（爆速ループ：計算は1回だけ）
    int best_idx = 0;
    double min_dist = 1e18;
    for(int i=0; i < (int)map_weights.size(); ++i) {
        double dist = calculateDist(map_weights[i], scaled_data);
        if(dist < min_dist) {
            min_dist = dist;
            best_idx = i;
        }
    }

    // C. 結果をペアで返す
    double exp = expectancy_map[best_idx];
    double risk = (best_idx < (int)risk_map.size()) ? risk_map[best_idx] : 0.05;

    return {exp, risk};
}

double SOMEvaluator::calculateDist(const std::vector<double>& a, const std::vector<double>& b) {
    double d = 0;
    for(size_t i=0; i < a.size(); ++i) {
        d += std::abs(a[i] - b[i]);
    }
    return d;
}