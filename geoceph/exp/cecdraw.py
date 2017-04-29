#!/usr/bin/python

from cepheval.cecli import CephEvalClient

if __name__ == "__main__":
  # cec = CephEvalClient('hdd','compute31','ceph',admin_key='/home/ceph/.ssh/id_rsa')
  #cec = CephEvalClient('ram','compute26','ceph',admin_key='/home/ceph/.ssh/id_rsa')
  # cec = CephEvalClient('ram-rtt-2ms','compute26','ceph',admin_key='/home/ceph/.ssh/id_rsa')
  cec = CephEvalClient('ram-rtt-4ms','compute26','ceph',admin_key='/home/ceph/.ssh/id_rsa')
  # cec.run()
  # cec.parse()
  cec.draw_fig1(fmt="%W %Y r%R c%C",flt={'W':['rand'],'C':[4]},outfile='rep-r')
  cec.draw_fig1(fmt="%W %Y r%R c%C",flt={'W':['write'],'C':[4]},outfile='rep-w')
  cec.draw_fig1(fmt="%W %Y r%R c%C",flt={'W':['rand'],'R':[3]},outfile='cli-r')
  cec.draw_fig1(fmt="%W %Y r%R c%C",flt={'W':['write'],'R':[3]},outfile='cli-w')
  # cec.draw_fig1(fmt="%W %Y r%R c%C",flt={'W':['write'],'C':[16,64]},outfile='all_write_highc')
