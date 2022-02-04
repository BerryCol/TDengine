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

#include "trow.h"
#include "tarray.h"

const uint8_t tdVTypeByte[3] = {
    TD_VTYPE_NORM_BYTE,  // TD_VTYPE_NORM
    TD_VTYPE_NONE_BYTE,  // TD_VTYPE_NONE
    TD_VTYPE_NULL_BYTE,  // TD_VTYPE_NULL
};

// static void dataColSetNEleNull(SDataCol *pCol, int nEle);
static void tdMergeTwoDataCols(SDataCols *target, SDataCols *src1, int *iter1, int limit1, SDataCols *src2, int *iter2,
                               int limit2, int tRows, bool forceSetNull);

static FORCE_INLINE void dataColSetNullAt(SDataCol *pCol, int index, bool setBitmap) {
  if (IS_VAR_DATA_TYPE(pCol->type)) {
    pCol->dataOff[index] = pCol->len;
    char *ptr = POINTER_SHIFT(pCol->pData, pCol->len);
    setVardataNull(ptr, pCol->type);
    pCol->len += varDataTLen(ptr);
  } else {
    setNull(POINTER_SHIFT(pCol->pData, TYPE_BYTES[pCol->type] * index), pCol->type, pCol->bytes);
    pCol->len += TYPE_BYTES[pCol->type];
  }
  if (setBitmap) {
    tdSetBitmapValType(pCol->pBitmap, index, TD_VTYPE_NONE);
  }
}

// static void dataColSetNEleNull(SDataCol *pCol, int nEle) {
//   if (IS_VAR_DATA_TYPE(pCol->type)) {
//     pCol->len = 0;
//     for (int i = 0; i < nEle; i++) {
//       dataColSetNullAt(pCol, i);
//     }
//   } else {
//     setNullN(pCol->pData, pCol->type, pCol->bytes, nEle);
//     pCol->len = TYPE_BYTES[pCol->type] * nEle;
//   }
// }

int32_t tdSetBitmapValTypeN(void *pBitmap, int16_t nEle, TDRowValT valType) {
  TASSERT(valType < TD_VTYPE_MAX);
  int16_t nBytes = nEle / TD_VTYPE_PARTS;
  for (int i = 0; i < nBytes; ++i) {
    *(uint8_t *)pBitmap = tdVTypeByte[valType];
    pBitmap = POINTER_SHIFT(pBitmap, 1);
  }
  int16_t nLeft = nEle - nBytes * TD_VTYPE_BITS;

  for (int j = 0; j < nLeft; ++j) {
    tdSetBitmapValType(pBitmap, j, valType);
  }
  return TSDB_CODE_SUCCESS;
}

static FORCE_INLINE void dataColSetNoneAt(SDataCol *pCol, int index, bool setBitmap) {
  if (IS_VAR_DATA_TYPE(pCol->type)) {
    pCol->dataOff[index] = pCol->len;
    char *ptr = POINTER_SHIFT(pCol->pData, pCol->len);
    setVardataNull(ptr, pCol->type);
    pCol->len += varDataTLen(ptr);
  } else {
    setNull(POINTER_SHIFT(pCol->pData, TYPE_BYTES[pCol->type] * index), pCol->type, pCol->bytes);
    pCol->len += TYPE_BYTES[pCol->type];
  }
  if(setBitmap) {
    tdSetBitmapValType(pCol->pBitmap, index, TD_VTYPE_NONE);
  }
}

static void dataColSetNEleNone(SDataCol *pCol, int nEle) {
  if (IS_VAR_DATA_TYPE(pCol->type)) {
    pCol->len = 0;
    for (int i = 0; i < nEle; ++i) {
      dataColSetNoneAt(pCol, i, false);
    }
  } else {
    setNullN(pCol->pData, pCol->type, pCol->bytes, nEle);
    pCol->len = TYPE_BYTES[pCol->type] * nEle;
  }
#ifdef TD_SUPPORT_BITMAP
  tdSetBitmapValTypeN(pCol->pBitmap, nEle, TD_VTYPE_NONE);
#endif
}

#if 0
void trbSetRowInfo(SRowBuilder *pRB, bool del, uint16_t sver) {
  // TODO
}

void trbSetRowVersion(SRowBuilder *pRB, uint64_t ver) {
  // TODO
}

void trbSetRowTS(SRowBuilder *pRB, TSKEY ts) {
  // TODO
}

int trbWriteCol(SRowBuilder *pRB, void *pData, col_id_t cid) {
  // TODO
  return 0;
}

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
#include "talgo.h"
#include "tarray.h"
#include "tcoding.h"
#include "tdataformat.h"
#include "ulog.h"
#include "wchar.h"



/**
 * Duplicate the schema and return a new object
 */
STSchema *tdDupSchema(const STSchema *pSchema) {

  int tlen = sizeof(STSchema) + sizeof(STColumn) * schemaNCols(pSchema);
  STSchema *tSchema = (STSchema *)malloc(tlen);
  if (tSchema == NULL) return NULL;

  memcpy((void *)tSchema, (void *)pSchema, tlen);

  return tSchema;
}

