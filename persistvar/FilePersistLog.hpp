#ifndef FILE_PERSIST_LOG_HPP
#define FILE_PERSIST_LOG_HPP

#include <pthread.h>
#include "MemLog.hpp"

namespace ns_persistent {

  // TODO:hardwired definitions need go to configuration file
  #define DEFAULT_FILE_PERSIST_LOG_DATA_PATH (".plog")
  #define METAFILE_SUFFIX ("meta")
  #define DATAFILE_SUFFIX ("data")

  // FilePersistLog is the default persist Log
  class FilePersistLog : public MemLog {
  private:
    // path of the data files
    const string m_sDataPath;

    // file descriptors for meta file and data file.
    int m_iMetaFileDesc;
    int m_iDataFileDesc;

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
  };
}

#endif//FILE_PERSIST_LOG_HPP
