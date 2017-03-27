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

  // size limit for the meta and data file
  #define METAFILE_SIZE (0x1ull<<30)
  #define DATAFILE_SIZE (0x1ull<<40)

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
  noexcept(false) : MemLog(name),
  m_sDataPath(dataPath){
    this->m_iMetaFileDesc = -1;
    this->m_iDataFileDesc = -1;
    free(this->m_pMeta); // release the memory allocated in MemLog
    free(this->m_pData); // release the memory allocated in MemLog
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
      ML_WRLOCK;
      if (msync(this->m_pMeta,sizeof(MetaHeader),MS_SYNC) != 0) {
        ML_UNLOCK;
        throw PERSIST_EXP_MSYNC(errno);
      }
      ML_UNLOCK;
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
    this->m_pData = nullptr; // prevent ~MemLog() destructor to release it again.
    if (this->m_pMeta != MAP_FAILED){
      munmap(m_pMeta,METAFILE_SIZE);
    }
    this->m_pMeta = nullptr; // prevent ~MemLog() destructor to release it again.
    if (this->m_iMetaFileDesc != -1){
      close(this->m_iMetaFileDesc);
    }
    if (this->m_iDataFileDesc != -1){
      close(this->m_iDataFileDesc);
    }
  }

  void FilePersistLog::append(const void *pdat, uint64_t size, const HLC & mhlc)
  noexcept(false) {
    ML_WRLOCK;

    // copy and flush data
    memcpy(NEXT_DATA,pdat,size);
    if (msync(ALIGN_TO_PAGE(NEXT_DATA),size + (((uint64_t)NEXT_DATA) % PAGE_SIZE),MS_SYNC) != 0) {
      ML_UNLOCK;
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
      ML_UNLOCK;
      throw PERSIST_EXP_MSYNC(errno);
    }

    // update and flush meta header
    META_HEADER->fields.eno ++;
    META_HEADER->fields.ofst += size;
    if (msync(this->m_pMeta,sizeof(MetaHeader),MS_SYNC) != 0) {
      ML_UNLOCK;
      throw PERSIST_EXP_MSYNC(errno);
    }
    
    ML_UNLOCK;
  }

  /*** -  deprecated
  const void * FilePersistLog::getEntry()
  noexcept(false) {
    void * ent = NULL;

    ML_RDLOCK;
    if (META_HEADER->fields.eno > 0) {
      ent = LOG_ENTRY_DATA(LOG_ENTRY_ARRAY + META_HEADER->fields.eno - 1);
    }
    ML_UNLOCK;

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
