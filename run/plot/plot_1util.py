import sys
import numpy as np
import matplotlib.pyplot as plt
from collections import defaultdict
import matplotlib.patches as patches

plt.rc('axes', titlesize=32)     # fontsize of the axes title
plt.rc('axes', labelsize=32)    # fontsize of the x and y labels
plt.rc('xtick', labelsize=32)    # fontsize of the tick labels
plt.rc('ytick', labelsize=32)    # fontsize of the tick labels
plt.rc('legend', fontsize=32)    # legend fontsize
plt.rc('figure', titlesize=17)  # fontsize of the figure title
plt.rcParams['pdf.fonttype'] = 42
plt.rcParams['ps.fonttype'] = 42
plt.grid(which='major', linewidth=0.5, linestyle='--')
plt.margins(x=0.01,y=0.01)

START = 0.0
END = 300.0

colors = ['#FB6C68', '#3172F5', '#FF9900',
          '#4AA399', '#f781bf', '#984ea3',
          '#999999', '#e41a1c', '#dede00']

if len(sys.argv) < 2:
    print(f"Usage: {sys.argv[0]} plot_name") 
    exit(1)

name = sys.argv[1]
basedir = "output/"
if len(sys.argv) > 2:
    basedir = sys.argv[2]

files = []

if name == "clos30":
    files = [
             (f"../{basedir}/flare55us_ws30perc_0.5s_60c30t_40a_a4b16_fbw12.txt", "Opera+Flare"),
             (f"../{basedir}/clos_bolt_ws30perc_0.5s_1q.txt", "Clos+Bolt"),
            ]
elif name == "clos35":
    files = [
             (f"../{basedir}/flare55us_ws35perc_0.5s_60c30t_40a_a4b16_fbw12.txt", "Opera+Flare"),
             (f"../{basedir}/clos_bolt_ws35perc_0.5s_1q.txt", "Clos+Bolt"),
            ]
else:
    print("Invalid plot_name")
    exit(1)

handles = []
i = 0

for fname,pname in files:
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
    if pname == "Clos+Bolt":
        lw = 3.0
    else:
        lw = 2.0
    print(f"50th:{np.percentile(utils,50)} 99th:{np.percentile(utils,99)}, avg:{sum(utils)/len(utils)}")
    p, = plt.plot(time, utils, linewidth=lw, color=colors[i], label=pname)
    handles.append(p)
    i += 1

if name == "progr":
    # Adding the optimal line
    plt.axhline(y=32.25, color='black', linestyle='--', linewidth=3.0, label="Optimal (32.25%)")
    plt.text(90, 20, "CW=4.4%", fontsize=22, color='black', weight='bold', ha='center', va='center')
    plt.text(240, 23, "CW=5.3%", fontsize=22, color='black', weight='bold',ha='center', va='center')
    plt.text(240, 19, "CW=14.8%", fontsize=22, color='black', weight='bold',ha='center', va='center')
    plt.text(255, 34, "Optimal", fontsize=22, color='black', weight='bold',ha='center', va='center')
    #plt.legend(handles=handles, bbox_to_anchor=[1,0.38])
    plt.legend(handles=handles, loc = 'lower right')
    plt.gca().add_patch(patches.FancyArrowPatch((95, 29), (95, 22), arrowstyle="-|>", mutation_scale=15, color="black", linewidth=2, zorder=3))
    plt.gca().add_patch(patches.FancyArrowPatch((150, 26), (190, 23), arrowstyle="-|>", mutation_scale=15, color="black", linewidth=2, zorder=3))
    plt.gca().add_patch(patches.FancyArrowPatch((150, 16), (185, 19), arrowstyle="-|>", mutation_scale=15, color="black", linewidth=2, zorder=3))
else:
    if name != "ws30perc_55us" and name != "ws30perc_15us":
        plt.legend(handles=handles)
    plt.axhline(y=32.25, color='black', linestyle='--', linewidth=3.0, label="Optimal (32.25%)")
    if name == 'clos30':
        plt.text(255, 34, "Optimal", fontsize=22, color='black', weight='bold',ha='center', va='center')
    if name == 'clos35':
        plt.text(255, 30.6, "Optimal", fontsize=22, color='black', weight='bold',ha='center', va='center')
plt.xlabel("Time (ms)")
plt.ylabel("Utilization (%)")
plt.ylim([0, 37])
#plt.yticks(np.arange(0, 33, 5.0))
plt.xticks([0, 100, 200, 300])
plt.yticks([0, 10, 20, 30, 35])

figname = ""
if name == "clos30":
    figname = "Fig19a"
elif name == "clos35":
    figname = "Fig19b"
else:
    print("Invalid plot_name")
    exit(1)

plt.savefig(f"../figures/{figname}.pdf", bbox_inches="tight")
