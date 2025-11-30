#include <stddef.h>
#include "Common_Rtc.h"

#define RTCREG_WREQ_BUSYA_MASK (1u << 0)

#define CXD56_SYS_MIRROR       (0x04000000)
#define CXD56_RTC0_BASE        (CXD56_SYS_MIRROR + 0x00108000)
#define CXD56_RTC1_BASE        (CXD56_SYS_MIRROR + 0x00109000)

#define RTC_RDREQ              (0x30)
#define RTC_RDPOSTCNT          (0x34)
#define RTC_RDPRECNT           (0x38)

#define RTC_RTPOSTCNT          (0x40)
#define RTC_RTPRECNT           (0x44)

#define putreg32(v, a) (*(volatile uint32_t *) (a) = (v))
#define getreg32(a)    (*(volatile uint32_t *) (a))

static uint32_t GetRtcBaseAddress(Common_RtcChannel_e channel)
{
    switch (channel) {
        case Common_RtcChannel_0:
            return CXD56_RTC0_BASE;

        case Common_RtcChannel_1:
            return CXD56_RTC1_BASE;

        default:
            return 0;
    }
}

uint64_t Common_Rtc_GetCountUninterruptible(Common_RtcChannel_e channel)
{
    uint64_t val;

    /* The pre register is latched with reading the post rtcounter register,
     * so these registers always have to be read in the below order,
     * 1st post -> 2nd pre.
     */

    uint32_t rtc_base_addr = GetRtcBaseAddress(channel);

    val  = (uint64_t) getreg32(rtc_base_addr + RTC_RTPOSTCNT) << 15;
    val |= getreg32(rtc_base_addr + RTC_RTPRECNT);

    return val;
}

uint64_t Common_Rtc_GetCount(Common_RtcChannel_e channel)
{
    uint64_t val;
    irqstate_t flags;

    flags = spin_lock_irqsave(NULL);

    val = Common_Rtc_GetCountUninterruptible(channel);

    spin_unlock_irqrestore(NULL, flags);
    return val;
}

uint64_t Common_Rtc_GetCountByCapture(Common_RtcChannel_e channel)
{
    uint64_t val;
    uint32_t rtc_base_addr = GetRtcBaseAddress(channel);

    putreg32(0x1, rtc_base_addr + RTC_RDREQ);
    while (getreg32(rtc_base_addr + RTC_RDREQ) & RTCREG_WREQ_BUSYA_MASK) {
        // Wait until the read request is completed
    }
    val  = (uint64_t) getreg32(rtc_base_addr + RTC_RDPOSTCNT) << 15;
    val |= getreg32(rtc_base_addr + RTC_RDPRECNT);

    return val;
}
