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

#ifndef _TD_DNODE_H_
#define _TD_DNODE_H_

#include "tdef.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------ TYPES EXPOSED ---------------- */
typedef struct SDnode SDnode;

/* ------------------------ Environment ------------------ */
typedef struct {
  int32_t  sver;
  int32_t  numOfCores;
  uint16_t numOfCommitThreads;
  bool     enableTelem;
  bool     printAuth;
  int32_t  rpcTimer;
  int32_t  rpcMaxTime;
  char     timezone[TSDB_TIMEZONE_LEN];
  char     locale[TSDB_LOCALE_LEN];
  char     charset[TSDB_LOCALE_LEN];
  char     buildinfo[64];
  char     gitinfo[48];
} SDnodeEnvCfg;

/**
 * @brief Initialize the environment
 *
 * @param pOption Option of the environment
 * @return int32_t 0 for success and -1 for failure
 */
int32_t dndInit(const SDnodeEnvCfg *pCfg);

/**
 * @brief clear the environment
 *
 */
void dndCleanup();

/* ------------------------ SDnode ----------------------- */
typedef struct {
  int32_t  numOfSupportVnodes;
  int32_t  statusInterval;
  float    numOfThreadsPerCore;
  float    ratioOfQueryCores;
  int32_t  maxShellConns;
  int32_t  shellActivityTimer;
  uint16_t serverPort;
  char     dataDir[TSDB_FILENAME_LEN];
  char     localEp[TSDB_EP_LEN];
  char     localFqdn[TSDB_FQDN_LEN];
  char     firstEp[TSDB_EP_LEN];
  char     secondEp[TSDB_EP_LEN];
} SDnodeObjCfg;

/**
 * @brief Initialize and start the dnode.
 *
 * @param pCfg Config of the dnode.
 * @return SDnode* The dnode object.
 */
SDnode *dndCreate(SDnodeObjCfg *pCfg);

/**
 * @brief Stop and cleanup the dnode.
 *
 * @param pDnode The dnode object to close.
 */
void dndClose(SDnode *pDnode);

#ifdef __cplusplus
}
#endif

#endif /*_TD_DNODE_H_*/
