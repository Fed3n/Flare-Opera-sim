import sys
import numpy as np
import matplotlib.pyplot as plt
from collections import defaultdict

plt.rc('axes', titlesize=30)     # fontsize of the axes title
plt.rc('axes', labelsize=30)    # fontsize of the x and y labels
plt.rc('xtick', labelsize=30)    # fontsize of the tick labels
plt.rc('ytick', labelsize=30)    # fontsize of the tick labels
plt.rc('legend', fontsize=22)    # legend fontsize
plt.rc('figure', titlesize=17)  # fontsize of the figure title
plt.rcParams['pdf.fonttype'] = 42
plt.rcParams['ps.fonttype'] = 42
plt.grid(which='major', linewidth=0.5, linestyle='--')
plt.margins(x=0.01,y=0.01)

START = 0.0
END = 300.0

colors = ['#222222', '#FB6C68', '#3172F5', 
          '#FF9900', '#4AA399', '#f781bf', 
          '#984ea3', '#999999', '#e41a1c', ]
'''
colors = ['#FB6C68', '#3172F5', '#FF9900',
          '#4AA399', '#f781bf', '#984ea3',
          '#999999', '#e41a1c', '#dede00']
'''

basedir = "output/"
if len(sys.argv) > 1:
    basedir = sys.argv[1]

files_a = [
         (f"../{basedir}/flare55us_ws30perc_0.5s_60c30t_40a_a4b16_fbw12.txt", "Flare"),
         (f"../{basedir}/bolt55us_ws30perc_0.5s_1q.txt", "Bolt"),
         (f"../{basedir}/xpass55us_ws30perc_0.3s_80c_40a_05winit.txt", "ExpressPass"),
         (f"../{basedir}/hbh55us_ws30perc_0.3s_10P_5D.txt", "HbH"),
         (f"../{basedir}/tdtcp55us_ws30perc_0.3s_40ecn_nosyn_10cwnd.txt", "TDTCP"),
         (f"../{basedir}/ndp55us_ws30perc_0.3s_80q.txt", "NDP"),
        ]
files_b = [
         (f"../{basedir}/flare15us_ws30perc_0.3s_16c8t_8a_a4b16_fbw12.txt", "Flare"),
         (f"../{basedir}/bolt15us_ws30perc_0.3s_1q.txt", "Bolt"),
         (f"../{basedir}/xpass15us_ws30perc_0.3s_20c_8a_05winit.txt", "ExpressPass"),
         (f"../{basedir}/hbh15us_ws30perc_0.3s_6P_2D.txt", "HbH"),
        ]

handles_a = []
handles_b = []

fig, axs = plt.subplots(2)

i = 0
for fname,pname in files_a:
    flows = defaultdict(list)
    utils = []
    time = []
    with open(fname, "r") as f:
        lines = f.readlines()
        for line in lines:
            line = line.split()
            if len(line) <= 0:
                continue
            if line[0] == "Util":
                util = float(line[1])*100
                t = float(line[2])
                if t < START:
                    continue
                if t > END:
                    break
                utils.append(util)
                time.append(t)
    print(pname)
    zorder = 0
    if pname == "Flare":
        zorder = 10
    p, = axs[0].plot(time, utils, linewidth=2.0, color=colors[i], label=pname, zorder=zorder)
    handles_a.append(p)
    axs[0].grid(which='major', linewidth=0.5, linestyle='--')
    axs[0].set_ylim([0, 33])
    axs[0].set_yticks([0, 15, 30])
    axs[0].set(ylabel="")
    i += 1
i = 0
for fname,pname in files_b:
    flows = defaultdict(list)
    utils = []
    time = []
    with open(fname, "r") as f:
        lines = f.readlines()
        for line in lines:
            line = line.split()
            if len(line) <= 0:
                continue
            if line[0] == "Util":
                util = float(line[1])*100
                t = float(line[2])
                if t < START:
                    continue
                if t > END:
                    break
                utils.append(util)
                time.append(t)
    print(pname)
    zorder = 0
    if pname == "Flare":
        zorder = 10
    print(f"Util 50th:{np.percentile(utils,50)} 90th:{np.percentile(utils,90)} 99th:{np.percentile(utils,99)}, avg:{sum(utils)/len(utils)}")
    p, = axs[1].plot(time, utils, linewidth=2.0, color=colors[i], label=pname, zorder=zorder)
    handles_b.append(p)
    axs[1].grid(which='major', linewidth=0.5, linestyle='--')
    axs[1].set_ylim([0, 33])
    axs[1].set_xticks([0, 100, 200, 300])
    axs[1].set_yticks([0, 15, 30])
    axs[1].set(ylabel="            Utilization (%)")
    i += 1

axs[0].axhline(y=32.25, color='black', linestyle='--', linewidth=3.0, label="Optimal (32.25%)")
axs[1].axhline(y=32.25, color='black', linestyle='--', linewidth=3.0, label="Optimal (32.25%)")
axs[0].set_ylim([0, 39])
axs[1].set_ylim([0, 39])
axs[0].text(160, 7, r"55$\mu s$", fontsize=30, color='black', ha='center', va='center')
axs[1].text(160, 7, r"15$\mu s$", fontsize=30, color='black', ha='center', va='center')
axs[0].text(255, 35.5, "Optimal", fontsize=21, color='black', weight='bold',ha='center', va='center')
axs[1].text(255, 35.5, "Optimal", fontsize=21, color='black', weight='bold',ha='center', va='center')

axs[0].axes.xaxis.set_ticklabels([])
axs[0].set_xticks([0, 100, 200, 300])
axs[1].set(xlabel="Time (ms)")
plt.savefig(f"../figures/Fig15b.pdf", bbox_inches="tight")
