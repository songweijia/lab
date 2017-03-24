#ifndef MEM_LOG_H
#define MEM_LOG_H

#include <pthread.h>
#include "PersistLog.h"

namespace ns_persistent {

  // MemLog is for volatile variables.
  class MemLog : public PersistLog {
  private:
    void * m_pMeta;
    void * m_pData;

    // mutex
    pthread_rwlock_t m_rwlock;

  public:

    //Constructor
    MemLog(const string &name) noexcept(false);
    //Destructor
    virtual ~MemLog() noexcept(true);

    //Derived from PersistLog
    virtual void append(const void * pdata, uint64_t size) noexcept(false);
    virtual int64_t getLength() noexcept(false);
    virtual const void* getEntry(int64_t eno) noexcept(false);
  };
}

#endif//MEM_LOG_H
