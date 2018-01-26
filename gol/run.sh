#!/bin/bash
for((l=1;l<=1;l++))
do
  for((i=10;i<=1024;i+=4))
  do
  #  sudo perf stat -e instructions,cycles,cpu/event=0xa2,umask=0x10,cmask=0/ taskset 0x80 ./gol 10 $i 1000 2>> output
    sudo rdtset -t 'l3=0xfffff;cpu=4' -k -c 4 nice --20 ./gol 10 $i >> output.$l
  done
done
