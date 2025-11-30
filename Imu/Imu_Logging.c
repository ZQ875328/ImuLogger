#include "Imu_Logging.h"

#include <aio.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <nuttx/sensors/cxd5602pwbimu.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include "Logging_Buffer_public.h"
#include "Logging_public.h"
#include "PowerCtrl_public.h"

#define CXD5602PWBIMU_DEVPATH "/dev/imu0"

#define NUM_BUFFERS           (4)
#define BUFFER_SIZE           (128 * 1024)
#define IMU_RECORD_NUM        ((BUFFER_SIZE - sizeof(LogHeader_t) - sizeof(LogFooter_t)) / sizeof(cxd5602pwbimu_data_t))

typedef struct tagImuLogbuffer_t {
    LogHeader_t          header;
    cxd5602pwbimu_data_t body[IMU_RECORD_NUM];
    LogFooter_t          footer;
} ImuLogBuffer_t;
static_assert(sizeof(ImuLogBuffer_t) == BUFFER_SIZE, "ImuLogBuffer_t size mismatch");

typedef struct tagImuLogging_t {
    int                   eventFd;
    int                   shutdownHandlerId;
    uint32_t              seqId;
    Logging_Buffer_Desc_t logdesc;
} ImuLogging_t;

static ImuLogBuffer_t imuLogging_buffer[NUM_BUFFERS];
static ImuLogging_t imuLogging_instance;

static ImuLogging_t* GetInstance(void)
{
    return &imuLogging_instance;
}

static void ShutdownHandler(void)
{
    ImuLogging_t* self = GetInstance();

    int fd  = open("/var/fifo/imu_event", O_WRONLY);
    int ret = write(fd, &(uint64_t){ 1 }, sizeof(uint64_t));

    close(fd);

    printf("ShutdownHandler: write eventfd returned %d\n", ret);
}

static int SetupSensor(int fd, int rate, int adrange, int gdrange, int nfifos)
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
} /* SetupSensor */

uint32_t Imu_Logging_Run(void)
{
    // int pipefd[2];

    // pipe(pipefd);
    // printf("pipefd[0]: %d, pipefd[1]: %d\n", pipefd[0], pipefd[1]);
    mqd_t mq = Logging_OpenQueue(true);
    ImuLogging_t* self = GetInstance();

    // int efd = pipefd[1];

    // if (efd == -1) {
    //     perror("eventfd");
    //     return 1;
    // }
    // self->eventFd = efd;
    int ret = PowerCtrl_SetShutdownCallback(ShutdownHandler);

    if (ret == ERROR) {
        printf("ERROR: Failed to set shutdown callback. %d\n", ret);
        // close(efd);
        return 1;
    }
    self->shutdownHandlerId = ret;
    self->seqId = 0;

    int fd;
    struct pollfd fds[2];

    ret = mkfifo("/var/fifo/imu_event", 0666);
    printf("mkfifo ret: %d, errno: %d\n", ret, errno);
    fds[0].fd = open("/var/fifo/imu_event", O_RDWR);
    if (fds[0].fd < 0) {
        printf("ERROR: Failed to create event FIFO. %d\n", errno);
        return 1;
    }
    fds[0].events = POLLIN;
    printf("Event FIFO created and opened: %d\n", fds[0].fd);
    // struct timespec start, now, delta;
    // cxd5602pwbimu_data_t *outbuf = NULL;

    /* Sensing parameters, see start sensing function. */

    const int samplerate = 1920;
    const int adrange    = 16;
    const int gdrange    = 1000;
    const int nfifos     = 4;

    fd = open(CXD5602PWBIMU_DEVPATH, O_RDONLY);
    if (fd < 0) {
        printf("ERROR: Device %s open failure. fd: %d error:%d\n", CXD5602PWBIMU_DEVPATH, fd, errno);
        return 1;
    }
    printf("Opened device %s successfully. fd: %d\n", CXD5602PWBIMU_DEVPATH, fd);

    // outbuf = (cxd5602pwbimu_data_t *)malloc(sizeof(cxd5602pwbimu_data_t) * samplerate * 10);
    // if (outbuf == NULL)
    // {
    //     printf("ERROR: Output buffer allocation failed.\n");
    //     return 1;
    // }

    fds[1].fd     = fd;
    fds[1].events = POLLIN;

    ret = SetupSensor(fd, samplerate, adrange, gdrange, nfifos);
    if (ret) {
        close(fd);
        return ret;
    }

    uint32_t seqId = 0;
    int errval     = 0;

    bool isRunning = true;
    while (isRunning) {
        ImuLogBuffer_t* buff = &imuLogging_buffer[seqId % NUM_BUFFERS];
        for (uint32_t i = 0; i < IMU_RECORD_NUM; ++i) {
            ssize_t ret = poll(fds, 2, 1000);

            if (ret < 0) {
                errval = errno;
                if (errval != EINTR) {
                    printf("ERROR: poll failed. %d\n", errval);
                }
                break;
            }
            if (ret == 0) {
                printf("Timeout!\n");
            }
            if (i == 0) {
                Logging_Buffer_Init(&self->logdesc, LoggingUser_IMU, seqId, buff, sizeof(ImuLogBuffer_t));
            }

            if (fds[1].revents & POLLIN) {
                ret = read(fd, &buff->body[i], sizeof(cxd5602pwbimu_data_t));
                if (ret != sizeof(cxd5602pwbimu_data_t)) {
                    printf("ERROR: read size mismatch! %d\n", ret);
                }
                Logging_Buffer_Update(&self->logdesc, sizeof(cxd5602pwbimu_data_t));
            }
            if (fds[0].revents & POLLIN) {
                printf("Received shutdown signal.\n");
                uint64_t value;
                ret = read(fds[0].fd, &value, sizeof(value));
                printf("Eventfd read value: %llu\n", value);
                if (ret < 0) {
                    errval = errno;
                    printf("ERROR: eventfd read failed. %d\n", errval);
                    break;
                }
                if (value > 0) {
                    printf("Shutdown signal received.\n");
                    errval = -1; // Set to 0 to indicate graceful shutdown
                    break;
                }
                isRunning = false; // Exit the loop if shutdown signal is received
            }
            if (!isRunning) {
                break;
            }
        }
        Logging_Buffer_Finalize(&self->logdesc);
        LoggingDesc_t desc = { 0 };
        desc.ptr  = buff;
        desc.user = LoggingUser_IMU;
        desc.type = LoggingType_WRITE;
        desc.size = sizeof(ImuLogBuffer_t);
        Logging_SendQueue(mq, &desc);
        if (errval != 0) {
            break;
        }
        seqId++;
    }

    LoggingDesc_t endDesc;
    endDesc.ptr  = NULL;
    endDesc.user = LoggingUser_IMU;
    endDesc.type = LoggingType_END;
    endDesc.size = 0;
    Logging_SendQueue(mq, &endDesc);
    PowerCtrl_NotifyStop(self->shutdownHandlerId);

    /* Save the latest written position */

    close(fd);

    printf("Finished.\n");

    return 0;
} /* Imu_Logging_Run */
