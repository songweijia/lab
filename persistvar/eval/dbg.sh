#!/bin/bash
BIN=./ptst
MAX_LOG_ENTRY=64
MAX_DATA_SIZE=4096

function clean {
  rm -rf .plog /dev/shm/volatile_t
  ec=$?
  if [ $ec == 0 ]; then
    echo "log cleaned."
  else
    echo "clean log failed with errcode=$ec"
  fi
}

# write more than MAX_LOG_ENTRY-1 entries
# to incur exception
function test_1 {
  VER=10000
  V=0
  for((i=0;i<${MAX_LOG_ENTRY};i++))
  do
    VER=$((${VER}+1))
    V=$((${V}+9))
    ${BIN} set ${V} ${VER}
  done
}

# test with 3 times MAX_LOG_ENTRY
function test_2 {
  VER=10000
  V=0
  CNT=0
  TH=$((${MAX_LOG_ENTRY}-1))
  TOT_LOG=`expr ${MAX_LOG_ENTRY} \* 3`
  for ((i=0;i<${TOT_LOG};i++))
  do
    VER=$((${VER}+1))
    V=$((${V}+9))
    ${BIN} set ${V} ${VER}
    if [ $? != 0 ]; then
      exit;
    fi
    CNT=$((${CNT}+1))
    if [ $CNT == $TH ]; then
      ${BIN} trimbyver ${VER}
      CNT=0
    fi
  done
}

#################
if [ $# != 1 ]; then
  echo "USAGE: $0 [n]"
  echo -e "\t1 - overflow the log."
  echo -e "\t2 - overflow the logdata."
  echo -e "\tx - clean the log."
else
  case $1 in
  1)
    test_1
    ;;
  2)
    test_2
    ;;
  x)
    clean
    ;;
  *)
    echo "$1 not implemented"
    ;;
  esac
fi
