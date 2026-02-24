import pandas as pd
import matplotlib.pyplot as plt

input_csv = pd.read_csv('./data/all_trades_history.csv')
time_stamps = input_csv[input_csv.keys()[0]]
total_pnl_pcts = input_csv[input_csv.keys()[3]]

plt.xlabel('time[s]')
plt.ylabel('total_pnl_pct[%]')

plt.plot(time_stamps - time_stamps[0], total_pnl_pcts, linestyle='solid')
plt.show()