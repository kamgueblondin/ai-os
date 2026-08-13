#ifndef OVERLAY_H
#define OVERLAY_H

#include <stdint.h>
#include "os_syscalls.h"

#define OV_OK            0
#define OV_ERR_NOTFOUND -1
#define OV_ERR_EXISTS   -2
#define OV_ERR_NOTDIR   -3
#define OV_ERR_ISDIR    -4
#define OV_ERR_NOTEMPTY -5
#define OV_ERR_NOSPACE  -6
#define OV_ERR_INVAL    -7
#define OV_ERR_PROTECTED -8

void overlay_init(void);
int overlay_mkdir(const char* path);
int overlay_write(const char* path, const char* data, uint32_t n);
int overlay_append(const char* path, const char* data, uint32_t n);
int overlay_unlink(const char* path);
int overlay_rename(const char* oldpath, const char* newpath);
int overlay_copy(const char* src, const char* dst);
int overlay_read(const char* path, char* buf, uint32_t max);
int overlay_stat(const char* path, os_dirent_t* out);
int overlay_listdir(const char* path, os_dirent_t* out, int start, int max_n);
int overlay_is_dir(const char* path);

#endif
