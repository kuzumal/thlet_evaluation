#ifndef THLET_SWITCH_HELPER_H
#define THLET_SWITCH_HELPER_H
#include <linux/sched.h>

#define DEV_MAJOR            452
#define DEV_MINOR            0
#define DEV_NAME             "thlet_switch_helper"
#define THLET_SWITCH_ITER    100
#define THLET_SWITCH_THREADS 2
#define THLET_LOOPS          500
#define THLET_RUNNING_CORE   19

#define THLET_IOCTL_START _IO(DEV_MAJOR, 0)
#define THLET_IOCTL_FINISH _IO(DEV_MAJOR, 1)
#define THLET_IOCTL_CLEAR _IO(DEV_MAJOR, 2)
#endif