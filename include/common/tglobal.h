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

#ifndef _TD_COMMON_GLOBAL_H_
#define _TD_COMMON_GLOBAL_H_

#include "tarray.h"
#include "tdef.h"

#ifdef __cplusplus
extern "C" {
#endif

// cluster
extern char     tsFirst[];
extern char     tsSecond[];
extern char     tsLocalFqdn[];
extern char     tsLocalEp[];
extern uint16_t tsServerPort;
extern int32_t  tsVersion;
extern int32_t  tsStatusInterval;
extern bool     tsEnableTelemetryReporting;

// common
extern int32_t tsRpcTimer;
extern int32_t tsRpcMaxTime;
extern bool    tsRpcForceTcp;  // all commands go to tcp protocol if this is enabled
extern int32_t tsMaxConnections;
extern int32_t tsMaxShellConns;
extern int32_t tsShellActivityTimer;
extern int32_t tsMaxTmrCtrl;
extern float   tsNumOfThreadsPerCore;
extern int32_t tsNumOfCommitThreads;
extern float   tsRatioOfQueryCores;
extern int32_t tsCompressMsgSize;
extern int32_t tsCompressColData;
extern int32_t tsMaxNumOfDistinctResults;
extern int32_t tsCompatibleModel;
extern bool    tsEnableSlaveQuery;
extern bool    tsPrintAuth;
extern int64_t tsTickPerDay[3];
extern int32_t tsMultiProcess;

// monitor
extern bool     tsEnableMonitor;
extern int32_t  tsMonitorInterval;
extern char     tsMonitorFqdn[];
extern uint16_t tsMonitorPort;
extern int32_t  tsMonitorMaxLogs;
extern bool     tsMonitorComp;

// query buffer management
extern int32_t tsQueryBufferSize;  // maximum allowed usage buffer size in MB for each data node during query processing
extern int64_t tsQueryBufferSizeBytes;   // maximum allowed usage buffer size in byte for each data node
extern bool    tsRetrieveBlockingModel;  // retrieve threads will be blocked
extern bool    tsKeepOriginalColumnName;
extern bool    tsDeadLockKillQuery;

// client
extern int32_t tsMaxWildCardsLen;
extern int32_t tsMaxRegexStringLen;
extern int32_t tsMaxNumOfOrderedResults;
extern int32_t tsMinSlidingTime;
extern int32_t tsMinIntervalTime;
extern int32_t tsMaxStreamComputDelay;
extern int32_t tsStreamCompStartDelay;
extern int32_t tsRetryStreamCompDelay;
extern float   tsStreamComputDelayRatio;  // the delayed computing ration of the whole time window
extern int32_t tsProjectExecInterval;
extern int64_t tsMaxRetentWindow;

// build info
extern char version[];
extern char compatible_version[];
extern char gitinfo[];
extern char buildinfo[];

// lossy
extern char     tsLossyColumns[];
extern double   tsFPrecision;
extern double   tsDPrecision;
extern uint32_t tsMaxRange;
extern uint32_t tsCurRange;
extern char     tsCompressor[];

// tfs
extern int32_t  tsDiskCfgNum;
extern SDiskCfg tsDiskCfg[];

#define NEEDTO_COMPRESSS_MSG(size) (tsCompressMsgSize != -1 && (size) > tsCompressMsgSize)

int32_t taosCreateLog(const char *logname, int32_t logFileNum, const char *cfgDir, const char *envFile,
                      const char *apolloUrl, SArray *pArgs, bool tsc);
int32_t taosInitCfg(const char *cfgDir, const char *envFile, const char *apolloUrl, SArray *pArgs, bool tsc);
void    taosCleanupCfg();
void    taosCfgDynamicOptions(const char *option, const char *value);

struct SConfig *taosGetCfg();

#ifdef __cplusplus
}
#endif

#endif /*_TD_COMMON_GLOBAL_H_*/
