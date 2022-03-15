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

#ifndef _TD_SNODE_H_
#define _TD_SNODE_H_

#include "tmsg.h"
#include "trpc.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SND_UNIQUE_THREAD_NUM 2
#define SND_SHARED_THREAD_NUM 2

/* ------------------------ TYPES EXPOSED ------------------------ */
typedef struct SDnode SDnode;
typedef struct SSnode SSnode;
typedef int32_t (*SendReqToDnodeFp)(SDnode *pDnode, struct SEpSet *epSet, struct SRpcMsg *pMsg);
typedef int32_t (*SendReqToMnodeFp)(SDnode *pDnode, struct SRpcMsg *pMsg);
typedef void (*SendRedirectRspFp)(SDnode *pDnode, struct SRpcMsg *pMsg);

typedef struct {
  int64_t numOfErrors;
} SSnodeLoad;

typedef struct {
  int32_t           sver;
  int32_t           dnodeId;
  int64_t           clusterId;
  SDnode           *pDnode;
  SendReqToDnodeFp  sendReqToDnodeFp;
  SendReqToMnodeFp  sendReqToMnodeFp;
  SendRedirectRspFp sendRedirectRspFp;
} SSnodeOpt;

/* ------------------------ SSnode ------------------------ */
/**
 * @brief Start one Snode in Dnode.
 *
 * @param path Path of the snode.
 * @param pOption Option of the snode.
 * @return SSnode* The snode object.
 */
SSnode *sndOpen(const char *path, const SSnodeOpt *pOption);

/**
 * @brief Stop Snode in Dnode.
 *
 * @param pSnode The snode object to close.
 */
void sndClose(SSnode *pSnode);

/**
 * @brief Get the statistical information of Snode
 *
 * @param pSnode The snode object.
 * @param pLoad Statistics of the snode.
 * @return int32_t 0 for success, -1 for failure.
 */
int32_t sndGetLoad(SSnode *pSnode, SSnodeLoad *pLoad);

/**
 * @brief Process a query message.
 *
 * @param pSnode The snode object.
 * @param pMsg The request message
 * @param pRsp The response message
 * @return int32_t 0 for success, -1 for failure
 */
int32_t sndProcessMsg(SSnode *pSnode, SRpcMsg *pMsg, SRpcMsg **pRsp);

int32_t sndProcessUMsg(SSnode *pSnode, SRpcMsg *pMsg);

int32_t sndProcessSMsg(SSnode *pSnode, SRpcMsg *pMsg);

/**
 * @brief Drop a snode.
 *
 * @param path Path of the snode.
 */
void sndDestroy(const char *path);

#ifdef __cplusplus
}
#endif

#endif /*_TD_SNODE_H_*/
