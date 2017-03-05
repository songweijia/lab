#include "FilePersistLog.h"

namespace ns_persistent{

  FilePersistLog::FilePersistLog(const string &name, const string &dataPath) 
  noexcept(false) : PersistLog(name){
    this->dataPath = dataPath;
    load();
  }

  void FilePersistLog::load()
  noexcept(false){
    //TODO: load log, if not exist, create it
    throw PERSIST_EXP_UNIMPLEMENTED;
  }

  FilePersistLog::~FilePersistLog()
  noexcept(true){
  }

  void FilePersistLog::append(const void *pdat, uint64_t size)
  noexcept(false){
    //TODO
    throw PERSIST_EXP_UNIMPLEMENTED;
  }

  uint64_t FilePersistLog::getLength()
  noexcept(false){
    //TODO
    throw PERSIST_EXP_UNIMPLEMENTED;
  }

  const void * FilePersistLog::getEntry(int64_t eno)
  noexcept(false){
    //TODO
    throw PERSIST_EXP_UNIMPLEMENTED;
  }

  const void * FilePersistLog::getEntry()
  noexcept(false){
    //TODO
    throw PERSIST_EXP_UNIMPLEMENTED;
  }
}
