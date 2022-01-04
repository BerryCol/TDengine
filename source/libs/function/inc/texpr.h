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

#ifndef _TD_COMMON_EXPR_H_
#define _TD_COMMON_EXPR_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "os.h"

#include "tmsg.h"
#include "taosdef.h"
#include "tskiplist.h"
#include "function.h"

struct tExprNode;
struct SSchema;

#define QUERY_COND_REL_PREFIX_IN "IN|"
#define QUERY_COND_REL_PREFIX_LIKE "LIKE|"
#define QUERY_COND_REL_PREFIX_MATCH "MATCH|"
#define QUERY_COND_REL_PREFIX_NMATCH "NMATCH|"

#define QUERY_COND_REL_PREFIX_IN_LEN   3
#define QUERY_COND_REL_PREFIX_LIKE_LEN 5
#define QUERY_COND_REL_PREFIX_MATCH_LEN 6
#define QUERY_COND_REL_PREFIX_NMATCH_LEN 7

typedef bool (*__result_filter_fn_t)(const void *, void *);
typedef void (*__do_filter_suppl_fn_t)(void *, void *);

/**
 * this structure is used to filter data in tags, so the offset of filtered tag column in tagdata string is required
 */
typedef struct tQueryInfo {
  uint8_t       optr;     // expression operator
  SSchema       sch;      // schema of tags
  char*         q;
  __compar_fn_t compare;  // filter function
  bool          indexed;  // indexed columns
} tQueryInfo;

typedef struct SExprTraverseSupp {
  __result_filter_fn_t   nodeFilterFn;
  __do_filter_suppl_fn_t setupInfoFn;
  void                  *pExtInfo;
} SExprTraverseSupp;

tExprNode* exprTreeFromTableName(const char* tbnameCond);

bool exprTreeApplyFilter(tExprNode *pExpr, const void *pItem, SExprTraverseSupp *param);

void buildFilterSetFromBinary(void **q, const char *buf, int32_t len);

#ifdef __cplusplus
}
#endif

#endif  /*_TD_COMMON_EXPR_H_*/
