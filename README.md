# Virtual Trading Bot with SOM

A real-time trading bot that uses Self-Organizing Maps (SOM) to automatically trade virtual positions based on order book imbalance signals.

## How It Works

1. **WebSocket Connection**: Connects to Binance stream to receive real-time order book data for 4 trading pairs (ATOM, ETH, SOL, BTC).

2. **Market Data Collection**: Extracts bid/ask prices and volumes, calculating order book imbalance for each symbol.

3. **SOM Evaluation**: Passes the current imbalance and BTC data through a pre-trained SOM model to generate an expectancy score.

4. **Virtual Trading**:
   - If expectancy > 0.15: Opens a virtual LONG position
   - Holds position for 30 seconds
   - Automatically closes and saves PnL to CSV

5. **Model Retraining**: Every 30 minutes, the bot retrains the SOM model using collected market data via Python script.

## Data Flow

```
WebSocket (Binance)
    ↓
Extract: bid, ask, imbalance
    ↓
Store market data → data/*_market_data.csv
    ↓
Load SOM model → predict expectancy
    ↓
If expectancy > 0.15 → Execute virtual trade
    ↓
After 30 seconds → Close trade & save to data/*_trades.csv
    ↓
Every 30 min → Python retrains SOM
```

## Output Files

- **data/SYMBOL_market_data.csv**: Board imbalance data for SOM training
- **data/SYMBOL_trades.csv**: Virtual trading results (entry, exit, PnL)

## Key Parameters

| Parameter | Value | Purpose |
|-----------|-------|---------|
| Entry Signal | expectancy > 0.15 | BUY trigger |
| Hold Time | 30 seconds | Position duration |
| Data Save Interval | 30 seconds | Market data frequency |
| Model Retraining | Every 30 minutes | SOM update |

## Setup

1. Install C++ dependencies:
   ```
   vcpkg install fmt nlohmann-json ixwebsocket
   ```

2. Set up Python environment:
   ```
   python -m venv .venv
   .venv\Scripts\activate
   pip install numpy pandas scikit-learn
   ```

3. Build:
   ```
   cmake --build build --config Release
   ```

4. Run:
   ```
   ./build/Release/My-MM.exe
   ```

## Project Structure

```
.
├── main.cpp              # WebSocket, trading loop
├── ScanMarket.cpp        # Market data collection & CSV save
├── ExecuteTrade.cpp      # Trade entry logic
├── SOMEvaluator.cpp      # SOM prediction
├── train_som.py          # SOM retraining script
├── data/                 # Generated market & trade data
└── models/               # SOM weights (loaded at startup)
```
