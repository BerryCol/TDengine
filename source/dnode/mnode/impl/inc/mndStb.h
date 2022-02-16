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

#ifndef _TD_MND_STB_H_
#define _TD_MND_STB_H_

#include "mndInt.h"

#ifdef __cplusplus
extern "C" {
#endif

int32_t  mndInitStb(SMnode *pMnode);
void     mndCleanupStb(SMnode *pMnode);
SStbObj *mndAcquireStb(SMnode *pMnode, char *stbName);
void     mndReleaseStb(SMnode *pMnode, SStbObj *pStb);
SSdbRaw *mndStbActionEncode(SStbObj *pStb);
int32_t  mndValidateStbInfo(SMnode *pMnode, SSTableMetaVersion *pStbs, int32_t numOfStbs, void **ppRsp,
                            int32_t *pRspLen);

#ifdef __cplusplus
}
#endif

#endif /*_TD_MND_STB_H_*/
