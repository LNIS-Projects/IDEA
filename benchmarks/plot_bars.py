import numpy as np
import matplotlib as mpl
import matplotlib.pyplot as plt
import matplotlib.ticker as mt
import sys
import os.path
import numpy.lib.recfunctions as rcf

plt.rcParams['font.size'] = '9'
plt.style.use('tableau-colorblind10')
plt.rcParams['font.family'] = 'serif'
input_file, input_label, prev_file, prev_label, output_file = sys.argv[1:] if len(sys.argv) > 1 else ('everything.tsv', "Latest LSOracle", 'previous/everything.tsv', "Previous LSOracle", 'everything.png')

n = np.genfromtxt(input_file, delimiter='\t', names=True, dtype=None, encoding="UTF-8")

if os.path.isfile(prev_file):
    p = np.genfromtxt(prev_file, delimiter='\t', names=True, dtype=n.dtype, encoding="UTF-8")
else:
    p = np.asarray([], dtype=n.dtype)

d = rcf.join_by("circuit", n, p, jointype='leftouter', r1postfix='', r2postfix='_prev')
np.sort(d, order=["unopt_nodes"])

colwidth = 21*400./2409
textwidth = 43*400./2409

width = 0.20
prev_exists = 1 if os.path.isfile(prev_file) else np.nan

fig, (nodes, depth, ndp, delay, adp, area) = plt.subplots(1,6)

nodes_d = np.dstack((1 - d['final_nodes'] / d['unopt_aig_nodes'],
                     1 - d['abc_aig_nodes'] / d['unopt_aig_nodes'],
                     prev_exists * (1 - d['final_nodes_prev'] / d['unopt_aig_nodes'])))[0]
depth_d = np.dstack((1 - d['final_levels'] / d['unopt_aig_levels'],
                     1 - d['abc_aig_levels'] / d['unopt_aig_levels'],
                     prev_exists * (1 - d['final_levels_prev'] / d['unopt_aig_levels'])))[0]

ndp_u = d['unopt_aig_nodes'] * d['unopt_aig_levels']
ndp_d = np.dstack((1 - d['final_nodes'] * d['final_levels'] / ndp_u,
                   1 - d['abc_aig_nodes'] * d['abc_aig_levels'] / ndp_u,
                   prev_exists * (1 - d['final_nodes_prev'] * d['final_levels'] /ndp_u)))[0]
area_d = np.dstack((1 - d['area'] / d['unopt_area'],
                    1 - d['abc_area']/d['unopt_area'],
                    prev_exists * (1 - d['area_prev']/d['unopt_area'])))[0]
adp_u = d['unopt_arrival'] * d['unopt_area']
adp_d = np.dstack((1 - d['area'] * d['arrival'] / adp_u,
                   1 - d['abc_area'] * d['abc_arrival'] / adp_u,
                   prev_exists * (1 - d['area_prev'] * d['arrival_prev'] / adp_u)))[0]

delay_d = np.dstack((1 - d['arrival'] / d['unopt_arrival'],
                     1 - d['abc_arrival'] / d['unopt_arrival'],
                     prev_exists * (1 - d['arrival_prev'] / d['unopt_arrival'])))[0]

fig, ((nodes, depth, ndp), (area, delay, adp))  = plt.subplots(2,3)

for m, t, ax, da in [
        ("nodes", "Nodes", nodes, nodes_d),
        ("depth", "Depth", depth, depth_d),
        ("ndp", "Node Depth Product", ndp, ndp_d),
        ("area", "Area", area, area_d),
        ("delay", "Delay", delay, delay_d),
        ("adp", "Area Delay Product", adp, adp_d),
]:
    ax.margins(x=0.10)
    ax.set(title=t)
    x = np.arange(len(da)+1)
    u = np.append(da[:,0], np.average(da[:,0]))
    a = np.append(da[:,1], np.average(da[:,1]))
    l = np.append(da[:,2], np.average(da[:,2]))

    labels = np.append(d["circuit"], "average")

    farm = ax.barh(x+width, u, width, label=input_label)
    lsoracle = ax.barh(x, l, width, label=prev_label)
    abc = ax.barh(x-width, a, width, label="ABC")

    ax.set_xlabel("% improvement over original circuit")
    ax.set_yticks(x)
    ax.set_yticklabels(labels)

    ax.bar_label(farm, [f"{f:.0%}" for f in u], padding=3, fontsize='x-small')
    ax.bar_label(abc, [f"{f:.0%}" for f in a], padding=3, fontsize='x-small')
    ax.bar_label(lsoracle, [f"{f:.0%}" for f in l], padding=3, fontsize='x-small')

    ax.axvline(color="black")
    ax.xaxis.set_major_formatter("{x:.0%}")
nodes.legend()
fig.set_figheight(20)
fig.set_figwidth(12)

fig.tight_layout()
fig.savefig(output_file)
