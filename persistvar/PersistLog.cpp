#include "PersistLog.h"

namespace ns_persistent {

  PersistLog::PersistLog(const string &name) noexcept(true){
    this->name = name;
  }

  PersistLog::~PersistLog() noexcept(true){
  }
}
