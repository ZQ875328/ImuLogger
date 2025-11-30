#include <fcntl.h>
#include <nuttx/config.h>
#include <stdio.h>
#include <sys/stat.h>

#include "Common_DebugPrint.h"
#include "Logging_Writer.h"
#include "PowerCtrl_public.h"

#include "Logging.h"
#include "Logging_public.h"

#define MAX_PATH_LENGTH (32)

typedef struct tagLogging_main_t {
    int  shutdownHandlerId;
    bool isShutdown;
} Logging_main_t;

static Logging_main_t logging_main_instance;

static Logging_main_t* GetInstance(void)
{
    return &logging_main_instance;
}

static void ShutdownNotify(void)
{
    mqd_t mq = Logging_OpenQueue(true);
    LoggingDesc_t desc = { 0 };

    desc.type = LoggingType_SHUTDOWN;

    Logging_SendQueue(mq, &desc);
    Logging_CloseQueue(mq);
}

int main(int argc, char* argv[])
{
    Logging_main_t* self = GetInstance();

    struct stat info;

    /** @note SD カードのマウントを待ち合わせる */
    while (stat("/mnt/sd0", &info) != 0) {
        PRINT_DEBUG("Waiting for /mnt/sd0 to be ready...\n");
        usleep(100000); // 100ms
    }

    int ret = Logging_Writer_Initialize();
    if (ret != OK) {
        PRINT_ERROR("Logging_Writer_Initialize failed: %d\n", ret);
        return -1;
    }
    self->shutdownHandlerId = PowerCtrl_SetShutdownCallback(ShutdownNotify);

    mqd_t mq = Logging_CreateQueue();

    bool isStopped = false;
    int32_t count;
    while (isStopped == false) {
        LoggingDesc_t desc;
        int ret = mq_receive(mq, (FAR char *) &desc, sizeof(LoggingDesc_t), 0);
        if (ret < 0) {
            PRINT_ERROR("mq_receive err(errno:%d)\n", errno);
            return -1;
        }
        PRINT_DEBUG("Received message: %d, %d\n", desc.user, desc.size);
        switch (desc.type) {
            case LoggingType_WRITE:
                if (desc.ptr == NULL || desc.size == 0) {
                    PRINT_ERROR("Invalid data received: ptr=%p, size=%d\n", desc.ptr, desc.size);
                    continue;
                }
                PRINT_DEBUG("Writing data: type=%x user=%x ptr=%x size=%x callback=%p\n", desc.type, desc.user,
                    desc.ptr, desc.size, desc.callback);

                Logging_Writer_Write(desc.ptr, desc.size);
                break;
            case LoggingType_SHUTDOWN:
                self->isShutdown = true;
            case LoggingType_END:
                count = Logging_DecrementOpenCount();
                PRINT_DEBUG("Open count decremented: %d, isShutdown=%d\n", count, self->isShutdown);
                isStopped = count <= 0 && self->isShutdown;
                break;
            default:
                break;
        }
        if (desc.callback != NULL) {
            desc.callback();
        }
    }
    Logging_Writer_Close();
    PowerCtrl_NotifyStop(self->shutdownHandlerId);
    return 0;
} /* main */
