#ifndef MEM_LOG_HPP
#define MEM_LOG_HPP

#include <pthread.h>
#include "PersistLog.hpp"

namespace ns_persistent {

  // meta header format
  typedef union meta_header {
    struct {
      int64_t eno;      // number of entry
      uint64_t ofst;    // next available offset in Data File.
    } fields;
    uint8_t bytes[32];
  } MetaHeader;

  // log entry format
  typedef union log_entry {
    struct {
      uint64_t dlen;    // length of the data
      uint64_t ofst;    // offset of the data
      HLC hlc;          // HLC clock of the data
    } fields;
    uint8_t bytes[32];
  } LogEntry;

  // helpers
  #define META_HEADER ((MetaHeader*)this->m_pMeta)
  #define LOG_ENTRY_ARRAY       ((LogEntry*)((uint8_t*)this->m_pMeta + sizeof(MetaHeader)))
  #define NEXT_LOG_ENTRY        (LOG_ENTRY_ARRAY + META_HEADER->fields.eno)
  #define CURR_LOG_ENTRY        (NEXT_LOG_ENTRY - 1)
  #define NEXT_DATA             ((void *)((uint64_t)this->m_pData + META_HEADER->fields.ofst))
  #define PAGE_SIZE             (getpagesize())
  #define ALIGN_TO_PAGE(x)      ((void *)(((uint64_t)(x))-((uint64_t)(x))%PAGE_SIZE))
  #define LOG_ENTRY_DATA(e)     ((void *)((uint8_t *)this->m_pData+(e)->fields.ofst))

  // MemLog is for volatile variables.
  class MemLog : public PersistLog {
  protected:
    void * m_pMeta;
    void * m_pData;

    // read/write lock
    pthread_rwlock_t m_rwlock;
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
  
    // HLC clock of the latest event
    // We record the timestamp of the latest event with this attribute,
    // so that even when the RTC clock is turned backward, for example,
    // by NTP, we will not create a new event with a time stamp lower than
    // m_hlcLE. Example: if the latest log entry has an HLC (r1,l1), a read
    // requesting state at HLC (r2,l2) happens when RTC clock is r3. If r1 < r2 <r3
    // stands, we think it's safe to return the state at (r1,l1) because any
    // new event will get an HLC clock at least at (r3,0). However, if NTP turns
    // the RTC back to r3' with r3'<r2, the next update may get a clock lower
    // than (r2,0). Then a fllowing read at HLC(r2,l2) may return a state
    // different from our previous read! m_hlcLE is introduced to prevent
    // this from happenning. The m_hlcLE is updated with the latest events
    // including a successful read. The uncomping event should always get an
    // HLC timestamp latter than this.
    HLC m_hlcLE;

  public:

    //Constructor
    MemLog(const string &name) noexcept(false);
    //Destructor
    virtual ~MemLog() noexcept(true);

    //Derived from PersistLog
    virtual void append(const void * pdata, uint64_t size, const HLC &hlc) noexcept(false);
    virtual int64_t getLength() noexcept(false);
    virtual const void* getEntry(int64_t eno) noexcept(false);
    virtual const void* getEntry(const HLC &hlc) noexcept(false);
  };
}

#endif//MEM_LOG_HPP
