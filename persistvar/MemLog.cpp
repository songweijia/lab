#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include "util.hpp"
#include "MemLog.hpp"

using namespace std;

namespace ns_persistent{

  // size limit for the meta and data file
  #define META_SIZE (0x1ull<<20)
  #define DATA_SIZE (0x1ull<<30)

  ////////////////////////
  // visible to outside //
  ////////////////////////

  MemLog::MemLog (const string &name) 
  noexcept(false) : PersistLog(name) {
    this->m_pMeta = nullptr;
    this->m_pData = nullptr;
    this->m_pMeta = aligned_alloc(PAGE_SIZE,META_SIZE);
    if (this->m_pMeta == nullptr) {
      throw PERSIST_EXP_ALLOC(errno);
    }
    this->m_pData = aligned_alloc(PAGE_SIZE,DATA_SIZE);
    if (this->m_pData == nullptr) {
      throw PERSIST_EXP_ALLOC(errno);
    }
    if (pthread_rwlock_init(&this->m_rwlock,NULL) != 0) {
      throw PERSIST_EXP_RWLOCK_INIT(errno);
    }
  }

  MemLog::~MemLog ()
  noexcept(true){
    pthread_rwlock_destroy(&this->m_rwlock);
    if (this->m_pData != nullptr){
      free(this->m_pData);
    }
    if (this->m_pMeta != nullptr){
      free(this->m_pMeta);
    }
  }

  void MemLog::append (const void *pdat, uint64_t size, const HLC &mhlc)
  noexcept(false) {
    dbg_trace("{0} append event ({1},{2})",this->m_sName, mhlc.m_rtc_us, mhlc.m_logic);
    ML_WRLOCK;
    // copy data
    memcpy(NEXT_DATA,pdat,size);

    // fill log entry
    NEXT_LOG_ENTRY->fields.dlen = size;
    NEXT_LOG_ENTRY->fields.ofst = META_HEADER->fields.ofst;
    this->m_hlcLE = (mhlc > this->m_hlcLE)?mhlc:this->m_hlcLE;
    this->m_hlcLE.tick();
    NEXT_LOG_ENTRY->fields.hlc_r = this->m_hlcLE.m_rtc_us;
    NEXT_LOG_ENTRY->fields.hlc_l = this->m_hlcLE.m_logic;

    // update meta header
    META_HEADER->fields.eno ++;
    META_HEADER->fields.ofst += size;
    
    dbg_trace("{0} append log ({1},{2})",this->m_sName, this->m_hlcLE.m_rtc_us, this->m_hlcLE.m_logic);
    ML_UNLOCK;
  }

  int64_t MemLog::getLength ()
  noexcept(false) {

    ML_RDLOCK;
    int64_t len = META_HEADER->fields.eno;
    ML_UNLOCK;

    return len;
  }

  const void * MemLog::getEntry (int64_t eidx)
    noexcept(false) {

    ML_RDLOCK;
    if (META_HEADER->fields.eno < eidx || (eidx + (int64_t)META_HEADER->fields.eno) < 0 ) {
      ML_UNLOCK;
      throw PERSIST_EXP_INV_ENTRY_IDX(eidx);
    }
    int64_t ridx = (eidx < 0)?((int64_t)META_HEADER->fields.eno + eidx):eidx;
    ML_UNLOCK;

    dbg_trace("{0} getEntry at ({1},{2})",this->m_sName,(LOG_ENTRY_ARRAY + ridx)->fields.hlc_r,(LOG_ENTRY_ARRAY + ridx)->fields.hlc_l);

    return LOG_ENTRY_DATA(LOG_ENTRY_ARRAY + ridx);
  }

  // find the log entry by HLC timestmap:rhlc
  static LogEntry * binarySearch(const HLC &rhlc, LogEntry *logArr, int64_t len) {
    if (len <= 0) {
      dbg_trace("binary Search failed...EMPTY LOG");
      return nullptr;
    }
    int64_t head = 0, tail = len - 1;
    int64_t pivot = 0;
    while (head <= tail) {
      pivot = (head + tail)/2;
      dbg_trace("Search range: {0}->[{1},{2}]",pivot,head,tail);
      if ((logArr+pivot)->fields.hlc_r == rhlc.m_rtc_us && 
        (logArr+pivot)->fields.hlc_l == rhlc.m_logic){
        break; // found
      } else if ((logArr+pivot)->fields.hlc_r < rhlc.m_rtc_us ||
        ((logArr+pivot)->fields.hlc_r == rhlc.m_rtc_us &&
          (logArr+pivot)->fields.hlc_l < rhlc.m_logic)){
        if (pivot + 1 >= len) {
          break; // found
        } else if ((logArr+pivot+1)->fields.hlc_r > rhlc.m_rtc_us ||
          ((logArr+pivot+1)->fields.hlc_r == rhlc.m_rtc_us &&
            (logArr+pivot+1)->fields.hlc_l > rhlc.m_logic)){
          break; // found
        } else { // search right
          head = pivot + 1;
        }
      } else { // search left
        tail = pivot - 1;
        if (head > tail) {
          dbg_trace("binary Search failed...Object does not exist.");
          return nullptr;
        }
      }
    }
    return logArr + pivot;
  }

  const void * MemLog::getEntry(const HLC &rhlc)
  noexcept(false) {

    uint64_t now = read_rtc_us();
    LogEntry * ple = nullptr;

    ML_RDLOCK;

    if (this->m_hlcLE < rhlc) {
      if (now <= rhlc.m_rtc_us) {
        // read future
        ML_UNLOCK;
        throw PERSIST_EXP_READ_FUTURE;
      } else {
        // update m_hlcLE to read clock
        ML_UNLOCK;
        ML_WRLOCK;
        this->m_hlcLE = rhlc;
        ML_UNLOCK;
        ML_RDLOCK;
      }
    }

    //binary search
    dbg_trace("{0} - begin binary search.",this->m_sName);
    ple = binarySearch(rhlc,LOG_ENTRY_ARRAY,META_HEADER->fields.eno);
    dbg_trace("{0} - end binary search.",this->m_sName);

    ML_UNLOCK;

    // no object exists before the requested timestamp.
    if (ple == nullptr){
      return nullptr;
    }

    dbg_trace("{0} getEntry at ({1},{2})",this->m_sName,ple->fields.hlc_r,ple->fields.hlc_l);

    return LOG_ENTRY_DATA(ple);
  }
}
