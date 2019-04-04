import matplotlib.pyplot as plt
import matplotlib
import numpy as np
import csv
import os

fileloc = os.path.join(os.path.dirname(os.path.abspath(__file__)), "yolov3-tiny-pirate_progress_part_2.csv")

# it's time to collect !
results = {}

with open(fileloc) as csvfile:
    spamreader = csv.reader(csvfile, delimiter=';', quotechar='|')
    headers = next(spamreader, None)

    for header in headers:
        results[header] = []

    for row in spamreader:
        h = 0
        for column in row:
            try:
                val = float(column)
            except Exception:
                val = 0.0
            results[headers[h]].append(val)
            h += 1

# now its time to plot
fig, ax = plt.subplots()
ax.plot(results["batch"], results["loss"])
ax.set(xlabel='batches', ylabel='loss', title='DNN result (loss)')
ax.grid()
plt.show()
