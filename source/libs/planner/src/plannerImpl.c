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

#include "plannerImpl.h"
#include "functionMgt.h"

#define CHECK_ALLOC(p, res) \
  do { \
    if (NULL == (p)) { \
      pCxt->errCode = TSDB_CODE_OUT_OF_MEMORY; \
      return (res); \
    } \
  } while (0)

#define CHECK_CODE(exec, res) \
  do { \
    int32_t code = (exec); \
    if (TSDB_CODE_SUCCESS != code) { \
      pCxt->errCode = code; \
      return (res); \
    } \
  } while (0)

typedef struct SPlanContext {
  int32_t errCode;
  int32_t planNodeId;
} SPlanContext;

static SLogicNode* createQueryLogicNode(SPlanContext* pCxt, SNode* pStmt);
static SLogicNode* createLogicNodeByTable(SPlanContext* pCxt, SSelectStmt* pSelect, SNode* pTable);

typedef struct SRewriteExprCxt {
  int32_t errCode;
  SNodeList* pExprs;
} SRewriteExprCxt;

static EDealRes doRewriteExpr(SNode** pNode, void* pContext) {
  switch (nodeType(*pNode)) {
    case QUERY_NODE_OPERATOR:
    case QUERY_NODE_LOGIC_CONDITION:
    case QUERY_NODE_FUNCTION: {
      SRewriteExprCxt* pCxt = (SRewriteExprCxt*)pContext;
      SNode* pExpr;
      int32_t index = 0;
      FOREACH(pExpr, pCxt->pExprs) {
        if (nodesEqualNode(pExpr, *pNode)) {
          SColumnNode* pCol = (SColumnNode*)nodesMakeNode(QUERY_NODE_COLUMN);
          CHECK_ALLOC(pCol, DEAL_RES_ERROR);
          SExprNode* pToBeRewrittenExpr = (SExprNode*)(*pNode);
          pCol->node.resType = pToBeRewrittenExpr->resType;
          strcpy(pCol->node.aliasName, pToBeRewrittenExpr->aliasName);
          strcpy(pCol->colName, ((SExprNode*)pExpr)->aliasName);
          nodesDestroyNode(*pNode);
          *pNode = (SNode*)pCol;
          return DEAL_RES_IGNORE_CHILD;
        }
        ++index;
      }
      break;
    }
    default:
      break;
  }

  return DEAL_RES_CONTINUE;
}

typedef struct SNameExprCxt {
  int32_t planNodeId;
  int32_t rewriteId;
} SNameExprCxt;

static EDealRes doNameExpr(SNode* pNode, void* pContext) {
  switch (nodeType(pNode)) {
    case QUERY_NODE_OPERATOR:
    case QUERY_NODE_LOGIC_CONDITION:
    case QUERY_NODE_FUNCTION: {
      SNameExprCxt* pCxt = (SNameExprCxt*)pContext;
      sprintf(((SExprNode*)pNode)->aliasName, "#expr_%d_%d", pCxt->planNodeId, pCxt->rewriteId++);
      return DEAL_RES_IGNORE_CHILD;
    }
    default:
      break;
  }

  return DEAL_RES_CONTINUE;
}

static int32_t rewriteExpr(int32_t planNodeId, int32_t rewriteId, SNodeList* pExprs, SSelectStmt* pSelect, ESqlClause clause) {
  SNameExprCxt nameCxt = { .planNodeId = planNodeId, .rewriteId = rewriteId };
  nodesWalkList(pExprs, doNameExpr, &nameCxt);
  SRewriteExprCxt cxt = { .errCode = TSDB_CODE_SUCCESS, .pExprs = pExprs };
  nodesRewriteSelectStmt(pSelect, clause, doRewriteExpr, &cxt);
  return cxt.errCode;
}

