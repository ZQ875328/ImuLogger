#include "Logging_public.h"

#include "Logging.h"

#include <fcntl.h>
#include <nuttx/config.h>
#include <pthread.h>

#include "Common_DebugPrint.h"

#define MESSAGE_QUEUE_MAX (32)

typedef struct tagLogging_t {
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    bool            isQueueCreated;
    int32_t         openCount;
} Logging_t;

const static char* queue = "logging_queue";

static Logging_t logging_instance = {
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .cond  = PTHREAD_COND_INITIALIZER,
    .isQueueCreated = false,
};

static Logging_t* GetInstance(void)
{
    return &logging_instance;
}

mqd_t Logging_CreateQueue(void)
{
    Logging_t* self = GetInstance();
    mqd_t mq;
    struct mq_attr attr;

    attr.mq_maxmsg  = MESSAGE_QUEUE_MAX;
    attr.mq_msgsize = sizeof(LoggingDesc_t);
    attr.mq_flags   = 0;

    mq = mq_open(queue, O_CREAT | O_RDWR, 0666, &attr);
    if (mq == (mqd_t) ERROR) {
        PRINT_ERROR("Message queue open error:%d", errno);
        return ERROR;
    }

    pthread_mutex_lock(&self->mutex);
    self->isQueueCreated = true;
    pthread_cond_broadcast(&self->cond);
    pthread_mutex_unlock(&self->mutex);

    return mq;
}

mqd_t Logging_OpenQueue(bool isIncrementOpenCount)
{
    Logging_t* self = GetInstance();

    pthread_mutex_lock(&self->mutex);
    while (!self->isQueueCreated) {
        PRINT_DEBUG("Waiting for message queue to be created...");
        pthread_cond_wait(&self->cond, &self->mutex);
    }
    if (isIncrementOpenCount) {
        self->openCount++;
    }
    pthread_mutex_unlock(&self->mutex);
    PRINT_DEBUG("Open count incremented: %d", self->openCount);

    mqd_t mq;

    mq = mq_open(queue, O_RDWR);
    if (mq == (mqd_t) ERROR) {
        PRINT_ERROR("Message queue open error:%d", errno);
        return ERROR;
    }

    return mq;
}

void Logging_CloseQueue(mqd_t mq)
{
    if (mq != (mqd_t) ERROR) {
        mq_close(mq);
    }
}

int Logging_SendQueue(mqd_t mq, LoggingDesc_t* desc)
{
    int ret;

    ret = mq_send(mq, (FAR const char *) desc, sizeof(LoggingDesc_t), 0);
    if (ret < 0) {
        PRINT_ERROR("mq_send err(errno:%d)", errno);
        return ERROR;
    }

    return OK;
}

int Logging_ReceiveQueue(mqd_t mq, LoggingDesc_t* desc)
{
    int ret;

    ret = mq_receive(mq, (FAR char *) desc, sizeof(LoggingDesc_t), 0);
    if (ret < 0) {
        PRINT_ERROR("mq_receive err(errno:%d)", errno);
        return ERROR;
    }

    return OK;
}

int32_t Logging_DecrementOpenCount(void)
{
    Logging_t* self = GetInstance();

    pthread_mutex_lock(&self->mutex);
    int32_t count = --self->openCount;
    pthread_mutex_unlock(&self->mutex);

    return count;
}
