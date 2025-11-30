#ifndef COMMON_RTC_H
#define COMMON_RTC_H

#include <stdint.h>

typedef enum tagCommon_RtcChannel_e {
    Common_RtcChannel_0 = 0,
    Common_RtcChannel_1,
} Common_RtcChannel_e;

uint64_t Common_Rtc_GetCountUninterruptible(Common_RtcChannel_e channel);
uint64_t Common_Rtc_GetCount(Common_RtcChannel_e channel);

#endif /* COMMON_RTC_H */