static SLogicNode* pushLogicNode(SPlanContext* pCxt, SLogicNode* pRoot, SLogicNode* pNode) {
  if (TSDB_CODE_SUCCESS != pCxt->errCode) {
    goto error;
  }

  if (NULL == pRoot) {
    return pNode;
  }

  if (NULL == pNode) {
    return pRoot;
  }

  if (NULL == pNode->pChildren) {
    pNode->pChildren = nodesMakeList();
    if (NULL == pNode->pChildren) {
      goto error;
    }
  }
  if (TSDB_CODE_SUCCESS != nodesListAppend(pNode->pChildren, (SNode*)pRoot)) {
    goto error;
  }
  pRoot->pParent = pNode;
  return pNode;
error:
  nodesDestroyNode((SNode*)pNode);
  return pRoot;
}

static SLogicNode* createScanLogicNode(SPlanContext* pCxt, SSelectStmt* pSelect, SRealTableNode* pRealTable) {
  SScanLogicNode* pScan = (SScanLogicNode*)nodesMakeNode(QUERY_NODE_LOGIC_PLAN_SCAN);
  CHECK_ALLOC(pScan, NULL);
  pScan->node.id = pCxt->planNodeId++;

  pScan->pMeta = pRealTable->pMeta;

  // set columns to scan
  SNodeList* pCols = NULL;
  CHECK_CODE(nodesCollectColumns(pSelect, SQL_CLAUSE_FROM, pRealTable->table.tableAlias, &pCols), (SLogicNode*)pScan);
  if (NULL != pCols) {
    pScan->pScanCols = nodesCloneList(pCols);
    CHECK_ALLOC(pScan->pScanCols, (SLogicNode*)pScan);
  }

  // set output
  if (NULL != pCols) {
    pScan->node.pTargets = nodesCloneList(pCols);
    CHECK_ALLOC(pScan->node.pTargets, (SLogicNode*)pScan);
  }

  return (SLogicNode*)pScan;
}

static SLogicNode* createSubqueryLogicNode(SPlanContext* pCxt, SSelectStmt* pSelect, STempTableNode* pTable) {
  SLogicNode* pRoot = createQueryLogicNode(pCxt, pTable->pSubquery);
  CHECK_ALLOC(pRoot, NULL);
  SNode* pNode;
  FOREACH(pNode, pRoot->pTargets) {
    strcpy(((SColumnNode*)pNode)->tableAlias, pTable->table.tableAlias);
  }
  return pRoot;
}

static SLogicNode* createJoinLogicNode(SPlanContext* pCxt, SSelectStmt* pSelect, SJoinTableNode* pJoinTable) {
  SJoinLogicNode* pJoin = (SJoinLogicNode*)nodesMakeNode(QUERY_NODE_LOGIC_PLAN_JOIN);
  CHECK_ALLOC(pJoin, NULL);
  pJoin->node.id = pCxt->planNodeId++;

  pJoin->joinType = pJoinTable->joinType;

  // set left and right node
  pJoin->node.pChildren = nodesMakeList();
  CHECK_ALLOC(pJoin->node.pChildren, (SLogicNode*)pJoin);
  SLogicNode* pLeft = createLogicNodeByTable(pCxt, pSelect, pJoinTable->pLeft);
  CHECK_ALLOC(pLeft, (SLogicNode*)pJoin);
  CHECK_CODE(nodesListAppend(pJoin->node.pChildren, (SNode*)pLeft), (SLogicNode*)pJoin);
  SLogicNode* pRight = createLogicNodeByTable(pCxt, pSelect, pJoinTable->pRight);
  CHECK_ALLOC(pRight, (SLogicNode*)pJoin);
  CHECK_CODE(nodesListAppend(pJoin->node.pChildren, (SNode*)pRight), (SLogicNode*)pJoin);

  // set on conditions
  if (NULL != pJoinTable->pOnCond) {
    pJoin->pOnConditions = nodesCloneNode(pJoinTable->pOnCond);
    CHECK_ALLOC(pJoin->pOnConditions, (SLogicNode*)pJoin);
  }

  // set the output
  pJoin->node.pTargets = nodesCloneList(pLeft->pTargets);
  CHECK_ALLOC(pJoin->node.pTargets, (SLogicNode*)pJoin);
  SNodeList* pTargets = nodesCloneList(pRight->pTargets);
  CHECK_ALLOC(pTargets, (SLogicNode*)pJoin);
  nodesListAppendList(pJoin->node.pTargets, pTargets);

  return (SLogicNode*)pJoin;
}

