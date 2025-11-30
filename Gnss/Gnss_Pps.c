#include "Gnss_Pps.h"

#include <arch/board/board.h>
#include <arch/chip/gnss.h>
#include <errno.h>
#include <fcntl.h>
#include <nuttx/config.h>
#include <poll.h>
#include <stdio.h>
#include <sys/ioctl.h>

#include <arch/chip/irq.h>
#include <nuttx/arch.h>
#include <nuttx/irq.h>

#include "Common_DebugPrint.h"
#include "Common_Rtc.h"
#include "Logging_Buffer_public.h"
#include "Logging_public.h"

#define CXD56_INTC_BASE   0xe0045000
#define CXD56_INTC_ENABLE (CXD56_INTC_BASE + 0x1C)
#define CXD56_INTC_INVERT (CXD56_INTC_BASE + 0x2C)
#define putreg32(v, a) (*(volatile uint32_t *) (a) = (v))
#define getreg32(a)    (*(volatile uint32_t *) (a))

typedef enum tagIntEdge_e {
    IntEdge_RISING = 0,
    IntEdge_FALLING,
} IntEdge_e;

typedef struct tagGnss_Pps_t {
    IntEdge_e edge;
    uint32_t  head;
    uint32_t  tail;
    sem_t     smph;
    uint64_t  times[32];
    uint64_t  count;
    time_t    time;
} Gnss_Pps_t;

static Gnss_Pps_t gnssPps_instance = {
    .edge  = IntEdge_RISING,
    .head  = 0,
    .tail  = 0,
    .times = { 0 },
};

static Gnss_Pps_t* GetGnssPpsInstance(void)
{
    return &gnssPps_instance;
}

static int pps_handler(int irq, void* context, void* arg)
{
    Gnss_Pps_t* self = GetGnssPpsInstance();
    uint32_t pin     = (uint32_t) (uintptr_t) arg;

    if (self->edge == IntEdge_RISING) {
        self->times[self->head] = Common_Rtc_GetCountByCapture(Common_RtcChannel_0);
        self->head++;
        PRINT_DEBUG("PPS: %d %llu", pin, self->times[self->head]);
        self->head %= 32;
    }

    self->edge ^= 1;
    cxd56_gpioint_invert(pin);

    // irqstate_t flags;
    // uint32_t val;

    // flags = spin_lock_irqsave(NULL);

    // val  = getreg32(CXD56_INTC_INVERT);
    // val ^= 1 << 29;
    // putreg32(val, CXD56_INTC_INVERT);

    // spin_unlock_irqrestore(NULL, flags);
    return 0;
}

int Gnss_Pps_Init(void)
{
    Gnss_Pps_t* self = GetGnssPpsInstance();

    sem_init(&self->smph, 0, 0);
    sem_setprotocol(&self->smph, SEM_PRIO_NONE);

    // irq_attach(CXD56_IRQ_GPS_OR, pps_handler, NULL);
    // up_enable_irq(CXD56_IRQ_GPS_OR);
    // board_gpio_config(uint32_t pin, int mode, bool input, bool drive, int pull);
    // printf("GNSS PPS initialized %x %x %x\n", getreg32(0x04100818), getreg32(0x4102014), getreg32(0x041007C0));
    // board_gpio_config(PIN_GNSS_1PPS_OUT, 1, true, false, PIN_FLOAT);
    printf("GNSS PPS initialized %x %x %x\n", getreg32(0x04100818), getreg32(0x4102014), getreg32(0x041007C0));
    board_gpio_intconfig(PIN_GNSS_1PPS_OUT, INT_HIGH_LEVEL, false, pps_handler);
    printf("GNSS PPS initialized %x %x %x\n", getreg32(0x04100818), getreg32(0x4102014), getreg32(0x041007C0));
    board_gpio_int(PIN_GNSS_1PPS_OUT, true);

    printf("GNSS PPS initialized %x %x %x\n", getreg32(0x04100818), getreg32(0x4102014), getreg32(0x041007C0));
    // putreg32(0x1010100, 0x04100818);
    // putreg32(0x10100, 0x4102014);
    // uint32_t reg = getreg32(0x041007C0);
    // reg &= ~(0b11 << 8);
    // reg |= (1 << 8); // Set to 1PPS mode
    // putreg32(reg, 0x041007C0);
    // printf("GNSS PPS initialized %x %x %x\n", getreg32(0x04100818), getreg32(0x4102014), getreg32(0x041007C0));
    // board_gpio_config(PIN_HIF_IRQ_OUT, 0, true, false, PIN_PULLDOWN);
    // board_gpio_intconfig(PIN_HIF_IRQ_OUT, INT_HIGH_LEVEL, false, pps_handler);
}

int Gnss_Pps_SetTime(time_t time)
{
    Gnss_Pps_t* self = GetGnssPpsInstance();
    irqstate_t flags;

    self->count = Common_Rtc_GetCount(Common_RtcChannel_0);
    self->time  = time;
    return 0;
}

int Gnss_Pps_main(void)
{
    Gnss_Pps_t* self = GetGnssPpsInstance();
    int ret;

    while (true) {
        ret = sem_wait(&self->smph);
        if (ret < 0) {
            return ret;
        }

        if (self->edge == IntEdge_RISING) {
            uint64_t count = self->times[self->tail];
            self->tail++;
            self->tail %= 32;
        }
    }

    return OK;
}
