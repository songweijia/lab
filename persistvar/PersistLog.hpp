#ifndef PERSIST_LOG_HPP
#define	PERSIST_LOG_HPP

#include <stdio.h>
#include <inttypes.h>
#include <map>
#include <string>
#include "PersistException.hpp"
#include "HLC.hpp"

using namespace std;

namespace ns_persistent {

  // Storage type:
  enum StorageType{
    ST_FILE=0,
    ST_MEM,
    ST_3DXP
  };

  // Persistent log interfaces
  class PersistLog{
  protected:
    // LogName
    const string m_sName;
  public:
    // Constructor:
    // Remark: the constructor will check the persistent storage
    // to make sure if this named log(by "name" in the template 
    // parameters) is already there. If it is, load it from disk.
    // Otherwise, create the log.
    PersistLog(const string &name) noexcept(true);
    virtual ~PersistLog() noexcept(true);
    // Persistent Append
    virtual void append(const void * pdata, uint64_t size, const HLC & mhlc) noexcept(false) = 0;
    // Get length of the log 
    virtual int64_t getLength() noexcept(false) = 0;
    // Get a version
    virtual const void* getEntry(int64_t eno) noexcept(false) = 0;
    // Get the latest version - deprecated.
    // virtual const void* getEntry() noexcept(false) = 0;
    // Get a version specified by hlc
    virtual const void* getEntry(const HLC & hlc) noexcept(false) = 0;
  };
}

#endif//PERSIST_LOG_HPP