static SLogicNode* createLogicNodeByTable(SPlanContext* pCxt, SSelectStmt* pSelect, SNode* pTable) {
  switch (nodeType(pTable)) {
    case QUERY_NODE_REAL_TABLE:
      return createScanLogicNode(pCxt, pSelect, (SRealTableNode*)pTable);
    case QUERY_NODE_TEMP_TABLE:
      return createSubqueryLogicNode(pCxt, pSelect, (STempTableNode*)pTable);
    case QUERY_NODE_JOIN_TABLE:
      return createJoinLogicNode(pCxt, pSelect, (SJoinTableNode*)pTable);
    default:
      break;
  }
  return NULL;
}

typedef struct SCreateColumnCxt {
  int32_t errCode;
  SNodeList* pList;
} SCreateColumnCxt;

static EDealRes doCreateColumn(SNode* pNode, void* pContext) {
  SCreateColumnCxt* pCxt = (SCreateColumnCxt*)pContext;
  switch (nodeType(pNode)) {
    case QUERY_NODE_COLUMN: {
      SNode* pCol = nodesCloneNode(pNode);
      CHECK_ALLOC(pCol, DEAL_RES_ERROR);
      CHECK_CODE(nodesListAppend(pCxt->pList, pCol), DEAL_RES_ERROR);
      return DEAL_RES_IGNORE_CHILD;
    }
    case QUERY_NODE_OPERATOR:
    case QUERY_NODE_LOGIC_CONDITION:
    case QUERY_NODE_FUNCTION: {
      SExprNode* pExpr = (SExprNode*)pNode;
      SColumnNode* pCol = (SColumnNode*)nodesMakeNode(QUERY_NODE_COLUMN);
      CHECK_ALLOC(pCol, DEAL_RES_ERROR);
      pCol->node.resType = pExpr->resType;
      strcpy(pCol->colName, pExpr->aliasName);
      CHECK_CODE(nodesListAppend(pCxt->pList, (SNode*)pCol), DEAL_RES_ERROR);
      return DEAL_RES_IGNORE_CHILD;
    }
    default:
      break;
  }

  return DEAL_RES_CONTINUE;
}

static SNodeList* createColumnByRewriteExps(SPlanContext* pCxt, SNodeList* pExprs) {
  SCreateColumnCxt cxt = { .errCode = TSDB_CODE_SUCCESS, .pList = nodesMakeList() };
  CHECK_ALLOC(cxt.pList, NULL);

  nodesWalkList(pExprs, doCreateColumn, &cxt);
  if (TSDB_CODE_SUCCESS != cxt.errCode) {
    nodesDestroyList(cxt.pList);
    return NULL;
  }
  return cxt.pList;
}

