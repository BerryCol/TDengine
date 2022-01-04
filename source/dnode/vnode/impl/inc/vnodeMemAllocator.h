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

#ifndef _TD_VNODE_MEM_ALLOCATOR_H_
#define _TD_VNODE_MEM_ALLOCATOR_H_

#include "os.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SVArenaNode {
  TD_SLIST_NODE(SVArenaNode);
  uint64_t size;  // current node size
  void *   ptr;
  char     data[];
} SVArenaNode;

typedef struct SVMemAllocator {
  T_REF_DECLARE()
  TD_DLIST_NODE(SVMemAllocator);
  uint64_t     capacity;
  uint64_t     ssize;
  uint64_t     lsize;
  SVArenaNode *pNode;
  TD_SLIST(SVArenaNode) nlist;
} SVMemAllocator;

SVMemAllocator *vmaCreate(uint64_t capacity, uint64_t ssize, uint64_t lsize);
void            vmaDestroy(SVMemAllocator *pVMA);
void            vmaReset(SVMemAllocator *pVMA);
void *          vmaMalloc(SVMemAllocator *pVMA, uint64_t size);
void            vmaFree(SVMemAllocator *pVMA, void *ptr);
bool            vmaIsFull(SVMemAllocator *pVMA);

#ifdef __cplusplus
}
#endif

#endif /*_TD_VNODE_MEM_ALLOCATOR_H_*/
