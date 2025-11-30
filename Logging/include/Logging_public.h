#ifndef LOGGING_PUBLIC_H
#define LOGGING_PUBLIC_H

#include <mqueue.h>
#include <stdbool.h>
#include <stdint.h>

typedef void (*LoggingCallback_t)(void);

typedef enum tagLoggingUser_e {
    LoggingUser_IMU,
    LoggingUser_GNSS,
    LoggingUser_SYNCHRONIZE,
    LoggingUser_POWER,
} LoggingUser_e;

typedef enum tagLoggingType_e {
    LoggingType_WRITE,
    LoggingType_END,
    LoggingType_SHUTDOWN,
} LoggingType_e;

typedef struct tagLoggingDesc_t {
    LoggingType_e     type    : 8;
    LoggingUser_e     user    : 8;
    uint32_t          reserved: 16;
    void*             ptr;
    uint32_t          size;
    LoggingCallback_t callback;
} LoggingDesc_t;

typedef struct tagLogHeader_t {
    LoggingUser_e user  : 8;
    uint32_t      seqId : 24;
    uint32_t      size;
    uint64_t      time;
} LogHeader_t;

typedef struct tagLogFooter_t {
    uint64_t time;
    uint32_t size;
    uint32_t crc;
} LogFooter_t;

mqd_t Logging_OpenQueue(bool isIncrementOpenCount);
void  Logging_CloseQueue(mqd_t mq);
int   Logging_SendQueue(mqd_t mq, LoggingDesc_t* desc);
int   Logging_ReceiveQueue(mqd_t mq, LoggingDesc_t* desc);

#endif /* LOGGING_PUBLIC_H */
