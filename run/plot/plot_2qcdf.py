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

fig, axs = plt.subplots(2, figsize=(6, 4.8))

START = 200.0
END = 300.0

i = 0
#plot on top
for fname, pname in files_a:
    qsize = []
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
            if line[0] == "Queue":
                tor = int(line[1])
                port = int(line[2])
                if port > 5:
                    qsize.append(float(line[4])/1E3)

    count, bins_count = np.histogram(qsize, bins=5000) 
    pdf = count / sum(count) 
    cdf = np.cumsum(pdf) 
    zorder = 0
    if pname == "Flare":
        zorder = 10
    p, = axs[0].plot(bins_count[1:], cdf, label=pname, linewidth=3.5, color=colors[i], zorder=zorder) 
    handles_a.append(p)
    print(pname)
    print(f"Qsize 50th:{np.percentile(qsize,50)} 90th:{np.percentile(qsize,90)} 99th:{np.percentile(qsize,99)}, avg:{sum(qsize)/len(qsize)}")
    axs[0].grid(which='major', linewidth=0.5, linestyle='--')
    axs[0].set_xlim([0, 400])
    axs[0].set_ylim([0, 1.1])
    axs[0].set(ylabel="CDF")
    axs[0].set_yticks([0, 0.5, 1.0])
    i += 1
i = 0
#plot on bottom
for fname, pname in files_b:
    qsize = []
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
            if line[0] == "Queue":
                tor = int(line[1])
                port = int(line[2])
                if port > 5:
                    qsize.append(float(line[4])/1E3)

    count, bins_count = np.histogram(qsize, bins=5000) 
    pdf = count / sum(count) 
    cdf = np.cumsum(pdf) 
    zorder = 0
    if pname == "Flare":
        zorder = 10
    p, = axs[1].plot(bins_count[1:], cdf, label=pname, linewidth=3.5, color=colors[i], zorder=zorder) 
    handles_b.append(p)
    print(pname)
    print(f"50th:{np.percentile(qsize,50)} 90th:{np.percentile(qsize,90)} 99th:{np.percentile(qsize,99)}, avg:{sum(qsize)/len(qsize)}")
    axs[1].grid(which='major', linewidth=0.5, linestyle='--')
    axs[1].set_xlim([0, 400])
    axs[1].set_ylim([0, 1.1])
    axs[1].set(ylabel="CDF")
    axs[1].set_yticks([0, 0.5, 1.0])
    i += 1

axs[0].axes.xaxis.set_ticklabels([])
axs[0].set_xticks([0, 100, 200, 300, 400])
axs[1].set(xlabel="Queue Size (KB)")
axs[0].text(260, 0.5, r"55$\mu s$", fontsize=30, color='black', ha='center', va='center')
axs[1].text(260, 0.5, r"15$\mu s$", fontsize=30, color='black', ha='center', va='center')
plt.xlabel("Queue Size (KB)")
plt.ylabel("CDF")
plt.xlim([0, 400])
plt.ylim([0, 1.05])
plt.xticks([0, 100, 200, 300, 400])
plt.savefig(f"../figures/Fig15a.pdf", bbox_inches="tight")
