/* HEADERS */

#include "PowerCtrl_public.h"

#include "PowerCtrl.h"

#include <arch/board/board.h>
#include <nuttx/config.h>
#include <pthread.h>

#include "Common_DebugPrint.h"

/* MACROS */

#define NUM_SHUTDOWN_CALLBACKS (4)
#define CALLBACK_BITMAP_MASK   ((uint32_t) (((uint64_t) 1 << NUM_SHUTDOWN_CALLBACKS) - 1))

/* TYPES */

typedef struct tagPowerCtrl_t {
    pthread_mutex_t              mutex;
    pthread_cond_t               shutdownCond;
    pthread_cond_t               bootCond;
    PowerCtrl_ShutdownCallback_t shutdownCallback[NUM_SHUTDOWN_CALLBACKS];
    uint32_t                     shutdownBitmap;
    bool                         isPowerReady;
    bool                         isShutdown;
} PowerCtrl_t;

/* PROTOTYPES */
static PowerCtrl_t* GetIpcInstance(void);

/* VARIABLES */

static PowerCtrl_t s_powerCtrl_instance = {
    .mutex          = PTHREAD_MUTEX_INITIALIZER,
    .shutdownCond   = PTHREAD_COND_INITIALIZER,
    .bootCond       = PTHREAD_COND_INITIALIZER,
    .shutdownBitmap = 0,
    .isShutdown     = false,
};

/* FUNCTIONS */

/**
 * @brief Get PowerCtrl IPC instance
 */
static PowerCtrl_t* GetIpcInstance(void)
{
    return &s_powerCtrl_instance;
}

/**
 * @brief Shutdown を実行する
 *
 * @note Handler へ通知を行い，停止完了を待ち合わせる．
 */
int PowerCtrl_Shutdown(void)
{
    PowerCtrl_t* self = GetIpcInstance();

    pthread_mutex_lock(&self->mutex);

    self->isShutdown = true;

    uint32_t bitmap = self->shutdownBitmap;

    while (bitmap) {
        uint32_t pos   = __builtin_clz(bitmap);
        uint32_t index = 31 - pos; // Convert position to index (0-based)

        if (self->shutdownCallback[index]) {
            self->shutdownCallback[index]();
            PRINT_INFO("Shutdown callback at index %d executed.", index);
        } else {
            PRINT_ERROR("No shutdown callback set at index %d.", index);
        }

        bitmap &= ~(1U << index); // Clear the executed callback
    }

    while (self->shutdownBitmap != 0) {
        pthread_cond_wait(&self->shutdownCond, &self->mutex);
    }

    pthread_mutex_unlock(&self->mutex);

    PRINT_INFO("PowerCtrl IPC shutdown.");
    return 0; // Return OK on successful shutdown
}

/**
 * @brief Shutdown コールバックを設定する
 *
 * @param callback Callback 関数
 * @return Handler ID
 */
int PowerCtrl_SetShutdownCallback(PowerCtrl_ShutdownCallback_t callback)
{
    PowerCtrl_t* self = GetIpcInstance();

    pthread_mutex_lock(&self->mutex);
    while (!self->isPowerReady) {
        PRINT_INFO("Waiting for power to be ready...");
        pthread_cond_wait(&self->bootCond, &self->mutex);
    }

    if (self->isShutdown) {
        PRINT_ERROR("Cannot set shutdown callback after shutdown.");
        return ERROR; // Cannot set callback after shutdown
    }

    uint32_t pos = __builtin_clz(~self->shutdownBitmap & CALLBACK_BITMAP_MASK);

    PRINT_DEBUG("Available shutdown callback position: %u %x", pos, ~self->shutdownBitmap & CALLBACK_BITMAP_MASK);

    if (pos >= 32) {
        PRINT_ERROR("No available shutdown callback slots.");
        return ERROR; // No available slot for the callback
    }

    uint32_t index = 31 - pos; // Convert position to index (0-based)

    self->shutdownBitmap |= (1U << index);
    self->shutdownCallback[index] = callback;

    pthread_mutex_unlock(&self->mutex);

    PRINT_INFO("PowerCtrl IPC shutdown callback set.");
    return index; // Return the index of the callback
} /* PowerCtrl_SetShutdownCallback */

/**
 * @brief 停止完了を通知する
 *
 * @param index SetShutdownCallback で返された Handler ID
 * @return 0 on success, -1 on error
 */
int PowerCtrl_NotifyStop(int index)
{
    PowerCtrl_t* self = GetIpcInstance();

    pthread_mutex_lock(&self->mutex);

    if (index < 0 || index >= NUM_SHUTDOWN_CALLBACKS) {
        PRINT_ERROR("Invalid shutdown callback index: %d", index);
        return ERROR; // Invalid index
    }

    if (!(self->shutdownBitmap & (1U << index))) {
        PRINT_ERROR("Shutdown callback at index %d is not set.", index);
        return ERROR; // Callback not set
    }

    self->shutdownCallback[index] = NULL; // Clear the callback
    self->shutdownBitmap &= ~(1U << index);
    if (self->shutdownBitmap == 0) {
        pthread_cond_signal(&self->shutdownCond); // Notify if all callbacks are cleared
    }

    pthread_mutex_unlock(&self->mutex);

    PRINT_INFO("PowerCtrl IPC shutdown callback at index %d notified.", index);
    return OK; // Return OK on successful notification
}

void PowerCtrl_PowerReady(void)
{
    PowerCtrl_t* self = GetIpcInstance();

    pthread_mutex_lock(&self->mutex);
    self->isPowerReady = true;
    pthread_cond_broadcast(&self->bootCond); // Notify that power is ready
    pthread_mutex_unlock(&self->mutex);

    PRINT_INFO("PowerCtrl IPC power is ready.");
}

// int PowerCtrl_WaitForShutdown(void)
// {
//     PowerCtrl_t* self = GetIpcInstance();

//     pthread_mutex_lock(&self->mutex);

//     while (self->shutdownBitmap != 0) {
//         pthread_cond_wait(&self->cond, &self->mutex);
//     }

//     pthread_mutex_unlock(&self->mutex);

//     PRINT_INFO("PowerCtrl IPC shutdown wait completed.");
//     return 0; // Return OK on successful wait
// }
