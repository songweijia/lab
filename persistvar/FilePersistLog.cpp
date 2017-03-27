#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include "FilePersistLog.hpp"

using namespace std;

namespace ns_persistent{

  /////////////////////////
  // internal structures //
  /////////////////////////

  // meta file header format
  typedef union meta_header {
    struct {
      int64_t eno;      // number of entry
      uint64_t ofst;    // next available offset in Data File.
    } fields;
    uint8_t bytes[32];
  } MetaHeader;

  // log entry format
  typedef union log_entry {
    struct {
      uint64_t dlen;    // length of the data
      uint64_t ofst;	// offset of the data
      HLC hlc;          // HLC clock of the data
    } fields;
    uint8_t bytes[32];
  } LogEntry;

  // helpers
  #define META_HEADER ((MetaHeader*)this->m_pMeta)
  #define LOG_ENTRY_ARRAY       ((LogEntry*)((uint8_t*)this->m_pMeta + sizeof(MetaHeader)))
  #define NEXT_LOG_ENTRY        (LOG_ENTRY_ARRAY + META_HEADER->fields.eno)
  #define CURR_LOG_ENTRY        (NEXT_LOG_ENTRY - 1)
  #define NEXT_DATA             ((void *)((uint64_t)this->m_pData + META_HEADER->fields.ofst))
  #define PAGE_SIZE             (getpagesize())
  #define ALIGN_TO_PAGE(x)      ((void *)(((uint64_t)(x))-((uint64_t)(x))%PAGE_SIZE))
  #define LOG_ENTRY_DATA(e)     ((void *)(((uint64_t)this->m_pData)+(e)->fields.ofst))

  // size limit for the meta and data file
  #define METAFILE_SIZE (0x1ull<<30)
  #define DATAFILE_SIZE (0x1ull<<40)

  // lock macro
  #define FPL_WRLOCK \
  do { \
    if (pthread_rwlock_wrlock(&this->m_rwlock) != 0) { \
      throw PERSIST_EXP_RWLOCK_WRLOCK(errno); \
    } \
  } while (0)

  #define FPL_RDLOCK \
  do { \
    if (pthread_rwlock_rdlock(&this->m_rwlock) != 0) { \
      throw PERSIST_EXP_RWLOCK_WRLOCK(errno); \
    } \
  } while (0)

  #define FPL_UNLOCK \
  do { \
    if (pthread_rwlock_unlock(&this->m_rwlock) != 0) { \
      throw PERSIST_EXP_RWLOCK_UNLOCK(errno); \
    } \
  } while (0)

  // verify the existence of a folder
  // Check if directory exists or not. Create it on absence.
  // return error if creating failed
  static void checkOrCreateDir(const string & dirPath) noexcept(false);

  // verify the existence of the meta file
  static bool checkOrCreateMetaFile(const string & metaFile) noexcept(false);

  // verify the existence of the data file
  static bool checkOrCreateDataFile(const string & dataFile) noexcept(false);

  ////////////////////////
  // visible to outside //
  ////////////////////////

  FilePersistLog::FilePersistLog(const string &name, const string &dataPath) 
  noexcept(false) : PersistLog(name),
  m_sDataPath(dataPath){
    this->m_iMetaFileDesc = -1;
    this->m_iDataFileDesc = -1;
    this->m_pMeta = MAP_FAILED;
    this->m_pData = MAP_FAILED;
    if (pthread_rwlock_init(&this->m_rwlock,NULL) != 0) {
      throw PERSIST_EXP_RWLOCK_INIT(errno);
    }
    load();
  }

