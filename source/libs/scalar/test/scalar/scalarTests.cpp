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

#include <gtest/gtest.h>
#include <iostream>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwrite-strings"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wformat"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-arith"
#include <addr_any.h>

#include "os.h"

#include "tglobal.h"
#include "taos.h"
#include "tdef.h"
#include "tvariant.h"
#include "tdatablock.h"
#include "stub.h"
#include "scalar.h"
#include "nodes.h"
#include "tlog.h"

namespace {

SColumnInfo createColumnInfo(int32_t colId, int32_t type, int32_t bytes) {
  SColumnInfo info = {0};
  info.colId = colId;
  info.type = type;
  info.bytes = bytes;
  return info;
}

int64_t scltLeftV = 21, scltRightV = 10;
double scltLeftVd = 21.0, scltRightVd = 10.0;

void scltFreeDataBlock(void *block) {
  blockDataDestroy(*(SSDataBlock **)block);
}

void scltInitLogFile() {
  const char   *defaultLogFileNamePrefix = "taoslog";
  const int32_t maxLogFileNum = 10;

  tsAsyncLog = 0;
  qDebugFlag = 159;
  strcpy(tsLogDir, "/var/log/taos");

  if (taosInitLog(defaultLogFileNamePrefix, maxLogFileNum) < 0) {
    printf("failed to open log file in directory:%s\n", tsLogDir);
  }
}

void scltAppendReservedSlot(SArray *pBlockList, int16_t *dataBlockId, int16_t *slotId, bool newBlock, int32_t rows, SColumnInfo *colInfo) {
  if (newBlock) {
    SSDataBlock *res = (SSDataBlock *)calloc(1, sizeof(SSDataBlock));
    res->info.numOfCols = 1;
    res->info.rows = rows;
    res->pDataBlock = taosArrayInit(1, sizeof(SColumnInfoData));
    SColumnInfoData idata = {0};
    idata.info  = *colInfo;

    taosArrayPush(res->pDataBlock, &idata);
    taosArrayPush(pBlockList, &res);
    
    blockDataEnsureCapacity(res, rows);

    *dataBlockId = taosArrayGetSize(pBlockList) - 1;
    *slotId = 0;
  } else {
    SSDataBlock *res = *(SSDataBlock **)taosArrayGetLast(pBlockList);
    res->info.numOfCols++;
    SColumnInfoData idata = {0};
    idata.info  = *colInfo;

    blockDataEnsureColumnCapacity(&idata, rows);

    taosArrayPush(res->pDataBlock, &idata);
    
    *dataBlockId = taosArrayGetSize(pBlockList) - 1;
    *slotId = taosArrayGetSize(res->pDataBlock) - 1;
  }
}

void scltMakeValueNode(SNode **pNode, int32_t dataType, void *value) {
  SNode *node = (SNode*)nodesMakeNode(QUERY_NODE_VALUE);
  SValueNode *vnode = (SValueNode *)node;
  vnode->node.resType.type = dataType;

  if (IS_VAR_DATA_TYPE(dataType)) {
    vnode->datum.p = (char *)malloc(varDataTLen(value));
    varDataCopy(vnode->datum.p, value);
    vnode->node.resType.bytes = varDataTLen(value);
  } else {
    vnode->node.resType.bytes = tDataTypes[dataType].bytes;
    assignVal((char *)nodesGetValueFromNode(vnode), (const char *)value, 0, dataType);
  }
  
  *pNode = (SNode *)vnode;
}

void scltMakeColumnNode(SNode **pNode, SSDataBlock **block, int32_t dataType, int32_t dataBytes, int32_t rowNum, void *value) {
  SNode *node = (SNode*)nodesMakeNode(QUERY_NODE_COLUMN);
  SColumnNode *rnode = (SColumnNode *)node;
  rnode->node.resType.type = dataType;
  rnode->node.resType.bytes = dataBytes;
  rnode->dataBlockId = 0;

  if (NULL == *block) {
    SSDataBlock *res = (SSDataBlock *)calloc(1, sizeof(SSDataBlock));
    res->info.numOfCols = 3;
    res->info.rows = rowNum;
    res->pDataBlock = taosArrayInit(3, sizeof(SColumnInfoData));
    for (int32_t i = 0; i < 2; ++i) {
      SColumnInfoData idata = {{0}};
      idata.info.type  = TSDB_DATA_TYPE_NULL;
      idata.info.bytes = 10;
      idata.info.colId = i + 1;

      int32_t size = idata.info.bytes * rowNum;
      idata.pData = (char *)calloc(1, size);
      taosArrayPush(res->pDataBlock, &idata);
    }

    SColumnInfoData idata = {{0}};
    idata.info.type  = dataType;
    idata.info.bytes = dataBytes;
    idata.info.colId = 3;
    int32_t size = idata.info.bytes * rowNum;
    idata.pData = (char *)calloc(1, size);
    taosArrayPush(res->pDataBlock, &idata);
    
    blockDataEnsureCapacity(res, rowNum);

    SColumnInfoData *pColumn = (SColumnInfoData *)taosArrayGetLast(res->pDataBlock);
    for (int32_t i = 0; i < rowNum; ++i) {
      colDataAppend(pColumn, i, (const char *)value, false);
      if (IS_VAR_DATA_TYPE(dataType)) {
        value = (char *)value + varDataTLen(value);
      } else {
        value = (char *)value + dataBytes;
      }
    }

    rnode->slotId = 2;
    rnode->colId = 3;

    *block = res;
  } else {
    SSDataBlock *res = *block;
    
    int32_t idx = taosArrayGetSize(res->pDataBlock);
    SColumnInfoData idata = {{0}};
    idata.info.type  = dataType;
    idata.info.bytes = dataBytes;
    idata.info.colId = 1 + idx;
    int32_t size = idata.info.bytes * rowNum;
    idata.pData = (char *)calloc(1, size);
    taosArrayPush(res->pDataBlock, &idata);
    res->info.numOfCols++;
    SColumnInfoData *pColumn = (SColumnInfoData *)taosArrayGetLast(res->pDataBlock);
    
    blockDataEnsureColumnCapacity(pColumn, rowNum);

    for (int32_t i = 0; i < rowNum; ++i) {
      colDataAppend(pColumn, i, (const char *)value, false);
      if (IS_VAR_DATA_TYPE(dataType)) {
        value = (char *)value + varDataTLen(value);
      } else {
        value = (char *)value + dataBytes;
      }
    }
    
    rnode->slotId = idx;
    rnode->colId = 1 + idx;
  }

  *pNode = (SNode *)rnode;
}

void scltMakeOpNode(SNode **pNode, EOperatorType opType, int32_t resType, SNode *pLeft, SNode *pRight) {
  SNode *node = (SNode*)nodesMakeNode(QUERY_NODE_OPERATOR);
  SOperatorNode *onode = (SOperatorNode *)node;
  onode->node.resType.type = resType;
  onode->node.resType.bytes = tDataTypes[resType].bytes;
  
  onode->opType = opType;
  onode->pLeft = pLeft;
  onode->pRight = pRight;

  *pNode = (SNode *)onode;
}


void scltMakeListNode(SNode **pNode, SNodeList *list, int32_t resType) {
  SNode *node = (SNode*)nodesMakeNode(QUERY_NODE_NODE_LIST);
  SNodeListNode *lnode = (SNodeListNode *)node;
  lnode->dataType.type = resType;
  lnode->pNodeList = list;

  *pNode = (SNode *)lnode;
}


void scltMakeLogicNode(SNode **pNode, ELogicConditionType opType, SNode **nodeList, int32_t nodeNum) {
  SNode *node = (SNode*)nodesMakeNode(QUERY_NODE_LOGIC_CONDITION);
  SLogicConditionNode *onode = (SLogicConditionNode *)node;
  onode->condType = opType;
  onode->node.resType.type = TSDB_DATA_TYPE_BOOL;
  onode->node.resType.bytes = sizeof(bool);

  onode->pParameterList = nodesMakeList();
  for (int32_t i = 0; i < nodeNum; ++i) {
    nodesListAppend(onode->pParameterList, nodeList[i]);
  }
  
  *pNode = (SNode *)onode;
}

void scltMakeTargetNode(SNode **pNode, int16_t dataBlockId, int16_t slotId, SNode *snode) {
  SNode *node = (SNode*)nodesMakeNode(QUERY_NODE_TARGET);
  STargetNode *onode = (STargetNode *)node;
  onode->pExpr = snode;
  onode->dataBlockId = dataBlockId;
  onode->slotId = slotId;
  
  *pNode = (SNode *)onode;
}



}

TEST(constantTest, bigint_add_bigint) {
  SNode *pLeft = NULL, *pRight = NULL, *opNode = NULL, *res = NULL;
  scltMakeValueNode(&pLeft, TSDB_DATA_TYPE_BIGINT, &scltLeftV);
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_BIGINT, &scltRightV);
  scltMakeOpNode(&opNode, OP_TYPE_ADD, TSDB_DATA_TYPE_DOUBLE, pLeft, pRight);
  
  int32_t code = scalarCalculateConstants(opNode, &res);
  ASSERT_EQ(code, 0);
  ASSERT_TRUE(res);
  ASSERT_EQ(nodeType(res), QUERY_NODE_VALUE);
  SValueNode *v = (SValueNode *)res;
  ASSERT_EQ(v->node.resType.type, TSDB_DATA_TYPE_DOUBLE);
  ASSERT_EQ(v->datum.d, (scltLeftV + scltRightV));
  nodesDestroyNode(res);
}

