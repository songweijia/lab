#ifndef PERSISTENT_HPP
#define PERSISTENT_HPP

#include <sys/types.h>
#include <inttypes.h>
#include <string>
#include <iostream>
#include <memory>
#include <pthread.h>
#include "PersistException.hpp"
#include "PersistLog.hpp"
#include "FilePersistLog.hpp"
#include "MemLog.hpp"
#include "SerializationSupport.hpp"

using namespace mutils;

namespace ns_persistent {

  // #define DEFINE_PERSIST_VAR(_t,_n) DEFINE_PERSIST_VAR(_t,_n,ST_FILE)
  #define DEFINE_PERSIST_VAR(_t,_n,_s) \
    Persistent<_t, _s> _n(# _n)
  #define DECLARE_PERSIST_VAR(_t,_n,_s) \
    extern DEFINE_PERSIST_VAR(_t,_n,_s)

  /* deprecated
  // number of versions in the cache
  #define MAX_NUM_CACHED_VERSION        (1024)
  #define VERSION_HASH(v)               ( (int)((v) & (MAX_NUM_CACHED_VERSION-1)) )

  // invalid version:
  #define INVALID_VERSION (-1ll)

  // read from cache
  #define VERSION_IS_CACHED(v)  (this->m_aCache[VERSION_HASH(v)].ver == (v))
  #define GET_CACHED_OBJ_PTR(v) (this->m_aCache[VERSION_HASH(v)].obj)
  */

  // Persistent represents a variable backed up by persistent storage. The
  // backend is PersistLog class. PersistLog handles only raw bytes and this
  // class is repsonsible for converting it back and forth between raw bytes
  // and ObjectType. But, the serialization/deserialization functionality is
  // actually defined by ObjectType and provided by Persistent users.
  // - ObjectType: user-defined type of the variable it is required to support
  //   serialization and deserialization as follows:
  //   // serialize
  //   void * ObjectType::serialize(const ObjectType & obj, uint64_t *psize)
  //   - obj: obj is the reference to the object to be serialized
  //   - psize: psize is a uint64_t pointer to receive the size of the serialized
  //     data.
  //   - Return value is a pointer to a new malloced buffer with the serialized
  //     //TODO: this may not be efficient for large object...open to be changed.
  //   // deserialize
  //   ObjectType * ObjectType::deserialize(const void *pdata)
  //   - pdata: a buffer of the serialized data
  //   - Return value is a pointer to a new created ObjectType deserialized from
  //     'pdata' buffer.
  // - StorageType: storage type is defined in PersistLog. The value could be
  //   ST_FILE/ST_MEM/ST_3DXP ... I will start with ST_FILE and extend it to
  //   other persistent Storage.
  template <typename ObjectType,
    StorageType storageType=ST_FILE>
  class Persistent{
  public:
      // constructor: this will guess the objectname from ObjectType
      Persistent<ObjectType,storageType>() noexcept(false): 
        Persistent<ObjectType,storageType>(
          *(Persistent<ObjectType,storageType>::getNameMaker().make())
        ){};
      // constructor: this will create a persisted variable. It will be
      // loaded from persistent storage defined by st, if it is already
      // defined there, otherwise a new variable as well as its persistent
      // representation is created.
      // - objectName: name of the given object.
      Persistent<ObjectType,storageType>(const char * objectName) noexcept(false){
        // Initialize log
        this->m_pLog = NULL;
        switch(storageType){

        // file system
        case ST_FILE:
          this->m_pLog = new FilePersistLog(objectName);
          if(this->m_pLog == NULL){
            throw PERSIST_EXP_NEW_FAILED_UNKNOWN;
          }
          break;
        // volatile
        case ST_MEM:
          this->m_pLog = new MemLog(objectName);
          if(this->m_pLog == NULL){
            throw PERSIST_EXP_NEW_FAILED_UNKNOWN;
          }
          break;

        default:
          throw PERSIST_EXP_STORAGE_TYPE_UNKNOWN(storageType);
        }

        /* deprecated
        // Initialize the version cache:
        int i;
        for (i=0;i<MAX_NUM_CACHED_VERSION;i++){
          this->m_aCache[i].ver = INVALID_VERSION;
          if (pthread_spin_init(&this->m_aCache[i].lck,PTHREAD_PROCESS_SHARED) != 0) {
            throw PERSIST_EXP_SPIN_INIT(errno);
          }
        }
        */
      };
      // destructor: release the resources
      virtual ~Persistent<ObjectType,storageType>() noexcept(false){
        /* deprecated
        // destroy the spinlocks
        int i;
        for (i=0;i<MAX_NUM_CACHED_VERSION;i++){
          pthread_spin_destroy(&this->m_aCache[i].lck);
        }
        */

        // destroy the in-memory log
        if(this->m_pLog != NULL){
          delete this->m_pLog;
        }
      };

      // get the latest Value of T. The user lambda will be fed with the latest object
      // zerocopy:this object will not live once it returns.
      // return value is decided by user lambda
      template <typename Func>
      auto get (
        const Func& fun, 
        DeserializationManager *dm=nullptr)
        noexcept(false) {
        // return this->getByLambda(-1,fun,dm);
        return this->get(-1L,fun,dm);
      };

      // get the latest Value of T, returns a unique pointer to the object
      std::unique_ptr<ObjectType> get ( DeserializationManager *dm=nullptr)
        noexcept(false) {
        return this->get(-1L,dm);
      };

      // get a version of Value T. the user lambda will be fed with the given object
      // zerocopy:this object will not live once it returns.
      // return value is decided by user lambda
      template <typename Func>
      auto get (
        int64_t version, 
        const Func& fun, 
        DeserializationManager *dm=nullptr)
        noexcept(false) {
        int64_t ver = version;

        if (ver < 0) {
          ver += this->m_pLog->getLength();
        }
        if (ver < 0 || ver >= this->m_pLog->getLength() ) {
          throw PERSIST_EXP_INV_VER(version);
        }

        return deserialize_and_run<ObjectType>(dm,(char *)this->m_pLog->getEntry(ver),fun);
      };

      // get a version of value T. returns a unique pointer to the object
      std::unique_ptr<ObjectType> get(
        int64_t version, 
        DeserializationManager *dm=nullptr)
        noexcept(false) {
         int64_t ver = version;

        if (ver < 0) {
          ver += this->m_pLog->getLength();
        }
        if (ver < 0 || ver >= this->m_pLog->getLength() ) {
          throw PERSIST_EXP_INV_VER(version);
        }

        return from_bytes<ObjectType>(dm,(char const *)this->m_pLog->getEntry(ver));      
      };

      // syntax sugar: get the specified version of T without DSM
      std::unique_ptr<ObjectType> operator [](int64_t version)
        noexcept(false) {
        return this->get(version);
      }

      /* deprecated
      // get the latest Value of T
      // virtual std::shared_ptr<ObjectType> get() noexcept(false)
      // This is a read-only value, could be access concurrently.
      virtual const ObjectType & get (DeserializationManager *dm=nullptr) noexcept(false) {

        // - get the latest version
        int64_t ver = this->m_pLog->getLength() - 1;
        if(ver < 0){
          throw PERSIST_EXP_EMPTY_LOG;
        }

        return this->get(ver,dm);
      };
      */

      // get number of the versions
      virtual int64_t getNumOfVersions() noexcept(false) {
        return this->m_pLog->getLength();
      };

      /* deprecated
      // get the value defined by version
      // version number >= 0
      // negtive number means how many versions we go back from the latest
      // version. -1 means the previous version.
      //virtual std::shared_ptr<ObjectType> get(const int64_t & version) noexcept(false){
      virtual const ObjectType & get(const int64_t & version, DeserializationManager* dm=nullptr) noexcept(false) {
        //TODO: use Matt's deserialization library
        int64_t ver = version;
        std::shared_ptr<ObjectType> pObj;

        if (ver < 0) {
          ver += this->m_pLog->getLength();
        }

        // - lock
        if (pthread_spin_lock(&this->m_aCache[VERSION_HASH(ver)].lck) != 0) {
          throw PERSIST_EXP_SPIN_LOCK(errno);
        }

        // - check and fill cache
         if( ! VERSION_IS_CACHED(ver) ){
          const void *raw = this->m_pLog->getEntry(ver);
          this->m_aCache[VERSION_HASH(ver)].obj.reset();
          this->m_aCache[VERSION_HASH(ver)].obj = 
            std::make_shared<ObjectType>(*ObjectType::deserialize(raw));
        }

        // - read from persistent log
        pObj = GET_CACHED_OBJ_PTR(ver);

        // - unlock
        if (pthread_spin_unlock(&this->m_aCache[VERSION_HASH(ver)].lck) != 0) {
          throw PERSIST_EXP_SPIN_UNLOCK(errno);
        }

        return pObj;
      };
      */

      // set value: this increases the version number by 1.
      virtual void set(const ObjectType &v) noexcept(false){
        auto size = bytes_size(v);
        char buf[size];
        bzero(buf,size);
        to_bytes(v,buf);
        this->m_pLog->append((void*)buf,size);
      };

#ifdef HLC_ENABLED

      //TODO
      // HLC based get/set: this feature will be added later, let's don't
      // worry about it now.
      // get the value defined by HLC time:
      virtual const ObjectType& get(const HLC &ts){ throw exp... };

      // set the value defined by HLC time:
      virtual void set(const ObjectType &v, const HLC &msg_ts)

#endif//HLC_ENABLED

      // internal _NameMaker class
      class _NameMaker{
      public:
        // Constructor
        _NameMaker() noexcept(false):
        m_sObjectTypeName(typeid(ObjectType).name()) {
          this->m_iCounter = 0;
          if (pthread_spin_init(&this->m_oLck,PTHREAD_PROCESS_SHARED) != 0) {
            throw PERSIST_EXP_SPIN_INIT(errno);
          }
        }

        // Destructor
        virtual ~_NameMaker() noexcept(true) {
          pthread_spin_destroy(&this->m_oLck);
        }

        // guess a name
        std::shared_ptr<const char *> make() noexcept(false) {
          int cnt;
          if (pthread_spin_lock(&this->m_oLck) != 0) {
            throw PERSIST_EXP_SPIN_LOCK(errno);
          }
          cnt = this->m_iCounter++;
          if (pthread_spin_unlock(&this->m_oLck) != 0) {
            throw PERSIST_EXP_SPIN_UNLOCK(errno);
          }
          char * buf = (char *)malloc((strlen(this->m_sObjectTypeName)+13)/8*8);
          sprintf(buf,"%s-%d",this->m_sObjectTypeName,cnt);
          return std::make_shared<const char *>((const char*)buf);
        }

      private:
        int m_iCounter;
        const char *m_sObjectTypeName;
        pthread_spinlock_t m_oLck;
      };

  private:
      // PersistLog
      PersistLog * m_pLog;

      /* deprecated
      // Version Cache
      struct {
        std::shared_ptr<ObjectType>     obj;
        int64_t                         ver;
        pthread_spinlock_t              lck;
      } m_aCache[MAX_NUM_CACHED_VERSION];
      */

      // get the static name maker.
      static _NameMaker & getNameMaker();
  };

  // How many times the constructor was called.
  template <typename ObjectType, StorageType storageType>
  typename Persistent<ObjectType,storageType>::_NameMaker & 
    Persistent<ObjectType,storageType>::getNameMaker() noexcept(false) {
    static Persistent<ObjectType,storageType>::_NameMaker nameMaker;
    return nameMaker;
  }
  /* use a static method instead
  template <typename ObjectType,StorageType storageType>
    typename Persistent<ObjectType,storageType>::_NameMaker Persistent<ObjectType,storageType>::s_oNameMaker;
  */

  template <typename ObjectType>
  class Volatile: public Persistent<ObjectType,ST_MEM>{
  public:
    // constructor: this will guess the objectname form ObjectType
    Volatile<ObjectType>() noexcept(false):
      Persistent<ObjectType,ST_MEM>(){
    };
    // destructor:
    virtual ~Volatile() noexcept(false){
      // do nothing
    };
  };
}


#endif//PERSIST_VAR_H
