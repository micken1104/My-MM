# Self-Evolving Trade Bot with SOM (Self-Organizing Maps)

A high-frequency trading bot that combines a high-performance C++ execution engine with a Python-based machine learning (SOM) core. The bot continuously learns market patterns and self-optimizes trading strategies in real-time.

## Key Features
- **Hybrid Architecture**: Low-latency market data processing in **C++** and complex pattern recognition in **Python**.
- **Self-Evolving Logic**: Periodically retrains the SOM model using the latest market data without downtime (Hot-Reload).
- **Dynamic Risk Management**: Automatically adjusts Stop-Loss (SL) levels based on historical volatility (Risk Map) learned by the AI.
- **Vectorized Learning**: High-speed training using NumPy vectorization for rapid strategy updates.

## Why SOM (Self-Organizing Maps)?
Unlike traditional supervised learning (like simple Regression or Neural Networks), SOM offers unique advantages for high-frequency market data:
1. **Non-linear Pattern Recognition**: Financial markets are chaotic. SOM excels at mapping high-dimensional market indicators (Imbalance, Volume, Diff) into a 2D topological map, revealing hidden structures that linear models miss.
2. **Clustering Market "States"**: SOM groups similar market conditions together. This allows the bot to identify the "Current Market Regime" and apply a strategy (Expectancy/Risk) specific to that state.
3. **Robustness to Noise**: By generalizing patterns onto a grid, SOM acts as a natural noise filter, preventing the bot from overreacting to minor price fluctuations.
4. **Visual Interpretability**: The Strategy Map allows humans to "see" what the AI is thinking. We can identify where the high-probability "Green Zones" are and how often the market visits those states.

## Tech Stack
- **C++20**: Market data handling, WebSocket (IXWebSocket), and execution logic.
- **Python 3.12**: NumPy, Pandas, Scikit-learn for SOM training and data analysis.
- **FMT Library**: Modern text formatting for C++.
- **Nlohmann JSON**: Robust JSON parsing for Binance API.

## System Architecture
1. **Data Collection**: C++ collects real-time order book imbalance and volume data.
2. **Periodic Retraining**: A background Python process retrains the SOM every 30 minutes.
3. **Model Hot-Reload**: The C++ engine reloads weights, expectancy maps, and risk maps seamlessly.
4. **Execution**: Dynamic position sizing based on Kelly Criterion and AI-generated expectancy.

## Strategy Visualization
The bot generates a "Strategy Map" where:
- **Green Zones**: High probability of price increase.
- **Red Zones**: High risk or expected price decrease.
- **Contours**: Data density indicating the reliability of the pattern.