/**
 * Encode a schema to dst, and return the next pointer
 */
int tdEncodeSchema(void **buf, STSchema *pSchema) {
  int tlen = 0;
  tlen += taosEncodeFixedI32(buf, schemaVersion(pSchema));
  tlen += taosEncodeFixedI32(buf, schemaNCols(pSchema));

  for (int i = 0; i < schemaNCols(pSchema); i++) {
    STColumn *pCol = schemaColAt(pSchema, i);
    tlen += taosEncodeFixedI8(buf, colType(pCol));
    tlen += taosEncodeFixedI16(buf, colColId(pCol));
    tlen += taosEncodeFixedI16(buf, colBytes(pCol));
  }

  return tlen;
}

/**
 * Decode a schema from a binary.
 */
void *tdDecodeSchema(void *buf, STSchema **pRSchema) {
  int version = 0;
  int numOfCols = 0;
  STSchemaBuilder schemaBuilder;

  buf = taosDecodeFixedI32(buf, &version);
  buf = taosDecodeFixedI32(buf, &numOfCols);

  if (tdInitTSchemaBuilder(&schemaBuilder, version) < 0) return NULL;

  for (int i = 0; i < numOfCols; i++) {
    int8_t  type = 0;
    int16_t colId = 0;
    int16_t bytes = 0;
    buf = taosDecodeFixedI8(buf, &type);
    buf = taosDecodeFixedI16(buf, &colId);
    buf = taosDecodeFixedI16(buf, &bytes);
    if (tdAddColToSchema(&schemaBuilder, type, colId, bytes) < 0) {
      tdDestroyTSchemaBuilder(&schemaBuilder);
      return NULL;
    }
  }

  *pRSchema = tdGetSchemaFromBuilder(&schemaBuilder);
  tdDestroyTSchemaBuilder(&schemaBuilder);
  return buf;
}

int tdInitTSchemaBuilder(STSchemaBuilder *pBuilder, int32_t version) {
  if (pBuilder == NULL) return -1;

  pBuilder->tCols = 256;
  pBuilder->columns = (STColumn *)malloc(sizeof(STColumn) * pBuilder->tCols);
  if (pBuilder->columns == NULL) return -1;

  tdResetTSchemaBuilder(pBuilder, version);
  return 0;
}

void tdDestroyTSchemaBuilder(STSchemaBuilder *pBuilder) {
  if (pBuilder) {
    tfree(pBuilder->columns);
  }
}

void tdResetTSchemaBuilder(STSchemaBuilder *pBuilder, int32_t version) {
  pBuilder->nCols = 0;
  pBuilder->tlen = 0;
  pBuilder->flen = 0;
  pBuilder->vlen = 0;
  pBuilder->version = version;
}

int tdAddColToSchema(STSchemaBuilder *pBuilder, int8_t type, int16_t colId, int16_t bytes) {
  if (!isValidDataType(type)) return -1;

  if (pBuilder->nCols >= pBuilder->tCols) {
    pBuilder->tCols *= 2;
    STColumn* columns = (STColumn *)realloc(pBuilder->columns, sizeof(STColumn) * pBuilder->tCols);
    if (columns == NULL) return -1;
    pBuilder->columns = columns;
  }

  STColumn *pCol = &(pBuilder->columns[pBuilder->nCols]);
  colSetType(pCol, type);
  colSetColId(pCol, colId);
  if (pBuilder->nCols == 0) {
    colSetOffset(pCol, 0);
  } else {
    STColumn *pTCol = &(pBuilder->columns[pBuilder->nCols-1]);
    colSetOffset(pCol, pTCol->offset + TYPE_BYTES[pTCol->type]);
  }

  if (IS_VAR_DATA_TYPE(type)) {
    colSetBytes(pCol, bytes);
    pBuilder->tlen += (TYPE_BYTES[type] + bytes);
    pBuilder->vlen += bytes - sizeof(VarDataLenT);
  } else {
    colSetBytes(pCol, TYPE_BYTES[type]);
    pBuilder->tlen += TYPE_BYTES[type];
    pBuilder->vlen += TYPE_BYTES[type];
  }

  pBuilder->nCols++;
  pBuilder->flen += TYPE_BYTES[type];

  ASSERT(pCol->offset < pBuilder->flen);

  return 0;
}

STSchema *tdGetSchemaFromBuilder(STSchemaBuilder *pBuilder) {
  if (pBuilder->nCols <= 0) return NULL;

  int tlen = sizeof(STSchema) + sizeof(STColumn) * pBuilder->nCols;

  STSchema *pSchema = (STSchema *)malloc(tlen);
  if (pSchema == NULL) return NULL;

  schemaVersion(pSchema) = pBuilder->version;
  schemaNCols(pSchema) = pBuilder->nCols;
  schemaTLen(pSchema) = pBuilder->tlen;
  schemaFLen(pSchema) = pBuilder->flen;
  schemaVLen(pSchema) = pBuilder->vlen;

  memcpy(schemaColAt(pSchema, 0), pBuilder->columns, sizeof(STColumn) * pBuilder->nCols);

  return pSchema;
}

