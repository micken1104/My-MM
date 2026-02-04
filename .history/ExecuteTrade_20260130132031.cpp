#include <fmt/color.h>

// 仮想的な注文ロジックの例
void execute_trade(double expectancy) {
    double balance = 10000.0;     // 現在の口座残高（例: 10,000ドル）
    double risk_free_rate = 2.0;  // 資金の何倍までリスクを取るか（レバレッジ係数）
    
    // expectancy は Python からきた「平均変化率(%)」平均して 0.05% 上昇する見込みがある
    // 例: 0.15 (%) -> 0.0015 (小数)
    double edge = expectancy / 100.0;

    // ケリー基準の超簡易版: ポジションサイズ = 残高 * (エッジ / リスク)
    // 期待値がプラスなら買い、マイナスなら売り
    double lot_size_usd = balance * std::abs(edge) * risk_free_rate;

    // あまりに大きな注文にならないよう上限を設定（安全策）
    double max_lot = balance * 0.5;  //全財産のn%まで
    lot_size_usd = std::min(lot_size_usd, max_lot);

    if (lot_size_usd > 10.0) { // 最小注文単位以上の時だけ実行(てすう)
        if (expectancy > 0) {
            fmt::print(fg(fmt::color::green), "📈 Long: ${:.2f} (Expectancy: {:.3f}%)\n", lot_size_usd, expectancy);
        } else {
            fmt::print(fg(fmt::color::red), "📉 Short: ${:.2f} (Expectancy: {:.3f}%)\n", lot_size_usd, expectancy);
        }
    }
}