#!/usr/bin/python

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import sys

if __name__ == '__main__':
  if len(sys.argv) <= 1:
    print "%s <file|mem> [col2,col3,...]" % sys.argv[0]
    exit()
  # STEP 1 collecting data
  cols = sys.argv[1:]
  dats = []
  for col in cols:
    dats.append(np.loadtxt("%s.dat" % col))
  
  # STEP 2 draw figs
  fig,ax1=plt.subplots()
  ax2=ax1.twinx()
  ax1_series = []
  ax2_series = []
  for i in range(len(cols)):
    ax1s, = ax1.plot(dats[i][:,0],dats[i][:,1])
    ax2s, = ax2.plot(dats[i][:,0],dats[i][:,4],ls='dashed')
    ax1_series.append(ax1s)
    ax2_series.append(ax2s)
  ax1.legend(ax1_series,["%s throughput" % c for c in cols],loc=2)
  ax2.legend(ax2_series,["%s latenty" % c for c in cols],loc=1)
  ax1.set_xlabel("Write size(Byte)")
  ax1.set_xscale("log",basex=2)
  y1_lim = ax1.get_ylim()
  y2_lim = ax2.get_ylim()
  ax1.set_ylim([y1_lim[0],y1_lim[1]*1.2])
  ax2.set_ylim([y2_lim[0],y2_lim[1]*1.2])
  ax1.set_ylabel("Throughput(MB/s)")
  ax2.set_ylabel("Latency(microsecond)")
  plt.title("Persistent<X> write Performance");
  plt.tight_layout()
  plt.savefig('out.eps')
  plt.close()
