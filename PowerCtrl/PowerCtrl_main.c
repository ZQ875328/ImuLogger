/* HEADERS */

#include <arch/board/board.h>
#include <fcntl.h>
#include <mqueue.h>
#include <nuttx/config.h>

#include "Common_DebugPrint.h"
#include "PowerCtrl.h"

/* MACROS */

#define POWER_SW    PIN_SPI3_MOSI
#define SYNCHRONIZE PIN_SPI3_MISO

#define PWR_EN      PIN_SPI2_SCK
#define PWR_MODE    PIN_SPI2_MISO
#define SD_EN       PIN_SPI2_MOSI

/* TYPES */

typedef enum tagPowerButton_e {
    PowerButton_OFF = 0,
    PowerButton_ON,
} PowerButton_e;

typedef enum tagPowerSource_e {
    PowerSource_BATTERY,
    PowerSource_USB,
} PowerSource_e;

typedef struct tagPowerCtrl_main_t {
    PowerButton_e state;
    PowerSource_e source;
    sem_t         smph;
} PowerCtrl_main_t;

/* PROTOTYPES */

static PowerCtrl_main_t* GetInstance(void);
static int               HandleGpioInterrupt(int irq, FAR void* context, FAR void* arg);
static int               Delay(uint32_t duration);
static int               Wait(void);
static int               Check1(void);
static void              Check2(void);
static void              InitLed(void);
static int               ActivatePower(void);
static void              DeactivatePower(void);
static void              InitInterrupt(void);
static void              WatchPowerButton(void);

/* VARIABLES */

static PowerCtrl_main_t s_powerCtrl_main_instance;

/* FUNCTIONS */

/**
 * @brief Get PowerCtrl instance
 */
static PowerCtrl_main_t* GetInstance(void)
{
    return &s_powerCtrl_main_instance;
}

/**
 * @brief POWER_SW ピンの割り込みハンドラ
 */
static int HandleGpioInterrupt(int irq, FAR void* context, FAR void* arg)
{
    PowerCtrl_main_t* self = GetInstance();
    uint32_t pin = (uint32_t) (uintptr_t) arg;

    PRINT_DEBUG("POWER_SW pin %x %d", pin, board_gpio_read(pin));
    cxd56_gpioint_invert(pin);
    sem_post(&self->smph);

    return OK;
}

/**
 * @brief Delay
 *
 * @note 割り込みが発生するか，指定した期間が経過するまで待機する．
 *
 * @param duration Ticks to wait
 *
 * @retval OK    指定した期間が経過した
 * @retval ERROR 割り込みが発生した
 */
static int Delay(uint32_t duration)
{
    PowerCtrl_main_t* self = GetInstance();
    int ret;

    ret = nxsem_tickwait(&self->smph, duration);
    if (ret == OK) {
        self->state ^= 1;
        return ERROR;
    }
    return OK;
}

/**
 * @brief 割り込みを待機する
 *
 * @note 割り込みが発生するまで待機する．
 *
 * @retval OK    指定した期間が経過した
 * @retval ERROR 割り込みが発生した
 */
static int Wait(void)
{
    PowerCtrl_main_t* self = GetInstance();
    int ret = sem_wait(&self->smph);

    if (ret == OK) {
        self->state ^= 1;
    }
    return ret;
}

/**
 * @brief ボタンの確定を確認する
 *
 * @note 割り込みが発生するか4秒経つまで待機する．
 *
 * @retval OK    指定した期間が経過した
 * @retval ERROR 割り込みが発生した
 */
