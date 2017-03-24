#ifndef HLC_HPP
#define HLC_HPP

#include <sys/types.h>
#include <inttypes.h>
#include <pthread.h>

class HLC{

private:
  uint64_t m_rtc_us; // real-time clock in microseconds
  uint64_t m_logic;  // logic clock
  pthread_spinlock_t m_oLck; // spinlock

public:
  // constructors
  HLC () noexcept(false); 

  // destructors
  virtual ~HLC() noexcept(false);

  // ticking method - thread safe
  virtual void tick () noexcept(false);
  virtual void tick (HLC & msgHlc) noexcept(false);

  // comparators
  virtual bool const operator > (const HLC & hlc) noexcept(true);
  virtual bool const operator < (const HLC & hlc) noexcept(true);
  virtual bool const operator == (const HLC & hlc) noexcept(true);
  virtual bool const operator >= (const HLC & hlc) noexcept(true);
  virtual bool const operator <= (const HLC & hlc) noexcept(true);
}

#define HLC_EXP(errcode,usercode) \
  ( (((errcode)&0xffffffffull)<<32) | ((usercode)&0xffffffffull) )
#define HLC_EXP_USERCODE(x) ((uint32_t)((x)&0xffffffffull))
#define HLC_EXP_READ_RTC(x)                     HLC_EXP(0,(x))
#define HLC_SPINLOCK_INIT(x)                    HLC_EXP(1,(x))
#define HLC_SPINLOCK_DESTROY(x)                 HLC_EXP(2,(x))

#endif//HLC_HPP