static SLogicNode* createAggLogicNode(SPlanContext* pCxt, SSelectStmt* pSelect) {
  SNodeList* pAggFuncs = NULL;
  CHECK_CODE(nodesCollectFuncs(pSelect, fmIsAggFunc, &pAggFuncs), NULL);
  if (NULL == pAggFuncs && NULL == pSelect->pGroupByList) {
    return NULL;
  }

  SAggLogicNode* pAgg = (SAggLogicNode*)nodesMakeNode(QUERY_NODE_LOGIC_PLAN_AGG);
  CHECK_ALLOC(pAgg, NULL);
  pAgg->node.id = pCxt->planNodeId++;

  // set grouyp keys, agg funcs and having conditions
  if (NULL != pSelect->pGroupByList) {
    pAgg->pGroupKeys = nodesCloneList(pSelect->pGroupByList);
    CHECK_ALLOC(pAgg->pGroupKeys, (SLogicNode*)pAgg);
  }
  if (NULL != pAggFuncs) {
    pAgg->pAggFuncs = nodesCloneList(pAggFuncs);
    CHECK_ALLOC(pAgg->pAggFuncs, (SLogicNode*)pAgg);
  }

  // rewrite the expression in subsequent clauses
  CHECK_CODE(rewriteExpr(pAgg->node.id, 1, pAgg->pGroupKeys, pSelect, SQL_CLAUSE_GROUP_BY), (SLogicNode*)pAgg);
  CHECK_CODE(rewriteExpr(pAgg->node.id, 1 + LIST_LENGTH(pAgg->pGroupKeys), pAgg->pAggFuncs, pSelect, SQL_CLAUSE_GROUP_BY), (SLogicNode*)pAgg);

  if (NULL != pSelect->pHaving) {
    pAgg->node.pConditions = nodesCloneNode(pSelect->pHaving);
    CHECK_ALLOC(pAgg->node.pConditions, (SLogicNode*)pAgg);
  }

  // set the output
  pAgg->node.pTargets = nodesMakeList();
  CHECK_ALLOC(pAgg->node.pTargets, (SLogicNode*)pAgg);
  if (NULL != pAgg->pGroupKeys) {
    SNodeList* pTargets = createColumnByRewriteExps(pCxt, pAgg->pGroupKeys);
    CHECK_ALLOC(pAgg->node.pTargets, (SLogicNode*)pAgg);
    nodesListAppendList(pAgg->node.pTargets, pTargets);
  }
  if (NULL != pAgg->pAggFuncs) {
    SNodeList* pTargets = createColumnByRewriteExps(pCxt, pAgg->pAggFuncs);
    CHECK_ALLOC(pTargets, (SLogicNode*)pAgg);
    nodesListAppendList(pAgg->node.pTargets, pTargets);
  }
  
  return (SLogicNode*)pAgg;
}

static SNodeList* createColumnByProjections(SPlanContext* pCxt, SNodeList* pExprs) {
  SNodeList* pList = nodesMakeList();
  CHECK_ALLOC(pList, NULL);
  SNode* pNode;
  FOREACH(pNode, pExprs) {
    SExprNode* pExpr = (SExprNode*)pNode;
    SColumnNode* pCol = (SColumnNode*)nodesMakeNode(QUERY_NODE_COLUMN);
    if (NULL == pCol) {
      goto error;
    }
    pCol->node.resType = pExpr->resType;
    strcpy(pCol->colName, pExpr->aliasName);
    if (TSDB_CODE_SUCCESS != nodesListAppend(pList, (SNode*)pCol)) {
      goto error;
    }
  }
  return pList;
error:
  nodesDestroyList(pList);
  return NULL;
}

static SLogicNode* createProjectLogicNode(SPlanContext* pCxt, SSelectStmt* pSelect) {
  SProjectLogicNode* pProject = (SProjectLogicNode*)nodesMakeNode(QUERY_NODE_LOGIC_PLAN_PROJECT);
  CHECK_ALLOC(pProject, NULL);
  pProject->node.id = pCxt->planNodeId++;

  pProject->pProjections = nodesCloneList(pSelect->pProjectionList);

  pProject->node.pTargets = createColumnByProjections(pCxt,pSelect->pProjectionList);
  CHECK_ALLOC(pProject->node.pTargets, (SLogicNode*)pProject);

  return (SLogicNode*)pProject;
}

static SLogicNode* createSelectLogicNode(SPlanContext* pCxt, SSelectStmt* pSelect) {
  SLogicNode* pRoot = createLogicNodeByTable(pCxt, pSelect, pSelect->pFromTable);
  if (TSDB_CODE_SUCCESS == pCxt->errCode && NULL != pSelect->pWhere) {
    pRoot->pConditions = nodesCloneNode(pSelect->pWhere);
    CHECK_ALLOC(pRoot->pConditions, pRoot);
  }
  if (TSDB_CODE_SUCCESS == pCxt->errCode) {
    pRoot = pushLogicNode(pCxt, pRoot, createAggLogicNode(pCxt, pSelect));
  }
  if (TSDB_CODE_SUCCESS == pCxt->errCode) {
    pRoot = pushLogicNode(pCxt, pRoot, createProjectLogicNode(pCxt, pSelect));
  }
  return pRoot;
}

