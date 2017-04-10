#!/usr/bin/python

import logging
import os
import glob
import paramiko
import numpy
from itertools import groupby
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import time
import util

logger = logging.getLogger(__name__)

class CephEvalClient:
  """
  CephEvalClient evaluates the performance of a ceph installation.
  1) Latency and throughput of RADOS with
  2) Various object size
  3) Different replication factors
  CephEvalClient needs the following information to run
  1) Access to ceph admin node
  2) Access to ceph client node
  Output of CephEvalClient is a set of raw data file as well as figures created in /res/<exp_env>/<test_desc> folder.
  - exp_env represents an environment. For example, we can use "amz_env1/2/3" to represents different environment setup in Amazon aws.
  - test_desc represents a test in such an environment. A test can have different replication factor, pool type(erasure/replication), object size, number of concurrent reader/writer/, duration of experiment, number of placement group, etc. Here we use conf_to_str() and str_to_conf() for translation between string and dict representation of it.
  """
  def __init__(self, exp_env, \
    admin_host, admin_username, admin_key=None, amdin_password=None, \
    client_host=None, client_username=None, client_key=None, client_password=None):
    """
    initialize the credentials of a CephEvalClient
    'exp_env' decribes the experiment environment. 
    """
    self._en = exp_env
    self._ah = admin_host
    self._au = admin_username
    self._ak = admin_key
    self._ap = amdin_password
    if client_host == None:
      self._ch = self._ah
      self._cu = self._au
      self._ck = self._ak
      self._cp = self._ap
    else:
      self._ch = client_host
      self._cu = client_username
      self._ck = client_key
      self._cp = client_password
    self._data = '/'.join(('res',self._en))
    self._data_file = 'cepheval.dat'
    self._test_pool_name = 'cephevalpool'
    self._test_tmp_dir = 'cephevaltmp'

  def gen_conf(self, \
    rep_factor=3, \
    objsize_byte=1024, \
    num_concurrent=16, \
    dur_sec=15, \
    pg_num=256, \
    pgp_num=256, \
    pool_type='replicated'):
    return { \
      "rep_factor":rep_factor, \
      "obj_size":objsize_byte, \
      "concurrent":num_concurrent, \
      "duration":dur_sec, \
      "pg_num":pg_num, \
      "pgp_num":pgp_num, \
      "pool_type":pool_type}

  def conf_to_str(self, conf):
    """
    return a string representation of the configuration
    """
    return "%d_%d_%d_%d_%d_%d_%s" % ( \
      conf['rep_factor'], \
      conf['obj_size'], \
      conf['concurrent'], \
      conf['duration'], \
      conf['pg_num'], \
      conf['pgp_num'], \
      conf['pool_type'])

  def str_to_conf(self, conf_str):
    """
    return a configuration from the string representation
    """
    tks = conf_str.split('_')
    return { \
      "rep_factor":int(tks[0]), \
      "obj_size":int(tks[1]), \
      "concurrent":int(tks[2]), \
      "duration":int(tks[3]), \
      "pg_num":int(tks[4]), \
      "pgp_num":int(tks[5]), \
      "pool_type":tks[6]}

  def run_one(self, conf):
    """
    run one configuration
    """
    # STEP 1: connect to admin
    ssh_client = paramiko.SSHClient()
    ssh_client.set_missing_host_key_policy(paramiko.client.AutoAddPolicy())
    ssh_client.connect(self._ah,username=self._au,password=self._ap,key_filename=self._ak)
    logger.debug("connected to ceph admin node: %s." % self._ah)
    # STEP 2: config ceph pool
    util.remote_exec(ssh_client,commands=["ceph osd pool delete %s %s --yes-i-really-really-mean-it" % (self._test_pool_name,self._test_pool_name)],verifiers=[None])
    # sleep 30 seconds after delete....
    time.sleep(30)
    util.remote_exec(ssh_client,commands=["ceph osd pool create %s %d %d %s" % (self._test_pool_name,conf["pg_num"],conf["pgp_num"],conf['pool_type'])],verifiers=[None])
    util.remote_exec(ssh_client,commands=["ceph osd pool set %s size %d" % (self._test_pool_name,conf['rep_factor'])],verifiers=[None])
    util.remote_exec(ssh_client,commands=["ceph osd pool set %s min_size %d" % (self._test_pool_name,conf['rep_factor'])],verifiers=[None])
    # STEP 3: close connection [admin]
    ssh_client.close()
    logger.debug("disconnected from ceph admin node: %s." % self._ah)
    # STEP 4: connect to client
    ssh_client.connect(self._ch,username=self._cu,password=self._cp,key_filename=self._ck)
    logger.debug("connected to ceph client node: %s." % self._ch)
    # STEP 5: run experiment
    util.remote_exec(ssh_client,commands=[ \
      "rm -rf %s; mkdir %s" % (self._test_tmp_dir,self._test_tmp_dir), \
      "rados bench %d write -p %s -b %d -t %d --no-cleanup > %s/write_%s" % (conf['duration'],self._test_pool_name,conf['obj_size'],conf['concurrent'],self._test_tmp_dir,self.conf_to_str(conf)), \
      "rados bench %d seq -p %s -t %d > %s/seq_%s" % (conf['duration'],self._test_pool_name,conf['concurrent'],self._test_tmp_dir,self.conf_to_str(conf)), \
      "rados bench %d rand -p %s -t %d > %s/rand_%s" % (conf['duration'],self._test_pool_name,conf['concurrent'],self._test_tmp_dir,self.conf_to_str(conf)) \
      ],verifiers=[None,None,None,None])
    # STEP 6: collect results
    for tn in ['write','seq','rand']:
      util.download(ssh_client,"%s/%s_%s" % (self._test_tmp_dir,tn,self.conf_to_str(conf)), '/'.join((self._data,"%s_%s" % (tn,self.conf_to_str(conf)))))
    # STEP 7: close connection [client]
    ssh_client.close()
    logger.debug("disconnected from ceph client node: %s." % self._ch)

  def parse_file(self, raw_fn):
    """
    parse the data file into a vector:
    [<rep_factor>,<obj_size>,<concurrent>,<duration>,<pg_num>,<pgp_num>,<rep0/erasure1>,<w0/s1/r2>,thp_avg,thp_stddev,thp_min,thp_max,lat_avg,lat_stddev,lat_min,lat_max]
    """
    tks = raw_fn.split('/')[-1].split('_')
    conf = self.str_to_conf('_'.join(tks[1:]))
    # pool type: replicated or erasure
    pt = -1
    if conf['pool_type'] == 'erasure':
      pt = 1
    elif conf['pool_type'] == 'replicated':
      pt = 0
    # write/sequential read/random read
    exp = -1
    if tks[0] == 'write':
      exp = 0
    elif tks[0] == 'seq':
      exp = 1
    elif tks[0] == 'rand':
      exp = 2
    thp = [-1,-1,-1,-1]
    lat = [-1,-1,-1,-1]
    f = open(raw_fn,'r')
    while True:
      line = f.readline()
      if line == '':
        break
      if "Bandwidth (MB/sec):" in line:
        thp[0] = float(line.split()[-1]) # average throughput
      elif "Stddev Bandwidth:" in line:
        thp[1] = float(line.split()[-1]) # stddev throughput
      elif "Min bandwidth (MB/sec):" in line:
        thp[2] = float(line.split()[-1]) # minimum throughput
      elif "Max bandwidth (MB/sec):" in line:
        thp[3] = float(line.split()[-1]) # maximum throughput
      elif "Average Latency" in line:
        lat[0] = float(line.split()[-1]) # average latency
      elif "Stddev Latency" in line:
        lat[1] = float(line.split()[-1]) # stddev latency
      elif "Min latency" in line:
        lat[2] = float(line.split()[-1]) # minimum latency
      elif "Max latency" in line:
        lat[3] = float(line.split()[-1]) # maximum latency
    f.close()
    vec = [conf['rep_factor'],conf['obj_size'],conf['concurrent'],conf['duration'],conf['pg_num'],conf['pgp_num'],pt,exp] + thp + lat
    return vec

  def parse_all(self):
    """
    collecting all datas and make a big numpy matrix
    """
    m = None
    for fn in glob.glob('/'.join((self._data, '*_replicated'))) + glob.glob('/'.join((self._data,'*_erasure'))):
      val = self.parse_file(fn)
      if m is None:
        m = numpy.array([val])
      else:
        m = numpy.concatenate((m,[val]))
    return m

  def run(self):
    """
    run the experiment
    """
    # STEP 1: validation-data path
    if not os.path.exists(self._data):
      os.makedirs(self._data)
    elif not os.path.isdir(self._data):
      os.remove(self._data)
      os.makedirs(self._data)
    # STEP 2: run exp...
    #for rf in [1,3,5]:
    for rf in [5,3,1]:
