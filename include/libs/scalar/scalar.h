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
#ifndef TDENGINE_SCALAR_H
#define TDENGINE_SCALAR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "function.h"
#include "nodes.h"
#include "querynodes.h"

typedef struct SFilterInfo SFilterInfo;

/*
pNode will be freed in API;
*pRes need to freed in caller
*/
int32_t scalarCalculateConstants(SNode *pNode, SNode **pRes);

/* 
pDst need to freed in caller 
*/
int32_t scalarCalculate(SNode *pNode, SArray *pBlockList, SScalarParam *pDst);

int32_t scalarGetOperatorParamNum(EOperatorType type);
int32_t scalarGenerateSetFromList(void **data, void *pNode, uint32_t type);

int32_t vectorGetConvertType(int32_t type1, int32_t type2);
int32_t vectorConvertImpl(SScalarParam* pIn, SScalarParam* pOut);

int32_t absFunction(SScalarParam *pInput, int32_t inputNum, SScalarParam *pOutput);
int32_t logFunction(SScalarParam *pInput, int32_t inputNum, SScalarParam *pOutput);
int32_t powFunction(SScalarParam *pInput, int32_t inputNum, SScalarParam *pOutput);
int32_t sqrtFunction(SScalarParam *pInput, int32_t inputNum, SScalarParam *pOutput);

int32_t sinFunction(SScalarParam *pInput, int32_t inputNum, SScalarParam *pOutput);
int32_t cosFunction(SScalarParam *pInput, int32_t inputNum, SScalarParam *pOutput);
int32_t tanFunction(SScalarParam *pInput, int32_t inputNum, SScalarParam *pOutput);
int32_t asinFunction(SScalarParam *pInput, int32_t inputNum, SScalarParam *pOutput);
int32_t acosFunction(SScalarParam *pInput, int32_t inputNum, SScalarParam *pOutput);
int32_t atanFunction(SScalarParam *pInput, int32_t inputNum, SScalarParam *pOutput);

int32_t ceilFunction(SScalarParam *pInput, int32_t inputNum, SScalarParam *pOutput);
int32_t floorFunction(SScalarParam *pInput, int32_t inputNum, SScalarParam *pOutput);
int32_t roundFunction(SScalarParam *pInput, int32_t inputNum, SScalarParam *pOutput);

#ifdef __cplusplus
}
#endif

#endif  // TDENGINE_SCALAR_H
