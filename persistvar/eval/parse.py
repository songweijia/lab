#!/usr/bin/python
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import sys

# load data
def loadraw(fname):
  data_dict = {}
  f = open(fname,'r')
  while True:
    line = f.readline()
    if line == "":
      break;
    tks = line.split()
    if len(tks) != 5:
      continue
    k = int(tks[0])
    thp = float(tks[1])
    lat = float(tks[3])
    if not k in data_dict:
      data_dict[k] = ([],[])
    data_dict[k][0].append(thp)
    data_dict[k][1].append(lat)
  # calculate
  raw_arr = []
  for k in data_dict:
    v = data_dict[k]
    thp_avg = sum(v[0])/len(v[0])
    thp_min = min(v[0])
    thp_max = max(v[0])
    lat_avg = sum(v[1])/len(v[1])
    lat_min = min(v[1])
    lat_max = max(v[1])
    raw_arr.append([k,thp_avg,thp_min,thp_max,lat_avg,lat_min,lat_max])
  np_arr = np.array(raw_arr)
  return np_arr[np_arr[:,0].argsort()]

if __name__ == '__main__':
  if len(sys.argv) != 2:
    print "usage: %s <file|mem>" % (sys.argv[0])
    exit()
  np.savetxt("%s.dat" % sys.argv[1],loadraw("%s.raw" % sys.argv[1]))
