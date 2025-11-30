#ifndef LOGGING_WRITER_H
#define LOGGING_WRITER_H

#include <sys/types.h>

int Logging_Writer_Initialize(void);
int Logging_Writer_Write(void* data, size_t size);
int Logging_Writer_Close(void);

#endif /* LOGGING_WRITER_H */