TEST(constantTest, double_sub_bigint) {
  SNode *pLeft = NULL, *pRight = NULL, *opNode = NULL, *res = NULL;
  scltMakeValueNode(&pLeft, TSDB_DATA_TYPE_DOUBLE, &scltLeftVd);
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_BIGINT, &scltRightV);
  scltMakeOpNode(&opNode, OP_TYPE_SUB, TSDB_DATA_TYPE_DOUBLE, pLeft, pRight);
  
  int32_t code = scalarCalculateConstants(opNode, &res);
  ASSERT_EQ(code, 0);
  ASSERT_TRUE(res);
  ASSERT_EQ(nodeType(res), QUERY_NODE_VALUE);
  SValueNode *v = (SValueNode *)res;
  ASSERT_EQ(v->node.resType.type, TSDB_DATA_TYPE_DOUBLE);
  ASSERT_EQ(v->datum.d, (scltLeftVd - scltRightV));
  nodesDestroyNode(res);  
}

TEST(constantTest, tinyint_and_smallint) {
  SNode *pLeft = NULL, *pRight = NULL, *opNode = NULL, *res = NULL;
  scltMakeValueNode(&pLeft, TSDB_DATA_TYPE_TINYINT, &scltLeftV);
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_SMALLINT, &scltRightV);
  scltMakeOpNode(&opNode, OP_TYPE_BIT_AND, TSDB_DATA_TYPE_BIGINT, pLeft, pRight);
  
  int32_t code = scalarCalculateConstants(opNode, &res);
  ASSERT_EQ(code, 0);
  ASSERT_TRUE(res);
  ASSERT_EQ(nodeType(res), QUERY_NODE_VALUE);
  SValueNode *v = (SValueNode *)res;
  ASSERT_EQ(v->node.resType.type, TSDB_DATA_TYPE_BIGINT);
  ASSERT_EQ(v->datum.i, (int64_t)scltLeftV & (int64_t)scltRightV);
  nodesDestroyNode(res);  
}

TEST(constantTest, bigint_or_double) {
  SNode *pLeft = NULL, *pRight = NULL, *opNode = NULL, *res = NULL;
  scltMakeValueNode(&pLeft, TSDB_DATA_TYPE_BIGINT, &scltLeftV);
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_DOUBLE, &scltRightVd);
  scltMakeOpNode(&opNode, OP_TYPE_BIT_OR, TSDB_DATA_TYPE_BIGINT, pLeft, pRight);
  
  int32_t code = scalarCalculateConstants(opNode, &res);
  ASSERT_EQ(code, 0);
  ASSERT_TRUE(res);
  ASSERT_EQ(nodeType(res), QUERY_NODE_VALUE);
  SValueNode *v = (SValueNode *)res;
  ASSERT_EQ(v->node.resType.type, TSDB_DATA_TYPE_BIGINT);
  ASSERT_EQ(v->datum.i, (int64_t)scltLeftV | (int64_t)scltRightVd);
  nodesDestroyNode(res);
}

TEST(constantTest, int_or_binary) {
  SNode *pLeft = NULL, *pRight = NULL, *opNode = NULL, *res = NULL;
  char binaryStr[64] = {0};
  sprintf(&binaryStr[2], "%d", scltRightV);
  varDataSetLen(binaryStr, strlen(&binaryStr[2]));
  scltMakeValueNode(&pLeft, TSDB_DATA_TYPE_INT, &scltLeftV);
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_BINARY, binaryStr);
  scltMakeOpNode(&opNode, OP_TYPE_BIT_OR, TSDB_DATA_TYPE_BIGINT, pLeft, pRight);
  
  int32_t code = scalarCalculateConstants(opNode, &res);
  ASSERT_EQ(code, 0);
  ASSERT_TRUE(res);
  ASSERT_EQ(nodeType(res), QUERY_NODE_VALUE);
  SValueNode *v = (SValueNode *)res;
  ASSERT_EQ(v->node.resType.type, TSDB_DATA_TYPE_BIGINT);
  ASSERT_EQ(v->datum.b, scltLeftV | scltRightV);
  nodesDestroyNode(res);
}


TEST(constantTest, int_greater_double) {
  SNode *pLeft = NULL, *pRight = NULL, *opNode = NULL, *res = NULL;
  scltMakeValueNode(&pLeft, TSDB_DATA_TYPE_INT, &scltLeftV);
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_DOUBLE, &scltRightVd);
  scltMakeOpNode(&opNode, OP_TYPE_GREATER_THAN, TSDB_DATA_TYPE_BOOL, pLeft, pRight);
  
  int32_t code = scalarCalculateConstants(opNode, &res);
  ASSERT_EQ(code, 0);
  ASSERT_TRUE(res);
  ASSERT_EQ(nodeType(res), QUERY_NODE_VALUE);
  SValueNode *v = (SValueNode *)res;
  ASSERT_EQ(v->node.resType.type, TSDB_DATA_TYPE_BOOL);
  ASSERT_EQ(v->datum.b, scltLeftV > scltRightVd);
  nodesDestroyNode(res);
}

TEST(constantTest, int_greater_equal_binary) {
  SNode *pLeft = NULL, *pRight = NULL, *opNode = NULL, *res = NULL;
  char binaryStr[64] = {0};
  sprintf(&binaryStr[2], "%d", scltRightV);
  varDataSetLen(binaryStr, strlen(&binaryStr[2]));
  scltMakeValueNode(&pLeft, TSDB_DATA_TYPE_INT, &scltLeftV);
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_BINARY, binaryStr);
  scltMakeOpNode(&opNode, OP_TYPE_GREATER_THAN, TSDB_DATA_TYPE_BOOL, pLeft, pRight);
  
  int32_t code = scalarCalculateConstants(opNode, &res);
  ASSERT_EQ(code, 0);
  ASSERT_TRUE(res);
  ASSERT_EQ(nodeType(res), QUERY_NODE_VALUE);
  SValueNode *v = (SValueNode *)res;
  ASSERT_EQ(v->node.resType.type, TSDB_DATA_TYPE_BOOL);
  ASSERT_EQ(v->datum.b, scltLeftV > scltRightVd);
  nodesDestroyNode(res);
}

TEST(constantTest, tinyint_lower_ubigint) {
  SNode *pLeft = NULL, *pRight = NULL, *opNode = NULL, *res = NULL;
  scltMakeValueNode(&pLeft, TSDB_DATA_TYPE_TINYINT, &scltLeftV);
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_UBIGINT, &scltRightV);
  scltMakeOpNode(&opNode, OP_TYPE_LOWER_THAN, TSDB_DATA_TYPE_BOOL, pLeft, pRight);
  
  int32_t code = scalarCalculateConstants(opNode, &res);
  ASSERT_EQ(code, 0);
  ASSERT_TRUE(res);
  ASSERT_EQ(nodeType(res), QUERY_NODE_VALUE);
  SValueNode *v = (SValueNode *)res;
  ASSERT_EQ(v->node.resType.type, TSDB_DATA_TYPE_BOOL);
  ASSERT_EQ(v->datum.b, scltLeftV < scltRightV);
  nodesDestroyNode(res);
}

TEST(constantTest, usmallint_lower_equal_ubigint) {
  SNode *pLeft = NULL, *pRight = NULL, *opNode = NULL, *res = NULL;
  int32_t leftv = 1;
  int64_t rightv = 1;
  scltMakeValueNode(&pLeft, TSDB_DATA_TYPE_USMALLINT, &leftv);
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_UBIGINT, &rightv);
  scltMakeOpNode(&opNode, OP_TYPE_LOWER_EQUAL, TSDB_DATA_TYPE_BOOL, pLeft, pRight);
  
  int32_t code = scalarCalculateConstants(opNode, &res);
  ASSERT_EQ(code, 0);
  ASSERT_TRUE(res);
  ASSERT_EQ(nodeType(res), QUERY_NODE_VALUE);
  SValueNode *v = (SValueNode *)res;
  ASSERT_EQ(v->node.resType.type, TSDB_DATA_TYPE_BOOL);
  ASSERT_EQ(v->datum.b, leftv <= rightv);
  nodesDestroyNode(res);
}

TEST(constantTest, int_equal_smallint1) {
  SNode *pLeft = NULL, *pRight = NULL, *opNode = NULL, *res = NULL;
  int32_t leftv = 1;
  int16_t rightv = 1;
  scltMakeValueNode(&pLeft, TSDB_DATA_TYPE_INT, &leftv);
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_SMALLINT, &rightv);
  scltMakeOpNode(&opNode, OP_TYPE_EQUAL, TSDB_DATA_TYPE_BOOL, pLeft, pRight);
  
  int32_t code = scalarCalculateConstants(opNode, &res);
  ASSERT_EQ(code, 0);
  ASSERT_TRUE(res);
  ASSERT_EQ(nodeType(res), QUERY_NODE_VALUE);
  SValueNode *v = (SValueNode *)res;
  ASSERT_EQ(v->node.resType.type, TSDB_DATA_TYPE_BOOL);
  ASSERT_EQ(v->datum.b, leftv == rightv);
  nodesDestroyNode(res);
}

TEST(constantTest, int_equal_smallint2) {
  SNode *pLeft = NULL, *pRight = NULL, *opNode = NULL, *res = NULL;
  int32_t leftv = 0, rightv = 1;
  scltMakeValueNode(&pLeft, TSDB_DATA_TYPE_INT, &leftv);
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_SMALLINT, &rightv);
  scltMakeOpNode(&opNode, OP_TYPE_EQUAL, TSDB_DATA_TYPE_BOOL, pLeft, pRight);
  
  int32_t code = scalarCalculateConstants(opNode, &res);
  ASSERT_EQ(code, 0);
  ASSERT_TRUE(res);
  ASSERT_EQ(nodeType(res), QUERY_NODE_VALUE);
  SValueNode *v = (SValueNode *)res;
  ASSERT_EQ(v->node.resType.type, TSDB_DATA_TYPE_BOOL);
  ASSERT_EQ(v->datum.b, leftv == rightv);
  nodesDestroyNode(res);
}

