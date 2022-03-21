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

#ifndef _TD_MND_H_
#define _TD_MND_H_

#include "monitor.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------ TYPES EXPOSED ------------------------ */
typedef struct SMgmtWrapper SMgmtWrapper;
typedef struct SMnode       SMnode;

typedef struct {
  int32_t        dnodeId;
  int64_t        clusterId;
  int8_t         replica;
  int8_t         selfIndex;
  SReplica       replicas[TSDB_MAX_REPLICA];
  SMgmtWrapper  *pWrapper;
  PutToQueueFp   putToWriteQFp;
  PutToQueueFp   putToReadQFp;
  SendReqFp      sendReqFp;
  SendMnodeReqFp sendMnodeReqFp;
  SendRspFp      sendRspFp;
} SMnodeOpt;

/* ------------------------ SMnode ------------------------ */
/**
 * @brief Open a mnode.
 *
 * @param path Path of the mnode.
 * @param pOption Option of the mnode.
 * @return SMnode* The mnode object.
 */
SMnode *mndOpen(const char *path, const SMnodeOpt *pOption);

/**
 * @brief Close a mnode.
 *
 * @param pMnode The mnode object to close.
 */
void mndClose(SMnode *pMnode);

/**
 * @brief Close a mnode.
 *
 * @param pMnode The mnode object to close.
 * @param pOption Options of the mnode.
 * @return int32_t 0 for success, -1 for failure.
 */
int32_t mndAlter(SMnode *pMnode, const SMnodeOpt *pOption);

/**
 * @brief Start mnode
 *
 * @param pMnode The mnode object.
 */
int32_t mndStart(SMnode *pMnode);

/**
 * @brief Get mnode monitor info.
 *
 * @param pMnode The mnode object.
 * @param pClusterInfo
 * @param pVgroupInfo
 * @param pGrantInfo
 * @return int32_t 0 for success, -1 for failure.
 */
int32_t mndGetMonitorInfo(SMnode *pMnode, SMonClusterInfo *pClusterInfo, SMonVgroupInfo *pVgroupInfo,
                          SMonGrantInfo *pGrantInfo);

/**
 * @brief Get user authentication info.
 *
 * @param pMnode The mnode object.
 * @param user
 * @param spi
 * @param encrypt
 * @param secret
 * @param ckey
 * @return int32_t 0 for success, -1 for failure.
 */
int32_t mndRetriveAuth(SMnode *pMnode, char *user, char *spi, char *encrypt, char *secret, char *ckey);

/**
 * @brief Process the read, write, sync request.
 *
 * @param pMsg The request msg.
 * @return int32_t 0 for success, -1 for failure.
 */
int32_t mndProcessMsg(SNodeMsg *pMsg);

#ifdef __cplusplus
}
#endif

#endif /*_TD_MND_H_*/