static SLogicNode* createQueryLogicNode(SPlanContext* pCxt, SNode* pStmt) {
  switch (nodeType(pStmt)) {
    case QUERY_NODE_SELECT_STMT:
      return createSelectLogicNode(pCxt, (SSelectStmt*)pStmt);    
    default:
      break;
  }
}

int32_t createLogicPlan(SNode* pNode, SLogicNode** pLogicNode) {
  SPlanContext cxt = { .errCode = TSDB_CODE_SUCCESS, .planNodeId = 1 };
  SLogicNode* pRoot = createQueryLogicNode(&cxt, pNode);
  if (TSDB_CODE_SUCCESS != cxt.errCode) {
    nodesDestroyNode((SNode*)pRoot);
    return cxt.errCode;
  }
  *pLogicNode = pRoot;
  return TSDB_CODE_SUCCESS;
}

int32_t optimize(SLogicNode* pLogicNode) {
  // todo
  return TSDB_CODE_SUCCESS;
}

typedef struct SSubLogicPlan {
  SNode* pRoot; // SLogicNode
  bool haveSuperTable;
  bool haveSystemTable;
} SSubLogicPlan;

int32_t splitLogicPlan(SSubLogicPlan* pLogicPlan) {
  // todo
  return TSDB_CODE_SUCCESS;
}

typedef struct SSlotIndex {
  int16_t tupleId;
  int16_t slotId;
} SSlotIndex;

typedef struct SPhysiPlanContext {
  int32_t errCode;
  int16_t nextTupleId;
  SArray* pTupleHelper;
} SPhysiPlanContext;

static int32_t getSlotKey(SNode* pNode, char* pKey) {
  if (QUERY_NODE_COLUMN == nodeType(pNode)) {
    return sprintf(pKey, "%s.%s", ((SColumnNode*)pNode)->tableAlias, ((SColumnNode*)pNode)->colName);
  } else {
    return sprintf(pKey, "%s", ((SExprNode*)pNode)->aliasName);
  }
}

static SNode* createColumnRef(SNode* pNode, int16_t tupleId, int16_t slotId) {
  SColumnRefNode* pCol = (SColumnRefNode*)nodesMakeNode(QUERY_NODE_COLUMN_REF);
  if (NULL == pCol) {
    return NULL;
  }
  pCol->dataType = ((SExprNode*)pNode)->resType;
  pCol->tupleId = tupleId;
  pCol->slotId = slotId;
  pCol->columnId = (QUERY_NODE_COLUMN == nodeType(pNode) ? ((SColumnNode*)pNode)->colId : -1);
  return (SNode*)pCol;
}

static SNode* createSlotDesc(SPhysiPlanContext* pCxt, const SNode* pNode, int16_t slotId) {
  SSlotDescNode* pSlot = (SSlotDescNode*)nodesMakeNode(QUERY_NODE_SLOT_DESC);
  CHECK_ALLOC(pSlot, NULL);
  pSlot->slotId = slotId;
  pSlot->dataType = ((SExprNode*)pNode)->resType;
  pSlot->srcTupleId = -1;
  pSlot->srcSlotId = -1;
  pSlot->reserve = false;
  pSlot->output = true;
  return (SNode*)pSlot;
}

static SNode* createTarget(SNode* pNode, int16_t tupleId, int16_t slotId) {
  STargetNode* pTarget = (STargetNode*)nodesMakeNode(QUERY_NODE_TARGET);
  if (NULL == pTarget) {
    return NULL;
  }
  pTarget->tupleId = tupleId;
  pTarget->slotId = slotId;
  pTarget->pExpr = nodesCloneNode(pNode);
  if (NULL == pTarget->pExpr) {
    nodesDestroyNode((SNode*)pTarget);
    return NULL;
  }
  return (SNode*)pTarget;
}

