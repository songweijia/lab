#!/bin/bash
# inside data center (RTT<500us)
./cec.py in_az_hdd

# across az - 0.5 ms (RTT=1ms)
for((i=26;i<=32;i++))
do
  ssh compute${i} "sudo tc qdisc add dev br0 root netem delay 0.5ms"
done
./cec.py cross_az_hdd

# domestic - 30 ms (RTT=60ms)
for((i=26;i<=32;i++))
do
  ssh compute${i} "sudo tc qdisc change dev br0 root netem delay 30ms"
done
./cec.py domestic_hdd

# worldwide - 100 ms (RTT=200ms)
for((i=26;i<=32;i++))
do
  ssh compute${i} "sudo tc qdisc change dev br0 root netem delay 100ms"
done
./cec.py worldwide_hdd

for((i=26;i<=32;i++))
do
  ssh compute${i} "sudo tc qdisc delete dev br0 root netem delay 100ms"
done