TEST(constantTest, int_not_equal_smallint1) {
  SNode *pLeft = NULL, *pRight = NULL, *opNode = NULL, *res = NULL;
  int32_t leftv = 1, rightv = 1;
  scltMakeValueNode(&pLeft, TSDB_DATA_TYPE_INT, &leftv);
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_SMALLINT, &rightv);
  scltMakeOpNode(&opNode, OP_TYPE_NOT_EQUAL, TSDB_DATA_TYPE_BOOL, pLeft, pRight);
  
  int32_t code = scalarCalculateConstants(opNode, &res);
  ASSERT_EQ(code, 0);
  ASSERT_TRUE(res);
  ASSERT_EQ(nodeType(res), QUERY_NODE_VALUE);
  SValueNode *v = (SValueNode *)res;
  ASSERT_EQ(v->node.resType.type, TSDB_DATA_TYPE_BOOL);
  ASSERT_EQ(v->datum.b, leftv != rightv);
  nodesDestroyNode(res);
}

TEST(constantTest, int_not_equal_smallint2) {
  SNode *pLeft = NULL, *pRight = NULL, *opNode = NULL, *res = NULL;
  int32_t leftv = 0, rightv = 1;
  scltMakeValueNode(&pLeft, TSDB_DATA_TYPE_INT, &leftv);
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_SMALLINT, &rightv);
  scltMakeOpNode(&opNode, OP_TYPE_NOT_EQUAL, TSDB_DATA_TYPE_BOOL, pLeft, pRight);
  
  int32_t code = scalarCalculateConstants(opNode, &res);
  ASSERT_EQ(code, 0);
  ASSERT_TRUE(res);
  ASSERT_EQ(nodeType(res), QUERY_NODE_VALUE);
  SValueNode *v = (SValueNode *)res;
  ASSERT_EQ(v->node.resType.type, TSDB_DATA_TYPE_BOOL);
  ASSERT_EQ(v->datum.b, leftv != rightv);
  nodesDestroyNode(res);
}



TEST(constantTest, int_in_smallint1) {
  scltInitLogFile();
  
  SNode *pLeft = NULL, *pRight = NULL, *listNode = NULL, *res = NULL, *opNode = NULL;
  int32_t leftv = 1, rightv1 = 1,rightv2 = 2,rightv3 = 3;
  scltMakeValueNode(&pLeft, TSDB_DATA_TYPE_INT, &leftv);
  SNodeList* list = nodesMakeList();
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_SMALLINT, &rightv1);
  nodesListAppend(list, pRight);
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_SMALLINT, &rightv2);
  nodesListAppend(list, pRight);
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_SMALLINT, &rightv3);
  nodesListAppend(list, pRight);
  scltMakeListNode(&listNode,list, TSDB_DATA_TYPE_INT);
  scltMakeOpNode(&opNode, OP_TYPE_IN, TSDB_DATA_TYPE_BOOL, pLeft, listNode);
  
  int32_t code = scalarCalculateConstants(opNode, &res);
  ASSERT_EQ(code, 0);
  ASSERT_TRUE(res);
  ASSERT_EQ(nodeType(res), QUERY_NODE_VALUE);
  SValueNode *v = (SValueNode *)res;
  ASSERT_EQ(v->node.resType.type, TSDB_DATA_TYPE_BOOL);
  ASSERT_EQ(v->datum.b, true);
  nodesDestroyNode(res);
}

TEST(constantTest, int_in_smallint2) {
  scltInitLogFile();
  
  SNode *pLeft = NULL, *pRight = NULL, *listNode = NULL, *res = NULL, *opNode = NULL;
  int32_t leftv = 4, rightv1 = 1,rightv2 = 2,rightv3 = 3;
  scltMakeValueNode(&pLeft, TSDB_DATA_TYPE_INT, &leftv);
  SNodeList* list = nodesMakeList();
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_SMALLINT, &rightv1);
  nodesListAppend(list, pRight);
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_SMALLINT, &rightv2);
  nodesListAppend(list, pRight);
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_SMALLINT, &rightv3);
  nodesListAppend(list, pRight);
  scltMakeListNode(&listNode,list, TSDB_DATA_TYPE_INT);
  scltMakeOpNode(&opNode, OP_TYPE_IN, TSDB_DATA_TYPE_BOOL, pLeft, listNode);
  
  int32_t code = scalarCalculateConstants(opNode, &res);
  ASSERT_EQ(code, 0);
  ASSERT_TRUE(res);
  ASSERT_EQ(nodeType(res), QUERY_NODE_VALUE);
  SValueNode *v = (SValueNode *)res;
  ASSERT_EQ(v->node.resType.type, TSDB_DATA_TYPE_BOOL);
  ASSERT_EQ(v->datum.b, false);
  nodesDestroyNode(res);
}

TEST(constantTest, int_not_in_smallint1) {
  SNode *pLeft = NULL, *pRight = NULL, *listNode = NULL, *res = NULL, *opNode = NULL;
  int32_t leftv = 1, rightv1 = 1,rightv2 = 2,rightv3 = 3;
  scltMakeValueNode(&pLeft, TSDB_DATA_TYPE_INT, &leftv);
  SNodeList* list = nodesMakeList();
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_SMALLINT, &rightv1);
  nodesListAppend(list, pRight);
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_SMALLINT, &rightv2);
  nodesListAppend(list, pRight);
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_SMALLINT, &rightv3);
  nodesListAppend(list, pRight);
  scltMakeListNode(&listNode,list, TSDB_DATA_TYPE_INT);
  scltMakeOpNode(&opNode, OP_TYPE_NOT_IN, TSDB_DATA_TYPE_BOOL, pLeft, listNode);
  
  int32_t code = scalarCalculateConstants(opNode, &res);
  ASSERT_EQ(code, 0);
  ASSERT_TRUE(res);
  ASSERT_EQ(nodeType(res), QUERY_NODE_VALUE);
  SValueNode *v = (SValueNode *)res;
  ASSERT_EQ(v->node.resType.type, TSDB_DATA_TYPE_BOOL);
  ASSERT_EQ(v->datum.b, false);
  nodesDestroyNode(res);
}

TEST(constantTest, int_not_in_smallint2) {
  scltInitLogFile();
  
  SNode *pLeft = NULL, *pRight = NULL, *listNode = NULL, *res = NULL, *opNode = NULL;
  int32_t leftv = 4, rightv1 = 1,rightv2 = 2,rightv3 = 3;
  scltMakeValueNode(&pLeft, TSDB_DATA_TYPE_INT, &leftv);
  SNodeList* list = nodesMakeList();
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_SMALLINT, &rightv1);
  nodesListAppend(list, pRight);
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_SMALLINT, &rightv2);
  nodesListAppend(list, pRight);
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_SMALLINT, &rightv3);
  nodesListAppend(list, pRight);
  scltMakeListNode(&listNode,list, TSDB_DATA_TYPE_INT);
  scltMakeOpNode(&opNode, OP_TYPE_NOT_IN, TSDB_DATA_TYPE_BOOL, pLeft, listNode);
  
  int32_t code = scalarCalculateConstants(opNode, &res);
  ASSERT_EQ(code, 0);
  ASSERT_TRUE(res);
  ASSERT_EQ(nodeType(res), QUERY_NODE_VALUE);
  SValueNode *v = (SValueNode *)res;
  ASSERT_EQ(v->node.resType.type, TSDB_DATA_TYPE_BOOL);
  ASSERT_EQ(v->datum.b, true);
  nodesDestroyNode(res);
}

TEST(constantTest, binary_like_binary1) {
  SNode *pLeft = NULL, *pRight = NULL, *opNode = NULL, *res = NULL;
  char leftv[64] = {0}, rightv[64] = {0};
  sprintf(&leftv[2], "%s", "abc");
  varDataSetLen(leftv, strlen(&leftv[2]));
  sprintf(&rightv[2], "%s", "a_c");
  varDataSetLen(rightv, strlen(&rightv[2]));
  scltMakeValueNode(&pLeft, TSDB_DATA_TYPE_BINARY, leftv);
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_BINARY, rightv);
  scltMakeOpNode(&opNode, OP_TYPE_LIKE, TSDB_DATA_TYPE_BOOL, pLeft, pRight);
  
  int32_t code = scalarCalculateConstants(opNode, &res);
  ASSERT_EQ(code, 0);
  ASSERT_TRUE(res);
  ASSERT_EQ(nodeType(res), QUERY_NODE_VALUE);
  SValueNode *v = (SValueNode *)res;
  ASSERT_EQ(v->node.resType.type, TSDB_DATA_TYPE_BOOL);
  ASSERT_EQ(v->datum.b, true);
  nodesDestroyNode(res);
}

TEST(constantTest, binary_like_binary2) {
  SNode *pLeft = NULL, *pRight = NULL, *opNode = NULL, *res = NULL;
  char leftv[64] = {0}, rightv[64] = {0};
  sprintf(&leftv[2], "%s", "abc");
  varDataSetLen(leftv, strlen(&leftv[2]));
  sprintf(&rightv[2], "%s", "ac");
  varDataSetLen(rightv, strlen(&rightv[2]));
  scltMakeValueNode(&pLeft, TSDB_DATA_TYPE_BINARY, leftv);
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_BINARY, rightv);
  scltMakeOpNode(&opNode, OP_TYPE_LIKE, TSDB_DATA_TYPE_BOOL, pLeft, pRight);
  
  int32_t code = scalarCalculateConstants(opNode, &res);
  ASSERT_EQ(code, 0);
  ASSERT_TRUE(res);
  ASSERT_EQ(nodeType(res), QUERY_NODE_VALUE);
  SValueNode *v = (SValueNode *)res;
  ASSERT_EQ(v->node.resType.type, TSDB_DATA_TYPE_BOOL);
  ASSERT_EQ(v->datum.b, false);
  nodesDestroyNode(res);
}