#      for oso in range(3,24,2):
      for oso in range(5,24,2):
        for concurrent in [1,4,16]:
          self.run_one(self.gen_conf(rep_factor=rf,objsize_byte=(1<<oso),num_concurrent=concurrent));

  def parse(self):
    """
    parse data and put them in a file specified by self._data_file
    """
    m = self.parse_all()
    if m is None:
      logger.warn("Parse failed: no data available...")
    else:
      numpy.savetxt('/'.join((self._data,self._data_file)),m,fmt="%.4f")

  def draw_fig1(self,fmt="%W %Y %R",flt={},outfile='fig1'):
    """
    Draw a figure with two y-axis: the throughput and the latency.
    The x-axis is object size (log scale).
    'fmt' specifies the legend we use to describe each line.
    P - (P)ool type: replicated/erasure
    W - (W)orkload: write/seq(r)/rand(r)
    R - (R)eplication factor: 1/3/5
    C - number of (C)oncurrent clients
    D - (D)uration of each test
    g - number of placement (g)roups
    G - number of placement (G)roups for placement
    Y - (Y)-axis: thp/lat
    'flt' specifies which series to include.
    P - (P)ool type: replicated/erasure
    W - (W)orkload: write/seq/rand
    R - (R)eplication factor: 1/3/5
    C - number of (C)oncurrent clients
    example for 'flt':
    flt = { \
      'P': ['replicated'];
      'W': ['write','seq','rand'];
      'R': [1,3,5]
      'C': [1,4,16,64]
    """
    # STEP 1 Load data
    dat = numpy.loadtxt('/'.join((self._data,self._data_file)))
    ind = numpy.lexsort((dat[:,7],dat[:,6],dat[:,5],dat[:,4],dat[:,3],dat[:,2],dat[:,0]))
    sdat = dat[ind]
    # STEP 2 prepare plot
    fig,ax1 = plt.subplots()
    ax2 = ax1.twinx()
    lns = []
    lbs = []
    ptDict = {0:'replicated',1:'erasure'}
    wlDict = {0:'write',1:'seq',2:'rand'}
    markers = ["o","v","s","P","^","*","<","p","h",">"]
    cnt = 0
    # STEP 3 plot series
    for (k,g) in groupby(sdat, lambda x:map(x.__getitem__,[0,2,3,4,5,6,7])):
      # STEP 3.1 get legend
      fP = ptDict[int(k[5])]
      fW = wlDict[int(k[6])]
      fR = str(int(k[0]))
      fC = str(int(k[1]))
      fD = str(int(k[2]))
      fg = str(int(k[3]))
      fG = str(int(k[4]))
      lb = fmt.replace('%P',fP) \
        .replace('%W',fW) \
        .replace('%R',fR) \
        .replace('%C',fC) \
        .replace('%D',fD) \
        .replace('%g',fg) \
        .replace('%G',fG)
      # STEP 3.1.5 - filter
      if 'P' in flt and fP not in flt['P']:
        logger.info("skip P=%s" % fP)
        continue
      if 'W' in flt and fW not in flt['W']:
        logger.info("skip W=%s" % fW)
        continue
      if 'R' in flt and fR not in str(flt['R']):
        logger.info("skip R=%s" % fR)
        continue
      if 'C' in flt and fC not in str(flt['C']):
        logger.info("skip C=%s" % fC)
        continue
      logger.info("Added series: P=%s W=%s R=%s C=%s", fP,fW,fR,fC)
      # STEP 3.2 plot
      s = numpy.array(list(g))
      ss = s[s[:,1].argsort()]
      ln_thp, = ax1.plot(ss[:,1],ss[:,8],marker=markers[cnt%len(markers)])
      lb_thp = lb.replace('%Y','thp')
      lns.append(ln_thp)
      lbs.append(lb_thp)
      ln_lat, = ax2.plot(ss[:,1],ss[:,12],'--',marker=markers[cnt%len(markers)])
      lb_lat = lb.replace('%Y','lat')
      lns.append(ln_lat)
      lbs.append(lb_lat)
      cnt = cnt + 1
    # STEP 4 lables
    ax1.set_xscale('log',basex=2)
    ax1.set_xlabel('Object size (Bytes)')
    ax1.set_ylabel('Throughput (MB/s)')
    ax2.set_ylabel('Latency (sec)')
    ax1.legend(lns,lbs,ncol=2,loc=0)
    y1lim = ax1.get_ylim()
    y2lim = ax2.get_ylim()
    ax1.set_ylim((0,y1lim[1]*1.2))
    ax2.set_ylim((0,y2lim[1]*1.2))
    # tight
    # fig.set_size_inches(16,12)
    fig.tight_layout()
    plt.savefig('/'.join((self._data,'%s.eps' % outfile)))
    plt.close()
