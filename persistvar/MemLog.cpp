#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include "MemLog.h"

using namespace std;

namespace ns_persistent{

  /////////////////////////
  // internal structures //
  /////////////////////////

  // meta file header format
  typedef union meta_header {
    struct {
      int64_t eno;      // number of entry
      uint64_t ofst;    // next available offset in Data buffer.
    } fields;
    uint8_t bytes[32];
  } MetaHeader;

  // log entry format
  typedef union log_entry {
    struct {
      uint64_t dlen;    // length of the data
      uint64_t ofst;	// offset of the data
    };
    uint8_t bytes[32];
  } LogEntry;

  // helpers
  #define META_HEADER ((MetaHeader*)this->m_pMeta)
  #define LOG_ENTRY_ARRAY       ((LogEntry*)((uint8_t*)this->m_pMeta + sizeof(MetaHeader)))
  #define NEXT_LOG_ENTRY        (LOG_ENTRY_ARRAY + META_HEADER->fields.eno)
  #define NEXT_DATA             ((void *)((uint64_t)this->m_pData + META_HEADER->fields.ofst))
  #define PAGE_SIZE             (getpagesize())
  #define ALIGN_TO_PAGE(x)      ((void *)(((uint64_t)(x))-((uint64_t)(x))%PAGE_SIZE))
  #define LOG_ENTRY_DATA(e)     ((void *)(((uint64_t)this->m_pData)+(e)->ofst))

  // size limit for the meta and data file
  #define META_SIZE (0x1ull<<20)
  #define DATA_SIZE (0x1ull<<30)

  // lock macro
  #define ML_WRLOCK \
  do { \
    if (pthread_rwlock_wrlock(&this->m_rwlock) != 0) { \
      throw PERSIST_EXP_RWLOCK_WRLOCK(errno); \
    } \
  } while (0)

  #define ML_RDLOCK \
  do { \
    if (pthread_rwlock_rdlock(&this->m_rwlock) != 0) { \
      throw PERSIST_EXP_RWLOCK_WRLOCK(errno); \
    } \
  } while (0)

  #define ML_UNLOCK \
  do { \
    if (pthread_rwlock_unlock(&this->m_rwlock) != 0) { \
      throw PERSIST_EXP_RWLOCK_UNLOCK(errno); \
    } \
  } while (0)

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

  void MemLog::append (const void *pdat, uint64_t size)
  noexcept(false) {
    ML_WRLOCK;

    // copy data
    memcpy(NEXT_DATA,pdat,size);

    // fill log entry
    NEXT_LOG_ENTRY->dlen = size;
    NEXT_LOG_ENTRY->ofst = META_HEADER->fields.ofst;

    // update meta header
    META_HEADER->fields.eno ++;
    META_HEADER->fields.ofst += size;
    
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

    return LOG_ENTRY_DATA(LOG_ENTRY_ARRAY + ridx);
  }
}
