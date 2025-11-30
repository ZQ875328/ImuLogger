#ifndef LOGGING_H
#define LOGGING_H
#include <mqueue.h>
#include <stdint.h>

mqd_t   Logging_CreateQueue(void);
int32_t Logging_DecrementOpenCount(void);

#endif /* LOGGING_H */
