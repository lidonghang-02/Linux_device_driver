#ifndef _IOCTL_CHR_H_
#define _IOCTL_CHR_H_

#define IOCTL_CHR_MAGIC 'c'

#define IOCTL_CLEAN _IO(IOCTL_CHR_MAGIC, 0)
#define IOCTL_GET_LEN _IOR(IOCTL_CHR_MAGIC, 1, unsigned int)
#define IOCTL_SET_LEN _IOW(IOCTL_CHR_MAGIC, 2, unsigned int)

#define IOCTL_MAXNR 2

#endif // _IOCTL_CHR_H_