static int Check1(void)
{
    int ret = OK;

    for (uint32_t i = 0; i < 10; ++i) {
        for (uint32_t led = GPIO_LED1; led <= GPIO_LED4; ++led) {
            board_gpio_write(led, 1);
            ret = Delay(MSEC2TICK(100));
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

/**
 * @brief ボタンの状態変化を確認する
 *
 * @note 割り込みが発生するまで待機する．
 */
static void Check2(void)
{
    uint32_t i = 0;

    while (true) {
        i++;
        for (uint32_t led = GPIO_LED1; led <= GPIO_LED4; ++led) {
            board_gpio_write(led, i % 2);
        }
        int ret = Delay(MSEC2TICK(100));
        if (ret == ERROR) {
            break;
        }
    }
    for (uint32_t led = GPIO_LED1; led <= GPIO_LED4; ++led) {
        board_gpio_write(led, 0);
    }
}

/**
 * @brief LED の初期化
 */
static void InitLed(void)
{
    board_gpio_config(GPIO_LED1, 0, false, true, PIN_FLOAT);
    board_gpio_write(GPIO_LED1, 0);

    board_gpio_config(GPIO_LED2, 0, false, true, PIN_FLOAT);
    board_gpio_write(GPIO_LED2, 0);

    board_gpio_config(GPIO_LED3, 0, false, true, PIN_FLOAT);
    board_gpio_write(GPIO_LED3, 0);

    board_gpio_config(GPIO_LED4, 0, false, true, PIN_FLOAT);
    board_gpio_write(GPIO_LED4, 0);
}

/**
 * @brief 電源を有効化する
 *
 * @retval OK    電源の有効化した
 * @retval ERROR 電源の有効化を中断した
 */
static int ActivatePower(void)
{
    PowerCtrl_main_t* self = GetInstance();

    int power_sw = board_gpio_read(POWER_SW);

    if (power_sw == 1) {
        /** @note 電源ボタンが押されてなければバッテリー動作でないとする． */
        self->source = PowerSource_USB;
        PRINT_DEBUG("Power source: USB");
    } else {
        self->source = PowerSource_BATTERY;
        PRINT_DEBUG("Power source: Battery");

        if (Check1() == ERROR) {
            PRINT_DEBUG("Stop activating power, pin %x %d", POWER_SW, board_gpio_read(POWER_SW));
            return ERROR;
        }
        board_gpio_config(PWR_EN, 0, false, false, PIN_FLOAT);
        board_gpio_config(PWR_MODE, 0, false, false, PIN_FLOAT);
        board_gpio_write(PWR_EN, 1);
        board_gpio_write(PWR_MODE, 0);
        Check2();
    }

    return OK;
}

/**
 * @brief 電源を無効化する
 */
static void DeactivatePower(void)
{
    board_gpio_write(PWR_EN, 0);
    board_gpio_write(PWR_MODE, 0);
}

/**
 * @brief 割り込みを初期化する
 *
 * @note POWER_SW ピンの割り込みを設定する．
 */
static void InitInterrupt(void)
{
    PowerCtrl_main_t* self = GetInstance();

    sem_init(&self->smph, 0, 0);
    sem_setprotocol(&self->smph, SEM_PRIO_NONE);

    self->state = PowerButton_ON;
    board_gpio_config(POWER_SW, 0, true, false, PIN_FLOAT);

    /** @note Edge 検出は初期状態を保証出来ない．レベル検出と invert と組み合わせる． */
    board_gpio_intconfig(POWER_SW, INT_HIGH_LEVEL, true, HandleGpioInterrupt);
    board_gpio_int(POWER_SW, true);
    PRINT_DEBUG("POWER_SW pin %x %d", POWER_SW, board_gpio_read(POWER_SW));
}

/**
 * @brief 電源ボタンを監視する
 *
 * @note POWER_SW ピンの割り込みを待機し，電源の有効化と無効化を行う．
 */
static void WatchPowerButton(void)
{
    PowerCtrl_main_t* self = GetInstance();
    irqstate_t flags       = enter_critical_section();

    while (true) {
        int ret = Wait();
        if (ret == OK && self->state == PowerButton_ON) {
            PRINT_DEBUG("Semaphore acquired");
            ret = Check1();
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
}

/* MAIN FUNCTION */

int main(int argc, char* argv[])
{
    InitLed();
    InitInterrupt();
    int ret = ActivatePower();
    if (ret != OK) {
        PRINT_ERROR("Failed to activate power");
        return ERROR;
    }
    PowerCtrl_PowerReady();
    WatchPowerButton();
    PowerCtrl_Shutdown();
    DeactivatePower();
    return 0;
}