/**
 * Initialize a data row
 */
void tdInitDataRow(STpRow *row, STSchema *pSchema) {
  dataRowSetLen(row, TD_DATA_ROW_HEAD_SIZE + schemaFLen(pSchema));
  dataRowSetVersion(row, schemaVersion(pSchema));
}

STpRow tdNewDataRowFromSchema(STSchema *pSchema) {
  int32_t size = dataRowMaxBytesFromSchema(pSchema);

  STpRow *row = malloc(size);
  if (row == NULL) return NULL;

  tdInitDataRow(row, pSchema);
  return row;
}

/**
 * Free the STpRow object
 */
void tdFreeDataRow(STpRow *row) {
  if (row) free(row);
}

STpRow tdDataRowDup(STpRow *row) {
  STpRow trow = malloc(dataRowLen(row));
  if (trow == NULL) return NULL;

  dataRowCpy(trow, row);
  return trow;
}


void dataColInit(SDataCol *pDataCol, STColumn *pCol, int maxPoints) {
  pDataCol->type = colType(pCol);
  pDataCol->colId = colColId(pCol);
  pDataCol->bytes = colBytes(pCol);
  pDataCol->offset = colOffset(pCol) + TD_DATA_ROW_HEAD_SIZE;

  pDataCol->len = 0;
}

static FORCE_INLINE const void *tdGetColDataOfRowUnsafe(SDataCol *pCol, int row) {
  if (IS_VAR_DATA_TYPE(pCol->type)) {
    return POINTER_SHIFT(pCol->pData, pCol->dataOff[row]);
  } else {
    return POINTER_SHIFT(pCol->pData, TYPE_BYTES[pCol->type] * row);
  }
}

bool isNEleNull(SDataCol *pCol, int nEle) {
  if(isAllRowsNull(pCol)) return true;
  for (int i = 0; i < nEle; i++) {
    if (!isNull(tdGetColDataOfRowUnsafe(pCol, i), pCol->type)) return false;
  }
  return true;
}



void dataColSetOffset(SDataCol *pCol, int nEle) {
  ASSERT(((pCol->type == TSDB_DATA_TYPE_BINARY) || (pCol->type == TSDB_DATA_TYPE_NCHAR)));

  void *tptr = pCol->pData;
  // char *tptr = (char *)(pCol->pData);

  VarDataOffsetT offset = 0;
  for (int i = 0; i < nEle; i++) {
    pCol->dataOff[i] = offset;
    offset += varDataTLen(tptr);
    tptr = POINTER_SHIFT(tptr, varDataTLen(tptr));
  }
}

SDataCols *tdNewDataCols(int maxCols, int maxRows) {
  SDataCols *pCols = (SDataCols *)calloc(1, sizeof(SDataCols));
  if (pCols == NULL) {
    uDebug("malloc failure, size:%" PRId64 " failed, reason:%s", (int64_t)sizeof(SDataCols), strerror(errno));
    return NULL;
  }

  pCols->maxPoints = maxRows;
  pCols->maxCols = maxCols;
  pCols->numOfRows = 0;
  pCols->numOfCols = 0;

  if (maxCols > 0) {
    pCols->cols = (SDataCol *)calloc(maxCols, sizeof(SDataCol));
    if (pCols->cols == NULL) {
      uDebug("malloc failure, size:%" PRId64 " failed, reason:%s", (int64_t)sizeof(SDataCol) * maxCols,
             strerror(errno));
      tdFreeDataCols(pCols);
      return NULL;
    }
    int i;
    for(i = 0; i < maxCols; i++) {
      pCols->cols[i].spaceSize = 0;
      pCols->cols[i].len = 0;
      pCols->cols[i].pData = NULL;
      pCols->cols[i].dataOff = NULL;
    }
  }

  return pCols;
}

int tdInitDataCols(SDataCols *pCols, STSchema *pSchema) {
  int i;
  int oldMaxCols = pCols->maxCols;
  if (schemaNCols(pSchema) > oldMaxCols) {
    pCols->maxCols = schemaNCols(pSchema);
    void* ptr = (SDataCol *)realloc(pCols->cols, sizeof(SDataCol) * pCols->maxCols);
    if (ptr == NULL) return -1;
    pCols->cols = ptr;
    for(i = oldMaxCols; i < pCols->maxCols; i++) {
      pCols->cols[i].pData = NULL;
      pCols->cols[i].dataOff = NULL;
      pCols->cols[i].spaceSize = 0;
    }
  }

  tdResetDataCols(pCols);
  pCols->numOfCols = schemaNCols(pSchema);

  for (i = 0; i < schemaNCols(pSchema); i++) {
    dataColInit(pCols->cols + i, schemaColAt(pSchema, i), pCols->maxPoints);
  }
  
  return 0;
}

