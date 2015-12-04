#include "DateTruncate.h"
#include "ExtractFromTime.h"

#ifndef __CUDACC__
#include <glog/logging.h>
#endif

extern "C" __attribute__((noinline))
#ifdef __CUDACC__
__device__
#endif
    time_t create_epoch(int year) {
  // 1972 was a leap year
  int year_offset = (year - 1972);
  // get basic time not taking leap days into account for 1972
  time_t time_calc = 2 * 365 * SECSPERDAY;
  time_calc += year_offset * 365 * SECSPERDAY;
  // now calculate number of leap years
  // this math doesn't work due to century rule
  // int leap_years = (year_offset >0) ? (year_offset-1)/4 +1  : year_offset/4;
  // handle weird century case
  int leap_years = 0;
  int year_count;
  if (year_offset > 0) {
    for (year_count = 1972; year_count < year; year_count += 4) {
      if (year_count % 100 == 0 && year_count % 400 != 0)
        continue;
      leap_years++;
    }
  } else {
    int year_count;
    for (year_count = 1968; year_count >= year; year_count -= 4) {
      if (year_count % 100 == 0 && year_count % 400 != 0)
        continue;
      leap_years--;
    }
  }
  time_calc += leap_years * SECSPERDAY;
  return time_calc;
}

/*
 * @brief support the SQL DATE_TRUNC function
 */
extern "C" __attribute__((noinline))
#ifdef __CUDACC__
__device__
#endif
    time_t DateTruncate(DatetruncField field, time_t timeval) {

  switch (field) {
    case dtMICROSECOND:
    case dtMILLISECOND:
    case dtSECOND:
      /* this is the limit of current granularity*/
      return timeval;
    case dtMINUTE: {
      time_t ret = (uint64_t)(timeval / SECSPERMIN) * SECSPERMIN;
      // in the case of a negative time we still want to push down so need to push one more
      if (ret < 0)
        ret -= SECSPERMIN;
      return ret;
    }
    case dtHOUR: {
      time_t ret = (uint64_t)(timeval / SECSPERHOUR) * SECSPERHOUR;
      // in the case of a negative time we still want to push down so need to push one more
      if (ret < 0)
        ret -= SECSPERHOUR;
      return ret;
    }
    case dtDAY: {
      time_t ret = (uint64_t)(timeval / SECSPERDAY) * SECSPERDAY;
      // in the case of a negative time we still want to push down so need to push one more
      if (ret < 0)
        ret -= SECSPERDAY;
      return ret;
    }
    case dtWEEK: {
      time_t day = (uint64_t)(timeval / SECSPERDAY) * SECSPERDAY;
      if (day < 0)
        day -= SECSPERDAY;
      int dow = extract_dow(&day);
      return day - (dow * SECSPERDAY);
    }
    default:
      break;
  }

  // use ExtractFromTime functions where available
  // have to do some extra work for these ones
  tm tm_struct;
  gmtime_r_newlib(&timeval, &tm_struct);
  switch (field) {
    case dtMONTH: {
      // clear the time
      time_t day = (uint64_t)(timeval / SECSPERDAY) * SECSPERDAY;
      if (day < 0)
        day -= SECSPERDAY;
      // calculate the day of month offset
      int dom = tm_struct.tm_mday;
      return day - ((dom - 1) * SECSPERDAY);
    }
    case dtYEAR: {
      // clear the time
      time_t day = (uint64_t)(timeval / SECSPERDAY) * SECSPERDAY;
      if (day < 0)
        day -= SECSPERDAY;
      // calculate the day of year offset
      int doy = tm_struct.tm_yday;
      return day - ((doy)*SECSPERDAY);
    }
    case dtDECADE: {
      int year = tm_struct.tm_year + YEAR_BASE;
      int decade_start = ((year - 1) / 10) * 10 + 1;
      return create_epoch(decade_start);
    }
    case dtCENTURY: {
      int year = tm_struct.tm_year + YEAR_BASE;
      int century_start = ((year - 1) / 100) * 100 + 1;
      return create_epoch(century_start);
    }
    case dtMILLENNIUM: {
      int year = tm_struct.tm_year + YEAR_BASE;
      int millennium_start = ((year - 1) / 1000) * 1000 + 1;
      return create_epoch(millennium_start);
    }
    default:
#ifdef __CUDACC__
      return -1;
#else
      CHECK(false);
#endif
  }
}

extern "C"
#ifdef __CUDACC__
    __device__
#endif
        time_t DateTruncateNullable(DatetruncField field, time_t timeval, const int64_t null_val) {
  if (timeval == null_val) {
    return null_val;
  }
  return DateTruncate(field, timeval);
}
