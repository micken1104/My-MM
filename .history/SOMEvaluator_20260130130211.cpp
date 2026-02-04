#include "SOMEvaluator.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include <iostream>

bool SOMEvaluator::loadModel(const std::string& weights_csv, const std::string& expectancy_csv, const std::string& params_csv) {
    map_weights.clear();
    exp_map.clear();
    mins.clear();
    maxs.clear();

    std::string line, val;

    // 1. 重み (som_map_weights.csv) 400x6
    // Python側はヘッダーなしで np.savetxt しているので、全行読み込む
    std::ifstream wf(weights_csv);
    if (!wf.is_open()) return false;
    while (std::getline(wf, line)) {
        std::vector<double> row;
        std::stringstream ss(line);
        while (std::getline(ss, val, ',')) {
            row.push_back(std::stod(val));
        }
        if (!row.empty()) map_weights.push_back(row);
    }

    // 2. シグナル (som_expectancy.csv) 400x1 (フラット)
    // Python側は expectancy.flatten() を np.savetxt しているので、1行1要素
    std::ifstream sf(expectancy_csv);
    if (!sf.is_open()) return false;
    while (std::getline(sf, line)) {
        if (!line.empty()) {
            // stodしてからintにキャスト(Pythonが浮動小数点形式で保存する場合があるため)
            signal_map.push_back(static_cast<int>(std::stod(line)));
        }
    }

    // 3. スケーリング定数 (som_scaling_params.csv) 
    // Python側は scaling_params.to_csv でヘッダーあり(min, max)
    std::ifstream pf(params_csv);
    if (!pf.is_open()) return false;
    std::getline(pf, line); // ヘッダー(min, max)を読み飛ばす
    while (std::getline(pf, line)) {
        std::stringstream ss(line);
        std::string min_val, max_val;
        if (std::getline(ss, min_val, ',') && std::getline(ss, max_val, ',')) {
            mins.push_back(std::stod(min_val));
            maxs.push_back(std::stod(max_val));
        }
    }

    // ロード結果のチェック
    if (map_weights.size() != 400 || signal_map.size() != 400 || mins.size() != 6) {
        std::cerr << "SOM Load Error: Weights=" << map_weights.size() 
                  << ", expectancy=" << signal_map.size() 
                  << ", Params=" << mins.size() << std::endl;
        return false;
    }

    return true; 
}

double SOMEvaluator::predict(const std::vector<double>& raw_data) {
    if (mins.empty() || map_weights.empty()) return 0;

    // A. スケーリング（PythonのMinMaxScalerと同じ計算）
    std::vector<double> scaled_data(raw_data.size());
    for(size_t i=0; i < raw_data.size(); ++i) {
        // (x - min) / (max - min)
        scaled_data[i] = (raw_data[i] - mins[i]) / (maxs[i] - mins[i] + 1e-9);
        
        // 範囲外をクリップ（0〜1に収める）
        if (scaled_data[i] < 0.0) scaled_data[i] = 0.0;
        if (scaled_data[i] > 1.0) scaled_data[i] = 1.0;
    }

    // B. BMU探索（爆速ループ）
    int best_idx = 0;
    double min_dist = 1e18;
    for(int i=0; i < (int)map_weights.size(); ++i) {
        double dist = calculateDist(map_weights[i], scaled_data);
        if(dist < min_dist) {
            min_dist = dist;
            best_idx = i;
        }
    }

    // C. その座標のシグナルを返す
    return expectancy_map[best_idx];
}

double SOMEvaluator::calculateDist(const std::vector<double>& a, const std::vector<double>& b) {
    double d = 0;
    // ループ回数は固定(6)なので非常に高速
    for(size_t i=0; i < a.size(); ++i) {
        d += std::abs(a[i] - b[i]);
    }
    return d;
}