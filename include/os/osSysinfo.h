/*
 * Copyright (c) 2019 TAOS Data, Inc. <jhtao@taosdata.com>
 *
 * This program is free software: you can use, redistribute, and/or modify
 * it under the terms of the GNU Affero General Public License, version 3
 * or later ("AGPL"), as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _TD_OS_SYSINFO_H_
#define _TD_OS_SYSINFO_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "os.h"
  
#define TSDB_LOCALE_LEN   64
#define TSDB_TIMEZONE_LEN 96

typedef struct {
  int64_t total;
  int64_t used;
  int64_t avail;
} SDiskSize;

typedef struct SDiskSpace {
  int64_t   reserved;
  SDiskSize size;
} SDiskSpace;

extern int64_t tsPageSize;
extern int64_t tsOpenMax;
extern int64_t tsStreamMax;
extern int32_t tsNumOfCores;
extern int32_t tsTotalMemoryMB;
extern char    tsTimezone[];
extern char    tsLocale[];
extern char    tsCharset[];  // default encode string



int32_t taosGetDiskSize(char *dataDir, SDiskSize *diskSize);
int32_t taosGetCpuCores();
void    taosGetSystemInfo();
bool    taosReadProcIO(int64_t *rchars, int64_t *wchars);
bool    taosGetProcIO(float *readKB, float *writeKB);
bool    taosGetCardInfo(int64_t *bytes, int64_t *rbytes, int64_t *tbytes);
bool    taosGetBandSpeed(float *bandSpeedKb);
void    taosGetDisk();
bool    taosGetCpuUsage(float *sysCpuUsage, float *procCpuUsage);
bool    taosGetProcMemory(float *memoryUsedMB);
bool    taosGetSysMemory(float *memoryUsedMB);
void    taosPrintOsInfo();
int     taosSystem(const char *cmd);
void    taosKillSystem();
int32_t taosGetSystemUUID(char *uid, int32_t uidlen);
char *  taosGetCmdlineByPID(int pid);
void    taosSetCoreDump(bool enable);

typedef struct {
  char sysname[_UTSNAME_MACHINE_LENGTH];
  char nodename[_UTSNAME_MACHINE_LENGTH];
  char release[_UTSNAME_MACHINE_LENGTH];
  char version[_UTSNAME_MACHINE_LENGTH];
  char machine[_UTSNAME_MACHINE_LENGTH];
} SysNameInfo;

SysNameInfo taosGetSysNameInfo();

#ifdef __cplusplus
}
#endif

#endif
