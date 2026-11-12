#ifndef SD_DISKIO_H
#define SD_DISKIO_H

#include <stdint.h>

/* Kiem tra the da khoi tao con phan hoi hay khong. */
uint8_t SD_DiskProbe(void);

/*
 * Thu reset the mot lan truoc khi khoi tao day du.
 *
 * Ham nay reset trang thai the:
 * chi goi khi SD dang offline, khong co file dang mo
 * va khong co thao tac doc/ghi khac.
 */
uint8_t SD_DiskTryReset(void);

#endif