TEST(constantTest, binary_not_like_binary1) {
  SNode *pLeft = NULL, *pRight = NULL, *opNode = NULL, *res = NULL;
  char leftv[64] = {0}, rightv[64] = {0};
  sprintf(&leftv[2], "%s", "abc");
  varDataSetLen(leftv, strlen(&leftv[2]));
  sprintf(&rightv[2], "%s", "a%c");
  varDataSetLen(rightv, strlen(&rightv[2]));
  scltMakeValueNode(&pLeft, TSDB_DATA_TYPE_BINARY, leftv);
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_BINARY, rightv);
  scltMakeOpNode(&opNode, OP_TYPE_NOT_LIKE, TSDB_DATA_TYPE_BOOL, pLeft, pRight);
  
  int32_t code = scalarCalculateConstants(opNode, &res);
  ASSERT_EQ(code, 0);
  ASSERT_TRUE(res);
  ASSERT_EQ(nodeType(res), QUERY_NODE_VALUE);
  SValueNode *v = (SValueNode *)res;
  ASSERT_EQ(v->node.resType.type, TSDB_DATA_TYPE_BOOL);
  ASSERT_EQ(v->datum.b, false);
  nodesDestroyNode(res);
}

TEST(constantTest, binary_not_like_binary2) {
  SNode *pLeft = NULL, *pRight = NULL, *opNode = NULL, *res = NULL;
  char leftv[64] = {0}, rightv[64] = {0};
  sprintf(&leftv[2], "%s", "abc");
  varDataSetLen(leftv, strlen(&leftv[2]));
  sprintf(&rightv[2], "%s", "ac");
  varDataSetLen(rightv, strlen(&rightv[2]));
  scltMakeValueNode(&pLeft, TSDB_DATA_TYPE_BINARY, leftv);
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_BINARY, rightv);
  scltMakeOpNode(&opNode, OP_TYPE_NOT_LIKE, TSDB_DATA_TYPE_BOOL, pLeft, pRight);
  
  int32_t code = scalarCalculateConstants(opNode, &res);
  ASSERT_EQ(code, 0);
  ASSERT_TRUE(res);
  ASSERT_EQ(nodeType(res), QUERY_NODE_VALUE);
  SValueNode *v = (SValueNode *)res;
  ASSERT_EQ(v->node.resType.type, TSDB_DATA_TYPE_BOOL);
  ASSERT_EQ(v->datum.b, true);
  nodesDestroyNode(res);
}

TEST(constantTest, binary_match_binary1) {
  SNode *pLeft = NULL, *pRight = NULL, *opNode = NULL, *res = NULL;
  char leftv[64] = {0}, rightv[64] = {0};
  sprintf(&leftv[2], "%s", "abc");
  varDataSetLen(leftv, strlen(&leftv[2]));
  sprintf(&rightv[2], "%s", ".*");
  varDataSetLen(rightv, strlen(&rightv[2]));
  scltMakeValueNode(&pLeft, TSDB_DATA_TYPE_BINARY, leftv);
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_BINARY, rightv);
  scltMakeOpNode(&opNode, OP_TYPE_MATCH, TSDB_DATA_TYPE_BOOL, pLeft, pRight);
  
  int32_t code = scalarCalculateConstants(opNode, &res);
  ASSERT_EQ(code, 0);
  ASSERT_TRUE(res);
  ASSERT_EQ(nodeType(res), QUERY_NODE_VALUE);
  SValueNode *v = (SValueNode *)res;
  ASSERT_EQ(v->node.resType.type, TSDB_DATA_TYPE_BOOL);
  ASSERT_EQ(v->datum.b, true);
  nodesDestroyNode(res);
}

TEST(constantTest, binary_match_binary2) {
  SNode *pLeft = NULL, *pRight = NULL, *opNode = NULL, *res = NULL;
  char leftv[64] = {0}, rightv[64] = {0};
  sprintf(&leftv[2], "%s", "abc");
  varDataSetLen(leftv, strlen(&leftv[2]));
  sprintf(&rightv[2], "%s", "abc.+");
  varDataSetLen(rightv, strlen(&rightv[2]));
  scltMakeValueNode(&pLeft, TSDB_DATA_TYPE_BINARY, leftv);
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_BINARY, rightv);
  scltMakeOpNode(&opNode, OP_TYPE_MATCH, TSDB_DATA_TYPE_BOOL, pLeft, pRight);
  
  int32_t code = scalarCalculateConstants(opNode, &res);
  ASSERT_EQ(code, 0);
  ASSERT_TRUE(res);
  ASSERT_EQ(nodeType(res), QUERY_NODE_VALUE);
  SValueNode *v = (SValueNode *)res;
  ASSERT_EQ(v->node.resType.type, TSDB_DATA_TYPE_BOOL);
  ASSERT_EQ(v->datum.b, false);
  nodesDestroyNode(res);
}

TEST(constantTest, binary_not_match_binary1) {
  SNode *pLeft = NULL, *pRight = NULL, *opNode = NULL, *res = NULL;
  char leftv[64] = {0}, rightv[64] = {0};
  sprintf(&leftv[2], "%s", "abc");
  varDataSetLen(leftv, strlen(&leftv[2]));
  sprintf(&rightv[2], "%s", "a[1-9]c");
  varDataSetLen(rightv, strlen(&rightv[2]));
  scltMakeValueNode(&pLeft, TSDB_DATA_TYPE_BINARY, leftv);
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_BINARY, rightv);
  scltMakeOpNode(&opNode, OP_TYPE_NMATCH, TSDB_DATA_TYPE_BOOL, pLeft, pRight);
  
  int32_t code = scalarCalculateConstants(opNode, &res);
  ASSERT_EQ(code, 0);
  ASSERT_TRUE(res);
  ASSERT_EQ(nodeType(res), QUERY_NODE_VALUE);
  SValueNode *v = (SValueNode *)res;
  ASSERT_EQ(v->node.resType.type, TSDB_DATA_TYPE_BOOL);
  ASSERT_EQ(v->datum.b, true);
  nodesDestroyNode(res);
}

TEST(constantTest, binary_not_match_binary2) {
  SNode *pLeft = NULL, *pRight = NULL, *opNode = NULL, *res = NULL;
  char leftv[64] = {0}, rightv[64] = {0};
  sprintf(&leftv[2], "%s", "abc");
  varDataSetLen(leftv, strlen(&leftv[2]));
  sprintf(&rightv[2], "%s", "a[ab]c");
  varDataSetLen(rightv, strlen(&rightv[2]));
  scltMakeValueNode(&pLeft, TSDB_DATA_TYPE_BINARY, leftv);
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_BINARY, rightv);
  scltMakeOpNode(&opNode, OP_TYPE_NMATCH, TSDB_DATA_TYPE_BOOL, pLeft, pRight);
  
  int32_t code = scalarCalculateConstants(opNode, &res);
  ASSERT_EQ(code, 0);
  ASSERT_TRUE(res);
  ASSERT_EQ(nodeType(res), QUERY_NODE_VALUE);
  SValueNode *v = (SValueNode *)res;
  ASSERT_EQ(v->node.resType.type, TSDB_DATA_TYPE_BOOL);
  ASSERT_EQ(v->datum.b, false);
  nodesDestroyNode(res);
}

TEST(constantTest, int_is_null1) {
  SNode *pLeft = NULL, *pRight = NULL, *opNode = NULL, *res = NULL;
  int32_t leftv = 1, rightv = 1;
  scltMakeValueNode(&pLeft, TSDB_DATA_TYPE_INT, &leftv);
  scltMakeOpNode(&opNode, OP_TYPE_IS_NULL, TSDB_DATA_TYPE_BOOL, pLeft, pRight);
  
  int32_t code = scalarCalculateConstants(opNode, &res);
  ASSERT_EQ(code, 0);
  ASSERT_TRUE(res);
  ASSERT_EQ(nodeType(res), QUERY_NODE_VALUE);
  SValueNode *v = (SValueNode *)res;
  ASSERT_EQ(v->node.resType.type, TSDB_DATA_TYPE_BOOL);
  ASSERT_EQ(v->datum.b, false);
  nodesDestroyNode(res);
}

TEST(constantTest, int_is_null2) {
  SNode *pLeft = NULL, *pRight = NULL, *opNode = NULL, *res = NULL;
  int32_t leftv = TSDB_DATA_INT_NULL, rightv = 1;
  scltMakeValueNode(&pLeft, TSDB_DATA_TYPE_NULL, &leftv);
  scltMakeOpNode(&opNode, OP_TYPE_IS_NULL, TSDB_DATA_TYPE_BOOL, pLeft, pRight);
  
  int32_t code = scalarCalculateConstants(opNode, &res);
  ASSERT_EQ(code, 0);
  ASSERT_TRUE(res);
  ASSERT_EQ(nodeType(res), QUERY_NODE_VALUE);
  SValueNode *v = (SValueNode *)res;
  ASSERT_EQ(v->node.resType.type, TSDB_DATA_TYPE_BOOL);
  ASSERT_EQ(v->datum.b, true);
  nodesDestroyNode(res);
}

TEST(constantTest, int_is_not_null1) {
  SNode *pLeft = NULL, *pRight = NULL, *opNode = NULL, *res = NULL;
  int32_t leftv = 1, rightv = 1;
  scltMakeValueNode(&pLeft, TSDB_DATA_TYPE_INT, &leftv);
  scltMakeOpNode(&opNode, OP_TYPE_IS_NOT_NULL, TSDB_DATA_TYPE_BOOL, pLeft, pRight);
  
  int32_t code = scalarCalculateConstants(opNode, &res);
  ASSERT_EQ(code, 0);
  ASSERT_TRUE(res);
  ASSERT_EQ(nodeType(res), QUERY_NODE_VALUE);
  SValueNode *v = (SValueNode *)res;
  ASSERT_EQ(v->node.resType.type, TSDB_DATA_TYPE_BOOL);
  ASSERT_EQ(v->datum.b, true);
  nodesDestroyNode(res);
}