SDataCols *tdFreeDataCols(SDataCols *pCols) {
  int i;
  if (pCols) {
    if(pCols->cols) {
      int maxCols = pCols->maxCols;
      for(i = 0; i < maxCols; i++) {
        SDataCol *pCol = &pCols->cols[i];
        tfree(pCol->pData);
      }
      free(pCols->cols);
      pCols->cols = NULL;
    }
    free(pCols);
  }
  return NULL;
}

SDataCols *tdDupDataCols(SDataCols *pDataCols, bool keepData) {
  SDataCols *pRet = tdNewDataCols(pDataCols->maxCols, pDataCols->maxPoints);
  if (pRet == NULL) return NULL;

  pRet->numOfCols = pDataCols->numOfCols;
  pRet->sversion = pDataCols->sversion;
  if (keepData) pRet->numOfRows = pDataCols->numOfRows;

  for (int i = 0; i < pDataCols->numOfCols; i++) {
    pRet->cols[i].type = pDataCols->cols[i].type;
    pRet->cols[i].colId = pDataCols->cols[i].colId;
    pRet->cols[i].bytes = pDataCols->cols[i].bytes;
    pRet->cols[i].offset = pDataCols->cols[i].offset;

    if (keepData) {
      if (pDataCols->cols[i].len > 0) {
        if(tdAllocMemForCol(&pRet->cols[i], pRet->maxPoints) < 0) {
          tdFreeDataCols(pRet);
          return NULL;
        }
        pRet->cols[i].len = pDataCols->cols[i].len;
        memcpy(pRet->cols[i].pData, pDataCols->cols[i].pData, pDataCols->cols[i].len);
        if (IS_VAR_DATA_TYPE(pRet->cols[i].type)) {
          int dataOffSize = sizeof(VarDataOffsetT) * pDataCols->maxPoints;
          memcpy(pRet->cols[i].dataOff, pDataCols->cols[i].dataOff, dataOffSize);
        }
      }
    }
  }

  return pRet;
}

void tdResetDataCols(SDataCols *pCols) {
  if (pCols != NULL) {
    pCols->numOfRows = 0;
    for (int i = 0; i < pCols->maxCols; i++) {
      dataColReset(pCols->cols + i);
    }
  }
}
#endif

STSRow* tdRowDup(STSRow *row) {
  STSRow* trow = malloc(TD_ROW_LEN(row));
  if (trow == NULL) return NULL;

  tdRowCpy(trow, row);
  return trow;
}

int tdAppendValToDataCol(SDataCol *pCol, TDRowValT valType, const void *val, int numOfRows, int maxPoints) {
  ASSERT(pCol != NULL);

  // Assume that, the columns not specified during insert/upsert is None.
  if (isAllRowsNone(pCol)) {
    if (tdValIsNone(valType)) {
      // all None value yet, just return
      return 0;
    }

    if (tdAllocMemForCol(pCol, maxPoints) < 0) return -1;
    if (numOfRows > 0) {
      // Find the first not None value, fill all previous values as None
      dataColSetNEleNone(pCol, numOfRows);
    }
  }
  if (!tdValTypeIsNorm(valType)) {
    // TODO:
    // 1. back compatibility and easy to debug with codes of 2.0 to save NULL values.
    // 2. later on, considering further optimization, don't save Null/None for VarType.
    val = getNullValue(pCol->type);
  }
  if (IS_VAR_DATA_TYPE(pCol->type)) {
    // set offset
    pCol->dataOff[numOfRows] = pCol->len;
    // Copy data
    memcpy(POINTER_SHIFT(pCol->pData, pCol->len), val, varDataTLen(val));
    // Update the length
    pCol->len += varDataTLen(val);
  } else {
    ASSERT(pCol->len == TYPE_BYTES[pCol->type] * numOfRows);
    memcpy(POINTER_SHIFT(pCol->pData, pCol->len), val, pCol->bytes);
    pCol->len += pCol->bytes;
  }
#ifdef TD_SUPPORT_BITMAP
  tdSetBitmapValType(pCol->pBitmap, numOfRows, valType);
#endif
  return 0;
}

