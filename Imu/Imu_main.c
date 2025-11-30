/****************************************************************************
* examples/cxd5602pwbimu/cxd5602pwbimu_main.c
*
*   Copyright 2025 Sony Semiconductor Solutions Corporation
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in
*    the documentation and/or other materials provided with the
*    distribution.
* 3. Neither the name of Sony Semiconductor Solutions Corporation nor
*    the names of its contributors may be used to endorse or promote
*    products derived from this software without specific prior written
*    permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
* FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
* COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
* BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
* OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
* AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
* ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*
****************************************************************************/

/****************************************************************************
* Included Files
****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include <arch/board/board.h>
#include <asmp/mpshm.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include <nuttx/sensors/cxd5602pwbimu.h>

#include "Imu_Logging.h"
#include "Logging_public.h"

/****************************************************************************
* Pre-processor Definitions
****************************************************************************/

#define CXD5602PWBIMU_DEVPATH "/dev/imu0"
#define IMU_LOG_BUFFER_SIZE   (128 * 1024) // 128 KiB
#define IMU_RECORD_NUM                                                    \
        (IMU_LOG_BUFFER_SIZE - sizeof(LogHeader_t) - sizeof(LogFooter_t)) \
        / sizeof(cxd5602pwbimu_data_t)

#define itemsof(a) (sizeof(a) / sizeof(a[0]))

typedef struct tagImuRecordHeader_t {
    struct timespec startTime;
    uint32_t        reserved[6];
} ImuRecordHeader_t;

typedef struct tagImuRecord_t {
    LogHeader_t          header;
    cxd5602pwbimu_data_t body[IMU_RECORD_NUM];
    LogFooter_t          footer;
} ImuRecord_t;
static_assert(sizeof(ImuRecord_t) == IMU_LOG_BUFFER_SIZE, "ImuRecord_t size mismatch");

static ImuRecord_t buffer[4];

/****************************************************************************
* Private values
****************************************************************************/

/****************************************************************************
* Private Functions
****************************************************************************/

static int start_sensing(int fd, int rate, int adrange, int gdrange, int nfifos)
{
    cxd5602pwbimu_range_t range;
    int ret;

    /*
     * Set sampling rate. Available values (Hz) are below.
     *
     * 15 (default), 30, 60, 120, 240, 480, 960, 1920
     */

    ret = ioctl(fd, SNIOC_SSAMPRATE, rate);
    if (ret) {
        printf("ERROR: Set sampling rate failed. %d\n", errno);
        return 1;
    }

    /*
     * Set dynamic ranges for accelerometer and gyroscope.
     * Available values are below.
     *
     * accel: 2 (default), 4, 8, 16
     * gyro: 125 (default), 250, 500, 1000, 2000, 4000
     */

    range.accel = adrange;
    range.gyro  = gdrange;
    ret         = ioctl(fd, SNIOC_SDRANGE, (unsigned long) (uintptr_t) &range);
    if (ret) {
        printf("ERROR: Set dynamic range failed. %d\n", errno);
        return 1;
    }

    /*
     * Set hardware FIFO threshold.
     * Increasing this value will reduce the frequency with which data is
     * received.
     */

    ret = ioctl(fd, SNIOC_SFIFOTHRESH, nfifos);
    if (ret) {
        printf("ERROR: Set sampling rate failed. %d\n", errno);
        return 1;
    }

    /*
     * Start sensing, user can not change the all of configurations.
     */

    ret = ioctl(fd, SNIOC_ENABLE, 1);
    if (ret) {
        printf("ERROR: Enable failed. %d\n", errno);
        return 1;
    }

    return 0;
} /* start_sensing */

/****************************************************************************
* Public Functions
****************************************************************************/

/****************************************************************************
* sensor_main
****************************************************************************/

int main(int argc, FAR char* argv[])
{
    Imu_Logging_Run();
    return 0;
}
