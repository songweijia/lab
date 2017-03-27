#ifndef PERSISTENT_EXCEPTION_HPP
#define PERSISTENT_EXCEPTION_HPP

#include <inttypes.h>

namespace ns_persistent{
  // Exceptions definition
  #define PERSIST_EXP(errcode,usercode) \
    ((((errcode)&0xffffffffull)<<32)|((usercode)&0xffffffffull))
  #define PERSIST_EXP_USERCODE(x) ((uint32_t)((x)&0xffffffffull))
  #define PERSIST_EXP_UNIMPLEMENTED                     PERSIST_EXP(0,0)
  #define PERSIST_EXP_NEW_FAILED_UNKNOWN                PERSIST_EXP(1,0)
  #define PERSIST_EXP_STORAGE_TYPE_UNKNOWN(x)           PERSIST_EXP(2,(x))
  #define PERSIST_EXP_OPEN_FILE(x)                      PERSIST_EXP(3,(x))
  #define PERSIST_EXP_MMAP_FILE(x)                      PERSIST_EXP(4,(x))
  #define PERSIST_EXP_INV_PATH                          PERSIST_EXP(5,0)
  #define PERSIST_EXP_CREATE_PATH(x)                    PERSIST_EXP(6,(x))
  #define PERSIST_EXP_INV_FILE                          PERSIST_EXP(7,0)
  #define PERSIST_EXP_CREATE_FILE(x)                    PERSIST_EXP(8,(x))
  #define PERSIST_EXP_TRUNCATE_FILE(x)                  PERSIST_EXP(9,(x))
  #define PERSIST_EXP_RWLOCK_INIT(x)                    PERSIST_EXP(10,(x))
  #define PERSIST_EXP_RWLOCK_RDLOCK(x)                  PERSIST_EXP(11,(x))
  #define PERSIST_EXP_RWLOCK_WRLOCK(x)                  PERSIST_EXP(12,(x))
  #define PERSIST_EXP_RWLOCK_UNLOCK(x)                  PERSIST_EXP(13,(x))
  #define PERSIST_EXP_MSYNC(x)                          PERSIST_EXP(14,(x))
  #define PERSIST_EXP_INV_ENTRY_IDX(x)                  PERSIST_EXP(15,(x))
  #define PERSIST_EXP_EMPTY_LOG                         PERSIST_EXP(16,0)
  #define PERSIST_EXP_SPIN_INIT(x)                      PERSIST_EXP(17,(x))
  #define PERSIST_EXP_SPIN_LOCK(x)                      PERSIST_EXP(18,(x))
  #define PERSIST_EXP_SPIN_UNLOCK(x)                    PERSIST_EXP(19,(x))
  #define PERSIST_EXP_INV_VER(x)                        PERSIST_EXP(20,(x))
  #define PERSIST_EXP_ALLOC(x)                          PERSIST_EXP(21,(x))
  #define PERSIST_EXP_READ_FUTURE                       PERSIST_EXP(22,0)
}

#endif//PERSISTENT_EXCEPTION_HPP