TEST(constantTest, int_is_not_null2) {
  SNode *pLeft = NULL, *pRight = NULL, *opNode = NULL, *res = NULL;
  int32_t leftv = 1, rightv = 1;
  scltMakeValueNode(&pLeft, TSDB_DATA_TYPE_NULL, &leftv);
  scltMakeOpNode(&opNode, OP_TYPE_IS_NOT_NULL, TSDB_DATA_TYPE_BOOL, pLeft, pRight);
  
  int32_t code = scalarCalculateConstants(opNode, &res);
  ASSERT_EQ(code, 0);
  ASSERT_TRUE(res);
  ASSERT_EQ(nodeType(res), QUERY_NODE_VALUE);
  SValueNode *v = (SValueNode *)res;
  ASSERT_EQ(v->node.resType.type, TSDB_DATA_TYPE_BOOL);
  ASSERT_EQ(v->datum.b, false);
  nodesDestroyNode(res);
}

TEST(constantTest, int_add_int_is_true1) {
  SNode *pLeft = NULL, *pRight = NULL, *opNode = NULL, *res = NULL;
  int32_t leftv = 1, rightv = 1;
  scltMakeValueNode(&pLeft, TSDB_DATA_TYPE_INT, &leftv);
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_INT, &rightv);
  scltMakeOpNode(&opNode, OP_TYPE_ADD, TSDB_DATA_TYPE_DOUBLE, pLeft, pRight);
  scltMakeOpNode(&opNode, OP_TYPE_IS_TRUE, TSDB_DATA_TYPE_BOOL, opNode, NULL);
  
  int32_t code = scalarCalculateConstants(opNode, &res);
  ASSERT_EQ(code, 0);
  ASSERT_TRUE(res);
  ASSERT_EQ(nodeType(res), QUERY_NODE_VALUE);
  SValueNode *v = (SValueNode *)res;
  ASSERT_EQ(v->node.resType.type, TSDB_DATA_TYPE_BOOL);
  ASSERT_EQ(v->datum.b, true);
  nodesDestroyNode(res);
}

TEST(constantTest, int_add_int_is_true2) {
  SNode *pLeft = NULL, *pRight = NULL, *opNode = NULL, *res = NULL;
  int32_t leftv = 1, rightv = -1;
  scltMakeValueNode(&pLeft, TSDB_DATA_TYPE_INT, &leftv);
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_INT, &rightv);
  scltMakeOpNode(&opNode, OP_TYPE_ADD, TSDB_DATA_TYPE_DOUBLE, pLeft, pRight);
  scltMakeOpNode(&opNode, OP_TYPE_IS_TRUE, TSDB_DATA_TYPE_BOOL, opNode, NULL);
  
  int32_t code = scalarCalculateConstants(opNode, &res);
  ASSERT_EQ(code, 0);
  ASSERT_TRUE(res);
  ASSERT_EQ(nodeType(res), QUERY_NODE_VALUE);
  SValueNode *v = (SValueNode *)res;
  ASSERT_EQ(v->node.resType.type, TSDB_DATA_TYPE_BOOL);
  ASSERT_EQ(v->datum.b, false);
  nodesDestroyNode(res);
}


TEST(constantTest, int_greater_int_is_true1) {
  SNode *pLeft = NULL, *pRight = NULL, *opNode = NULL, *res = NULL;
  int32_t leftv = 1, rightv = 1;
  scltMakeValueNode(&pLeft, TSDB_DATA_TYPE_INT, &leftv);
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_INT, &rightv);
  scltMakeOpNode(&opNode, OP_TYPE_GREATER_THAN, TSDB_DATA_TYPE_BOOL, pLeft, pRight);
  scltMakeOpNode(&opNode, OP_TYPE_IS_TRUE, TSDB_DATA_TYPE_BOOL, opNode, NULL);
  
  int32_t code = scalarCalculateConstants(opNode, &res);
  ASSERT_EQ(code, 0);
  ASSERT_TRUE(res);
  ASSERT_EQ(nodeType(res), QUERY_NODE_VALUE);
  SValueNode *v = (SValueNode *)res;
  ASSERT_EQ(v->node.resType.type, TSDB_DATA_TYPE_BOOL);
  ASSERT_EQ(v->datum.b, false);
  nodesDestroyNode(res);
}

TEST(constantTest, int_greater_int_is_true2) {
  SNode *pLeft = NULL, *pRight = NULL, *opNode = NULL, *res = NULL;
  int32_t leftv = 1, rightv = 0;
  scltMakeValueNode(&pLeft, TSDB_DATA_TYPE_INT, &leftv);
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_INT, &rightv);
  scltMakeOpNode(&opNode, OP_TYPE_GREATER_THAN, TSDB_DATA_TYPE_BOOL, pLeft, pRight);
  scltMakeOpNode(&opNode, OP_TYPE_IS_TRUE, TSDB_DATA_TYPE_BOOL, opNode, NULL);
  
  int32_t code = scalarCalculateConstants(opNode, &res);
  ASSERT_EQ(code, 0);
  ASSERT_TRUE(res);
  ASSERT_EQ(nodeType(res), QUERY_NODE_VALUE);
  SValueNode *v = (SValueNode *)res;
  ASSERT_EQ(v->node.resType.type, TSDB_DATA_TYPE_BOOL);
  ASSERT_EQ(v->datum.b, true);
  nodesDestroyNode(res);
}

TEST(constantTest, greater_and_lower) {
  SNode *pval1 = NULL, *pval2 = NULL, *opNode1 = NULL, *opNode2 = NULL, *logicNode = NULL, *res = NULL;
  bool eRes[5] = {false, false, true, true, true};
  int64_t v1 = 333, v2 = 222, v3 = -10, v4 = 20;
  SNode *list[2] = {0};
  scltMakeValueNode(&pval1, TSDB_DATA_TYPE_BIGINT, &v1);
  scltMakeValueNode(&pval2, TSDB_DATA_TYPE_BIGINT, &v2);
  scltMakeOpNode(&opNode1, OP_TYPE_GREATER_THAN, TSDB_DATA_TYPE_BOOL, pval1, pval2);
  scltMakeValueNode(&pval1, TSDB_DATA_TYPE_BIGINT, &v3);
  scltMakeValueNode(&pval2, TSDB_DATA_TYPE_BIGINT, &v4);
  scltMakeOpNode(&opNode2, OP_TYPE_LOWER_THAN, TSDB_DATA_TYPE_BOOL, pval1, pval2);
  list[0] = opNode1;
  list[1] = opNode2;
  scltMakeLogicNode(&logicNode, LOGIC_COND_TYPE_AND, list, 2);
  
  int32_t code = scalarCalculateConstants(logicNode, &res);
  ASSERT_EQ(code, 0);
  ASSERT_TRUE(res);
  ASSERT_EQ(nodeType(res), QUERY_NODE_VALUE);
  SValueNode *v = (SValueNode *)res;
  ASSERT_EQ(v->node.resType.type, TSDB_DATA_TYPE_BOOL);
  ASSERT_EQ(v->datum.b, true);
  nodesDestroyNode(res);
}



TEST(columnTest, smallint_value_add_int_column) {
  scltInitLogFile();
  
  SNode *pLeft = NULL, *pRight = NULL, *opNode = NULL;
  int32_t leftv = 1;
  int16_t rightv[5]= {0, -5, -4, 23, 100};
  double eRes[5] = {1.0, -4, -3, 24, 101};
  SSDataBlock *src = NULL;
  int32_t rowNum = sizeof(rightv)/sizeof(rightv[0]);
  scltMakeValueNode(&pLeft, TSDB_DATA_TYPE_INT, &leftv);
  scltMakeColumnNode(&pRight, &src, TSDB_DATA_TYPE_SMALLINT, sizeof(int16_t), rowNum, rightv);
  scltMakeOpNode(&opNode, OP_TYPE_ADD, TSDB_DATA_TYPE_DOUBLE, pLeft, pRight);

  SArray *blockList = taosArrayInit(2, POINTER_BYTES);
  taosArrayPush(blockList, &src);
  SColumnInfo colInfo = createColumnInfo(1, TSDB_DATA_TYPE_DOUBLE, sizeof(double));
  int16_t dataBlockId = 0, slotId = 0;
  scltAppendReservedSlot(blockList, &dataBlockId, &slotId, true, rowNum, &colInfo);
  scltMakeTargetNode(&opNode, dataBlockId, slotId, opNode);
  
  int32_t code = scalarCalculate(opNode, blockList, NULL);
  ASSERT_EQ(code, 0);

  SSDataBlock *res = *(SSDataBlock **)taosArrayGetLast(blockList);
  ASSERT_EQ(res->info.rows, rowNum);
  SColumnInfoData *column = (SColumnInfoData *)taosArrayGetLast(res->pDataBlock);
  ASSERT_EQ(column->info.type, TSDB_DATA_TYPE_DOUBLE);
  ASSERT_EQ(column->info.bytes, tDataTypes[TSDB_DATA_TYPE_DOUBLE].bytes);
  for (int32_t i = 0; i < rowNum; ++i) {
    ASSERT_EQ(*((double *)colDataGetData(column, i)), eRes[i]);
  }

  taosArrayDestroyEx(blockList, scltFreeDataBlock);
  nodesDestroyNode(opNode);
}

