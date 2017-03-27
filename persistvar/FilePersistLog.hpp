#ifndef FILE_PERSIST_LOG_HPP
#define FILE_PERSIST_LOG_HPP

#include <pthread.h>
#include "PersistLog.hpp"

namespace ns_persistent {

  // TODO:hardwired definitions need go to configuration file
  #define DEFAULT_FILE_PERSIST_LOG_DATA_PATH (".plog")
  #define METAFILE_SUFFIX ("meta")
  #define DATAFILE_SUFFIX ("data")

  // FilePersistLog is the default persist Log
  class FilePersistLog : public PersistLog {
  private:
    // path of the data files
    const string m_sDataPath;

    // file descriptors for meta file and data file.
    int m_iMetaFileDesc;
    int m_iDataFileDesc;

    // memory mapped area for meta and data
    void * m_pMeta;
    void * m_pData;

    // read/write lock
    pthread_rwlock_t m_rwlock;

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

    // load the log from files. This method may through exceptions if read from
    // file failed.
    virtual void load() noexcept(false);

  public:

    //Constructor
    FilePersistLog(const string &name,const string &dataPath) noexcept(false);
    FilePersistLog(const string &name) noexcept(false):
    FilePersistLog(name,DEFAULT_FILE_PERSIST_LOG_DATA_PATH){
    };
    //Destructor
    virtual ~FilePersistLog() noexcept(true);

    //Derived from PersistLog
    virtual void append(const void * pdata, uint64_t size, const HLC &hlc) noexcept(false);
    virtual int64_t getLength() noexcept(false);
    virtual const void* getEntry(int64_t eno) noexcept(false);
    virtual const void* getEntry(const HLC &hlc) noexcept(false);
    //virtual const void* getEntry() noexcept(false); - deprecated
  };
}

#endif//FILE_PERSIST_LOG_HPP
