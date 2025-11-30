#include <nuttx/config.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sdk/config.h>
#include <nuttx/arch.h>
#include <arch/cxd56xx/scu.h>
#include <arch/cxd56xx/adc.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/boardctl.h>

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <debug.h>

#include <arch/chip/scu.h>
#include <arch/chip/adc.h>
#include "Battery_Logging.h"

#define BATTERY_SENSE    "/dev/lpadc2"

int main(int argc, char *argv[])
{
    Battery_Logging_Run();    
    return 0;
}
