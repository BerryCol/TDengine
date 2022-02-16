/**
 * @file func.cpp
 * @author slguan (slguan@taosdata.com)
 * @brief MNODE module func tests
 * @version 1.0
 * @date 2022-01-24
 *
 * @copyright Copyright (c) 2022
 *
 */

#include "sut.h"

class MndTestFunc : public ::testing::Test {
 protected:
  static void SetUpTestSuite() { test.Init("/tmp/mnode_test_func", 9038); }
  static void TearDownTestSuite() { test.Cleanup(); }

  static Testbase test;

 public:
  void SetUp() override {}
  void TearDown() override {}
};

Testbase MndTestFunc::test;

TEST_F(MndTestFunc, 01_Show_Func) {
  test.SendShowMetaReq(TSDB_MGMT_TABLE_FUNC, "");
  CHECK_META("show functions", 7);

  CHECK_SCHEMA(0, TSDB_DATA_TYPE_BINARY, TSDB_FUNC_NAME_LEN + VARSTR_HEADER_SIZE, "name");
  CHECK_SCHEMA(1, TSDB_DATA_TYPE_BINARY, PATH_MAX + VARSTR_HEADER_SIZE, "comment");
  CHECK_SCHEMA(2, TSDB_DATA_TYPE_INT, 4, "aggregate");
  CHECK_SCHEMA(3, TSDB_DATA_TYPE_BINARY, TSDB_TYPE_STR_MAX_LEN + VARSTR_HEADER_SIZE, "outputtype");
  CHECK_SCHEMA(4, TSDB_DATA_TYPE_TIMESTAMP, 8, "create_time");
  CHECK_SCHEMA(5, TSDB_DATA_TYPE_INT, 4, "code_len");
  CHECK_SCHEMA(6, TSDB_DATA_TYPE_INT, 4, "bufsize");

  test.SendShowRetrieveReq();
  EXPECT_EQ(test.GetShowRows(), 0);
}

