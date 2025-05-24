#include <nuttx/config.h>
#include <arch/board/board.h>

#include <mqueue.h>
#include <fcntl.h>
#include "Common_DebugPrint.h"

#define POWER_SW       PIN_SPI3_MOSI
#define SYNCHRONIZE    PIN_SPI3_MISO

#define PWR_EN         PIN_SPI2_SCK
#define PWR_MODE       PIN_SPI2_MISO
#define SD_EN          PIN_SPI2_MOSI

typedef enum tagPowerButton_e
{
    PowerButton_OFF = 0,
    PowerButton_ON,
} PowerButton_e;

typedef struct tagPowerCtrl_t
{
    PowerButton_e state;
    sem_t smph;
} PowerCtrl_t;

static PowerCtrl_t* GetInstance(void);
static int gpio_handler(int irq, FAR void *context, FAR void *arg);
static int delay(uint32_t duration);
static int wait(void);
static int check(void);
static int check2(void);
static void InitLed(void);
static void ActivatePower(void);
static void DeactivatePower(void);
static void InitInterrupt(void);

static PowerCtrl_t s_powerCtrl_instance;

static PowerCtrl_t* GetInstance(void)
{
    return &s_powerCtrl_instance;
}

static int gpio_handler(int irq, FAR void *context, FAR void *arg)
{
    PowerCtrl_t* self = GetInstance();
    uint32_t pin = (uint32_t)(uintptr_t)arg;
    PRINT_DEBUG("POWER_SW pin %x %d", pin, board_gpio_read(pin));
    cxd56_gpioint_invert(pin);
    sem_post(&self->smph);
    
    return OK;
}

static int delay(uint32_t duration)
{
    PowerCtrl_t* self = GetInstance();
    int ret;
    ret = nxsem_tickwait(&self->smph, duration);
    if (ret == OK) {
        self->state ^= 1;
        return ERROR;
    }
    return OK;
}

static int wait(void)
{
    PowerCtrl_t* self = GetInstance();
    int ret = sem_wait(&self->smph);
    if (ret == OK) {
        self->state ^= 1;
    }
    return ret;
}

static int check(void) {
    int ret = OK;
    for (uint32_t i = 0; i < 10; ++i) {
        for (uint32_t led = GPIO_LED1; led <= GPIO_LED4; ++led) {
            board_gpio_write(led, 1);
            ret = delay(MSEC2TICK(100));
            board_gpio_write(led, 0);
            if (ret == ERROR) {
                break;
            }
        }
        if (ret == ERROR) {
            break;
        }
    }
    return ret;
}

static int check2(void) {
    int ret = OK;
    uint32_t i = 0;
    while (true) {
        i++;
        for (uint32_t led = GPIO_LED1; led <= GPIO_LED4; ++led) {
            board_gpio_write(led, i % 2);
        }
        ret = delay(MSEC2TICK(100));
        if (ret == ERROR) {
            break;
        }
    }
    for (uint32_t led = GPIO_LED1; led <= GPIO_LED4; ++led) {
        board_gpio_write(led, 0);
    }
    return ret;
}

static void InitLed(void) {
    board_gpio_config(GPIO_LED1, 0, false, true, PIN_FLOAT);
    board_gpio_write(GPIO_LED1, 0);
  
    board_gpio_config(GPIO_LED2, 0, false, true, PIN_FLOAT);
    board_gpio_write(GPIO_LED2, 0);
  
    board_gpio_config(GPIO_LED3, 0, false, true, PIN_FLOAT);
    board_gpio_write(GPIO_LED3, 0);
  
    board_gpio_config(GPIO_LED4, 0, false, true, PIN_FLOAT);
    board_gpio_write(GPIO_LED4, 0);
}


static void ActivatePower(void) {
    PowerCtrl_t* self = GetInstance();
    PRINT_DEBUG("POWER_SW pin %x %d", POWER_SW, board_gpio_read(POWER_SW));

    if (self->state == PowerButton_OFF) {
        return;
    }
    if (check() == ERROR) {
        PRINT_DEBUG("POWER_SW pin %x %d", POWER_SW, board_gpio_read(POWER_SW));
        PRINT_DEBUG("Error activating power");
        return;
    }
    board_gpio_config(PWR_EN, 0, false, false, PIN_FLOAT); 
    board_gpio_config(PWR_MODE, 0, false, false, PIN_FLOAT); 
    board_gpio_write(PWR_EN, 1);
    // board_gpio_write(PWR_MODE, 1);
    check2();
}

static void DeactivatePower(void) {
    board_gpio_write(PWR_EN, 0);
    board_gpio_write(PWR_MODE, 0);
}

static void InitInterrupt(void) {
    PowerCtrl_t* self = GetInstance();
    sem_init(&self->smph, 0, 0);
    sem_setprotocol(&self->smph, SEM_PRIO_NONE);

    self->state = PowerButton_ON;
    board_gpio_config(POWER_SW, 0, true, false, PIN_FLOAT);
    board_gpio_intconfig(POWER_SW, INT_HIGH_LEVEL,    true, gpio_handler);
    board_gpio_int(POWER_SW, true);
    PRINT_DEBUG("POWER_SW pin %x %d", POWER_SW, board_gpio_read(POWER_SW));
}

int main(int argc, char *argv[])
{
    PowerCtrl_t* self = GetInstance();
    InitLed();
    InitInterrupt();
    ActivatePower();

    uint64_t y;

    irqstate_t flags = enter_critical_section();
    while (true) {
        int ret = wait();
        if (ret == OK && self->state == PowerButton_ON) {
            PRINT_DEBUG("Semaphore acquired");
            ret = check();
            if (ret == 0) {
                PRINT_INFO("LED blinked successfully");
                DeactivatePower();
                break;
            } else {
                PRINT_DEBUG("Canceled blinking LED: %d", ret);
            }
        }
    }
    leave_critical_section(flags);

    return 0;
}
