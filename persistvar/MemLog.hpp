#ifndef MEM_LOG_HPP
#define MEM_LOG_HPP

#include <pthread.h>
#include "PersistLog.hpp"

namespace ns_persistent {

  // MemLog is for volatile variables.
  class MemLog : public PersistLog {
  private:
    void * m_pMeta;
    void * m_pData;

    // read/write lock
    pthread_rwlock_t m_rwlock;

    // HLC clock of the latest event
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
