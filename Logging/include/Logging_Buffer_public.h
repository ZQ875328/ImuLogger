#ifndef LOGGING_BUFFER_PUBLIC_H
#define LOGGING_BUFFER_PUBLIC_H

#include <stdint.h>
#include "Logging_public.h"

typedef struct tagLogging_Buffer_Desc_t {
    LogHeader_t* header;
    void*        body;
    LogFooter_t* footer;
} Logging_Buffer_Desc_t;

void Logging_Buffer_Init(Logging_Buffer_Desc_t* desc, LoggingUser_e user, uint32_t seqId, void* buff,
    uint32_t size);
bool     Logging_Buffer_Write(Logging_Buffer_Desc_t* desc, void* data, uint32_t size);
void     Logging_Buffer_Update(Logging_Buffer_Desc_t* desc, uint32_t size);
uint32_t Logging_Buffer_GetRemainingSize(Logging_Buffer_Desc_t* desc);
void*    Logging_Buffer_GetNextPos(Logging_Buffer_Desc_t* desc);
void     Logging_Buffer_Finalize(Logging_Buffer_Desc_t* desc);

#endif /* LOGGING_BUFFER_PUBLIC_H */
