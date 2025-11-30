#include "Logging_Buffer_public.h"

#include <nuttx/crc32.h>
#include <string.h>

#include "Common_Rtc.h"

static void updateCrc(Logging_Buffer_Desc_t* desc, void* data, uint32_t size);

static void updateCrc(Logging_Buffer_Desc_t* desc, void* data, uint32_t size)
{
    desc->footer->crc = crc32part(data, size, desc->footer->crc);
}

void Logging_Buffer_Init(Logging_Buffer_Desc_t* desc, LoggingUser_e user, uint32_t seqId, void* buff, uint32_t size)
{
    desc->header = (LogHeader_t *) buff;
    desc->body   = (uint8_t *) buff + sizeof(LogHeader_t);
    desc->footer = (LogFooter_t *) (buff + size - sizeof(LogFooter_t));

    desc->header->user  = user;
    desc->header->seqId = seqId;
    desc->header->size  = size;
    desc->header->time  = Common_Rtc_GetCount(Common_RtcChannel_1);

    desc->footer->size = 0;
    desc->footer->crc  = 0xFFFFFFFF;
    updateCrc(desc, desc->header, sizeof(LogHeader_t));
}

bool Logging_Buffer_Write(Logging_Buffer_Desc_t* desc, void* data, uint32_t size)
{
    void* ptr = desc->body + desc->footer->size;

    memcpy(ptr, data, size);
    updateCrc(desc, ptr, size);
    desc->footer->size += size;
    return true;
}

void Logging_Buffer_Update(Logging_Buffer_Desc_t* desc, uint32_t size)
{
    void* ptr = desc->body + desc->footer->size;

    updateCrc(desc, ptr, size);
    desc->footer->size += size;
}

uint32_t Logging_Buffer_GetRemainingSize(Logging_Buffer_Desc_t* desc)
{
    return desc->header->size - sizeof(LogHeader_t) - sizeof(LogFooter_t) - desc->footer->size;
}

void* Logging_Buffer_GetNextPos(Logging_Buffer_Desc_t* desc)
{
    return desc->body + desc->footer->size;
}

void Logging_Buffer_Finalize(Logging_Buffer_Desc_t* desc)
{
    memset(desc->body + desc->footer->size, 0,
        Logging_Buffer_GetRemainingSize(desc));
    desc->footer->time = Common_Rtc_GetCount(Common_RtcChannel_1);
    updateCrc(desc, desc->footer, sizeof(LogFooter_t) - sizeof(desc->footer->crc));
    desc->footer->crc = ~desc->footer->crc; // Finalize CRC by inverting it
}
