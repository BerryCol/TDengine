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

#ifndef _TD_VNODE_BUFFER_POOL_H_
#define _TD_VNODE_BUFFER_POOL_H_

#include "tlist.h"
#include "vnode.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SVBufPool SVBufPool;

int   vnodeOpenBufPool(SVnode *pVnode);
void  vnodeCloseBufPool(SVnode *pVnode);
int   vnodeBufPoolSwitch(SVnode *pVnode);
int   vnodeBufPoolRecycle(SVnode *pVnode);
void *vnodeMalloc(SVnode *pVnode, uint64_t size);
bool  vnodeBufPoolIsFull(SVnode *pVnode);

SMemAllocatorFactory *vBufPoolGetMAF(SVnode *pVnode);

#ifdef __cplusplus
}
#endif

#endif /*_TD_VNODE_BUFFER_POOL_H_*/