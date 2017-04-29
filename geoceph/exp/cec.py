#!/usr/bin/python
import sys
from cepheval.cecli import CephEvalClient

if __name__ == "__main__":
#  cec = CephEvalClient('ssd','compute26','ceph',admin_key='/home/ceph/.ssh/id_rsa')
  cec = CephEvalClient(sys.argv[1],'compute26','ceph',admin_key='/home/ceph/.ssh/id_rsa')
#  cec.run()
  cec.parse()
  cec.draw_fig1(fmt="%W %Y r%R c%C",flt={'W':['rand'],'C':[32]},outfile='rep-r-c32')
  cec.draw_fig1(fmt="%W %Y r%R c%C",flt={'W':['write'],'C':[32]},outfile='rep-w-c32')
  cec.draw_fig1(fmt="%W %Y r%R c%C",flt={'W':['rand'],'C':[1]},outfile='rep-r-c1')
  cec.draw_fig1(fmt="%W %Y r%R c%C",flt={'W':['write'],'C':[1]},outfile='rep-w-c1')
  cec.draw_fig1(fmt="%W %Y r%R c%C",flt={'W':['rand'],'R':[3]},outfile='cli-r')
  cec.draw_fig1(fmt="%W %Y r%R c%C",flt={'W':['write'],'R':[3]},outfile='cli-w')