static int32_t addTupleDesc(SPhysiPlanContext* pCxt, SNodeList* pList, STupleDescNode* pTuple, SNodeList** pOutput) {
  pTuple->tupleId = pCxt->nextTupleId++;

  SHashObj* pHash = NULL;
  if (NULL == pTuple->pSlots) {
    pTuple->pSlots = nodesMakeList();
    CHECK_ALLOC(pTuple->pSlots, TSDB_CODE_OUT_OF_MEMORY);

    pHash = taosHashInit(LIST_LENGTH(pList), taosGetDefaultHashFunction(TSDB_DATA_TYPE_BINARY), true, HASH_NO_LOCK);
    CHECK_ALLOC(pHash, TSDB_CODE_OUT_OF_MEMORY);
    if (NULL == taosArrayInsert(pCxt->pTupleHelper, pTuple->tupleId, &pHash)) {
      taosHashCleanup(pHash);
      return TSDB_CODE_OUT_OF_MEMORY;
    }
  } else {
    pHash = taosArrayGetP(pCxt->pTupleHelper, pTuple->tupleId);
  }
  
  *pOutput = nodesMakeList();
  CHECK_ALLOC(*pOutput, TSDB_CODE_OUT_OF_MEMORY);

  SNode* pNode = NULL;
  int16_t slotId = 0;
  FOREACH(pNode, pList) {
    SNode* pSlot = createSlotDesc(pCxt, pNode, slotId);
    CHECK_ALLOC(pSlot, TSDB_CODE_OUT_OF_MEMORY);
    if (TSDB_CODE_SUCCESS != nodesListAppend(pTuple->pSlots, (SNode*)pSlot)) {
      nodesDestroyNode(pSlot);
      return TSDB_CODE_OUT_OF_MEMORY;
    }

    SNode* pTarget = createTarget(pNode, pTuple->tupleId, slotId);
    CHECK_ALLOC(pTarget, TSDB_CODE_OUT_OF_MEMORY);
    if (TSDB_CODE_SUCCESS != nodesListAppend(*pOutput, pTarget)) {
      nodesDestroyNode(pTarget);
      return TSDB_CODE_OUT_OF_MEMORY;
    }

    SSlotIndex index = { .tupleId = pTuple->tupleId, .slotId = slotId };
    char name[TSDB_TABLE_NAME_LEN + TSDB_COL_NAME_LEN];
    int32_t len = getSlotKey(pNode, name);
    CHECK_CODE(taosHashPut(pHash, name, len, &index, sizeof(SSlotIndex)), TSDB_CODE_OUT_OF_MEMORY);

    ++slotId;
  }
  return TSDB_CODE_SUCCESS;
}

typedef struct STransformCxt {
  int32_t errCode;
  SHashObj* pHash;
} STransformCxt;

static EDealRes doTransform(SNode** pNode, void* pContext) {
  if (QUERY_NODE_COLUMN == nodeType(*pNode)) {
    STransformCxt* pCxt = (STransformCxt*)pContext;
    char name[TSDB_TABLE_NAME_LEN + TSDB_COL_NAME_LEN];
    int32_t len = getSlotKey(*pNode, name);
    SSlotIndex* pIndex = taosHashGet(pCxt->pHash, name, len);
    if (NULL != pIndex) {
      *pNode = createColumnRef(*pNode, pIndex->tupleId, pIndex->slotId);
      CHECK_ALLOC(*pNode, DEAL_RES_ERROR);
      return DEAL_RES_IGNORE_CHILD;
    }
  }
  return DEAL_RES_CONTINUE;
}

static SNode* transformForPhysiPlan(SPhysiPlanContext* pCxt, int16_t tupleId, SNode* pNode) {
  SNode* pRes = nodesCloneNode(pNode);
  CHECK_ALLOC(pRes, NULL);
  STransformCxt cxt = { .errCode = TSDB_CODE_SUCCESS, .pHash = taosArrayGetP(pCxt->pTupleHelper, tupleId) };
  nodesRewriteNode(&pRes, doTransform, &cxt);
  if (TSDB_CODE_SUCCESS != cxt.errCode) {
    nodesDestroyNode(pRes);
    return NULL;
  }
  return pRes;
}

