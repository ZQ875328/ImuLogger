#ifndef POWERCTRL_PUBLIC_H
#define POWERCTRL_PUBLIC_H
/* TYPES */

typedef void (*PowerCtrl_ShutdownCallback_t)(void);

/* PROTOTYPES */

int PowerCtrl_SetShutdownCallback(PowerCtrl_ShutdownCallback_t callback);
int PowerCtrl_NotifyStop(int index);

#endif /* POWERCTRL_PUBLIC_H */
