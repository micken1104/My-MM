import math
import os
from pathlib import Path

import pandas as pd


def safe_div(numerator: float, denominator: float) -> float:
    if denominator == 0:
        return float("nan")
    return numerator / denominator


def max_streak(mask: pd.Series) -> int:
    if mask.empty:
        return 0
    groups = (mask != mask.shift()).cumsum()
    streak_sizes = mask.groupby(groups).cumcount() + 1
    return int(streak_sizes[mask].max()) if mask.any() else 0


def round_or_nan(value: float, digits: int = 4) -> float:
    if pd.isna(value):
        return float("nan")
    return round(float(value), digits)


def load_trade_files() -> list[Path]:
    data_dir = Path("./data/current")
    if not data_dir.exists():
        return []
    return sorted(data_dir.glob("*_trades.csv"))


def normalize_trade_frame(raw_df: pd.DataFrame) -> pd.DataFrame:
    if raw_df.empty:
        return raw_df

    if raw_df.shape[1] >= 12:
        df = raw_df.iloc[:, :12].copy()
        df.columns = ["ts", "symbol", "entry", "exit", "pnl", "reason", "side", "gross_pnl", "net_pnl", "hold_sec", "tp_rate", "sl_rate"]
        if "pnl" not in df.columns and "net_pnl" in df.columns:
            df["pnl"] = df["net_pnl"]
    elif raw_df.shape[1] >= 6:
        df = raw_df.iloc[:, :6].copy()
        df.columns = ["ts", "symbol", "entry", "exit", "pnl", "reason"]
        df["side"] = "LONG"
        df["gross_pnl"] = df["pnl"]
        df["net_pnl"] = df["pnl"]
        df["hold_sec"] = pd.NA
        df["tp_rate"] = pd.NA
        df["sl_rate"] = pd.NA
    else:
        return pd.DataFrame()

    return df


def calculate_metrics(df: pd.DataFrame, symbol: str) -> dict:
    df = df.copy()
    df["ts"] = pd.to_numeric(df["ts"], errors="coerce")
    df["entry"] = pd.to_numeric(df["entry"], errors="coerce")
    df["exit"] = pd.to_numeric(df["exit"], errors="coerce")
    df["pnl"] = pd.to_numeric(df["pnl"], errors="coerce")
    df = df.dropna(subset=["pnl", "ts"])

    if df.empty:
        return {
            "Symbol": symbol,
            "Total": 0,
        }

    df["reason"] = df["reason"].fillna("").astype(str)
    df["dt"] = pd.to_datetime(df["ts"], unit="s", utc=True, errors="coerce")

    total_trades = len(df)
    wins = df["pnl"] > 0
    losses = df["pnl"] < 0

    win_count = int(wins.sum())
    loss_count = int(losses.sum())
    flat_count = int((df["pnl"] == 0).sum())

    tp_count = int(df["reason"].str.contains("TP", case=False, na=False).sum())
    sl_count = int(df["reason"].str.contains("SL", case=False, na=False).sum())
    tu_mask = df["reason"].str.contains("Time Up", case=False, na=False)
    tu_count = int(tu_mask.sum())

    avg_win = df.loc[wins, "pnl"].mean()
    avg_loss_abs = -df.loc[losses, "pnl"].mean()
    gross_profit = df.loc[wins, "pnl"].sum()
    gross_loss_abs = -df.loc[losses, "pnl"].sum()

    equity_curve = df["pnl"].cumsum()
    running_peak = equity_curve.cummax()
    drawdown = equity_curve - running_peak
    max_drawdown = drawdown.min() if not drawdown.empty else float("nan")

    pnl_std = df["pnl"].std(ddof=1)
    var95 = df["pnl"].quantile(0.05)
    cvar95 = df.loc[df["pnl"] <= var95, "pnl"].mean()

    sharpe_like = safe_div(df["pnl"].mean(), pnl_std) * math.sqrt(total_trades) if pnl_std and not pd.isna(pnl_std) else float("nan")
    payoff_ratio = safe_div(avg_win, avg_loss_abs)
    win_rate = safe_div(win_count, total_trades)
    kelly_f = win_rate - safe_div((1 - win_rate), payoff_ratio) if not pd.isna(payoff_ratio) else float("nan")

    metrics = {
        "Symbol": symbol,
        "Total": total_trades,
        "Win%": round_or_nan(win_rate * 100, 2),
        "Loss%": round_or_nan(safe_div(loss_count, total_trades) * 100, 2),
        "Flat%": round_or_nan(safe_div(flat_count, total_trades) * 100, 2),
        "TP%": round_or_nan(safe_div(tp_count, total_trades) * 100, 2),
        "SL%": round_or_nan(safe_div(sl_count, total_trades) * 100, 2),
        "TimeUp%": round_or_nan(safe_div(tu_count, total_trades) * 100, 2),
        "NetPnL%": round_or_nan(df["pnl"].sum()),
        "AvgPnL%": round_or_nan(df["pnl"].mean()),
        "MedianPnL%": round_or_nan(df["pnl"].median()),
        "StdPnL": round_or_nan(pnl_std),
        "BestTrade%": round_or_nan(df["pnl"].max()),
        "WorstTrade%": round_or_nan(df["pnl"].min()),
        "AvgWin%": round_or_nan(avg_win),
        "AvgLoss%": round_or_nan(-avg_loss_abs),
        "Expectancy%": round_or_nan(df["pnl"].mean()),
        "ProfitFactor": round_or_nan(safe_div(gross_profit, gross_loss_abs), 3),
        "Payoff": round_or_nan(payoff_ratio, 3),
        "SharpeLike": round_or_nan(sharpe_like, 3),
        "MaxDD%": round_or_nan(max_drawdown),
        "Recovery": round_or_nan(safe_div(df["pnl"].sum(), abs(max_drawdown)) if max_drawdown < 0 else float("nan"), 3),
        "VaR95%": round_or_nan(var95),
        "CVaR95%": round_or_nan(cvar95),
        "MaxWinStreak": max_streak(wins),
        "MaxLossStreak": max_streak(losses),
        "TU_PnL_Avg": round_or_nan(df.loc[tu_mask, "pnl"].mean()),
        "TU_Win%": round_or_nan(df.loc[tu_mask, "pnl"].gt(0).mean() * 100 if tu_count else float("nan"), 2),
        "KellyF": round_or_nan(kelly_f, 3),
        "StartUTC": df["dt"].min().strftime("%Y-%m-%d %H:%M:%S") if df["dt"].notna().any() else "-",
        "EndUTC": df["dt"].max().strftime("%Y-%m-%d %H:%M:%S") if df["dt"].notna().any() else "-",
    }
    return metrics


