import argparse
from pathlib import Path

import pandas as pd


def parse_args() -> argparse.Namespace:
	parser = argparse.ArgumentParser(
		description=(
			"Compare BTC volatility with changes in FRB rate expectation proxy "
			"(default: FRED DGS2)."
		)
	)
	parser.add_argument("--btc-data", default="data/current/BTCUSDT_market_data.csv")
	parser.add_argument(
		"--source",
		choices=["fred", "local"],
		default="fred",
		help="Rate data source. Use local CSV when offline.",
	)
	parser.add_argument(
		"--series",
		default="DGS2",
		help="FRED series id used as FRB expectation proxy (e.g., DGS2, DFF).",
	)
	parser.add_argument("--local-rate-csv", default="")
	parser.add_argument("--local-date-col", default="date")
	parser.add_argument("--local-value-col", default="implied_rate")
	parser.add_argument("--start", default="")
	parser.add_argument("--end", default="")
	parser.add_argument("--max-lag", type=int, default=10)
	parser.add_argument(
		"--save-merged",
		default="data/current/btc_rate_daily_merged.csv",
		help="Path to save merged daily data.",
	)
	parser.add_argument(
		"--save-plot",
		default="images/current/btc_vs_frb_rate_expectation.png",
		help="Path to save comparison plot.",
	)
	return parser.parse_args()


def load_btc_daily(btc_csv: str) -> pd.DataFrame:
	df = pd.read_csv(btc_csv, usecols=["timestamp", "price", "volatility"])
	df["dt"] = pd.to_datetime(df["timestamp"], unit="s", utc=True)
	df = df.sort_values("dt")
	df = df.set_index("dt")

	daily_vol = df["volatility"].resample("D").mean().rename("btc_vol")
	daily_price = df["price"].resample("D").last().rename("btc_price")
	daily_ret = daily_price.pct_change().rename("btc_ret")

	btc_daily = pd.concat([daily_vol, daily_price, daily_ret], axis=1).dropna()
	return btc_daily


def fetch_fred_series(series_id: str, start: pd.Timestamp, end: pd.Timestamp) -> pd.Series:
	url = f"https://fred.stlouisfed.org/graph/fredgraph.csv?id={series_id}"
	fred_df = pd.read_csv(url)
	date_col = None
	for candidate in ["DATE", "observation_date", "date"]:
		if candidate in fred_df.columns:
			date_col = candidate
			break

	if date_col is None or series_id not in fred_df.columns:
		raise ValueError(f"Unexpected FRED schema for series {series_id}.")

	fred_df = fred_df.rename(columns={date_col: "date", series_id: "rate_level"})
	fred_df["date"] = pd.to_datetime(fred_df["date"], utc=True)
	fred_df["rate_level"] = pd.to_numeric(fred_df["rate_level"], errors="coerce")
	fred_df = fred_df.dropna(subset=["rate_level"])

	mask = (fred_df["date"] >= start) & (fred_df["date"] <= end)
	series = fred_df.loc[mask].set_index("date")["rate_level"]
	return series.sort_index()


def load_local_series(path: str, date_col: str, value_col: str) -> pd.Series:
	local_df = pd.read_csv(path)
	if date_col not in local_df.columns or value_col not in local_df.columns:
		raise ValueError(
			f"local CSV must contain columns '{date_col}' and '{value_col}'."
		)

	local_df[date_col] = pd.to_datetime(local_df[date_col], utc=True)
	local_df[value_col] = pd.to_numeric(local_df[value_col], errors="coerce")
	local_df = local_df.dropna(subset=[value_col])
	return local_df.set_index(date_col)[value_col].sort_index().rename("rate_level")


def make_lag_corr_table(df: pd.DataFrame, max_lag: int) -> pd.DataFrame:
	rows = []
	for lag in range(-max_lag, max_lag + 1):
		shifted = df["rate_change_bp"].shift(lag)
		corr = df["btc_vol"].corr(shifted)
		rows.append({"lag_days": lag, "corr": corr})
	return pd.DataFrame(rows)