TEST_F(MndTestFunc, 02_Create_Func) {
  {
    SCreateFuncReq createReq = {0};
    strcpy(createReq.name, "");

    int32_t contLen = tSerializeSCreateFuncReq(NULL, 0, &createReq);
    void*   pReq = rpcMallocCont(contLen);
    tSerializeSCreateFuncReq(pReq, contLen, &createReq);

    SRpcMsg* pRsp = test.SendReq(TDMT_MND_CREATE_FUNC, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_INVALID_FUNC_NAME);
  }

  {
    SCreateFuncReq createReq = {0};
    strcpy(createReq.name, "f1");

    int32_t contLen = tSerializeSCreateFuncReq(NULL, 0, &createReq);
    void*   pReq = rpcMallocCont(contLen);
    tSerializeSCreateFuncReq(pReq, contLen, &createReq);

    SRpcMsg* pRsp = test.SendReq(TDMT_MND_CREATE_FUNC, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_INVALID_FUNC_COMMENT);
  }

  {
    SCreateFuncReq createReq = {0};
    strcpy(createReq.name, "f1");
    createReq.commentSize = TSDB_FUNC_COMMENT_LEN + 1;

    int32_t contLen = tSerializeSCreateFuncReq(NULL, 0, &createReq);
    void*   pReq = rpcMallocCont(contLen);
    tSerializeSCreateFuncReq(pReq, contLen, &createReq);

    SRpcMsg* pRsp = test.SendReq(TDMT_MND_CREATE_FUNC, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_INVALID_FUNC_COMMENT);
  }

  {
    SCreateFuncReq createReq = {0};
    strcpy(createReq.name, "f1");
    createReq.commentSize = TSDB_FUNC_COMMENT_LEN;

    int32_t contLen = tSerializeSCreateFuncReq(NULL, 0, &createReq);
    void*   pReq = rpcMallocCont(contLen);
    tSerializeSCreateFuncReq(pReq, contLen, &createReq);

    SRpcMsg* pRsp = test.SendReq(TDMT_MND_CREATE_FUNC, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_INVALID_FUNC_CODE);
  }

  {
    SCreateFuncReq createReq = {0};
    strcpy(createReq.name, "f1");
    createReq.commentSize = TSDB_FUNC_COMMENT_LEN;
    createReq.codeSize = TSDB_FUNC_CODE_LEN + 1;

    int32_t contLen = tSerializeSCreateFuncReq(NULL, 0, &createReq);
    void*   pReq = rpcMallocCont(contLen);
    tSerializeSCreateFuncReq(pReq, contLen, &createReq);

    SRpcMsg* pRsp = test.SendReq(TDMT_MND_CREATE_FUNC, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_INVALID_FUNC_CODE);
  }

  {
    SCreateFuncReq createReq = {0};
    strcpy(createReq.name, "f1");
    createReq.commentSize = TSDB_FUNC_COMMENT_LEN;
    createReq.codeSize = TSDB_FUNC_CODE_LEN;

    int32_t contLen = tSerializeSCreateFuncReq(NULL, 0, &createReq);
    void*   pReq = rpcMallocCont(contLen);
    tSerializeSCreateFuncReq(pReq, contLen, &createReq);

    SRpcMsg* pRsp = test.SendReq(TDMT_MND_CREATE_FUNC, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_INVALID_FUNC_CODE);
  }

  {
    SCreateFuncReq createReq = {0};
    strcpy(createReq.name, "f1");
    createReq.commentSize = TSDB_FUNC_COMMENT_LEN;
    createReq.codeSize = TSDB_FUNC_CODE_LEN;
    createReq.pCode[0] = 'a';

    int32_t contLen = tSerializeSCreateFuncReq(NULL, 0, &createReq);
    void*   pReq = rpcMallocCont(contLen);
    tSerializeSCreateFuncReq(pReq, contLen, &createReq);

    SRpcMsg* pRsp = test.SendReq(TDMT_MND_CREATE_FUNC, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_INVALID_FUNC_BUFSIZE);
  }

  {
    SCreateFuncReq createReq = {0};
    strcpy(createReq.name, "f1");
    createReq.commentSize = TSDB_FUNC_COMMENT_LEN;
    createReq.codeSize = TSDB_FUNC_CODE_LEN;
    createReq.pCode[0] = 'a';
    createReq.bufSize = TSDB_FUNC_BUF_SIZE + 1;

    int32_t contLen = tSerializeSCreateFuncReq(NULL, 0, &createReq);
    void*   pReq = rpcMallocCont(contLen);
    tSerializeSCreateFuncReq(pReq, contLen, &createReq);

    SRpcMsg* pRsp = test.SendReq(TDMT_MND_CREATE_FUNC, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_INVALID_FUNC_BUFSIZE);
  }

  for (int32_t i = 0; i < 3; ++i) {
    SCreateFuncReq createReq = {0};
    strcpy(createReq.name, "f1");
    createReq.commentSize = TSDB_FUNC_COMMENT_LEN;
    createReq.codeSize = TSDB_FUNC_CODE_LEN;
    createReq.pCode[0] = 'a';
    createReq.bufSize = TSDB_FUNC_BUF_SIZE + 1;
    createReq.igExists = 0;
    if (i == 2) createReq.igExists = 1;
    createReq.funcType = 1;
    createReq.scriptType = 2;
    createReq.outputType = TSDB_DATA_TYPE_SMALLINT;
    createReq.outputLen = 12;
    createReq.bufSize = 4;
    createReq.signature = 5;
    for (int32_t i = 0; i < createReq.commentSize - 1; ++i) {
      createReq.pComment[i] = 'm';
    }
    for (int32_t i = 0; i < createReq.codeSize - 1; ++i) {
      createReq.pCode[i] = 'd';
    }

    int32_t contLen = tSerializeSCreateFuncReq(NULL, 0, &createReq);
    void*   pReq = rpcMallocCont(contLen);
    tSerializeSCreateFuncReq(pReq, contLen, &createReq);

    SRpcMsg* pRsp = test.SendReq(TDMT_MND_CREATE_FUNC, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    if (i == 0 || i == 2) {
      ASSERT_EQ(pRsp->code, 0);
    } else {
      ASSERT_EQ(pRsp->code, TSDB_CODE_MND_FUNC_ALREADY_EXIST);
    }
  }

  test.SendShowMetaReq(TSDB_MGMT_TABLE_FUNC, "");
  CHECK_META("show functions", 7);

  test.SendShowRetrieveReq();
  EXPECT_EQ(test.GetShowRows(), 1);

  CheckBinary("f1", TSDB_FUNC_NAME_LEN);
  CheckBinaryByte('m', TSDB_FUNC_COMMENT_LEN);
  CheckInt32(0);
  CheckBinary("SMALLINT", TSDB_TYPE_STR_MAX_LEN);
  CheckTimestamp();
  CheckInt32(TSDB_FUNC_CODE_LEN);
  CheckInt32(4);
}

TEST_F(MndTestFunc, 03_Retrieve_Func) {
  {
    SRetrieveFuncReq retrieveReq = {0};
    retrieveReq.numOfFuncs = 1;
    retrieveReq.pFuncNames = taosArrayInit(1, TSDB_FUNC_NAME_LEN);
    taosArrayPush(retrieveReq.pFuncNames, "f1");

    int32_t contLen = tSerializeSRetrieveFuncReq(NULL, 0, &retrieveReq);
    void*   pReq = rpcMallocCont(contLen);
    tSerializeSRetrieveFuncReq(pReq, contLen, &retrieveReq);
    taosArrayDestroy(retrieveReq.pFuncNames);

    SRpcMsg* pRsp = test.SendReq(TDMT_MND_RETRIEVE_FUNC, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, 0);

    SRetrieveFuncRsp retrieveRsp = {0};
    tDeserializeSRetrieveFuncRsp(pRsp->pCont, pRsp->contLen, &retrieveRsp);
    EXPECT_EQ(retrieveRsp.numOfFuncs, 1);
    EXPECT_EQ(retrieveRsp.numOfFuncs, (int32_t)taosArrayGetSize(retrieveRsp.pFuncInfos));

    SFuncInfo* pFuncInfo = (SFuncInfo*)taosArrayGet(retrieveRsp.pFuncInfos, 0);

    EXPECT_STREQ(pFuncInfo->name, "f1");
    EXPECT_EQ(pFuncInfo->funcType, 1);
    EXPECT_EQ(pFuncInfo->scriptType, 2);
    EXPECT_EQ(pFuncInfo->outputType, TSDB_DATA_TYPE_SMALLINT);
    EXPECT_EQ(pFuncInfo->outputLen, 12);
    EXPECT_EQ(pFuncInfo->bufSize, 4);
    EXPECT_EQ(pFuncInfo->signature, 5);
    EXPECT_EQ(pFuncInfo->commentSize, TSDB_FUNC_COMMENT_LEN);
    EXPECT_EQ(pFuncInfo->codeSize, TSDB_FUNC_CODE_LEN);

    char comments[TSDB_FUNC_COMMENT_LEN] = {0};
    for (int32_t i = 0; i < TSDB_FUNC_COMMENT_LEN - 1; ++i) {
      comments[i] = 'm';
    }
    char codes[TSDB_FUNC_CODE_LEN] = {0};
    for (int32_t i = 0; i < TSDB_FUNC_CODE_LEN - 1; ++i) {
      codes[i] = 'd';
    }
    EXPECT_STREQ(comments, pFuncInfo->pComment);
    EXPECT_STREQ(codes, pFuncInfo->pCode);
    taosArrayDestroy(retrieveRsp.pFuncInfos);
  }

  {
    SRetrieveFuncReq retrieveReq = {0};
    retrieveReq.numOfFuncs = 0;
    retrieveReq.pFuncNames = taosArrayInit(1, TSDB_FUNC_NAME_LEN);

    int32_t contLen = tSerializeSRetrieveFuncReq(NULL, 0, &retrieveReq);
    void*   pReq = rpcMallocCont(contLen);
    tSerializeSRetrieveFuncReq(pReq, contLen, &retrieveReq);
    taosArrayDestroy(retrieveReq.pFuncNames);

    SRpcMsg* pRsp = test.SendReq(TDMT_MND_RETRIEVE_FUNC, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_INVALID_FUNC_RETRIEVE);
  }

  {
    SRetrieveFuncReq retrieveReq = {0};
    retrieveReq.numOfFuncs = TSDB_FUNC_MAX_RETRIEVE + 1;
    retrieveReq.pFuncNames = taosArrayInit(TSDB_FUNC_MAX_RETRIEVE + 1, TSDB_FUNC_NAME_LEN);
    for (int32_t i = 0; i < TSDB_FUNC_MAX_RETRIEVE + 1; ++i) {
      taosArrayPush(retrieveReq.pFuncNames, "1");
    }

    int32_t contLen = tSerializeSRetrieveFuncReq(NULL, 0, &retrieveReq);
    void*   pReq = rpcMallocCont(contLen);
    tSerializeSRetrieveFuncReq(pReq, contLen, &retrieveReq);
    taosArrayDestroy(retrieveReq.pFuncNames);

    SRpcMsg* pRsp = test.SendReq(TDMT_MND_RETRIEVE_FUNC, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_INVALID_FUNC_RETRIEVE);
  }

  {
    SRetrieveFuncReq retrieveReq = {0};
    retrieveReq.numOfFuncs = 1;
    retrieveReq.pFuncNames = taosArrayInit(1, TSDB_FUNC_NAME_LEN);
    taosArrayPush(retrieveReq.pFuncNames, "f2");

    int32_t contLen = tSerializeSRetrieveFuncReq(NULL, 0, &retrieveReq);
    void*   pReq = rpcMallocCont(contLen);
    tSerializeSRetrieveFuncReq(pReq, contLen, &retrieveReq);
    taosArrayDestroy(retrieveReq.pFuncNames);

    SRpcMsg* pRsp = test.SendReq(TDMT_MND_RETRIEVE_FUNC, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_INVALID_FUNC);
  }

  {
    SCreateFuncReq createReq = {0};
    strcpy(createReq.name, "f2");
    createReq.commentSize = 1024;
    createReq.codeSize = 9527;
    createReq.igExists = 1;
    createReq.funcType = 2;
    createReq.scriptType = 3;
    createReq.outputType = TSDB_DATA_TYPE_BINARY;
    createReq.outputLen = 24;
    createReq.bufSize = 6;
    createReq.signature = 18;
    for (int32_t i = 0; i < createReq.commentSize - 1; ++i) {
      createReq.pComment[i] = 'p';
    }
    for (int32_t i = 0; i < createReq.codeSize - 1; ++i) {
      createReq.pCode[i] = 'q';
    }

    int32_t contLen = tSerializeSCreateFuncReq(NULL, 0, &createReq);
    void*   pReq = rpcMallocCont(contLen);
    tSerializeSCreateFuncReq(pReq, contLen, &createReq);

    SRpcMsg* pRsp = test.SendReq(TDMT_MND_CREATE_FUNC, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, 0);

    test.SendShowMetaReq(TSDB_MGMT_TABLE_FUNC, "");
    CHECK_META("show functions", 7);

    test.SendShowRetrieveReq();
    EXPECT_EQ(test.GetShowRows(), 2);
  }

  {
    SRetrieveFuncReq retrieveReq = {0};
    retrieveReq.numOfFuncs = 1;
    retrieveReq.pFuncNames = taosArrayInit(1, TSDB_FUNC_NAME_LEN);
    taosArrayPush(retrieveReq.pFuncNames, "f2");

    int32_t contLen = tSerializeSRetrieveFuncReq(NULL, 0, &retrieveReq);
    void*   pReq = rpcMallocCont(contLen);
    tSerializeSRetrieveFuncReq(pReq, contLen, &retrieveReq);
    taosArrayDestroy(retrieveReq.pFuncNames);

    SRpcMsg* pRsp = test.SendReq(TDMT_MND_RETRIEVE_FUNC, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, 0);

    SRetrieveFuncRsp retrieveRsp = {0};
    tDeserializeSRetrieveFuncRsp(pRsp->pCont, pRsp->contLen, &retrieveRsp);
    EXPECT_EQ(retrieveRsp.numOfFuncs, 1);
    EXPECT_EQ(retrieveRsp.numOfFuncs, (int32_t)taosArrayGetSize(retrieveRsp.pFuncInfos));

    SFuncInfo* pFuncInfo = (SFuncInfo*)taosArrayGet(retrieveRsp.pFuncInfos, 0);

    EXPECT_STREQ(pFuncInfo->name, "f2");
    EXPECT_EQ(pFuncInfo->funcType, 2);
    EXPECT_EQ(pFuncInfo->scriptType, 3);
    EXPECT_EQ(pFuncInfo->outputType, TSDB_DATA_TYPE_BINARY);
    EXPECT_EQ(pFuncInfo->outputLen, 24);
    EXPECT_EQ(pFuncInfo->bufSize, 6);
    EXPECT_EQ(pFuncInfo->signature, 18);
    EXPECT_EQ(pFuncInfo->commentSize, 1024);
    EXPECT_EQ(pFuncInfo->codeSize, 9527);

    char comments[TSDB_FUNC_COMMENT_LEN] = {0};
    for (int32_t i = 0; i < 1024 - 1; ++i) {
      comments[i] = 'p';
    }
    char codes[TSDB_FUNC_CODE_LEN] = {0};
    for (int32_t i = 0; i < 9527 - 1; ++i) {
      codes[i] = 'q';
    }

    EXPECT_STREQ(comments, pFuncInfo->pComment);
    EXPECT_STREQ(codes, pFuncInfo->pCode);
    taosArrayDestroy(retrieveRsp.pFuncInfos);
  }

  {
    SRetrieveFuncReq retrieveReq = {0};
    retrieveReq.numOfFuncs = 2;
    retrieveReq.pFuncNames = taosArrayInit(1, TSDB_FUNC_NAME_LEN);
    taosArrayPush(retrieveReq.pFuncNames, "f2");
    taosArrayPush(retrieveReq.pFuncNames, "f1");

    int32_t contLen = tSerializeSRetrieveFuncReq(NULL, 0, &retrieveReq);
    void*   pReq = rpcMallocCont(contLen);
    tSerializeSRetrieveFuncReq(pReq, contLen, &retrieveReq);
    taosArrayDestroy(retrieveReq.pFuncNames);

    SRpcMsg* pRsp = test.SendReq(TDMT_MND_RETRIEVE_FUNC, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, 0);

    SRetrieveFuncRsp retrieveRsp = {0};
    tDeserializeSRetrieveFuncRsp(pRsp->pCont, pRsp->contLen, &retrieveRsp);
    EXPECT_EQ(retrieveRsp.numOfFuncs, 2);
    EXPECT_EQ(retrieveRsp.numOfFuncs, (int32_t)taosArrayGetSize(retrieveRsp.pFuncInfos));

    {
      SFuncInfo* pFuncInfo = (SFuncInfo*)taosArrayGet(retrieveRsp.pFuncInfos, 0);
      EXPECT_STREQ(pFuncInfo->name, "f2");
      EXPECT_EQ(pFuncInfo->funcType, 2);
      EXPECT_EQ(pFuncInfo->scriptType, 3);
      EXPECT_EQ(pFuncInfo->outputType, TSDB_DATA_TYPE_BINARY);
      EXPECT_EQ(pFuncInfo->outputLen, 24);
      EXPECT_EQ(pFuncInfo->bufSize, 6);
      EXPECT_EQ(pFuncInfo->signature, 18);
      EXPECT_EQ(pFuncInfo->commentSize, 1024);
      EXPECT_EQ(pFuncInfo->codeSize, 9527);

      char comments[TSDB_FUNC_COMMENT_LEN] = {0};
      for (int32_t i = 0; i < 1024 - 1; ++i) {
        comments[i] = 'p';
      }
      char codes[TSDB_FUNC_CODE_LEN] = {0};
      for (int32_t i = 0; i < 9527 - 1; ++i) {
        codes[i] = 'q';
      }

      EXPECT_STREQ(comments, pFuncInfo->pComment);
      EXPECT_STREQ(codes, pFuncInfo->pCode);
    }

    {
      SFuncInfo* pFuncInfo = (SFuncInfo*)taosArrayGet(retrieveRsp.pFuncInfos, 1);
      EXPECT_STREQ(pFuncInfo->name, "f1");
      EXPECT_EQ(pFuncInfo->funcType, 1);
      EXPECT_EQ(pFuncInfo->scriptType, 2);
      EXPECT_EQ(pFuncInfo->outputType, TSDB_DATA_TYPE_SMALLINT);
      EXPECT_EQ(pFuncInfo->outputLen, 12);
      EXPECT_EQ(pFuncInfo->bufSize, 4);
      EXPECT_EQ(pFuncInfo->signature, 5);
      EXPECT_EQ(pFuncInfo->commentSize, TSDB_FUNC_COMMENT_LEN);
      EXPECT_EQ(pFuncInfo->codeSize, TSDB_FUNC_CODE_LEN);

      char comments[TSDB_FUNC_COMMENT_LEN] = {0};
      for (int32_t i = 0; i < TSDB_FUNC_COMMENT_LEN - 1; ++i) {
        comments[i] = 'm';
      }
      char codes[TSDB_FUNC_CODE_LEN] = {0};
      for (int32_t i = 0; i < TSDB_FUNC_CODE_LEN - 1; ++i) {
        codes[i] = 'd';
      }
      EXPECT_STREQ(comments, pFuncInfo->pComment);
      EXPECT_STREQ(codes, pFuncInfo->pCode);
    }
    taosArrayDestroy(retrieveRsp.pFuncInfos);
  }

  {
    SRetrieveFuncReq retrieveReq = {0};
    retrieveReq.numOfFuncs = 2;
    retrieveReq.pFuncNames = taosArrayInit(1, TSDB_FUNC_NAME_LEN);
    taosArrayPush(retrieveReq.pFuncNames, "f2");
    taosArrayPush(retrieveReq.pFuncNames, "f3");

    int32_t contLen = tSerializeSRetrieveFuncReq(NULL, 0, &retrieveReq);
    void*   pReq = rpcMallocCont(contLen);
    tSerializeSRetrieveFuncReq(pReq, contLen, &retrieveReq);
    taosArrayDestroy(retrieveReq.pFuncNames);

    SRpcMsg* pRsp = test.SendReq(TDMT_MND_RETRIEVE_FUNC, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_INVALID_FUNC);
  }
}

TEST_F(MndTestFunc, 04_Drop_Func) {
  {
    SDropFuncReq dropReq = {0};
    strcpy(dropReq.name, "");

    int32_t contLen = tSerializeSDropFuncReq(NULL, 0, &dropReq);
    void*   pReq = rpcMallocCont(contLen);
    tSerializeSDropFuncReq(pReq, contLen, &dropReq);

    SRpcMsg* pRsp = test.SendReq(TDMT_MND_DROP_FUNC, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_INVALID_FUNC_NAME);
  }

  {
    SDropFuncReq dropReq = {0};
    strcpy(dropReq.name, "f3");

    int32_t contLen = tSerializeSDropFuncReq(NULL, 0, &dropReq);
    void*   pReq = rpcMallocCont(contLen);
    tSerializeSDropFuncReq(pReq, contLen, &dropReq);

    SRpcMsg* pRsp = test.SendReq(TDMT_MND_DROP_FUNC, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_FUNC_NOT_EXIST);
  }

  {
    SDropFuncReq dropReq = {0};
    strcpy(dropReq.name, "f3");
    dropReq.igNotExists = 1;

    int32_t contLen = tSerializeSDropFuncReq(NULL, 0, &dropReq);
    void*   pReq = rpcMallocCont(contLen);
    tSerializeSDropFuncReq(pReq, contLen, &dropReq);

    SRpcMsg* pRsp = test.SendReq(TDMT_MND_DROP_FUNC, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, 0);
  }

  {
    SDropFuncReq dropReq = {0};
    strcpy(dropReq.name, "f1");
    dropReq.igNotExists = 1;

    int32_t contLen = tSerializeSDropFuncReq(NULL, 0, &dropReq);
    void*   pReq = rpcMallocCont(contLen);
    tSerializeSDropFuncReq(pReq, contLen, &dropReq);

    SRpcMsg* pRsp = test.SendReq(TDMT_MND_DROP_FUNC, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, 0);
  }

  test.SendShowMetaReq(TSDB_MGMT_TABLE_FUNC, "");
  CHECK_META("show functions", 7);

  test.SendShowRetrieveReq();
  EXPECT_EQ(test.GetShowRows(), 1);

  // restart
  test.Restart();

  test.SendShowMetaReq(TSDB_MGMT_TABLE_FUNC, "");
  CHECK_META("show functions", 7);

  test.SendShowRetrieveReq();
  EXPECT_EQ(test.GetShowRows(), 1);

  CheckBinary("f2", TSDB_FUNC_NAME_LEN);
}
