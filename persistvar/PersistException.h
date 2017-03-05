#ifndef PERSISTENT_EXCEPTION_H
#define PERSISTENT_EXCEPTION_H

namespace ns_persistent{
  // Exceptions definition
  #define PERSIST_EXP(errcode,usercode) \
    ((((errcode)&0xffffffffull)<<32)|((usercode)&0xffffffffull))
  #define PERSIST_EXP_UNIMPLEMENTED                     PERSIST_EXP(0,0)
  #define PERSIST_EXP_NEW_FAILED_UNKNOWN                PERSIST_EXP(1,0)
  #define PERSIST_EXP_STORAGE_TYPE_UNKNOWN(x)           PERSIST_EXP(2,(x))
}

#endif//PERSISTENT_EXCEPTION_H