TEST(columnTest, bigint_column_multi_binary_column) {
  SNode *pLeft = NULL, *pRight = NULL, *opNode = NULL;
  int64_t leftv[5]= {1, 2, 3, 4, 5};
  char rightv[5][5]= {0};
  for (int32_t i = 0; i < 5; ++i) {
    rightv[i][2] = rightv[i][3] = '0';
    rightv[i][4] = '0' + i;
    varDataSetLen(rightv[i], 3);
  }
  double eRes[5] = {0, 2, 6, 12, 20};
  SSDataBlock *src = NULL;
  int32_t rowNum = sizeof(rightv)/sizeof(rightv[0]);
  scltMakeColumnNode(&pLeft, &src, TSDB_DATA_TYPE_BIGINT, sizeof(int64_t), rowNum, leftv);
  scltMakeColumnNode(&pRight, &src, TSDB_DATA_TYPE_BINARY, 5, rowNum, rightv);
  scltMakeOpNode(&opNode, OP_TYPE_MULTI, TSDB_DATA_TYPE_DOUBLE, pLeft, pRight);


  SArray *blockList = taosArrayInit(1, POINTER_BYTES);
  taosArrayPush(blockList, &src);
  SColumnInfo colInfo = createColumnInfo(1, TSDB_DATA_TYPE_DOUBLE, sizeof(double));
  int16_t dataBlockId = 0, slotId = 0;
  scltAppendReservedSlot(blockList, &dataBlockId, &slotId, false, rowNum, &colInfo);
  scltMakeTargetNode(&opNode, dataBlockId, slotId, opNode);
  
  int32_t code = scalarCalculate(opNode, blockList, NULL);
  ASSERT_EQ(code, 0);

  SSDataBlock *res = *(SSDataBlock **)taosArrayGetLast(blockList);
  ASSERT_EQ(res->info.rows, rowNum);
  SColumnInfoData *column = (SColumnInfoData *)taosArrayGetLast(res->pDataBlock);
  ASSERT_EQ(column->info.type, TSDB_DATA_TYPE_DOUBLE);
  ASSERT_EQ(column->info.bytes, tDataTypes[TSDB_DATA_TYPE_DOUBLE].bytes);
  for (int32_t i = 0; i < rowNum; ++i) {
    ASSERT_EQ(*((double *)colDataGetData(column, i)), eRes[i]);
  }
  taosArrayDestroyEx(blockList, scltFreeDataBlock);
  nodesDestroyNode(opNode);
}

TEST(columnTest, smallint_column_and_binary_column) {
  SNode *pLeft = NULL, *pRight = NULL, *opNode = NULL;
  int16_t leftv[5]= {1, 2, 3, 4, 5};
  char rightv[5][5]= {0};
  for (int32_t i = 0; i < 5; ++i) {
    rightv[i][2] = rightv[i][3] = '0';
    rightv[i][4] = '0' + i;
    varDataSetLen(rightv[i], 3);
  }
  int64_t eRes[5] = {0, 0, 2, 0, 4};
  SSDataBlock *src = NULL;
  int32_t rowNum = sizeof(rightv)/sizeof(rightv[0]);
  scltMakeColumnNode(&pLeft, &src, TSDB_DATA_TYPE_SMALLINT, sizeof(int16_t), rowNum, leftv);
  scltMakeColumnNode(&pRight, &src, TSDB_DATA_TYPE_BINARY, 5, rowNum, rightv);
  scltMakeOpNode(&opNode, OP_TYPE_BIT_AND, TSDB_DATA_TYPE_BIGINT, pLeft, pRight);
  
  SArray *blockList = taosArrayInit(1, POINTER_BYTES);
  taosArrayPush(blockList, &src);
  SColumnInfo colInfo = createColumnInfo(1, TSDB_DATA_TYPE_BIGINT, sizeof(int64_t));
  int16_t dataBlockId = 0, slotId = 0;
  scltAppendReservedSlot(blockList, &dataBlockId, &slotId, false, rowNum, &colInfo);
  scltMakeTargetNode(&opNode, dataBlockId, slotId, opNode);
  
  int32_t code = scalarCalculate(opNode, blockList, NULL);
  ASSERT_EQ(code, 0);

  SSDataBlock *res = *(SSDataBlock **)taosArrayGetLast(blockList);
  ASSERT_EQ(res->info.rows, rowNum);
  SColumnInfoData *column = (SColumnInfoData *)taosArrayGetLast(res->pDataBlock);
  ASSERT_EQ(column->info.type, TSDB_DATA_TYPE_BIGINT);
  ASSERT_EQ(column->info.bytes, tDataTypes[TSDB_DATA_TYPE_BIGINT].bytes);
  for (int32_t i = 0; i < rowNum; ++i) {
    ASSERT_EQ(*((int64_t *)colDataGetData(column, i)), eRes[i]);
  }
  taosArrayDestroyEx(blockList, scltFreeDataBlock);
  nodesDestroyNode(opNode);
}

TEST(columnTest, smallint_column_or_float_column) {
  SNode *pLeft = NULL, *pRight = NULL, *opNode = NULL;
  int16_t leftv[5]= {1, 2, 3, 4, 5};
  float rightv[5]= {2.0, 3.0, 4.1, 5.2, 6.0};
  int64_t eRes[5] = {3, 3, 7, 5, 7};
  SSDataBlock *src = NULL;
  int32_t rowNum = sizeof(rightv)/sizeof(rightv[0]);
  scltMakeColumnNode(&pLeft, &src, TSDB_DATA_TYPE_SMALLINT, sizeof(int16_t), rowNum, leftv);
  scltMakeColumnNode(&pRight, &src, TSDB_DATA_TYPE_FLOAT, sizeof(float), rowNum, rightv);
  scltMakeOpNode(&opNode, OP_TYPE_BIT_OR, TSDB_DATA_TYPE_BIGINT, pLeft, pRight);
  
  SArray *blockList = taosArrayInit(1, POINTER_BYTES);
  taosArrayPush(blockList, &src);
  SColumnInfo colInfo = createColumnInfo(1, TSDB_DATA_TYPE_BIGINT, sizeof(int64_t));
  int16_t dataBlockId = 0, slotId = 0;
  scltAppendReservedSlot(blockList, &dataBlockId, &slotId, true, rowNum, &colInfo);
  scltMakeTargetNode(&opNode, dataBlockId, slotId, opNode);
  
  int32_t code = scalarCalculate(opNode, blockList, NULL);
  ASSERT_EQ(code, 0);

  SSDataBlock *res = *(SSDataBlock **)taosArrayGetLast(blockList);
  ASSERT_EQ(res->info.rows, rowNum);
  SColumnInfoData *column = (SColumnInfoData *)taosArrayGetLast(res->pDataBlock);
  ASSERT_EQ(column->info.type, TSDB_DATA_TYPE_BIGINT);
  ASSERT_EQ(column->info.bytes, tDataTypes[TSDB_DATA_TYPE_BIGINT].bytes);
  for (int32_t i = 0; i < rowNum; ++i) {
    ASSERT_EQ(*((int64_t *)colDataGetData(column, i)), eRes[i]);
  }
  taosArrayDestroyEx(blockList, scltFreeDataBlock);
  nodesDestroyNode(opNode);
}

TEST(columnTest, smallint_column_or_double_value) {
  SNode *pLeft = NULL, *pRight = NULL, *opNode = NULL;
  int16_t leftv[5]= {1, 2, 3, 4, 5};
  double rightv= 10.2;
  int64_t eRes[5] = {11, 10, 11, 14, 15};
  SSDataBlock *src = NULL;
  int32_t rowNum = sizeof(leftv)/sizeof(leftv[0]);
  scltMakeColumnNode(&pLeft, &src, TSDB_DATA_TYPE_SMALLINT, sizeof(int16_t), rowNum, leftv);
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_DOUBLE, &rightv);
  scltMakeOpNode(&opNode, OP_TYPE_BIT_OR, TSDB_DATA_TYPE_BIGINT, pLeft, pRight);
  
  SArray *blockList = taosArrayInit(1, POINTER_BYTES);
  taosArrayPush(blockList, &src);
  SColumnInfo colInfo = createColumnInfo(1, TSDB_DATA_TYPE_BIGINT, sizeof(int64_t));
  int16_t dataBlockId = 0, slotId = 0;
  scltAppendReservedSlot(blockList, &dataBlockId, &slotId, true, rowNum, &colInfo);
  scltMakeTargetNode(&opNode, dataBlockId, slotId, opNode);
  
  int32_t code = scalarCalculate(opNode, blockList, NULL);
  ASSERT_EQ(code, 0);

  SSDataBlock *res = *(SSDataBlock **)taosArrayGetLast(blockList);
  ASSERT_EQ(res->info.rows, rowNum);
  SColumnInfoData *column = (SColumnInfoData *)taosArrayGetLast(res->pDataBlock);
  ASSERT_EQ(column->info.type, TSDB_DATA_TYPE_BIGINT);
  ASSERT_EQ(column->info.bytes, tDataTypes[TSDB_DATA_TYPE_BIGINT].bytes);
  for (int32_t i = 0; i < rowNum; ++i) {
    ASSERT_EQ(*((int64_t *)colDataGetData(column, i)), eRes[i]);
  }
  taosArrayDestroyEx(blockList, scltFreeDataBlock);
  nodesDestroyNode(opNode);
}

