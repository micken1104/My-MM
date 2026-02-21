#ifndef SOMEVALUATOR_H
#define SOMEVALUATOR_H

#include <vector>
#include <string>
#include <mutex>

/**
 * @brief SOMの推論結果を格納する構造体
 */
struct SOMResult {
    double expectancy; // 期待値（価格変化率の平均）
    double risk;       // リスク（価格変化率の標準偏差）
};

/**
 * @brief 自己組織化マップ(SOM)を用いて市場データを評価するクラス
 */
class SOMEvaluator {
public:
    SOMEvaluator() = default;
    ~SOMEvaluator() = default;

    /**
     * @brief 各種CSVファイルから学習済みモデルをロードする
     * @return ロード成功時 true
     */
    bool loadModel(const std::string& weights_csv, 
                   const std::string& expectancy_csv, 
                   const std::string& params_csv,
                   const std::string& risk_csv);

    /**
     * @brief 生データからBMU(最良一致ユニット)を特定し、期待値とリスクを返す
     * @param raw_data 特徴量ベクトル（スケーリング前）
     */
    SOMResult getPrediction(const std::vector<double>& raw_data);

private:
    /**
     * @brief ベクトル間の距離（L1ノルム/マンハッタン距離）を計算する
     */
    double calculateDist(const std::vector<double>& a, const std::vector<double>& b);

    // モデルデータ
    std::vector<std::vector<double>> map_weights;  // 各ノードの重みベクトル
    std::vector<double> expectancy_map;           // 各ノードの期待値
    std::vector<double> risk_map;                 // 各ノードのリスク（標準偏差）
    
    // スケーリングパラメータ
    std::vector<double> mins; // 各特徴量の最小値
    std::vector<double> maxs; // 各特徴量の最大値

    // スレッド安全性を確保するためのミューテックス
    mutable std::mutex mtx;
};

#endif // SOMEVALUATOR_H