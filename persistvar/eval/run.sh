#!/bin/bash
# run persistent experiment, collecting data
BINARY=../build/ptst
TMPOUT=tmpout

evaluate() {
  STORAGETYPE=$1
  OBJSIZE=$2
  NOPS=$3
  NLOOP=5
  for((i=0;i<=${NLOOP};i++))
  do
    ${BINARY} eval ${STORAGETYPE} ${OBJSIZE} ${NOPS} > ${TMPOUT}
    thp_MBps=`cat ${TMPOUT} | grep "throughput" | awk '{print $2}'`
    lat_us=`cat ${TMPOUT} | grep "latency" | awk '{print $2}'`
    echo "${OBJSIZE} ${thp_MBps} MBps ${lat_us} us"
    rm ${TMPOUT}
  done
}

for st in mem file
do
  for sz in 8 16 32 64 128 256 512 1024 2048 4096 8192 16384 32768 65536 131072 262144 524288 1048576
  # for sz in 8 16 32 64 128 256 512 1024 2048 4096 8192 16384 32768 65536
  # for sz in 2097152 4194304 8388608 16777216
  do
    evaluate ${st} ${sz} 1000 >> ${st}.raw
  done
done
