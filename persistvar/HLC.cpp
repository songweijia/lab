#include <time.h>
#include <errno.h>
#include "HLC.hpp"

// return microsecond
static uint64_t get_rtc_us () 
  noexcept(false) {
  struct timespec tp;
  if ( clock_gettime(CLOCK_REALTIME,&tp) != 0 ) {
    throw HLC_EXP_READ_RTC(errno);
  } else {
    return (uint64_t)tp.tv_sec*1000000 + tp.tv_nsec/1000;
  }
}

HLC::HLC () 
  noexcept(false) {
  this->m_rtc_us = get_rtc_us();
  this->m_logic = 0L;
  if (pthread_spin_init(&this->m_oLck,PTHREAD_PROCESS_SHARED) != 0) {
    throw HLC_SPINLOCK_INIT(errno);
  }
}

HLC::~HLC ()
  noexcept(false) {
  if (pthread_spin_destroy(&this->m_oLck) != 0) {
    throw HLC_SPINLOCK_DESTROY(errno);
  }
}

// TODO:READ/WRITE LOCK????
#define HLC_LOCK \
  if (pthread_spin_lock(&this->m_oLck) != 0) { \
    throw HLC_SPINLOCK_LOCK(errno); \
  }
#define HLC_UNLOCK \
  if (pthread_spin_unlock(&this->m_oLck) != 0) { \
    throw HLC_SPINLOCK_UNLOCK(errno); \
  }

void HLC::tick ()
  noexcept(false) {
  HLC_LOCK
  
  uint64_t rtc = get_rtc_us();
  if ( rtc == this->m_rtc_us ) {
    this->m_logic ++;
  } else {
    this->m_rtc_us = rtc;
    this->m_logic = 0ull;
  }

  HLC_UNLOCK
}