TEST(columnTest, smallint_column_greater_double_value) {
  SNode *pLeft = NULL, *pRight = NULL, *opNode = NULL;
  int16_t leftv[5]= {1, 2, 3, 4, 5};
  double rightv= 2.5;
  bool eRes[5] = {false, false, true, true, true};
  SSDataBlock *src = NULL;
  int32_t rowNum = sizeof(leftv)/sizeof(leftv[0]);
  scltMakeColumnNode(&pLeft, &src, TSDB_DATA_TYPE_SMALLINT, sizeof(int16_t), rowNum, leftv);
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_DOUBLE, &rightv);
  scltMakeOpNode(&opNode, OP_TYPE_GREATER_THAN, TSDB_DATA_TYPE_BOOL, pLeft, pRight);
  
  SArray *blockList = taosArrayInit(1, POINTER_BYTES);
  taosArrayPush(blockList, &src);
  SColumnInfo colInfo = createColumnInfo(1, TSDB_DATA_TYPE_BOOL, sizeof(bool));
  int16_t dataBlockId = 0, slotId = 0;
  scltAppendReservedSlot(blockList, &dataBlockId, &slotId, true, rowNum, &colInfo);
  scltMakeTargetNode(&opNode, dataBlockId, slotId, opNode);
  
  int32_t code = scalarCalculate(opNode, blockList, NULL);
  ASSERT_EQ(code, 0);

  SSDataBlock *res = *(SSDataBlock **)taosArrayGetLast(blockList);
  ASSERT_EQ(res->info.rows, rowNum);
  SColumnInfoData *column = (SColumnInfoData *)taosArrayGetLast(res->pDataBlock);
  ASSERT_EQ(column->info.type, TSDB_DATA_TYPE_BOOL);
  ASSERT_EQ(column->info.bytes, tDataTypes[TSDB_DATA_TYPE_BOOL].bytes);
  for (int32_t i = 0; i < rowNum; ++i) {
    ASSERT_EQ(*((bool *)colDataGetData(column, i)), eRes[i]);
  }
  taosArrayDestroyEx(blockList, scltFreeDataBlock);
  nodesDestroyNode(opNode);
}

TEST(columnTest, int_column_in_double_list) {
  SNode *pLeft = NULL, *pRight = NULL, *listNode = NULL, *opNode = NULL;
  int32_t leftv[5] = {1, 2, 3, 4, 5};
  double rightv1 = 1.1,rightv2 = 2.2,rightv3 = 3.3;
  bool eRes[5] = {true, true, true, false, false};  
  SSDataBlock *src = NULL;  
  int32_t rowNum = sizeof(leftv)/sizeof(leftv[0]);
  scltMakeColumnNode(&pLeft, &src, TSDB_DATA_TYPE_INT, sizeof(int32_t), rowNum, leftv);  
  SNodeList* list = nodesMakeList();
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_DOUBLE, &rightv1);
  nodesListAppend(list, pRight);
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_DOUBLE, &rightv2);
  nodesListAppend(list, pRight);
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_DOUBLE, &rightv3);
  nodesListAppend(list, pRight);
  scltMakeListNode(&listNode,list, TSDB_DATA_TYPE_INT);
  scltMakeOpNode(&opNode, OP_TYPE_IN, TSDB_DATA_TYPE_BOOL, pLeft, listNode);

  SArray *blockList = taosArrayInit(1, POINTER_BYTES);
  taosArrayPush(blockList, &src);
  SColumnInfo colInfo = createColumnInfo(1, TSDB_DATA_TYPE_BOOL, sizeof(bool));
  int16_t dataBlockId = 0, slotId = 0;
  scltAppendReservedSlot(blockList, &dataBlockId, &slotId, true, rowNum, &colInfo);
  scltMakeTargetNode(&opNode, dataBlockId, slotId, opNode);
  
  int32_t code = scalarCalculate(opNode, blockList, NULL);
  ASSERT_EQ(code, 0);

  SSDataBlock *res = *(SSDataBlock **)taosArrayGetLast(blockList);
  ASSERT_EQ(res->info.rows, rowNum);
  SColumnInfoData *column = (SColumnInfoData *)taosArrayGetLast(res->pDataBlock);
  ASSERT_EQ(column->info.type, TSDB_DATA_TYPE_BOOL);
  ASSERT_EQ(column->info.bytes, tDataTypes[TSDB_DATA_TYPE_BOOL].bytes);
  for (int32_t i = 0; i < rowNum; ++i) {
    ASSERT_EQ(*((bool *)colDataGetData(column, i)), eRes[i]);
  }
  taosArrayDestroyEx(blockList, scltFreeDataBlock);
  nodesDestroyNode(opNode);
}

TEST(columnTest, binary_column_in_binary_list) {
  SNode *pLeft = NULL, *pRight = NULL, *listNode = NULL, *opNode = NULL;
  bool eRes[5] = {true, true, false, false, false};  
  SSDataBlock *src = NULL;  
  char leftv[5][5]= {0};
  char rightv[3][5]= {0};
  for (int32_t i = 0; i < 5; ++i) {
    leftv[i][2] = 'a' + i;
    leftv[i][3] = 'b' + i;
    leftv[i][4] = '0' + i;
    varDataSetLen(leftv[i], 3);
  }  
  for (int32_t i = 0; i < 2; ++i) {
    rightv[i][2] = 'a' + i;
    rightv[i][3] = 'b' + i;
    rightv[i][4] = '0' + i;
    varDataSetLen(rightv[i], 3);
  }  
  for (int32_t i = 2; i < 3; ++i) {
    rightv[i][2] = 'a' + i;
    rightv[i][3] = 'a' + i;
    rightv[i][4] = 'a' + i;
    varDataSetLen(rightv[i], 3);
  }
  
  int32_t rowNum = sizeof(leftv)/sizeof(leftv[0]);
  scltMakeColumnNode(&pLeft, &src, TSDB_DATA_TYPE_BINARY, 3, rowNum, leftv);  
  SNodeList* list = nodesMakeList();
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_BINARY, rightv[0]);
  nodesListAppend(list, pRight);
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_BINARY, rightv[1]);
  nodesListAppend(list, pRight);
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_BINARY, rightv[2]);
  nodesListAppend(list, pRight);
  scltMakeListNode(&listNode,list, TSDB_DATA_TYPE_BINARY);
  scltMakeOpNode(&opNode, OP_TYPE_IN, TSDB_DATA_TYPE_BOOL, pLeft, listNode);
  
  SArray *blockList = taosArrayInit(1, POINTER_BYTES);
  taosArrayPush(blockList, &src);
  SColumnInfo colInfo = createColumnInfo(1, TSDB_DATA_TYPE_BOOL, sizeof(bool));
  int16_t dataBlockId = 0, slotId = 0;
  scltAppendReservedSlot(blockList, &dataBlockId, &slotId, false, rowNum, &colInfo);
  scltMakeTargetNode(&opNode, dataBlockId, slotId, opNode);
  
  int32_t code = scalarCalculate(opNode, blockList, NULL);
  ASSERT_EQ(code, 0);

  SSDataBlock *res = *(SSDataBlock **)taosArrayGetLast(blockList);
  ASSERT_EQ(res->info.rows, rowNum);
  SColumnInfoData *column = (SColumnInfoData *)taosArrayGetLast(res->pDataBlock);
  ASSERT_EQ(column->info.type, TSDB_DATA_TYPE_BOOL);
  ASSERT_EQ(column->info.bytes, tDataTypes[TSDB_DATA_TYPE_BOOL].bytes);
  for (int32_t i = 0; i < rowNum; ++i) {
    ASSERT_EQ(*((bool *)colDataGetData(column, i)), eRes[i]);
  }
  taosArrayDestroyEx(blockList, scltFreeDataBlock);
  nodesDestroyNode(opNode);
}

TEST(columnTest, binary_column_like_binary) {
  SNode *pLeft = NULL, *pRight = NULL, *opNode = NULL;
  char rightv[64] = {0};
  char leftv[5][5]= {0};
  SSDataBlock *src = NULL;  
  bool eRes[5] = {true, false, true, false, true};  
  
  for (int32_t i = 0; i < 5; ++i) {
    leftv[i][2] = 'a';
    leftv[i][3] = 'a';
    leftv[i][4] = '0' + i % 2;
    varDataSetLen(leftv[i], 3);
  }  
  
  int32_t rowNum = sizeof(leftv)/sizeof(leftv[0]);
  scltMakeColumnNode(&pLeft, &src, TSDB_DATA_TYPE_BINARY, 3, rowNum, leftv);  

  sprintf(&rightv[2], "%s", "__0");
  varDataSetLen(rightv, strlen(&rightv[2]));
  scltMakeValueNode(&pRight, TSDB_DATA_TYPE_BINARY, rightv);
  scltMakeOpNode(&opNode, OP_TYPE_LIKE, TSDB_DATA_TYPE_BOOL, pLeft, pRight);
  
  SArray *blockList = taosArrayInit(1, POINTER_BYTES);
  taosArrayPush(blockList, &src);
  SColumnInfo colInfo = createColumnInfo(1, TSDB_DATA_TYPE_BOOL, sizeof(bool));
  int16_t dataBlockId = 0, slotId = 0;
  scltAppendReservedSlot(blockList, &dataBlockId, &slotId, false, rowNum, &colInfo);
  scltMakeTargetNode(&opNode, dataBlockId, slotId, opNode);
  
  int32_t code = scalarCalculate(opNode, blockList, NULL);
  ASSERT_EQ(code, 0);

  SSDataBlock *res = *(SSDataBlock **)taosArrayGetLast(blockList);
  ASSERT_EQ(res->info.rows, rowNum);
  SColumnInfoData *column = (SColumnInfoData *)taosArrayGetLast(res->pDataBlock);
  ASSERT_EQ(column->info.type, TSDB_DATA_TYPE_BOOL);
  ASSERT_EQ(column->info.bytes, tDataTypes[TSDB_DATA_TYPE_BOOL].bytes);
  for (int32_t i = 0; i < rowNum; ++i) {
    ASSERT_EQ(*((bool *)colDataGetData(column, i)), eRes[i]);
  }

  taosArrayDestroyEx(blockList, scltFreeDataBlock);
  nodesDestroyNode(opNode);
}


