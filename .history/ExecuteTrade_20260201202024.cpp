#include <fmt/color.h>
#include <chrono>
#include <fmt/color.h>
#include "ScanMarket.h"

static auto last_trade_time = std::chrono::steady_clock::now();
static double virtual_balance = 10000.0; // 仮想的な現在の残高


// 仮想的な注文ロジックの例
void execute_trade(double expectancy, double current_price, std::string symbol, 
                   std::vector<TradeData>& pending_trades, double local_risk) {
    double risk_free_rate = 10.0;  // 資金の何倍までリスクを取るか（レバレッジ係数）
    // expectancy は Python からきた「平均変化率(%)」平均して 0.05% 上昇する見込みがある
    // 例: 0.15 (%) -> 0.0015 (小数)
    double edge = expectancy / 100.0;


    // 現在の時間を取得
    auto now = std::chrono::steady_clock::now();
    // 前回の実行から何秒経ったか計算
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_trade_time).count();


    // ケリー基準（期待値/分散）の超簡易版: ポジションサイズ = 残高 * (エッジ / リスク)
    // AIが算出した期待値の強弱に応じて動的にレバレッジを調整することで、リスクを抑えつつ幾何平均収益率を最大化させる設計にしています。
    // 理論上のケリー基準（フル・ケリー）は、数学的には資産を最大化させますが、負けが続いた時のドローダウン（資産の減り）が激しすぎるという欠点があります。
    //そのため、実際の投資では「ケリー基準で出た値の 1/2 や 1/10 だけを賭ける」という分数ケリーが一般的です。
    double safety_factor = 0.5; // 「1/10 ケリー」にする
    double lot_size_usd = virtual_balance * std::abs(edge) * risk_free_rate * safety_factor;
    // あまりに大きな注文にならないよう上限を設定（安全策）
    double max_lot = virtual_balance * 0.5;  //全財産のn%まで
    lot_size_usd = std::min(lot_size_usd, max_lot);

    // 期待値(%)の絶対値。例えば 0.03% 未満はノイズとして捨てる
    double abs_expectancy = std::abs(expectancy);
    

    if (elapsed % 10 == 0) { // ログが溢れないよう、10回に1回表示
        fmt::print("Check: Exp={:.3f}%, Size=${:.2f} (Target: >0.09%, >$10.0)\n", expectancy, lot_size_usd);
    }
    // 1. 期待値が手数料（往復0.04%〜0.1%程度）を明確に上回る確信があるか
    // 2. 注文サイズが十分か
    // 3. 前回のトレードから十分な時間が経過したか（頻度を落とす）
    // 初期テストでは、微小な期待値でのエントリーが手数料負けを引き起こすことを確認したため、期待値のしきい値の最適化（Threshold Optimization）とエントリー頻度の制限を行い、期待利得を最大化させる改善サイクルを回しています
    if (abs_expectancy > 0.12 && lot_size_usd > 10.0) { // 最小注文単位以上の時だけ実行(手数料とか，期待値(自信)が低いときに動かないように)
        // もし既に pending_trades にデータがあるなら、新しい注文は出さない
        if (!pending_trades.empty()) {
            return; 
        }

        TradeData order;
        order.symbol = symbol;
        order.entry_price = current_price;
        order.lot_size = lot_size_usd;
        order.side = (expectancy > 0) ? "LONG" : "SHORT";
        
        // --- ★ここを追加：SOMの座標から算出したリスクを損切幅に反映 ---
        // local_risk(%)を小数に戻して使用。例: SOMが0.05%と答えたら 0.0005。
        // ノイズ回避のため2倍（2σ）程度を損切ラインにするのが一般的です。
        double sl_percent = (local_risk / 100.0) * 2.0; 
        
        // 最低損切を 0.05%、最大を 0.2% などに制限（ガードレール）
        if (sl_percent < 0.0005) sl_percent = 0.0005;
        if (sl_percent > 0.0020) sl_percent = 0.0020;
        
        order.dynamic_sl = -sl_percent; // 損切なのでマイナス値
        
        order.entry_time = std::chrono::steady_clock::now();
        // UNIXエポック（1970/1/1）からの経過時間を取得
        order.entry_timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

        
        // トレード評価用のリストにだけ追加
        pending_trades.push_back(order);
        
        fmt::print(fg(fmt::color::cyan), "🛡️ [DYNAMIC SL] Set at: {:.3f}%\n", order.dynamic_sl * 100.0);
        fmt::print(fg(fmt::color::yellow), "🚀 [ORDER] {} {} | Price: ${:.2f} | Size: ${:.2f}\n", 
                   order.side, symbol, current_price, lot_size_usd);
        last_trade_time = now;
    }
}