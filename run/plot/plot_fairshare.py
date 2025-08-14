import sys
import numpy as np
import matplotlib.pyplot as plt
from collections import defaultdict

plt.rc('axes', titlesize=30)     # fontsize of the axes title
plt.rc('axes', labelsize=30)    # fontsize of the x and y labels
plt.rc('xtick', labelsize=30)    # fontsize of the tick labels
plt.rc('ytick', labelsize=30)    # fontsize of the tick labels
plt.rc('legend', fontsize=14)    # legend fontsize
plt.rc('figure', titlesize=17)  # fontsize of the figure title
plt.rcParams['pdf.fonttype'] = 42
plt.rcParams['ps.fonttype'] = 42
plt.grid(which='major', linewidth=0.5, linestyle='--')
plt.margins(x=0.01,y=0.01)

colors = ['#FB6C68', '#3172F5', '#FF9900',
          '#4AA399', '#f781bf', '#984ea3',
          '#999999', '#e41a1c', '#dede00']

handles = []
i = 0

basedir = "output/"
if len(sys.argv) > 2:
    basedir = sys.argv[2]

throughput = defaultdict(list)
time = defaultdict(list)
#91.0~ due to 64bytes being headers w/ 1.5KB MTU, and pulling at 95% rate
MAX_TP = 91.0

with open(f"../{basedir}/flare_fairshare.txt", "r") as f:
    lines = f.readlines()
    for line in lines:
        line = line.split()
        if len(line) <= 0:
            continue
        if line[0] == "TP":
            fid = int(line[1])
            tp = float(line[2])
            t = int(line[3]) #picosec
            t /= 1E12 #sec
            throughput[fid].append(tp/MAX_TP)
            time[fid].append(t)

for fid,tps in throughput.items():
    t = list(time[fid])
    print(tps)
    p, = plt.plot(t, list(tps), linewidth=2.5, color=colors[fid], label=f"Flow {fid}")
    handles.append(p)

#plt.legend(handles=handles)
plt.xlabel("Time (s)")
plt.ylabel("Norm. Thpt")
plt.ylim([0, 1.1])
plt.xticks([2, 4, 6])
plt.yticks([0, 0.25, 0.5, 0.75, 1])
plt.savefig("../figures/Fig10.pdf", bbox_inches="tight")
