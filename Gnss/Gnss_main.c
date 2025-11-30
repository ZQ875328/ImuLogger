#include <arch/board/board.h>
#include <arch/chip/gnss.h>
#include <errno.h>
#include <fcntl.h>
#include <nuttx/config.h>
#include <poll.h>
#include <stdio.h>
#include <sys/ioctl.h>

#include "Common_Rtc.h"
#include "Logging_Buffer_public.h"
#include "Logging_public.h"

#include "Gnss_Pps.h"

/****************************************************************************
* Pre-processor Definitions
****************************************************************************/

#define MY_GNSS_SIG0          18
#define MY_GNSS_SIG1          19
#define TEST_LOOP_TIME        600
#define TEST_RECORDING_CYCLE  1
#define TEST_NOTIFY_THRESHOLD CXD56_GNSS_PVTLOG_THRESHOLD_HALF
#define FILE_NAME_LEN         256

#if (TEST_NOTIFY_THRESHOLD == CXD56_GNSS_PVTLOG_THRESHOLD_HALF)
#  define PVTLOG_UNITNUM (CXD56_GNSS_PVTLOG_MAXNUM / 2)
#else
#  define PVTLOG_UNITNUM (CXD56_GNSS_PVTLOG_MAXNUM)
#endif
#define TEST_FILE_COUNT  (1 + (int) (TEST_LOOP_TIME / PVTLOG_UNITNUM))

// #define LOG_NUM          (sizeof(GnssPositionData_t))
#define NUM_BUFFERS     (2)
#define BUFFER_SIZE     (32 * 1024)
#define GNSS_RECORD_NUM ((BUFFER_SIZE - sizeof(LogHeader_t) - sizeof(LogFooter_t)) / sizeof(GnssPositionData_t))
#define GNSS_LOG_PADDING_SIZE                                    \
        (BUFFER_SIZE - sizeof(LogHeader_t) - sizeof(LogFooter_t) \
        - (GNSS_RECORD_NUM * sizeof(GnssPositionData_t)))

/****************************************************************************
* Private Types
****************************************************************************/

typedef struct cxd56_pvtlog_data_s cxd56_pvtlog_data_t;

struct cxd56_gnss_dms_s {
    int8_t   sign;
    uint8_t  degree;
    uint8_t  minute;
    uint32_t frac;
};

typedef struct tagCxd56GnssReceiver_t {
    /* [out] Position type; 0:Invalid, 1:GNSS,
     *       2:IMES, 3:user set, 4:previous
     */

    uint8_t type;

    /* [out] FALSE:SGPS, TRUE:DGPS */

    uint8_t dgps;

    /* [out] 1:Invalid, 2:2D, 3:3D */

    uint8_t pos_fixmode;

    /* [out] 1:Invalid, 2:2D VZ, 3:2D Offset,
     *       4:3D, 5:1D, 6:PRED
     */

    uint8_t vel_fixmode;

    /* [out] Nr of visible satellites */

    uint8_t numsv;

    /* [out] Nr of tracking satellites */

    uint8_t numsv_tracking;

    /* [out] Nr of satellites to calculate position */

    uint8_t numsv_calcpos;

    /* [out] Nr of satellites to calculate velocity */

    uint8_t numsv_calcvel;

    /* [out] bit field
     *     [7..5]Reserved
     *      [4]AEP Velocity
     *      [3]AEP Position
     *      [2]CEP Velocity
     *      [1]CEP Position,
     *      [0]user set
     */

    uint8_t assist;

    /* [out] 0:none, 1:exist */

    uint8_t pos_dataexist;

    /* [out] Using sv system, bit field;
     *   bit0:GPS, bit1:GLONASS, bit2:SBAS,
     *   bit3:QZSS_L1CA, bit4:IMES,
     *   bit5:QZSS_L1SAIF, bit6:BeiDou,
     *   bit7:Galileo
     */

    uint16_t svtype;

    /* [out] using sv system, bit field;
     *   bit0:GPS, bit1:GLONASS, bit2:SBAS,
     *   bit3:QZSS_L1CA, bit4:IMES,
     *   bit5:QZSS_L1SAIF, bit6:BeiDou,
     *   bit7:Galileo
     */

    uint16_t pos_svtype;

    /* [out] using sv system, bit field; bit0:GPS,
     *   bit0:GPS, bit1:GLONASS, bit2:SBAS,
     *   bit3:QZSS_L1CA, bit4:IMES,
     *   bit5:QZSS_L1SAIF, bit6:BeiDou,
     *   bit7:Galileo
     */

    uint16_t vel_svtype;

    /* [out] position source; 0:Invalid, 1:GNSS,
     *   2:IMES, 3:user set, 4:previous
     */

    uint32_t possource;

    /* [out] TCXO offset[Hz] */

    float tcxo_offset;

    /* [out] DOPs of Position */

    struct cxd56_gnss_dop_s pos_dop;

    /* [out] Weighted DOPs of Velocity */

    struct cxd56_gnss_dop_s vel_idx;

    /* [out] Accuracy of Position */

    struct cxd56_gnss_var_s pos_accuracy;

    /* [out] Latitude [degree] */

    double latitude;

    /* [out] Longitude [degree] */

    double longitude;

    /* [out] Altitude [m] */

    double altitude;

    /* [out] Geoid height [m] */

    double geoid;

    /* [out] Velocity [m/s] */

    float velocity;

    /* [out] Direction [degree] */

    float direction;

    /* [out] Current day (UTC) */

    struct cxd56_gnss_date_s date;

    /* [out] Current time (UTC) */

    struct cxd56_gnss_time_s time;

    /* [out] Current day (GPS) */

    struct cxd56_gnss_date_s gpsdate;

    /* [out] Current time (GPS) */

    struct cxd56_gnss_time_s gpstime;

    /* [out] Receive time (UTC) */

    struct cxd56_gnss_time_s receivetime;

    /* [out] For internal use */

    uint32_t priv;

    /* [out] Leap Second[sec] */

    int8_t leap_sec;

    /* [out] elapsed time
     *       from reset in ns
     */

    uint64_t time_ns;

    /* [out] elapsed time
     *       from GPS epoch in ns
     */

    int64_t full_bias_ns;
} Cxd56GnssReceiver_t;


