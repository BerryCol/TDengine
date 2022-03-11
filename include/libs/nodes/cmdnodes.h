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

#ifndef _TD_CMD_NODES_H_
#define _TD_CMD_NODES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "querynodes.h"

typedef struct SDatabaseOptions {
  int32_t numOfBlocks;
  int32_t cacheBlockSize;
  int8_t cachelast;
  int32_t compressionLevel;
  int32_t daysPerFile;
  int32_t fsyncPeriod;
  int32_t maxRowsPerBlock;
  int32_t minRowsPerBlock;
  int32_t keep;
  int32_t precision;
  int32_t quorum;
  int32_t replica;
  int32_t ttl;
  int32_t walLevel;
  int32_t numOfVgroups;
  int8_t singleStable;
  int8_t streamMode;
} SDatabaseOptions;

typedef struct SCreateDatabaseStmt {
  ENodeType type;
  char dbName[TSDB_DB_NAME_LEN];
  bool ignoreExists;
  SDatabaseOptions options;
} SCreateDatabaseStmt;

typedef struct SUseDatabaseStmt {
  ENodeType type;
  char dbName[TSDB_DB_NAME_LEN];
} SUseDatabaseStmt;

typedef struct SDropDatabaseStmt {
  ENodeType type;
  char dbName[TSDB_DB_NAME_LEN];
  bool ignoreNotExists;
} SDropDatabaseStmt;

typedef struct STableOptions {
  int32_t keep;
  int32_t ttl;
  char comments[TSDB_STB_COMMENT_LEN];
  SNodeList* pSma;
} STableOptions;

typedef struct SColumnDefNode {
  ENodeType type;
  char colName[TSDB_COL_NAME_LEN];
  SDataType dataType;
  char comments[TSDB_STB_COMMENT_LEN];
} SColumnDefNode;

typedef struct SCreateTableStmt {
  ENodeType type;
  char dbName[TSDB_DB_NAME_LEN];
  char tableName[TSDB_TABLE_NAME_LEN];
  bool ignoreExists;
  SNodeList* pCols;
  SNodeList* pTags;
  STableOptions options;
} SCreateTableStmt;

typedef struct SCreateSubTableClause {
  ENodeType type;
  char dbName[TSDB_DB_NAME_LEN];
  char tableName[TSDB_TABLE_NAME_LEN];
  char useDbName[TSDB_DB_NAME_LEN];
  char useTableName[TSDB_TABLE_NAME_LEN];
  bool ignoreExists;
  SNodeList* pSpecificTags;
  SNodeList* pValsOfTags;
} SCreateSubTableClause;

typedef struct SCreateMultiTableStmt {
  ENodeType type;
  SNodeList* pSubTables;
} SCreateMultiTableStmt;

typedef struct SDropTableClause {
  ENodeType type;
  char dbName[TSDB_DB_NAME_LEN];
  char tableName[TSDB_TABLE_NAME_LEN];
  bool ignoreNotExists;
} SDropTableClause;

typedef struct SDropTableStmt {
  ENodeType type;
  SNodeList* pTables;
} SDropTableStmt;

typedef struct SDropSuperTableStmt {
  ENodeType type;
  char dbName[TSDB_DB_NAME_LEN];
  char tableName[TSDB_TABLE_NAME_LEN];
  bool ignoreNotExists;
} SDropSuperTableStmt;

typedef struct SCreateUserStmt {
  ENodeType type;
  char useName[TSDB_USER_LEN];
  char password[TSDB_USET_PASSWORD_LEN];
} SCreateUserStmt;

typedef struct SAlterUserStmt {
  ENodeType type;
  char useName[TSDB_USER_LEN];
  char password[TSDB_USET_PASSWORD_LEN];
  int8_t alterType;
} SAlterUserStmt;

typedef struct SDropUserStmt {
  ENodeType type;
  char useName[TSDB_USER_LEN];
} SDropUserStmt;

typedef struct SCreateDnodeStmt {
  ENodeType type;
  char fqdn[TSDB_FQDN_LEN];
  int32_t port;
} SCreateDnodeStmt;

typedef struct SDropDnodeStmt {
  ENodeType type;
  int32_t dnodeId;
  char fqdn[TSDB_FQDN_LEN];
  int32_t port;
} SDropDnodeStmt;

typedef struct SShowStmt {
  ENodeType type;
  char dbName[TSDB_DB_NAME_LEN];
} SShowStmt;

#ifdef __cplusplus
}
#endif

#endif /*_TD_CMD_NODES_H_*/