  void FilePersistLog::load()
  noexcept(false){
    // STEP 0: check if data path exists
    checkOrCreateDir(this->m_sDataPath);
    // STEP 1: get file name
    const string metaFile = this->m_sDataPath + "/" + this->m_sName + "." + METAFILE_SUFFIX;
    const string dataFile = this->m_sDataPath + "/" + this->m_sName + "." + DATAFILE_SUFFIX;
    bool bCreate = checkOrCreateMetaFile(metaFile);
    checkOrCreateDataFile(dataFile);
    // STEP 2: open files
    this->m_iMetaFileDesc = open(metaFile.c_str(),O_RDWR);
    if (this->m_iMetaFileDesc == -1) {
      throw PERSIST_EXP_OPEN_FILE(errno);
    }
    this->m_iDataFileDesc = open(dataFile.c_str(),O_RDWR);
    if (this->m_iDataFileDesc == -1) {
      throw PERSIST_EXP_OPEN_FILE(errno);
    }
    // STEP 3: mmap to memory
    this->m_pMeta = mmap(NULL,METAFILE_SIZE,PROT_READ|PROT_WRITE,MAP_SHARED,this->m_iMetaFileDesc,0);
    if (this->m_pMeta == MAP_FAILED) {
      throw PERSIST_EXP_MMAP_FILE(errno);
    }
    this->m_pData = mmap(NULL,DATAFILE_SIZE,PROT_READ|PROT_WRITE,MAP_SHARED,this->m_iDataFileDesc,0);
    if (this->m_pData == MAP_FAILED) {
      throw PERSIST_EXP_MMAP_FILE(errno);
    }
    // STEP 4: initialize the header for new created Metafile
    if (bCreate) {
      MetaHeader *pmh = (MetaHeader *)this->m_pMeta;
      pmh->fields.eno = 0ull;
      pmh->fields.ofst = 0ull;
      FPL_WRLOCK;
      if (msync(this->m_pMeta,sizeof(MetaHeader),MS_SYNC) != 0) {
        FPL_UNLOCK;
        throw PERSIST_EXP_MSYNC(errno);
      }
      FPL_UNLOCK;
    }
    // STEP 5: update m_hlcLE with the latest event
    if (META_HEADER->fields.eno >0) {
      if (this->m_hlcLE < CURR_LOG_ENTRY->fields.hlc){
        this->m_hlcLE = CURR_LOG_ENTRY->fields.hlc;
      }
    }
  }

  FilePersistLog::~FilePersistLog()
  noexcept(true){
    pthread_rwlock_destroy(&this->m_rwlock);
    if (this->m_pData != MAP_FAILED){
      munmap(m_pData,DATAFILE_SIZE);
    }
    if (this->m_pMeta != MAP_FAILED){
      munmap(m_pMeta,METAFILE_SIZE);
    }
    if (this->m_iMetaFileDesc != -1){
      close(this->m_iMetaFileDesc);
    }
    if (this->m_iDataFileDesc != -1){
      close(this->m_iDataFileDesc);
    }
  }

  void FilePersistLog::append(const void *pdat, uint64_t size, const HLC & mhlc)
  noexcept(false) {
    FPL_WRLOCK;

    // copy and flush data
    memcpy(NEXT_DATA,pdat,size);
    if (msync(ALIGN_TO_PAGE(NEXT_DATA),size + (((uint64_t)NEXT_DATA) % PAGE_SIZE),MS_SYNC) != 0) {
      FPL_UNLOCK;
      throw PERSIST_EXP_MSYNC(errno);
    }

    // fill and flush log entry
    NEXT_LOG_ENTRY->fields.dlen = size;
    NEXT_LOG_ENTRY->fields.ofst = META_HEADER->fields.ofst;
    NEXT_LOG_ENTRY->fields.hlc = (mhlc > this->m_hlcLE)?mhlc:this->m_hlcLE;
    NEXT_LOG_ENTRY->fields.hlc.tick();
    this->m_hlcLE = NEXT_LOG_ENTRY->fields.hlc;
    if (msync(ALIGN_TO_PAGE(NEXT_LOG_ENTRY), 
        sizeof(LogEntry) + (((uint64_t)NEXT_LOG_ENTRY) % PAGE_SIZE),MS_SYNC) != 0) {
      FPL_UNLOCK;
      throw PERSIST_EXP_MSYNC(errno);
    }

    // update and flush meta header
    META_HEADER->fields.eno ++;
    META_HEADER->fields.ofst += size;
    if (msync(this->m_pMeta,sizeof(MetaHeader),MS_SYNC) != 0) {
      FPL_UNLOCK;
      throw PERSIST_EXP_MSYNC(errno);
    }
    
    FPL_UNLOCK;
  }

  int64_t FilePersistLog::getLength()
  noexcept(false) {

    FPL_RDLOCK;
    int64_t len = META_HEADER->fields.eno;
    FPL_UNLOCK;

    return len;
  }