typedef struct tagGnssPositionData_t {
    uint64_t data_timestamp;      /* [out] Timestamp  */
    uint32_t status;              /* [out] Positioning data
                                   *   status 0 : Valid,
                                   *         <0 : Invalid
                                   */
    uint32_t            svcount;  /* [out] Sv data count */
    Cxd56GnssReceiver_t receiver; /* [out] Receiver data */
} GnssPositionData_t;

typedef struct tagGnssLogBuffer_t {
    LogHeader_t        header;
    GnssPositionData_t body[GNSS_RECORD_NUM];
    uint8_t            reserved[GNSS_LOG_PADDING_SIZE];
    LogFooter_t        footer;
} GnssLogBuffer_t;
static_assert(sizeof(GnssLogBuffer_t) == BUFFER_SIZE, "GnssLogBuffer_t size mismatch");

typedef struct tagGnssLogging_t {
    int      shutdownHandlerId;
    uint32_t seqId;
    uint32_t logPos;
    int      eventFd;
} GnssLogging_t;

static GnssLogging_t gnssLogging_instance;

/****************************************************************************
* Private Data
****************************************************************************/

static struct cxd56_gnss_positiondata_s posdat;
static struct cxd56_pvtlog_s pvtlogdat;
static GnssLogBuffer_t gnssLogging_buffer[NUM_BUFFERS];

static GnssLogging_t* GetInstance(void)
{
    return &gnssLogging_instance;
}

static int gnss_setparams(int fd)
{
    int ret = 0;
    uint32_t set_satellite;
    struct cxd56_gnss_ope_mode_param_s set_opemode;

    /* Set the GNSS operation interval. */

    set_opemode.mode  = 1;    /* Operation mode:Normal(default). */
    set_opemode.cycle = 1000; /* Position notify cycle(msec step). */

    ret = ioctl(fd, CXD56_GNSS_IOCTL_SET_OPE_MODE, (uint32_t) &set_opemode);
    if (ret < 0) {
        printf("ioctl(CXD56_GNSS_IOCTL_SET_OPE_MODE) NG!!\n");
        goto _err;
    }

    /* Set the type of satellite system used by GNSS. */

    set_satellite = CXD56_GNSS_SAT_GPS
        | CXD56_GNSS_SAT_GALILEO
        | CXD56_GNSS_SAT_QZ_L1CA
        | CXD56_GNSS_SAT_QZ_L1S;

    ret = ioctl(fd, CXD56_GNSS_IOCTL_SELECT_SATELLITE_SYSTEM, set_satellite);
    if (ret < 0) {
        printf("ioctl(CXD56_GNSS_IOCTL_SELECT_SATELLITE_SYSTEM) NG!!\n");
        goto _err;
    }

_err:
    return ret;
}

static void ShutdownHandler(void)
{
    GnssLogging_t* self = GetInstance();

    eventfd_write(self->eventFd, 1);
}

#define getreg32(a) (*(volatile uint32_t *) (a))

