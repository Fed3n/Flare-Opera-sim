import sys
import numpy as np
import matplotlib.pyplot as plt
from collections import defaultdict

plt.rc('axes', titlesize=30)     # fontsize of the axes title
plt.rc('axes', labelsize=30)    # fontsize of the x and y labels
plt.rc('xtick', labelsize=30)    # fontsize of the tick labels
plt.rc('ytick', labelsize=30)    # fontsize of the tick labels
plt.rc('legend', fontsize=23)    # legend fontsize
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

f, (pa, pb) = plt.subplots(1, 2, sharey=False, facecolor='w')
f.set_figwidth(12.8)
plt.subplots_adjust(wspace = 0.4)

if len(sys.argv) < 2:
    print(f"Usage: {sys.argv[0]} plot_name") 
    exit(1)

name = sys.argv[1]
basedir = "output/"
if len(sys.argv) > 2:
    basedir = sys.argv[2]

files_a = []
files_b = []

if name == "ws30perc":
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
elif name == "had30perc":
    files_a = [
             (f"../{basedir}/flare55us_had30perc_0.5s_60c30t_40a_a4b16_fbw12.txt", "Flare"),
             (f"../{basedir}/bolt55us_had30perc_0.5s_1q.txt", "Bolt"),
             (f"../{basedir}/xpass55us_had30perc_0.3s_80c_40a_05winit.txt", "ExpressPass"),
             (f"../{basedir}/hbh55us_had30perc_0.3s_10P_5D.txt", "HbH"),
             (f"../{basedir}/tdtcp55us_had30perc_0.3s_40ecn_nosyn_10cwnd.txt", "TDTCP"),
            ]
    files_b = [
             (f"../{basedir}/flare15us_had30perc_0.3s_16c8t_8a_a4b16_fbw12.txt", "Flare"),
             (f"../{basedir}/bolt15us_had30perc_0.3s_1q.txt", "Bolt"),
             (f"../{basedir}/xpass15us_had30perc_0.3s_20c_8a_05winit.txt", "ExpressPass"),
             (f"../{basedir}/hbh15us_had30perc_0.3s_6P_2D.txt", "HbH"),
            ]
elif name == "rpc30perc":
    files_a = [
             (f"../{basedir}/flare55us_rpc30perc_0.5s_60c30t_40a_a4b16_fbw12.txt", "Flare"),
             (f"../{basedir}/bolt55us_rpc30perc_0.5s_1q.txt", "Bolt"),
             (f"../{basedir}/xpass55us_rpc30perc_0.3s_80c_40a_05winit.txt", "ExpressPass"),
             (f"../{basedir}/hbh55us_rpc30perc_0.3s_10P_5D.txt", "HbH"),
             (f"../{basedir}/tdtcp55us_rpc30perc_0.3s_40ecn_nosyn_10cwnd.txt", "TDTCP"),
            ]
    files_b = [
             (f"../{basedir}/flare15us_rpc30perc_0.3s_16c8t_8a_a4b16_fbw12.txt", "Flare"),
             (f"../{basedir}/bolt15us_rpc30perc_0.3s_1q.txt", "Bolt"),
             (f"../{basedir}/xpass15us_rpc30perc_0.3s_20c_8a_05winit.txt", "ExpressPass"),
             (f"../{basedir}/hbh15us_rpc30perc_0.3s_6P_2D.txt", "HbH"),
            ]
elif name == "clos":
    files_a = [
             (f"../{basedir}/flare55us_ws30perc_0.5s_60c30t_40a_a4b16_fbw12.txt", "Opera+Flare"),
             (f"../{basedir}/clos_bolt_ws30perc_0.5s_1q.txt", "Clos+Bolt"),
            ]
    files_b = [
             (f"../{basedir}/flare55us_ws35perc_0.5s_60c30t_40a_a4b16_fbw12.txt", "Opera+Flare"),
             (f"../{basedir}/clos_bolt_ws35perc_0.5s_1q.txt", "Clos+Bolt"),
            ]
    plt.rc('legend', fontsize=26)
    colors = ['#FB6C68', '#3172F5', '#FF9900',
          '#4AA399', '#f781bf', '#984ea3',
          '#999999', '#e41a1c', '#dede00']
elif name == "test":
    files_a = [
             ("testfct.txt", "Flare"),
            ]
    files_b = [
             ("testfct.txt", "Flare"),
            ]
else:
    print("Invalid plot_name")
    exit(1)

handles_a = []
handles_b = []
i = 0