// internal
static int32_t tdAppendTpRowToDataCol(STSRow *pRow, STSchema *pSchema, SDataCols *pCols) {
  ASSERT(pCols->numOfRows == 0 || dataColsKeyLast(pCols) < TD_ROW_TSKEY(pRow));

  int   rcol = 1;
  int   dcol = 1;
  void *pBitmap = tdGetBitmapAddrTp(pRow, pSchema->flen);

  SDataCol *pDataCol = &(pCols->cols[0]);
  if (pDataCol->colId == PRIMARYKEY_TIMESTAMP_COL_ID) {
    tdAppendValToDataCol(pDataCol, TD_VTYPE_NORM, &pRow->ts,  pCols->numOfRows, pCols->maxPoints);
  }

  while (dcol < pCols->numOfCols) {
    pDataCol = &(pCols->cols[dcol]);
    if (rcol >= schemaNCols(pSchema)) {
      tdAppendValToDataCol(pDataCol, TD_VTYPE_NULL, NULL, pCols->numOfRows, pCols->maxPoints);
      ++dcol;
      continue;
    }

    STColumn *pRowCol = schemaColAt(pSchema, rcol);
    SCellVal  sVal = {0};
    if (pRowCol->colId == pDataCol->colId) {
      if (tdGetTpRowValOfCol(&sVal, pRow, pBitmap, pRowCol->type, pRowCol->offset - sizeof(TSKEY), rcol - 1) < 0) {
        return terrno;
      }
      tdAppendValToDataCol(pDataCol, sVal.valType, sVal.val, pCols->numOfRows, pCols->maxPoints);
      ++dcol;
      ++rcol;
    } else if (pRowCol->colId < pDataCol->colId) {
      ++rcol;
    } else {
      tdAppendValToDataCol(pDataCol, TD_VTYPE_NULL, NULL, pCols->numOfRows, pCols->maxPoints);
      ++dcol;
    }
  }
  ++pCols->numOfRows;

  return TSDB_CODE_SUCCESS;
}
// internal
static int32_t tdAppendKvRowToDataCol(STSRow *pRow, STSchema *pSchema, SDataCols *pCols) {
  ASSERT(pCols->numOfRows == 0 || dataColsKeyLast(pCols) < TD_ROW_TSKEY(pRow));

  int   rcol = 0;
  int   dcol = 1;
  int   tRowCols = TD_ROW_NCOLS(pRow) - 1;  // the primary TS key not included in kvRowColIdx part
  int   tSchemaCols = schemaNCols(pSchema) - 1;
  void *pBitmap = tdGetBitmapAddrKv(pRow, TD_ROW_NCOLS(pRow));

  SDataCol *pDataCol = &(pCols->cols[0]);
  if (pDataCol->colId == PRIMARYKEY_TIMESTAMP_COL_ID) {
    tdAppendValToDataCol(pDataCol, TD_VTYPE_NORM, &pRow->ts, pCols->numOfRows, pCols->maxPoints);
  }

  while (dcol < pCols->numOfCols) {
    pDataCol = &(pCols->cols[dcol]);
    if (rcol >= tRowCols || rcol >= tSchemaCols) {
      tdAppendValToDataCol(pDataCol, TD_VTYPE_NULL, NULL, pCols->numOfRows, pCols->maxPoints);
      ++dcol;
      continue;
    }

    SKvRowIdx *pIdx = tdKvRowColIdxAt(pRow, rcol);
    int16_t    colIdx = -1;
    if (pIdx) {
      colIdx = POINTER_DISTANCE(pRow->data, pIdx) / sizeof(SKvRowIdx);
    }
    SCellVal sVal = {0};
    if (pIdx->colId == pDataCol->colId) {
      if (tdGetKvRowValOfCol(&sVal, pRow, pBitmap, pDataCol->type, pIdx->offset, colIdx) < 0) {
        return terrno;
      }
      tdAppendValToDataCol(pDataCol, sVal.valType, sVal.val, pCols->numOfRows, pCols->maxPoints);
      ++dcol;
      ++rcol;
    } else if (pIdx->colId < pDataCol->colId) {
      ++rcol;
    } else {
      tdAppendValToDataCol(pDataCol, TD_VTYPE_NULL, NULL, pCols->numOfRows, pCols->maxPoints);
      ++dcol;
    }
  }
  ++pCols->numOfRows;

  return TSDB_CODE_SUCCESS;
}

/**
 * @brief exposed
 *
 * @param pRow
 * @param pSchema
 * @param pCols
 * @param forceSetNull
 */
int32_t tdAppendSTSRowToDataCol(STSRow *pRow, STSchema *pSchema, SDataCols *pCols, bool forceSetNull) {
  if (TD_IS_TP_ROW(pRow)) {
    return tdAppendTpRowToDataCol(pRow, pSchema, pCols);
  } else if (TD_IS_KV_ROW(pRow)) {
    return tdAppendKvRowToDataCol(pRow, pSchema, pCols);
  } else {
    ASSERT(0);
  }
  return TSDB_CODE_SUCCESS;
}

int tdMergeDataCols(SDataCols *target, SDataCols *source, int rowsToMerge, int *pOffset, bool forceSetNull) {
  ASSERT(rowsToMerge > 0 && rowsToMerge <= source->numOfRows);
  ASSERT(target->numOfCols == source->numOfCols);
  int offset = 0;

  if (pOffset == NULL) {
    pOffset = &offset;
  }

  SDataCols *pTarget = NULL;

  if ((target->numOfRows == 0) || (dataColsKeyLast(target) < dataColsKeyAtRow(source, *pOffset))) {  // No overlap
    ASSERT(target->numOfRows + rowsToMerge <= target->maxPoints);
    for (int i = 0; i < rowsToMerge; i++) {
      for (int j = 0; j < source->numOfCols; j++) {
        if (source->cols[j].len > 0 || target->cols[j].len > 0) {
          SCellVal sVal = {0};
          if (tdGetColDataOfRow(&sVal, source->cols + j, i + (*pOffset)) < 0) {
            TASSERT(0);
          }
          tdAppendValToDataCol(target->cols + j, sVal.valType, sVal.val, target->numOfRows, target->maxPoints);
        }
      }
      target->numOfRows++;
    }
    (*pOffset) += rowsToMerge;
  } else {
    pTarget = tdDupDataCols(target, true);
    if (pTarget == NULL) goto _err;

    int iter1 = 0;
    tdMergeTwoDataCols(target, pTarget, &iter1, pTarget->numOfRows, source, pOffset, source->numOfRows,
                       pTarget->numOfRows + rowsToMerge, forceSetNull);
  }

  tdFreeDataCols(pTarget);
  return 0;

_err:
  tdFreeDataCols(pTarget);
  return -1;
}