int main(int argc, FAR char* argv[])
{
    GnssLogging_t* self = GetInstance();
    mqd_t mq = Logging_OpenQueue(true);
    Logging_Buffer_Desc_t logdesc;

    /* Initialize GNSS logging instance */
    self->shutdownHandlerId = PowerCtrl_SetShutdownCallback(ShutdownHandler);
    if (self->shutdownHandlerId < 0) {
        printf("Failed to set shutdown callback: %d\n", self->shutdownHandlerId);
        return self->shutdownHandlerId;
    }
    self->eventFd = eventfd(0, 0);
    if (self->eventFd < 0) {
        printf("Failed to create eventfd: %d\n", errno);
        return -errno;
    }
    self->seqId  = 0;
    self->logPos = 0;
    /* Open GNSS device */
    int fd = open("/dev/gps", O_RDONLY);
    if (fd < 0) {
        printf("Failed to open GNSS device: %d\n", errno);
        return -ENODEV;
    }
    /* Set GNSS parameters */
    int ret = gnss_setparams(fd);
    if (ret != 0) {
        printf("Failed to set GNSS parameters: %d\n", ret);
        close(fd);
        return ret;
    }

    ret = ioctl(fd, CXD56_GNSS_IOCTL_START, CXD56_GNSS_STMOD_COLD);
    if (ret < 0) {
        printf("Failed to start GNSS: %d\n", errno);
        close(fd);
        return ret;
    }
    Gnss_Pps_Init();


    ret = ioctl(fd, CXD56_GNSS_IOCTL_SET_1PPS_OUTPUT, 1);
    if (ret < 0) {
        printf("Failed to enable 1PPS output: %d\n", errno);
        close(fd);
        return ret;
    }
    printf("GNSS PPS initialized %x %x %x\n", getreg32(0x04100818), getreg32(0x4102014), getreg32(0x041007C0));

    printf("GNSS started successfully.\n");
    /* Main loop to read GNSS data */
    uint32_t seqId = 0;
    bool isRunning = true;
    while (isRunning) {
        GnssLogBuffer_t* buffer = &gnssLogging_buffer[seqId % NUM_BUFFERS];
        for (uint32_t i = 0; i < GNSS_RECORD_NUM; i++) {
            struct pollfd fds[2];
            fds[0].fd     = self->eventFd;
            fds[0].events = POLLIN;
            fds[1].fd     = fd;
            fds[1].events = POLLIN;
            ret = poll(&fds, 1, -1);
            if (ret < 0) {
                printf("Poll error: %d\n", errno);
                close(fd);
                return ret;
            }
            if (fds[1].revents & POLLIN) {
                if (i == 0) {
                    Logging_Buffer_Init(&logdesc, LoggingUser_GNSS, seqId, buffer, sizeof(GnssLogBuffer_t));
                }

                /* Read GNSS data */
                GnssPositionData_t* posData = &buffer->body[i];
                ret = read(fd, posData, sizeof(GnssPositionData_t));

                if (ret < 0) {
                    printf("Failed to read GNSS data: %d\n", errno);
                    close(fd);
                    return ret;
                }
                printf("idx:%d Read GNSS data: %d bytes\n", i, ret);
                printf("Position: Lat: %f, Lon: %f, Alt: %f\n",
                    buffer->body[i].receiver.latitude,
                    buffer->body[i].receiver.longitude,
                    buffer->body[i].receiver.altitude);
                printf("Time: %d-%02d-%02d %02d:%02d:%02d.%06d\n",
                    buffer->body[i].receiver.date.year,
                    buffer->body[i].receiver.date.month,
                    buffer->body[i].receiver.date.day,
                    buffer->body[i].receiver.time.hour,
                    buffer->body[i].receiver.time.minute,
                    buffer->body[i].receiver.time.sec,
                    buffer->body[i].receiver.time.usec);

                struct tm tm = { 0 };
                tm.tm_year = posData->receiver.date.year - 1900;
                tm.tm_mon  = posData->receiver.date.month - 1;
                tm.tm_mday = posData->receiver.date.day;
                tm.tm_hour = posData->receiver.time.hour;
                tm.tm_min  = posData->receiver.time.minute;
                tm.tm_sec  = posData->receiver.time.sec;

                time_t tt = mktime(&tm);

                // printf("logdesc %x %x %x %x %x\n", logdesc.header,
                //     logdesc.body, logdesc.footer, logdesc.header->size, logdesc.footer->size);
                Logging_Buffer_Update(&logdesc, sizeof(GnssPositionData_t));
            }
            if (fds[0].revents & POLLIN) {
                uint64_t value;
                ret = eventfd_read(self->eventFd, &value);
                if (ret < 0) {
                    printf("Failed to read eventfd: %d\n", errno);
                }
                if (value > 0) {
                    printf("Shutdown signal received.\n");
                    return 0; // Exit gracefully
                }
                isRunning = false; // Exit the loop
            }
            if (!isRunning) {
                break; // Exit the loop if shutdown signal received
            }
        }
        Logging_Buffer_Finalize(&logdesc);
        LoggingDesc_t desc;
        desc.ptr      = buffer;
        desc.user     = LoggingUser_GNSS;
        desc.type     = LoggingType_WRITE;
        desc.size     = sizeof(GnssLogBuffer_t);
        desc.callback = NULL; // No callback for this example
        Logging_SendQueue(mq, &desc);

        if (ret < 0) {
            printf("Poll error: %d\n", errno);
            break;
        }
        seqId++;
    }

    LoggingDesc_t desc = { 0 };
    desc.user = LoggingUser_GNSS;
    desc.type = LoggingType_END;
    Logging_SendQueue(mq, &desc);

    /* Set up signal handlers, logging, etc. */
} /* main */
