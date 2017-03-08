#ifndef PERSIST_VAR_H
#define PERSIST_VAR_H

#include <sys/types.h>
#include <inttypes.h>
#include <string>
#include <iostream>
#include "PersistException.h"
#include "PersistLog.h"
#include "FilePersistLog.h"

using namespace std;

namespace ns_persistent {

  extern const string NULL_OBJECT_NAME;

  // #define DEFINE_PERSIST_VAR(_t,_n) DEFINE_PERSIST_VAR(_t,_n,ST_FILE)
  #define DEFINE_PERSIST_VAR(_t,_n,_s) \
    PersistVar<_t, _s> _n(# _n)
  #define DECLARE_PERSIST_VAR(_t,_n,_s) \
    extern DEFINE_PERSIST_VAR(_t,_n,_s)

  // PersistVar represents a variable backed up by persistent storage. The
  // backend is PersistLog class. PersistLog handles only raw bytes and this
  // class is repsonsible for converting it back and forth between raw bytes
  // and ObjectType. But, the serialization/deserialization functionality is
  // actually defined by ObjectType and provided by PersistVar users.
  // - ObjectType: user-defined type of the variable it is required to support
  //   serialization and deserialization
  // - StorageType: storage type is defined in PersistLog. The value could be
  //   ST_FILE/ST_MEM/ST_3DXP ... I will start with ST_FILE and extend it to
  //   other persistent Storage.
  template <typename ObjectType,
    StorageType st=ST_FILE>
  class PersistVar{
  public:
      // constructor: this will create a persisted variable. It will be
      // loaded from persistent storage defined by st, if it is already
      // defined there, otherwise a new variable as well as its persistent
      // representation is created.
      // - objectName: name of the given object.
      PersistVar<ObjectType,st>(const char * objectName) noexcept(false){
        this->m_pLog = NULL;
        switch(st){

        case ST_FILE:
          this->m_pLog = new FilePersistLog(objectName);
          if(this->m_pLog == NULL){
            throw PERSIST_EXP_NEW_FAILED_UNKNOWN;
          }
          break;

        default:
          throw PERSIST_EXP_STORAGE_TYPE_UNKNOWN(st);
        }
      };
      // destructor: release the resources
      virtual ~PersistVar<ObjectType,st>() noexcept(false){
        if(this->m_pLog != NULL){
          delete this->m_pLog;
        }
      };

      // get the latest Value of T
      virtual const ObjectType& get() noexcept(false){
        //TODO: get the latest value
        throw PERSIST_EXP_UNIMPLEMENTED;
      };

      // get the value defined by version
      // version number >= 0
      // negtive number means how many versions we go back from the latest
      // version. -1 means the previous version.
      virtual const ObjectType& get(const int64_t & version) noexcept(false){
        //TODO: get the specified version
        throw PERSIST_EXP_UNIMPLEMENTED;
      };

      // set value: this increases the version number by 1.
      virtual void set(const ObjectType &v) noexcept(false){
        //TODO: get the specified version
        throw PERSIST_EXP_UNIMPLEMENTED;
      };

#ifdef HLC_ENABLED

      // HLC based get/set: this feature will be added later, let's don't
      // worry about it now.
      // get the value defined by HLC time:
      virtual const ObjectType& get(const HLC &ts){ throw exp... };

      // set the value defined by HLC time:
      virtual void set(const ObjectType &v, const HLC &msg_ts)

#endif//HLC_ENABLED

  private:
      // PersistLog
      PersistLog * m_pLog;
  };

}

#endif//PERSIST_VAR_H