// src2 data has more priority than src1
static void tdMergeTwoDataCols(SDataCols *target, SDataCols *src1, int *iter1, int limit1, SDataCols *src2, int *iter2,
                               int limit2, int tRows, bool forceSetNull) {
  tdResetDataCols(target);
  ASSERT(limit1 <= src1->numOfRows && limit2 <= src2->numOfRows);

  while (target->numOfRows < tRows) {
    if (*iter1 >= limit1 && *iter2 >= limit2) break;

    TSKEY key1 = (*iter1 >= limit1) ? INT64_MAX : dataColsKeyAt(src1, *iter1);
    TKEY  tkey1 = (*iter1 >= limit1) ? TKEY_NULL : dataColsTKeyAt(src1, *iter1);
    TSKEY key2 = (*iter2 >= limit2) ? INT64_MAX : dataColsKeyAt(src2, *iter2);
    // TKEY  tkey2 = (*iter2 >= limit2) ? TKEY_NULL : dataColsTKeyAt(src2, *iter2);

    ASSERT(tkey1 == TKEY_NULL || (!TKEY_IS_DELETED(tkey1)));

    if (key1 < key2) {
      for (int i = 0; i < src1->numOfCols; i++) {
        ASSERT(target->cols[i].type == src1->cols[i].type);
        if (src1->cols[i].len > 0 || target->cols[i].len > 0) {
          SCellVal sVal = {0};
          if (tdGetColDataOfRow(&sVal, src1->cols + i, *iter1) < 0) {
            TASSERT(0);
          }
          tdAppendValToDataCol(&(target->cols[i]), sVal.valType, sVal.val, target->numOfRows, target->maxPoints);
        }
      }

      target->numOfRows++;
      (*iter1)++;
    } else if (key1 >= key2) {
      // if ((key1 > key2) || (key1 == key2 && !TKEY_IS_DELETED(tkey2))) {
      if ((key1 > key2) || (key1 == key2)) {
        for (int i = 0; i < src2->numOfCols; i++) {
          SCellVal sVal = {0};
          ASSERT(target->cols[i].type == src2->cols[i].type);
          if (src2->cols[i].len > 0 && !isNull(src2->cols[i].pData, src2->cols[i].type)) {
            if (tdGetColDataOfRow(&sVal, src1->cols + i, *iter1) < 0) {
              TASSERT(0);
            }
            tdAppendValToDataCol(&(target->cols[i]), sVal.valType, sVal.val, target->numOfRows, target->maxPoints);
          } else if (!forceSetNull && key1 == key2 && src1->cols[i].len > 0) {
            if (tdGetColDataOfRow(&sVal, src1->cols + i, *iter1) < 0) {
              TASSERT(0);
            }
            tdAppendValToDataCol(&(target->cols[i]), sVal.valType, sVal.val, target->numOfRows, target->maxPoints);
          } else if (target->cols[i].len > 0) {
            dataColSetNullAt(&target->cols[i], target->numOfRows, true);
          }
        }
        target->numOfRows++;
      }

      (*iter2)++;
      if (key1 == key2) (*iter1)++;
    }

    ASSERT(target->numOfRows <= target->maxPoints);
  }
}

#if 0

SKVRow tdKVRowDup(SKVRow row) {
  SKVRow trow = malloc(kvRowLen(row));
  if (trow == NULL) return NULL;

  kvRowCpy(trow, row);
  return trow;
}

static int compareColIdx(const void* a, const void* b) {
  const SColIdx* x = (const SColIdx*)a;
  const SColIdx* y = (const SColIdx*)b;
  if (x->colId > y->colId) {
    return 1;
  }
  if (x->colId < y->colId) {
    return -1;
  }
  return 0;
}

void tdSortKVRowByColIdx(SKVRow row) {
  qsort(kvRowColIdx(row), kvRowNCols(row), sizeof(SColIdx), compareColIdx);
}

