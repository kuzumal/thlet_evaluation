#ifndef THLET_INTR_H
#define THLET_INTR_H

#define DEV_MAJOR 250
#define DEV_MINOR 0
#define DEV_NAME "thlet_intr"
#define RECV_CORE 1

#define IOCTL_NOACTION _IO(DEV_MAJOR, 0)
#define IOCTL_SENDIPI _IO(DEV_MAJOR, 1)
#define IOCTL_REGISTER_HANDLER _IO(DEV_MAJOR, 2)
#define IOCTL_TEST _IO(DEV_MAJOR, 3)

typedef struct {
  struct {
    int64_t intr;
    int64_t back;
  } ts;
} pipe_;

#endif