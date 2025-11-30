#include "Battery_Logging.h"

#include <aio.h>
#include <assert.h>
#include <debug.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arch/chip/adc.h>
#include <arch/chip/scu.h>

#include "Common_DebugPrint.h"
#include "Logging_Buffer_public.h"
#include "Logging_public.h"

#define BATTERY_SENSE      "/dev/lpadc0"

#define BUFFER_NUM         (4)
#define BUFFER_SIZE        (16 * 1024)
#define BATTERY_RECORD_NUM ((BUFFER_SIZE - sizeof(LogHeader_t) - sizeof(LogFooter_t)) / sizeof(uint16_t))

#define getreg32(a) (*(volatile uint32_t *) (a))

typedef struct tagBatteryLogBuffer_t {
    LogHeader_t header;
    uint16_t    body[BATTERY_RECORD_NUM];
    LogFooter_t footer;
} BatteryLogBuffer_t;
static_assert(sizeof(BatteryLogBuffer_t) == BUFFER_SIZE, "BatteryLogBuffer_t size mismatch");

typedef struct tagBatteryLogging_t {
    uint32_t      seqId;
    LoggingDesc_t logdesc;
} BatteryLogging_t;

static BatteryLogBuffer_t batteryLogging_buffer[BUFFER_NUM];
static BatteryLogging_t batteryLogging_instance;

static BatteryLogging_t* GetInstance(void)
{
    return &batteryLogging_instance;
}

void Battery_Logging_Run(void)
{
    BatteryLogging_t* self = GetInstance();
    mqd_t mq = Logging_OpenQueue(false);

    self->seqId = 0;
    int errval = 0;

    int fd = open(BATTERY_SENSE, O_RDONLY);
    if (fd < 0) {
        printf("open %s failed: %d\n", BATTERY_SENSE, errno);
        return 1;
    }

    /* SCU FIFO overwrite */

    int ret = ioctl(fd, SCUIOC_SETFIFOMODE, 1);
    if (ret < 0) {
        errval = errno;
        printf("ioctl(SETFIFOMODE) failed: %d\n", errval);
        close(fd);
        return 2;
    }

    /* Start A/D conversion */

    ret = ioctl(fd, ANIOC_CXD56_START, 0);
    if (ret < 0) {
        errval = errno;
        printf("ioctl(START) failed: %d\n", errval);
        close(fd);
        return 2;
    }

    uint32_t seqId = 0;
    while (true) {
        BatteryLogBuffer_t* buff = &batteryLogging_buffer[seqId % BUFFER_NUM];
        Logging_Buffer_Init(&self->logdesc, LoggingUser_POWER, seqId, buff, sizeof(BatteryLogBuffer_t));
        seqId++;
        while (true) {
            /* read data */
            uint16_t* val       = Logging_Buffer_GetNextPos(&self->logdesc);
            uint32_t remainSize = Logging_Buffer_GetRemainingSize(&self->logdesc);
            if (remainSize < sizeof(uint16_t)) {
                break;
            }

            ssize_t nbytes = read(fd, val, remainSize);
            up_mdelay(100);

            if (nbytes < 0 || nbytes & 1) {
                errval = errno;
                PRINT_ERROR("read failed:%d", errval);
                break;
            } else if (0 < nbytes) {
                PRINT_DEBUG("LPADC FIFO read %d %x", val[0] >> 6, getreg32(0x0418DC08));
                // for (int i = 0; i < nbytes / sizeof(uint16_t); ++i) {
                // PRINT_DEBUG("Read value[%d]: %d", i, val[i]);
                // }
                Logging_Buffer_Update(&self->logdesc, nbytes);
            }
        }
        if (errval != 0) {
            break;
        }
        /* Finalize log buffer */
        Logging_Buffer_Finalize(&self->logdesc);
        LoggingDesc_t desc = { 0 };
        desc.type = LoggingType_WRITE;
        desc.user = LoggingUser_POWER;
        desc.ptr  = buff;
        desc.size = sizeof(BatteryLogBuffer_t);
        PRINT_DEBUG("Writing data: type=%x user=%x ptr=%x size=%x callback=%p\n", desc.type, desc.user,
            desc.ptr, desc.size, desc.callback);

        Logging_SendQueue(mq, &desc);
    } /* Battery_Logging_Run */

    /* Stop A/D conversion */

    ret = ioctl(fd, ANIOC_CXD56_STOP, 0);
    if (ret < 0) {
        int errcode = errno;
        PRINT_ERROR("ioctl(STOP) failed: %d\n", errcode);
    }

    close(fd);

    return 0;
} /* Battery_Logging_Run */
