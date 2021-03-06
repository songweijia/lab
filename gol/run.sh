#!/bin/bash
for((l=1;l<=5;l++))
do
  # for((i=10;i<=16384;i+=16))
  for((i=10;i<=1024;i+=16))
  do
  #  sudo perf stat -e instructions,cycles,cpu/event=0xa2,umask=0x10,cmask=0/ taskset 0x80 ./gol 10 $i 1000 2>> output
    # for nth in 1 2 4 8 16
    for nth in 1
    do
      # for way in 1 3 f ff ffff
      for way in 1 ffff
      do
        echo "sudo rdtset -t \"l3=0x${way};cpu=1-${nth}\" -k -c 1-${nth} nice --20 ./gol 10 $i ${nth} >> data/${nth}-th-${way}-way-output.$l"
        sudo rdtset -t "l3=0x${way};cpu=1-${nth}" -k -c 1-${nth} nice --20 ./gol 10 $i ${nth} >> data/cat-rand/${nth}-th-${way}-way-output
      done
    done
  done
done
