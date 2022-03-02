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

#ifndef _TD_OS_ENV_H_
#define _TD_OS_ENV_H_

#include "osSysinfo.h"

#ifdef __cplusplus
extern "C" {
#endif

extern char    tsOsName[];
extern char    tsTimezone[];
extern char    tsCharset[];
extern char    tsLocale[];
extern int8_t  tsDaylight;
extern bool    tsEnableCoreFile;
extern int64_t tsPageSize;
extern int64_t tsOpenMax;
extern int64_t tsStreamMax;
extern int32_t tsNumOfCores;
extern int32_t tsTotalMemoryMB;

extern char configDir[];
extern char tsDataDir[];
extern char tsLogDir[];
extern char tsTempDir[];

extern SDiskSpace tsDataSpace;
extern SDiskSpace tsLogSpace;
extern SDiskSpace tsTempSpace;

void osInit();
void osUpdate();
bool osLogSpaceAvailable();
void osSetTimezone(const char *timezone);

#ifdef __cplusplus
}
#endif

#endif /*_TD_OS_ENV_H_*/