  const void * FilePersistLog::getEntry(int64_t eidx)
  noexcept(false) {

    FPL_RDLOCK;
    if (META_HEADER->fields.eno < eidx || (eidx + (int64_t)META_HEADER->fields.eno) < 0 ) {
      FPL_UNLOCK;
      throw PERSIST_EXP_INV_ENTRY_IDX(eidx);
    }
    int64_t ridx = (eidx < 0)?((int64_t)META_HEADER->fields.eno + eidx):eidx;
    FPL_UNLOCK;

    return LOG_ENTRY_DATA(LOG_ENTRY_ARRAY + ridx);
  }

  // find the log entry by HLC timestmap:rhlc
  static LogEntry * binarySearch(const HLC &rhlc, LogEntry *logArr, int64_t len) {
    if (len <= 0) {
      return nullptr;
    }
    int64_t head = 0, tail = len - 1;
    int64_t pivot = len/2;
    while (head <= tail) {
      if ((logArr+pivot)->fields.hlc == rhlc){
        break; // found
      } else if ((logArr+pivot)->fields.hlc < rhlc){
        if (pivot + 1 >= len) {
          break; // found
        } else if ((logArr+pivot)->fields.hlc > rhlc) {
          break; // found
        } else { // search right
          head = pivot + 1;
        }
      } else { // search left
        tail = pivot - 1;
        if (head > tail) {
          return nullptr;
        }
      }
    }
    return logArr + pivot;
  }

  const void * FilePersistLog::getEntry(const HLC &rhlc)
  noexcept(false) {

    uint64_t now = read_rtc_us();
    LogEntry * ple = nullptr;

    FPL_RDLOCK;

    if (this->m_hlcLE < rhlc) {
      if (now <= rhlc.m_rtc_us) {
        // read future
        FPL_UNLOCK;
        throw PERSIST_EXP_READ_FUTURE;
      } else {
        // update m_hlcLE to read clock
        FPL_UNLOCK;
        FPL_WRLOCK;
        this->m_hlcLE = rhlc;
        FPL_UNLOCK;
        FPL_RDLOCK;
      }
    }

    //binary search
    ple = binarySearch(rhlc,LOG_ENTRY_ARRAY,META_HEADER->fields.eno);

    FPL_UNLOCK;

    // no object exists before the requested timestamp.
    if (ple == nullptr){
      return nullptr;
    }

    return LOG_ENTRY_DATA(ple);
  }

  /*** -  deprecated
  const void * FilePersistLog::getEntry()
  noexcept(false) {
    void * ent = NULL;

    FPL_RDLOCK;
    if (META_HEADER->fields.eno > 0) {
      ent = LOG_ENTRY_DATA(LOG_ENTRY_ARRAY + META_HEADER->fields.eno - 1);
    }
    FPL_UNLOCK;

    return ent;
  }
  ***/

  //////////////////////////
  // invisible to outside //
  //////////////////////////
  void checkOrCreateDir(const string & dirPath) 
  noexcept(false) {
    struct stat sb;
    if (stat(dirPath.c_str(),&sb) == 0) {
      if (! S_ISDIR(sb.st_mode)) {
        throw PERSIST_EXP_INV_PATH;
      }
    } else { 
      // create it
      if (mkdir(dirPath.c_str(),0700) != 0) {
        throw PERSIST_EXP_CREATE_PATH(errno);
      }
    }
  }

  bool checkOrCreateFileWithSize(const string & file, uint64_t size)
  noexcept(false) {
    bool bCreate = false;
    struct stat sb;
    int fd;

    if (stat(file.c_str(),&sb) == 0) {
      if(! S_ISREG(sb.st_mode)) {
        throw PERSIST_EXP_INV_FILE;
      }
    } else {
      // create it
      bCreate = true;
    }

    fd = open(file.c_str(), O_RDWR|O_CREAT,S_IWUSR|S_IRUSR|S_IRGRP|S_IWGRP|S_IROTH);
    if (fd < 0) {
      throw PERSIST_EXP_CREATE_FILE(errno);
    }

    if (ftruncate(fd,size) != 0) {
      throw PERSIST_EXP_TRUNCATE_FILE(errno);
    }
    close(fd);
    return bCreate;
  }

  bool checkOrCreateMetaFile(const string & metaFile)
  noexcept(false) {
    return checkOrCreateFileWithSize(metaFile,METAFILE_SIZE);
  }

  bool checkOrCreateDataFile(const string & dataFile)
  noexcept(false) {
    return checkOrCreateFileWithSize(dataFile,DATAFILE_SIZE);
  }
}
