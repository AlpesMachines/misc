#!/usr/bin/env python3
import getopt
import sys
import matplotlib.pyplot as plt
from pathlib import Path

def read_raw(path, signed=True):
    with open(path, 'rb') as f:
        bin = f.read()
    bin_view = memoryview(bin)
    return list(bin_view.cast('b' if signed else 'B'))

def plot_raw(path, data):
    fig = plt.figure()
    ax = fig.add_subplot(1,1,1)
    ax.plot(data, linewidth=1)
    ax.axhline(y=0, color='k', linewidth=1)
    ax.set_xlim(0, len(data))
    ax.set_ylim(min(data), max(data))
    fig.savefig(path, bbox_inches='tight')

#raw = read_raw(name, signed=True)
#newname = Path(name).with_suffix('.png')
#print(newname)
#plot_raw(newname, raw)

optlist, args = getopt.getopt(sys.argv[1:], 'g')
for (opt, optval):
    if opt == '-g':
        render_graphs = True