static SNodeList* transformListForPhysiPlan(SPhysiPlanContext* pCxt, int16_t tupleId, SNodeList* pList) {
  SNodeList* pRes = nodesCloneList(pList);
  CHECK_ALLOC(pRes, NULL);
  STransformCxt cxt = { .errCode = TSDB_CODE_SUCCESS, .pHash = taosArrayGetP(pCxt->pTupleHelper, tupleId) };
  nodesRewriteList(pRes, doTransform, &cxt);
  if (TSDB_CODE_SUCCESS != cxt.errCode) {
    nodesDestroyList(pRes);
    return NULL;
  }
  return pRes;
}

static SPhysiNode* makePhysiNode(ENodeType type) {
  SPhysiNode* pPhysiNode = (SPhysiNode*)nodesMakeNode(type);
  if (NULL == pPhysiNode) {
    return NULL;
  }
  pPhysiNode->outputTuple.type = QUERY_NODE_TUPLE_DESC;
  return pPhysiNode;
}

static int32_t initScanPhysiNode(SPhysiPlanContext* pCxt, SScanLogicNode* pScanLogicNode, SScanPhysiNode* pScanPhysiNode) {
  CHECK_CODE(addTupleDesc(pCxt, pScanLogicNode->pScanCols, &pScanPhysiNode->node.outputTuple, &pScanPhysiNode->pScanCols), TSDB_CODE_OUT_OF_MEMORY);

  if (NULL != pScanLogicNode->node.pConditions) {
    pScanPhysiNode->node.pConditions = transformForPhysiPlan(pCxt, pScanPhysiNode->node.outputTuple.tupleId, pScanLogicNode->node.pConditions);
    CHECK_ALLOC(pScanPhysiNode->node.pConditions, TSDB_CODE_OUT_OF_MEMORY);
  }

  pScanPhysiNode->uid = pScanLogicNode->pMeta->uid;
  pScanPhysiNode->tableType = pScanLogicNode->pMeta->tableType;
  pScanPhysiNode->order = TSDB_ORDER_ASC;
  pScanPhysiNode->count = 1;
  pScanPhysiNode->reverse = 0;

  return TSDB_CODE_SUCCESS;
}

static SPhysiNode* createTagScanPhysiNode(SPhysiPlanContext* pCxt, SScanLogicNode* pScanLogicNode) {
  STagScanPhysiNode* pTagScan = (STagScanPhysiNode*)makePhysiNode(QUERY_NODE_PHYSICAL_PLAN_TAG_SCAN);
  CHECK_ALLOC(pTagScan, NULL);
  CHECK_CODE(initScanPhysiNode(pCxt, pScanLogicNode, (SScanPhysiNode*)pTagScan), (SPhysiNode*)pTagScan);
  return (SPhysiNode*)pTagScan;
}

static SPhysiNode* createTableScanPhysiNode(SPhysiPlanContext* pCxt, SScanLogicNode* pScanLogicNode) {
  STableScanPhysiNode* pTableScan = (STableScanPhysiNode*)makePhysiNode(QUERY_NODE_PHYSICAL_PLAN_TABLE_SCAN);
  CHECK_ALLOC(pTableScan, NULL);
  CHECK_CODE(initScanPhysiNode(pCxt, pScanLogicNode, (SScanPhysiNode*)pTableScan), (SPhysiNode*)pTableScan);
  pTableScan->scanFlag = pScanLogicNode->scanFlag;
  pTableScan->scanRange = pScanLogicNode->scanRange;
  return (SPhysiNode*)pTableScan;
}