for fname,pname in files_a:
    flows = defaultdict(list)
    with open(fname, "r") as f:
        lines = f.readlines()
        for line in lines:
            line = line.split()
            if len(line) <= 0:
                continue
            if line[0] == "FCT":
                flowsize = int(line[3])
                flowfct = float(line[4])*1E3
                flows[flowsize].append(flowfct)
            #set any unfinished flow FCT to INF
            if name == "Flare" and line[0] == "UNFINISHED":
                if(len(line) > 2):
                    flowsize = int(line[2])
                    flowfct = float("inf")
                    flows[flowsize].append(flowfct)

    flowsizes = [flowsize for (flowsize, flowfct) in flows.items()]
    flowsizes.sort()
    fcts = [[],[]]
    print(pname)
    zorder = 0
    if(pname == "Flare"):
        zorder = 1
    for flowsize in flowsizes:
        fcts[0].append(np.percentile(flows[flowsize], 50))
        fcts[1].append(np.percentile(flows[flowsize], 99))
    for k in range(len(fcts[1])):
        print(f"{flowsizes[k]}:{fcts[1][k]}")
    la, = pa.plot(flowsizes, fcts[1], label=pname, linewidth=3.5, color=colors[i], zorder=zorder)
    handles_a.append(la)
    i += 1
i = 0
for fname,pname in files_b:
    flows = defaultdict(list)
    with open(fname, "r") as f:
        lines = f.readlines()
        for line in lines:
            line = line.split()
            if len(line) <= 0:
                continue
            if line[0] == "FCT":
                flowsize = int(line[3])
                flowfct = float(line[4])*1E3
                flows[flowsize].append(flowfct)

    flowsizes = [flowsize for (flowsize, flowfct) in flows.items()]
    flowsizes.sort()
    fcts = [[],[]]
    print(pname)
    zorder = 0
    if(pname == "Flare"):
        zorder = 1
    for flowsize in flowsizes:
        fcts[0].append(np.percentile(flows[flowsize], 50))
        fcts[1].append(np.percentile(flows[flowsize], 99))
    '''
    for k in range(len(fcts[0])):
        print(f"{flowsizes[k]}:{fcts[0][k]}")
    '''
    for k in range(len(fcts[1])):
        print(f"{flowsizes[k]}:{fcts[1][k]}")
    lb, = pb.plot(flowsizes, fcts[1], label=pname, linewidth=3.5, color=colors[i], zorder=zorder)
    handles_b.append(lb)
    i += 1


pa.set_xscale("log")
pa.set_yscale("log")
pb.set_xscale("log")
pb.set_yscale("log")
pa.set_ylim(1, (10**6))
pb.set_ylim(1, (10**6))
pa.set(ylabel=r'99%-ile FCT $(\mu s)$')
pa.set(xlabel="Flow Size (Bytes)")
pb.set(ylabel=r'99%-ile FCT $(\mu s)$')
pb.set(xlabel="Flow Size (Bytes)")
pa.grid(which='major', linewidth=0.5, linestyle='--')
pb.grid(which='major', linewidth=0.5, linestyle='--')
pa.axvline(57440, linewidth=2, color="black")
pb.axvline(57440, linewidth=2, color="black")
if name != 'clos':
    pa.text(0.5,-0.35, r"(a) 55$\mu s$", size=30, ha="center", transform=pa.transAxes)
    pb.text(0.5,-0.35, r"(b) 15$\mu s$", size=30, ha="center", transform=pb.transAxes)
else:
    pa.text(0.5,-0.35, r"(a) 30% load", size=30, ha="center", transform=pa.transAxes)
    pb.text(0.5,-0.35, r"(b) 35% load", size=30, ha="center", transform=pb.transAxes)
if name == 'clos':
    leg = pa.legend(handles=handles_a, loc="upper left", ncol=4, framealpha=0.0, bbox_to_anchor=[0,1.25]) 
elif name == 'ws30perc':
    leg = pa.legend(handles=handles_a, loc="upper left", ncol=6, framealpha=0.0, bbox_to_anchor=[-0.25,1.22], columnspacing=0.8, handlelength=1.5, handletextpad=0.3)
else:
    leg = pa.legend(handles=handles_a, loc="upper left", ncol=5, framealpha=0.0, bbox_to_anchor=[0.0,1.22], columnspacing=0.8, handlelength=1.5, handletextpad=0.3)

figname = ""
if name == "ws30perc":
    figname = "Fig14"
elif name == "had30perc":
    figname = "Fig16"
elif name == "rpc30perc":
    figname = "Fig17"
elif name == "clos":
    figname = "Fig18"
else:
    print("Invalid plot_name")
    exit(1)
    
plt.savefig(f"../figures/{figname}.pdf", bbox_inches="tight")

