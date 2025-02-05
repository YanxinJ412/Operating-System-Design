import matplotlib.pyplot as plt
import numpy as np
import sys

HZ = 300
# Load data
if len(sys.argv) != 4:
    print("no data to plot")
    sys.exit(1)

# Get the filename from command-line arguments
profile = sys.argv[1]
title = sys.argv[2]
pic = sys.argv[3]
data = np.loadtxt(profile, delimiter=' ', dtype=str)
data = data[:-1]
data = data.astype(float)

# Extract columns (assuming time, minor_fault, major_fault)
time = data[:, 0] * 1000 / HZ
# time = data[:, 0] - 4.2949e9
# print(time)
total_min_flt = np.cumsum(data[:, 1])
total_maj_flt = np.cumsum(data[:, 2])
# print(total_min_flt)
plt.plot(time, total_min_flt, label="Accumulated Minor Fault Count")
plt.plot(time, total_maj_flt, label="Accumulated Major Fault Count")
plt.xlabel("Time(ms)")
plt.ylabel("Accumulated Page Fault Count")
plt.title(title)
plt.legend()
plt.savefig(pic)
plt.show()