int tdSetKVRowDataOfCol(SKVRow *orow, int16_t colId, int8_t type, void *value) {
  SColIdx *pColIdx = NULL;
  SKVRow   row = *orow;
  SKVRow   nrow = NULL;
  void *   ptr = taosbsearch(&colId, kvRowColIdx(row), kvRowNCols(row), sizeof(SColIdx), comparTagId, TD_GE);

  if (ptr == NULL || ((SColIdx *)ptr)->colId > colId) {  // need to add a column value to the row
    int diff = IS_VAR_DATA_TYPE(type) ? varDataTLen(value) : TYPE_BYTES[type];
    int nRowLen = kvRowLen(row) + sizeof(SColIdx) + diff;
    int oRowCols = kvRowNCols(row);

    ASSERT(diff > 0);
    nrow = malloc(nRowLen);
    if (nrow == NULL) return -1;

    kvRowSetLen(nrow, nRowLen);
    kvRowSetNCols(nrow, oRowCols + 1);

    memcpy(kvRowColIdx(nrow), kvRowColIdx(row), sizeof(SColIdx) * oRowCols);
    memcpy(kvRowValues(nrow), kvRowValues(row), kvRowValLen(row));

    pColIdx = kvRowColIdxAt(nrow, oRowCols);
    pColIdx->colId = colId;
    pColIdx->offset = kvRowValLen(row);

    memcpy(kvRowColVal(nrow, pColIdx), value, diff);  // copy new value

    tdSortKVRowByColIdx(nrow);

    *orow = nrow;
    free(row);
  } else {
    ASSERT(((SColIdx *)ptr)->colId == colId);
    if (IS_VAR_DATA_TYPE(type)) {
      void *pOldVal = kvRowColVal(row, (SColIdx *)ptr);

      if (varDataTLen(value) == varDataTLen(pOldVal)) { // just update the column value in place
        memcpy(pOldVal, value, varDataTLen(value));
      } else {  // need to reallocate the memory
        int16_t nlen = kvRowLen(row) + (varDataTLen(value) - varDataTLen(pOldVal));
        ASSERT(nlen > 0);
        nrow = malloc(nlen);
        if (nrow == NULL) return -1;

        kvRowSetLen(nrow, nlen);
        kvRowSetNCols(nrow, kvRowNCols(row));

        int zsize = sizeof(SColIdx) * kvRowNCols(row) + ((SColIdx *)ptr)->offset;
        memcpy(kvRowColIdx(nrow), kvRowColIdx(row), zsize);
        memcpy(kvRowColVal(nrow, ((SColIdx *)ptr)), value, varDataTLen(value));
        // Copy left value part
        int lsize = kvRowLen(row) - TD_KV_ROW_HEAD_SIZE - zsize - varDataTLen(pOldVal);
        if (lsize > 0) {
          memcpy(POINTER_SHIFT(nrow, TD_KV_ROW_HEAD_SIZE + zsize + varDataTLen(value)),
                 POINTER_SHIFT(row, TD_KV_ROW_HEAD_SIZE + zsize + varDataTLen(pOldVal)), lsize);
        }

        for (int i = 0; i < kvRowNCols(nrow); i++) {
          pColIdx = kvRowColIdxAt(nrow, i);

          if (pColIdx->offset > ((SColIdx *)ptr)->offset) {
            pColIdx->offset = pColIdx->offset - varDataTLen(pOldVal) + varDataTLen(value);
          }
        }

        *orow = nrow;
        free(row);
      }
    } else {
      memcpy(kvRowColVal(row, (SColIdx *)ptr), value, TYPE_BYTES[type]);
    }
  }

  return 0;
}

int tdEncodeKVRow(void **buf, SKVRow row) {
  // May change the encode purpose
  if (buf != NULL) {
    kvRowCpy(*buf, row);
    *buf = POINTER_SHIFT(*buf, kvRowLen(row));
  }

  return kvRowLen(row);
}

void *tdDecodeKVRow(void *buf, SKVRow *row) {
  *row = tdKVRowDup(buf);
  if (*row == NULL) return NULL;
  return POINTER_SHIFT(buf, kvRowLen(*row));
}

int tdInitKVRowBuilder(SKVRowBuilder *pBuilder) {
  pBuilder->tCols = 128;
  pBuilder->nCols = 0;
  pBuilder->pColIdx = (SColIdx *)malloc(sizeof(SColIdx) * pBuilder->tCols);
  if (pBuilder->pColIdx == NULL) return -1;
  pBuilder->alloc = 1024;
  pBuilder->size = 0;
  pBuilder->buf = malloc(pBuilder->alloc);
  if (pBuilder->buf == NULL) {
    free(pBuilder->pColIdx);
    return -1;
  }
  return 0;
}

void tdDestroyKVRowBuilder(SKVRowBuilder *pBuilder) {
  tfree(pBuilder->pColIdx);
  tfree(pBuilder->buf);
}

void tdResetKVRowBuilder(SKVRowBuilder *pBuilder) {
  pBuilder->nCols = 0;
  pBuilder->size = 0;
}