def build_plot(df: pd.DataFrame, lag_corr: pd.DataFrame, out_path: Path, series_label: str) -> None:
	import matplotlib.pyplot as plt

	fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(13, 9), sharex=False)

	ax1.plot(df.index, df["btc_vol"], label="BTC Daily Avg Volatility", color="tab:blue")
	ax1.set_ylabel("BTC Volatility")
	ax1.grid(True, linestyle=":", alpha=0.5)

	ax1b = ax1.twinx()
	ax1b.plot(
		df.index,
		df["rate_change_bp"],
		label=f"{series_label} Daily Change (bp)",
		color="tab:red",
		alpha=0.7,
	)
	ax1b.set_ylabel("Rate Change (bp)")

	lines1, labels1 = ax1.get_legend_handles_labels()
	lines2, labels2 = ax1b.get_legend_handles_labels()
	ax1.legend(lines1 + lines2, labels1 + labels2, loc="upper left")
	ax1.set_title("BTC Volatility vs FRB Rate Expectation Change")

	ax2.plot(lag_corr["lag_days"], lag_corr["corr"], marker="o", color="tab:green")
	ax2.axhline(0.0, color="black", linewidth=1)
	ax2.set_xlabel("Lag (days): + means rate leads BTC")
	ax2.set_ylabel("Correlation")
	ax2.set_title("Lag Correlation: BTC Volatility vs Rate Change")
	ax2.grid(True, linestyle=":", alpha=0.5)

	fig.tight_layout()
	out_path.parent.mkdir(parents=True, exist_ok=True)
	fig.savefig(out_path, dpi=160)
	plt.close(fig)


def main() -> None:
	args = parse_args()

	btc_daily = load_btc_daily(args.btc_data)
	if btc_daily.empty:
		raise ValueError("BTC market data is empty after daily aggregation.")

	start = pd.to_datetime(args.start, utc=True) if args.start else btc_daily.index.min()
	end = pd.to_datetime(args.end, utc=True) if args.end else btc_daily.index.max()

	btc_daily = btc_daily[(btc_daily.index >= start) & (btc_daily.index <= end)]
	if btc_daily.empty:
		raise ValueError("No BTC daily data remains in the selected date range.")

	if args.source == "fred":
		rate_level = fetch_fred_series(args.series, start, end)
		series_label = args.series
	else:
		if not args.local_rate_csv:
			raise ValueError("--local-rate-csv is required when --source local.")
		rate_level = load_local_series(
			args.local_rate_csv,
			date_col=args.local_date_col,
			value_col=args.local_value_col,
		)
		rate_level = rate_level[(rate_level.index >= start) & (rate_level.index <= end)]
		series_label = "local_rate"

	rate_df = rate_level.to_frame("rate_level")
	rate_df["rate_change_bp"] = rate_df["rate_level"].diff() * 100.0

	merged = btc_daily.join(rate_df, how="inner").dropna()
	if merged.empty:
		raise ValueError("Merged dataset is empty. Check date overlap and source data.")

	same_day_corr = merged["btc_vol"].corr(merged["rate_change_bp"])
	lag_corr = make_lag_corr_table(merged, max_lag=args.max_lag)
	best_idx = lag_corr["corr"].abs().idxmax()
	best_row = lag_corr.loc[best_idx]

	merged_out = Path(args.save_merged)
	merged_out.parent.mkdir(parents=True, exist_ok=True)
	merged.to_csv(merged_out)

	plot_saved = False
	try:
		build_plot(merged, lag_corr, Path(args.save_plot), series_label=series_label)
		plot_saved = True
	except ModuleNotFoundError:
		print("Plot skipped: matplotlib is not installed.")

	print("=== BTC vs FRB Rate Expectation Analysis ===")
	print(f"Rows merged: {len(merged)}")
	print(f"Date range: {merged.index.min().date()} to {merged.index.max().date()}")
	print(f"Rate source: {args.source} ({series_label})")
	print(f"Same-day correlation: {same_day_corr:.4f}")
	print(
		"Best lag correlation: "
		f"lag={int(best_row['lag_days'])}, corr={best_row['corr']:.4f} "
		"(+lag means rate leads BTC)"
	)
	print(f"Saved merged data: {merged_out}")
	if plot_saved:
		print(f"Saved plot: {args.save_plot}")


if __name__ == "__main__":
	main()
