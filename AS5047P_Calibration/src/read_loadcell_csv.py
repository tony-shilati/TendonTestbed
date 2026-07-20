import pandas as pd
import matplotlib.pyplot as plt

# If data.csv is in the same folder:
df = pd.read_csv("data.csv")

# If not, use the full path instead:
# df = pd.read_csv(r"C:\Users\master\data.csv")

print("Columns:", df.columns.tolist())
print(df.head())

# Convert ms to seconds
df["time_s"] = df["time_ms"] / 1000.0

plt.figure()
plt.plot(df["time_s"], df["raw_reading"])
plt.xlabel("Time (s)")
plt.ylabel("Raw ADC Reading")
plt.title("Load Cell Raw Output vs Time")
plt.grid(True)
plt.tight_layout()
plt.show()