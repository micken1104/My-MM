#include "SOMEvaluator.h"
#include <fstream>
#include <sstream>
#include <cmath>

bool SOMEvaluator::loadModel(const std::string& weights_csv, const std::string& signals_csv, const std::string& params_csv) {
    // 1. 重み (som_map_weights.csv) を読み込む
    // 2. シグナル (som_signals.csv) を読み込む
    // 3. スケーリング定数 (som_scaling_params.csv) を読み込む
    // ※ ここはifstreamを使った標準的なCSVパース処理を書きます
    return true; 
}

int SOMEvaluator::predict(const std::vector<double>& raw_data) {
    // A. スケーリング（PythonのMinMaxScalerと同じ計算）
    std::vector<double> scaled_data(raw_data.size());
    for(int i=0; i<raw_data.size(); ++i) {
        scaled_data[i] = (raw_data[i] - mins[i]) / (maxs[i] - mins[i] + 1e-9);
    }

    // B. BMU探索（爆速ループ）
    int best_idx = 0;
    double min_dist = 1e18;
    for(int i=0; i<map_weights.size(); ++i) {
        double dist = calculateDist(map_weights[i], scaled_data);
        if(dist < min_dist) {
            min_dist = dist;
            best_idx = i;
        }
    }

    // C. その座標のシグナルを返す
    return signal_map[best_idx];
}

double SOMEvaluator::calculateDist(const std::vector<double>& a, const std::vector<double>& b) {
    double d = 0;
    for(size_t i=0; i<a.size(); ++i) d += std::abs(a[i] - b[i]);
    return d;
}