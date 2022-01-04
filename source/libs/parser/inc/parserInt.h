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

#ifndef _TD_PARSER_INT_H_
#define _TD_PARSER_INT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "catalog.h"
#include "tname.h"
#include "astGenerator.h"

struct SSqlNode;


typedef struct SInternalField {
  TAOS_FIELD      field;
  bool            visible;
  SExprInfo      *pExpr;
} SInternalField;

typedef struct SMsgBuf {
  int32_t len;
  char   *buf;
} SMsgBuf;

// create table operation type
enum TSQL_CREATE_TABLE_TYPE {
  TSQL_CREATE_TABLE  = 0x1,
  TSQL_CREATE_STABLE = 0x2,
  TSQL_CREATE_CTABLE = 0x3,
  TSQL_CREATE_STREAM = 0x4,
};

void clearTableMetaInfo(STableMetaInfo* pTableMetaInfo);

void clearAllTableMetaInfo(SQueryStmtInfo* pQueryInfo, bool removeMeta, uint64_t id);

/**
 * Validate the sql info, according to the corresponding metadata info from catalog.
 * @param pCatalog
 * @param pSqlInfo
 * @param pQueryInfo a bounded AST with essential meta data from local buffer or mgmt node
 * @param id
 * @param msg
 * @return
 */
int32_t qParserValidateSqlNode(struct SCatalog* pCatalog, SSqlInfo* pSqlInfo, SQueryStmtInfo* pQueryInfo, int64_t id, char* msg, int32_t msgLen);

/**
 * validate the ddl ast, and convert the ast to the corresponding message format
 * @param pSqlInfo
 * @param output
 * @param type
 * @return
 */
int32_t qParserValidateDclSqlNode(SSqlInfo* pInfo, SParseBasicCtx* pCtx, SDclStmtInfo* pDcl, char* msgBuf, int32_t msgBufLen);

/**
 * Evaluate the numeric and timestamp arithmetic expression in the WHERE clause.
 * @param pNode
 * @param tsPrecision
 * @param msg
 * @param msgBufLen
 * @return
 */
int32_t evaluateSqlNode(SSqlNode* pNode, int32_t tsPrecision, SMsgBuf* pMsgBuf);

int32_t validateSqlNode(SSqlNode* pSqlNode, SQueryStmtInfo* pQueryInfo, SMsgBuf* pMsgBuf);

SQueryStmtInfo* createQueryInfo();

void destroyQueryInfo(SQueryStmtInfo* pQueryInfo);

int32_t checkForInvalidExpr(SQueryStmtInfo* pQueryInfo, SMsgBuf* pMsgBuf);

/**
 * Extract request meta info from the sql statement
 * @param pSqlInfo
 * @param pMetaInfo
 * @param msg
 * @param msgBufLen
 * @return
 */
int32_t qParserExtractRequestedMetaInfo(const SSqlInfo* pSqlInfo, SCatalogReq* pMetaInfo, char* msg, int32_t msgBufLen);

/**
 * Destroy the meta data request structure.
 * @param pMetaInfo
 */
void qParserClearupMetaRequestInfo(SCatalogReq* pMetaInfo);

#ifdef __cplusplus
}
#endif

#endif /*_TD_PARSER_INT_H_*/