SKVRow tdGetKVRowFromBuilder(SKVRowBuilder *pBuilder) {
  int tlen = sizeof(SColIdx) * pBuilder->nCols + pBuilder->size;
  if (tlen == 0) return NULL;

  tlen += TD_KV_ROW_HEAD_SIZE;

  SKVRow row = malloc(tlen);
  if (row == NULL) return NULL;

  kvRowSetNCols(row, pBuilder->nCols);
  kvRowSetLen(row, tlen);

  memcpy(kvRowColIdx(row), pBuilder->pColIdx, sizeof(SColIdx) * pBuilder->nCols);
  memcpy(kvRowValues(row), pBuilder->buf, pBuilder->size);

  return row;
}
#endif

STSRow* mergeTwoRows(void *buffer, STSRow* row1, STSRow *row2, STSchema *pSchema1, STSchema *pSchema2) {
#if 0
  ASSERT(TD_ROW_TSKEY(row1) == TD_ROW_TSKEY(row2));
  ASSERT(schemaVersion(pSchema1) == TD_ROW_SVER(row1));
  ASSERT(schemaVersion(pSchema2) == TD_ROW_SVER(row2));
  ASSERT(schemaVersion(pSchema1) >= schemaVersion(pSchema2));
#endif

#if 0
  SArray *stashRow = taosArrayInit(pSchema1->numOfCols, sizeof(SColInfo));
  if (stashRow == NULL) {
    return NULL;
  }

  STSRow  pRow = buffer;
  STpRow dataRow = memRowDataBody(pRow);
  memRowSetType(pRow, SMEM_ROW_DATA);
  dataRowSetVersion(dataRow, schemaVersion(pSchema1));  // use latest schema version
  dataRowSetLen(dataRow, (TDRowLenT)(TD_DATA_ROW_HEAD_SIZE + pSchema1->flen));

  TDRowLenT dataLen = 0, kvLen = TD_MEM_ROW_KV_HEAD_SIZE;

  int32_t  i = 0;  // row1
  int32_t  j = 0;  // row2
  int32_t  nCols1 = schemaNCols(pSchema1);
  int32_t  nCols2 = schemaNCols(pSchema2);
  SColInfo colInfo = {0};
  int32_t  kvIdx1 = 0, kvIdx2 = 0;

  while (i < nCols1) {
    STColumn *pCol = schemaColAt(pSchema1, i);
    void *    val1 = tdGetMemRowDataOfColEx(row1, pCol->colId, pCol->type, TD_DATA_ROW_HEAD_SIZE + pCol->offset, &kvIdx1);
    // if val1 != NULL, use val1;
    if (val1 != NULL && !isNull(val1, pCol->type)) {
      tdAppendColVal(dataRow, val1, pCol->type, pCol->offset);
      kvLen += tdGetColAppendLen(SMEM_ROW_KV, val1, pCol->type);
      setSColInfo(&colInfo, pCol->colId, pCol->type, val1);
      taosArrayPush(stashRow, &colInfo);
      ++i;  // next col
      continue;
    }

    void *val2 = NULL;
    while (j < nCols2) {
      STColumn *tCol = schemaColAt(pSchema2, j);
      if (tCol->colId < pCol->colId) {
        ++j;
        continue;
      }
      if (tCol->colId == pCol->colId) {
        val2 = tdGetMemRowDataOfColEx(row2, tCol->colId, tCol->type, TD_DATA_ROW_HEAD_SIZE + tCol->offset, &kvIdx2);
      } else if (tCol->colId > pCol->colId) {
        // set NULL
      }
      break;
    }  // end of while(j<nCols2)
    if (val2 == NULL) {
      val2 = (void *)getNullValue(pCol->type);
    }
    tdAppendColVal(dataRow, val2, pCol->type, pCol->offset);
    if (!isNull(val2, pCol->type)) {
      kvLen += tdGetColAppendLen(SMEM_ROW_KV, val2, pCol->type);
      setSColInfo(&colInfo, pCol->colId, pCol->type, val2);
      taosArrayPush(stashRow, &colInfo);
    }

    ++i;  // next col
  }

  dataLen = memRowTLen(pRow);

  if (kvLen < dataLen) {
    // scan stashRow and generate SKVRow
    memset(buffer, 0, sizeof(dataLen));
    STSRow tRow = buffer;
    memRowSetType(tRow, SMEM_ROW_KV);
    SKVRow kvRow = (SKVRow)memRowKvBody(tRow);
    int16_t nKvNCols = (int16_t) taosArrayGetSize(stashRow);
    kvRowSetLen(kvRow, (TDRowLenT)(TD_KV_ROW_HEAD_SIZE + sizeof(SColIdx) * nKvNCols));
    kvRowSetNCols(kvRow, nKvNCols);
    memRowSetKvVersion(tRow, pSchema1->version);

    int32_t toffset = 0;
    int16_t k;
    for (k = 0; k < nKvNCols; ++k) {
      SColInfo *pColInfo = taosArrayGet(stashRow, k);
      tdAppendKvColVal(kvRow, pColInfo->colVal, true, pColInfo->colId, pColInfo->colType, toffset);
      toffset += sizeof(SColIdx);
    }
    ASSERT(kvLen == memRowTLen(tRow));
  }
  taosArrayDestroy(stashRow);
  return buffer;
  #endif
  return NULL;
}
