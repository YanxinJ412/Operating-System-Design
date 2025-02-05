import matplotlib.pyplot as plt
import numpy as np
import sys

HZ = 300
N = [5, 11, 16, 20, 22]
# Load data
if len(sys.argv) != 6:
    print("no data to plot")
    sys.exit(1)

# Get the filename from command-line arguments
profiles = [sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5]]
total_cpu_utils = []
for i, profile in enumerate(profiles):
    data = np.loadtxt(profile, delimiter=' ', dtype=str)
    data = data[:-1]
    data = data.astype(float)
    jiffies = data[:, 0]
    diff_jiffies = jiffies[-1] - jiffies[0]
    print(diff_jiffies)
    total_cpu_util = np.sum(data[:, 3]) / diff_jiffies
    print(total_cpu_util)
    total_cpu_utils.append(total_cpu_util)
    # plt.plot(N[i], total_cpu_util, label=f"N={N[i]}")
# Plot
plt.plot(N, total_cpu_utils, label="Total utilization of all N copies of the work process")
plt.xlabel("N")
plt.xticks(N)
plt.ylabel("Total utilization")
plt.title("Total utilization of all N copies of the work process")
plt.legend()
plt.savefig("case_2.png")
plt.show()

