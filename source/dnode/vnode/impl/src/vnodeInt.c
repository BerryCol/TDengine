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

#define _DEFAULT_SOURCE
#include "vnodeInt.h"

int32_t vnodeAlter(SVnode *pVnode, const SVnodeCfg *pCfg) { return 0; }

int32_t vnodeCompact(SVnode *pVnode) { return 0; }

int32_t vnodeSync(SVnode *pVnode) { return 0; }

int32_t vnodeGetLoad(SVnode *pVnode, SVnodeLoad *pLoad) { return 0; }

int vnodeProcessSyncReq(SVnode *pVnode, SRpcMsg *pMsg, SRpcMsg **pRsp) {
  vInfo("sync message is processed");
  return 0;
}

int vnodeProcessConsumeReq(SVnode *pVnode, SRpcMsg *pMsg, SRpcMsg **pRsp) {
  vInfo("consume message is processed");
  return 0;
}