def print_hourly_edge(df_all: pd.DataFrame) -> None:
    if df_all.empty:
        return

    tmp = df_all.copy()
    tmp["hour"] = tmp["dt"].dt.hour
    hourly = (
        tmp.groupby(["Symbol", "hour"], as_index=False)
        .agg(Trades=("pnl", "count"), AvgPnL=("pnl", "mean"), WinRate=("pnl", lambda s: (s > 0).mean() * 100))
    )
    hourly = hourly[hourly["Trades"] >= 5]
    if hourly.empty:
        print("\n--- Hourly Edge (>=5 trades/hour) ---")
        print("Not enough trades for hourly segmentation.")
        print("--------------------------------------")
        return

    best_by_symbol = hourly.sort_values(["Symbol", "AvgPnL"], ascending=[True, False]).groupby("Symbol").head(3)
    best_by_symbol["AvgPnL"] = best_by_symbol["AvgPnL"].round(4)
    best_by_symbol["WinRate"] = best_by_symbol["WinRate"].round(2)

    print("\n--- Hourly Edge Top3 (>=5 trades/hour) ---")
    print(best_by_symbol.to_string(index=False))
    print("------------------------------------------")


def analyze_trade_quality() -> None:
    trade_files = load_trade_files()
    if not trade_files:
        print("No trade files found in ./data")
        return

    summary = []
    merged_frames = []

    for path in trade_files:
        symbol = path.stem.replace("_trades", "")
        raw_df = pd.read_csv(path, header=None)
        df = normalize_trade_frame(raw_df)
        if df.empty:
            continue

        metrics = calculate_metrics(df, symbol)
        summary.append(metrics)

        if "pnl" in df.columns:
            tmp = df.copy()
            tmp["pnl"] = pd.to_numeric(tmp["pnl"], errors="coerce")
            tmp["ts"] = pd.to_numeric(tmp["ts"], errors="coerce")
            tmp["dt"] = pd.to_datetime(tmp["ts"], unit="s", utc=True, errors="coerce")
            tmp["Symbol"] = symbol
            merged_frames.append(tmp.dropna(subset=["pnl", "dt"]))

    df_summary = pd.DataFrame(summary)
    if df_summary.empty:
        print("No valid trade data to analyze.")
        return

    numeric_cols = [
        "Total", "Win%", "Loss%", "Flat%", "TP%", "SL%", "TimeUp%", "NetPnL%", "AvgPnL%", "MedianPnL%",
        "StdPnL", "BestTrade%", "WorstTrade%", "AvgWin%", "AvgLoss%", "Expectancy%", "ProfitFactor", "Payoff",
        "SharpeLike", "MaxDD%", "Recovery", "VaR95%", "CVaR95%", "MaxWinStreak", "MaxLossStreak", "TU_PnL_Avg",
        "TU_Win%", "KellyF",
    ]

    portfolio_df = pd.concat(merged_frames, ignore_index=True) if merged_frames else pd.DataFrame(columns=["pnl", "dt", "Symbol"])
    if not portfolio_df.empty:
        portfolio_metrics = calculate_metrics(
            portfolio_df.rename(columns={"Symbol": "symbol"}),
            "PORTFOLIO",
        )
        df_summary = pd.concat([df_summary, pd.DataFrame([portfolio_metrics])], ignore_index=True)

    df_summary = df_summary.sort_values(["Symbol"], kind="stable")
    for col in numeric_cols:
        if col in df_summary.columns:
            df_summary[col] = pd.to_numeric(df_summary[col], errors="coerce")

    print("\n=== Trade Quality Analysis (Extended) ===")
    print(df_summary.to_string(index=False))
    print("=========================================\n")

    if not portfolio_df.empty:
        print_hourly_edge(portfolio_df)


if __name__ == "__main__":
    analyze_trade_quality()