TEST(columnTest, binary_column_is_true) {
  SNode *pLeft = NULL, *opNode = NULL;
  char leftv[5][5]= {0};
  SSDataBlock *src = NULL;  
  bool eRes[5] = {false, true, false, true, false};  
  
  for (int32_t i = 0; i < 5; ++i) {
    leftv[i][2] = '0' + i % 2;
    leftv[i][3] = 'a';
    leftv[i][4] = '0' + i % 2;
    varDataSetLen(leftv[i], 3);
  }  
  
  int32_t rowNum = sizeof(leftv)/sizeof(leftv[0]);
  scltMakeColumnNode(&pLeft, &src, TSDB_DATA_TYPE_BINARY, 3, rowNum, leftv);  

  scltMakeOpNode(&opNode, OP_TYPE_IS_TRUE, TSDB_DATA_TYPE_BOOL, pLeft, NULL);
  
  SArray *blockList = taosArrayInit(1, POINTER_BYTES);
  taosArrayPush(blockList, &src);
  SColumnInfo colInfo = createColumnInfo(1, TSDB_DATA_TYPE_BOOL, sizeof(bool));
  int16_t dataBlockId = 0, slotId = 0;
  scltAppendReservedSlot(blockList, &dataBlockId, &slotId, false, rowNum, &colInfo);
  scltMakeTargetNode(&opNode, dataBlockId, slotId, opNode);
  
  int32_t code = scalarCalculate(opNode, blockList, NULL);
  ASSERT_EQ(code, 0);

  SSDataBlock *res = *(SSDataBlock **)taosArrayGetLast(blockList);
  ASSERT_EQ(res->info.rows, rowNum);
  SColumnInfoData *column = (SColumnInfoData *)taosArrayGetLast(res->pDataBlock);
  ASSERT_EQ(column->info.type, TSDB_DATA_TYPE_BOOL);
  ASSERT_EQ(column->info.bytes, tDataTypes[TSDB_DATA_TYPE_BOOL].bytes);
  for (int32_t i = 0; i < rowNum; ++i) {
    ASSERT_EQ(*((bool *)colDataGetData(column, i)), eRes[i]);
  }
  taosArrayDestroyEx(blockList, scltFreeDataBlock);
  nodesDestroyNode(opNode);
}

TEST(columnTest, binary_column_is_null) {
  SNode *pLeft = NULL, *opNode = NULL;
  char leftv[5][5]= {0};
  SSDataBlock *src = NULL;  
  bool eRes[5] = {false, false, true, false, true};  
  
  for (int32_t i = 0; i < 5; ++i) {
    leftv[i][2] = '0' + i % 2;
    leftv[i][3] = 'a';
    leftv[i][4] = '0' + i % 2;
    varDataSetLen(leftv[i], 3);
  }  
  
  int32_t rowNum = sizeof(leftv)/sizeof(leftv[0]);
  scltMakeColumnNode(&pLeft, &src, TSDB_DATA_TYPE_BINARY, 3, rowNum, leftv); 

  SColumnInfoData *pcolumn = (SColumnInfoData *)taosArrayGetLast(src->pDataBlock);
  colDataAppend(pcolumn, 2, NULL, true);
  colDataAppend(pcolumn, 4, NULL, true);

  scltMakeOpNode(&opNode, OP_TYPE_IS_NULL, TSDB_DATA_TYPE_BOOL, pLeft, NULL);
  
  SArray *blockList = taosArrayInit(1, POINTER_BYTES);
  taosArrayPush(blockList, &src);
  SColumnInfo colInfo = createColumnInfo(1, TSDB_DATA_TYPE_BOOL, sizeof(bool));
  int16_t dataBlockId = 0, slotId = 0;
  scltAppendReservedSlot(blockList, &dataBlockId, &slotId, false, rowNum, &colInfo);
  scltMakeTargetNode(&opNode, dataBlockId, slotId, opNode);
  
  int32_t code = scalarCalculate(opNode, blockList, NULL);
  ASSERT_EQ(code, 0);

  SSDataBlock *res = *(SSDataBlock **)taosArrayGetLast(blockList);
  ASSERT_EQ(res->info.rows, rowNum);
  SColumnInfoData *column = (SColumnInfoData *)taosArrayGetLast(res->pDataBlock);
  ASSERT_EQ(column->info.type, TSDB_DATA_TYPE_BOOL);
  ASSERT_EQ(column->info.bytes, tDataTypes[TSDB_DATA_TYPE_BOOL].bytes);
  for (int32_t i = 0; i < rowNum; ++i) {
    ASSERT_EQ(*((bool *)colDataGetData(column, i)), eRes[i]);
  }
  taosArrayDestroyEx(blockList, scltFreeDataBlock);
  nodesDestroyNode(opNode);
}

TEST(columnTest, binary_column_is_not_null) {
  SNode *pLeft = NULL, *opNode = NULL;
  char leftv[5][5]= {0};
  SSDataBlock *src = NULL;  
  bool eRes[5] = {true, true, true, true, false};  
  
  for (int32_t i = 0; i < 5; ++i) {
    leftv[i][2] = '0' + i % 2;
    leftv[i][3] = 'a';
    leftv[i][4] = '0' + i % 2;
    varDataSetLen(leftv[i], 3);
  }  
  
  int32_t rowNum = sizeof(leftv)/sizeof(leftv[0]);
  scltMakeColumnNode(&pLeft, &src, TSDB_DATA_TYPE_BINARY, 3, rowNum, leftv);  
  
  SColumnInfoData *pcolumn = (SColumnInfoData *)taosArrayGetLast(src->pDataBlock);
  colDataAppend(pcolumn, 4, NULL, true);

  scltMakeOpNode(&opNode, OP_TYPE_IS_NOT_NULL, TSDB_DATA_TYPE_BOOL, pLeft, NULL);
  
  SArray *blockList = taosArrayInit(1, POINTER_BYTES);
  taosArrayPush(blockList, &src);
  SColumnInfo colInfo = createColumnInfo(1, TSDB_DATA_TYPE_BOOL, sizeof(bool));
  int16_t dataBlockId = 0, slotId = 0;
  scltAppendReservedSlot(blockList, &dataBlockId, &slotId, false, rowNum, &colInfo);
  scltMakeTargetNode(&opNode, dataBlockId, slotId, opNode);
  
  int32_t code = scalarCalculate(opNode, blockList, NULL);
  ASSERT_EQ(code, 0);

  SSDataBlock *res = *(SSDataBlock **)taosArrayGetLast(blockList);
  ASSERT_EQ(res->info.rows, rowNum);
  SColumnInfoData *column = (SColumnInfoData *)taosArrayGetLast(res->pDataBlock);
  ASSERT_EQ(column->info.type, TSDB_DATA_TYPE_BOOL);
  ASSERT_EQ(column->info.bytes, tDataTypes[TSDB_DATA_TYPE_BOOL].bytes);
  for (int32_t i = 0; i < rowNum; ++i) {
    ASSERT_EQ(*((bool *)colDataGetData(column, i)), eRes[i]);
  }
  taosArrayDestroyEx(blockList, scltFreeDataBlock);
  nodesDestroyNode(opNode);
}

TEST(columnTest, greater_and_lower) {
  SNode *pcol1 = NULL, *pcol2 = NULL, *opNode1 = NULL, *opNode2 = NULL, *logicNode = NULL;
  SNode *list[2] = {0};
  int16_t v1[5]= {1, 2, 3, 4, 5};
  int32_t v2[5]= {5, 1, 4, 2, 6};
  int64_t v3[5]= {1, 2, 3, 4, 5};
  int32_t v4[5]= {5, 3, 4, 2, 6};
  bool eRes[5] = {false, true, false, false, false};
  SSDataBlock *src = NULL;
  int32_t rowNum = sizeof(v1)/sizeof(v1[0]);
  scltMakeColumnNode(&pcol1, &src, TSDB_DATA_TYPE_SMALLINT, sizeof(int16_t), rowNum, v1);
  scltMakeColumnNode(&pcol2, &src, TSDB_DATA_TYPE_INT, sizeof(int32_t), rowNum, v2);
  scltMakeOpNode(&opNode1, OP_TYPE_GREATER_THAN, TSDB_DATA_TYPE_BOOL, pcol1, pcol2);
  scltMakeColumnNode(&pcol1, &src, TSDB_DATA_TYPE_BIGINT, sizeof(int64_t), rowNum, v3);
  scltMakeColumnNode(&pcol2, &src, TSDB_DATA_TYPE_INT, sizeof(int32_t), rowNum, v4);
  scltMakeOpNode(&opNode2, OP_TYPE_LOWER_THAN, TSDB_DATA_TYPE_BOOL, pcol1, pcol2);
  list[0] = opNode1;
  list[1] = opNode2;
  scltMakeLogicNode(&logicNode, LOGIC_COND_TYPE_AND, list, 2);
  
  SArray *blockList = taosArrayInit(1, POINTER_BYTES);
  taosArrayPush(blockList, &src);
  SColumnInfo colInfo = createColumnInfo(1, TSDB_DATA_TYPE_BOOL, sizeof(bool));
  int16_t dataBlockId = 0, slotId = 0;
  scltAppendReservedSlot(blockList, &dataBlockId, &slotId, false, rowNum, &colInfo);
  scltMakeTargetNode(&logicNode, dataBlockId, slotId, logicNode);
  
  int32_t code = scalarCalculate(logicNode, blockList, NULL);
  ASSERT_EQ(code, 0);

  SSDataBlock *res = *(SSDataBlock **)taosArrayGetLast(blockList);
  ASSERT_EQ(res->info.rows, rowNum);
  SColumnInfoData *column = (SColumnInfoData *)taosArrayGetLast(res->pDataBlock);
  ASSERT_EQ(column->info.type, TSDB_DATA_TYPE_BOOL);
  ASSERT_EQ(column->info.bytes, tDataTypes[TSDB_DATA_TYPE_BOOL].bytes);
  for (int32_t i = 0; i < rowNum; ++i) {
    ASSERT_EQ(*((bool *)colDataGetData(column, i)), eRes[i]);
  }
  taosArrayDestroyEx(blockList, scltFreeDataBlock);
  nodesDestroyNode(logicNode);
}


int main(int argc, char** argv) {
  taosSeedRand(taosGetTimestampSec());
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

#pragma GCC diagnostic pop