static SPhysiNode* createScanPhysiNode(SPhysiPlanContext* pCxt, SScanLogicNode* pScanLogicNode) {
  switch (pScanLogicNode->scanType) {
    case SCAN_TYPE_TAG:
      return createTagScanPhysiNode(pCxt, pScanLogicNode);
    case SCAN_TYPE_TABLE:
      return createTableScanPhysiNode(pCxt, pScanLogicNode);
    case SCAN_TYPE_STABLE:
    case SCAN_TYPE_STREAM:
      break;
    default:
      break;
  }
}

static SPhysiNode* createProjectPhysiNode(SPhysiPlanContext* pCxt, SProjectLogicNode* pProjectLogicNode) {
  SProjectPhysiNode* pProject = (SProjectPhysiNode*)makePhysiNode(QUERY_NODE_PHYSICAL_PLAN_PROJECT);
  CHECK_ALLOC(pProject, NULL);

  SNodeList* pProjections = transformListForPhysiPlan(pCxt, pProject->node.outputTuple.tupleId, pProjectLogicNode->pProjections);
  CHECK_ALLOC(pProjections, (SPhysiNode*)pProject);
  CHECK_CODE(addTupleDesc(pCxt, pProjections, &pProject->node.outputTuple, &pProject->pProjections), (SPhysiNode*)pProject);
  nodesDestroyList(pProjections);

  if (NULL != pProjectLogicNode->node.pConditions) {
    pProject->node.pConditions = transformForPhysiPlan(pCxt, pProject->node.outputTuple.tupleId, pProjectLogicNode->node.pConditions);
    CHECK_ALLOC(pProject->node.pConditions, (SPhysiNode*)pProject);
  }

  return (SPhysiNode*)pProject;
}

static SPhysiNode* createPhysiNode(SPhysiPlanContext* pCxt, SLogicNode* pLogicPlan) {
  SNodeList* pChildern = nodesMakeList();
  CHECK_ALLOC(pChildern, NULL);

  SNode* pLogicChild;
  FOREACH(pLogicChild, pLogicPlan->pChildren) {
    SNode* pChildPhyNode = (SNode*)createPhysiNode(pCxt, (SLogicNode*)pLogicChild);
    if (TSDB_CODE_SUCCESS != nodesListAppend(pChildern, pChildPhyNode)) {
      pCxt->errCode = TSDB_CODE_OUT_OF_MEMORY;
      nodesDestroyList(pChildern);
      return NULL;
    }
  }

  SPhysiNode* pPhyNode = NULL;
  switch (nodeType(pLogicPlan)) {
    case QUERY_NODE_LOGIC_PLAN_SCAN:
      pPhyNode = createScanPhysiNode(pCxt, (SScanLogicNode*)pLogicPlan);
      break;
    case QUERY_NODE_LOGIC_PLAN_JOIN:
      break;
    case QUERY_NODE_LOGIC_PLAN_AGG:
      break;
    case QUERY_NODE_LOGIC_PLAN_PROJECT:
      pPhyNode = createProjectPhysiNode(pCxt, (SProjectLogicNode*)pLogicPlan);
      break;
    default:
      break;
  }

  if (NULL != pPhyNode) {
    pPhyNode->pChildren = pChildern;
    SNode* pChild;
    FOREACH(pChild, pPhyNode->pChildren) {
      ((SPhysiNode*)pChild)->pParent = pPhyNode;
    }
  }

  return pPhyNode;
}

int32_t createPhysiPlan(SLogicNode* pLogicNode, SPhysiNode** pPhyNode) {
  SPhysiPlanContext cxt = { .errCode = TSDB_CODE_SUCCESS, .nextTupleId = 0, .pTupleHelper = taosArrayInit(32, POINTER_BYTES) };
  if (NULL == cxt.pTupleHelper) {
    return TSDB_CODE_OUT_OF_MEMORY;
  }
  *pPhyNode = createPhysiNode(&cxt, pLogicNode);
  return cxt.errCode;
}

int32_t buildPhysiPlan(SLogicNode* pLogicNode, SPhysiNode** pPhyNode) {
  // split
  // scale out
  // maping
  // create
  return TSDB_CODE_SUCCESS;
}
