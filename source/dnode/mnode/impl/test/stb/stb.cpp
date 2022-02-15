/**
 * @file stb.cpp
 * @author slguan (slguan@taosdata.com)
 * @brief MNODE module stb tests
 * @version 1.0
 * @date 2022-01-12
 *
 * @copyright Copyright (c) 2022
 *
 */

#include "sut.h"

class MndTestStb : public ::testing::Test {
 protected:
  static void SetUpTestSuite() { test.Init("/tmp/mnode_test_stb", 9034); }
  static void TearDownTestSuite() { test.Cleanup(); }

  static Testbase test;

 public:
  void SetUp() override {}
  void TearDown() override {}

  void* BuildCreateDbReq(const char* dbname, int32_t* pContLen);
  void* BuildDropDbReq(const char* dbname, int32_t* pContLen);
  void* BuildCreateStbReq(const char* stbname, int32_t* pContLen);
  void* BuildAlterStbAddTagReq(const char* stbname, const char* tagname, int32_t* pContLen);
  void* BuildAlterStbDropTagReq(const char* stbname, const char* tagname, int32_t* pContLen);
  void* BuildAlterStbUpdateTagNameReq(const char* stbname, const char* tagname, const char* newtagname,
                                      int32_t* pContLen);
  void* BuildAlterStbUpdateTagBytesReq(const char* stbname, const char* tagname, int32_t bytes, int32_t* pContLen);
  void* BuildAlterStbAddColumnReq(const char* stbname, const char* colname, int32_t* pContLen);
  void* BuildAlterStbDropColumnReq(const char* stbname, const char* colname, int32_t* pContLen);
  void* BuildAlterStbUpdateColumnBytesReq(const char* stbname, const char* colname, int32_t bytes, int32_t* pContLen);
};

Testbase MndTestStb::test;

void* MndTestStb::BuildCreateDbReq(const char* dbname, int32_t* pContLen) {
  SCreateDbReq createReq = {0};
  strcpy(createReq.db, dbname);
  createReq.numOfVgroups = 2;
  createReq.cacheBlockSize = 16;
  createReq.totalBlocks = 10;
  createReq.daysPerFile = 10;
  createReq.daysToKeep0 = 3650;
  createReq.daysToKeep1 = 3650;
  createReq.daysToKeep2 = 3650;
  createReq.minRows = 100;
  createReq.maxRows = 4096;
  createReq.commitTime = 3600;
  createReq.fsyncPeriod = 3000;
  createReq.walLevel = 1;
  createReq.precision = 0;
  createReq.compression = 2;
  createReq.replications = 1;
  createReq.quorum = 1;
  createReq.update = 0;
  createReq.cacheLastRow = 0;
  createReq.ignoreExist = 1;

  int32_t contLen = tSerializeSCreateDbReq(NULL, 0, &createReq);
  void*   pReq = rpcMallocCont(contLen);
  tSerializeSCreateDbReq(pReq, contLen, &createReq);

  *pContLen = contLen;
  return pReq;
}

void* MndTestStb::BuildDropDbReq(const char* dbname, int32_t* pContLen) {
  SDropDbReq dropdbReq = {0};
  strcpy(dropdbReq.db, dbname);

  int32_t contLen = tSerializeSDropDbReq(NULL, 0, &dropdbReq);
  void*   pReq = rpcMallocCont(contLen);
  tSerializeSDropDbReq(pReq, contLen, &dropdbReq);

  *pContLen = contLen;
  return pReq;
}

void* MndTestStb::BuildCreateStbReq(const char* stbname, int32_t* pContLen) {
  SMCreateStbReq createReq = {0};
  createReq.numOfColumns = 2;
  createReq.numOfTags = 3;
  createReq.igExists = 0;
  createReq.pColumns = taosArrayInit(createReq.numOfColumns, sizeof(SField));
  createReq.pTags = taosArrayInit(createReq.numOfTags, sizeof(SField));
  strcpy(createReq.name, stbname);

  {
    SField field = {0};
    field.bytes = 8;
    field.type = TSDB_DATA_TYPE_TIMESTAMP;
    strcpy(field.name, "ts");
    taosArrayPush(createReq.pColumns, &field);
  }

  {
    SField field = {0};
    field.bytes = 12;
    field.type = TSDB_DATA_TYPE_BINARY;
    strcpy(field.name, "col1");
    taosArrayPush(createReq.pColumns, &field);
  }

  {
    SField field = {0};
    field.bytes = 2;
    field.type = TSDB_DATA_TYPE_TINYINT;
    strcpy(field.name, "tag1");
    taosArrayPush(createReq.pTags, &field);
  }

  {
    SField field = {0};
    field.bytes = 8;
    field.type = TSDB_DATA_TYPE_BIGINT;
    strcpy(field.name, "tag2");
    taosArrayPush(createReq.pTags, &field);
  }

  {
    SField field = {0};
    field.bytes = 16;
    field.type = TSDB_DATA_TYPE_BINARY;
    strcpy(field.name, "tag3");
    taosArrayPush(createReq.pTags, &field);
  }

  int32_t tlen = tSerializeSMCreateStbReq(NULL, &createReq);
  void*   pHead = rpcMallocCont(tlen);

  void* pBuf = pHead;
  tSerializeSMCreateStbReq(&pBuf, &createReq);
  *pContLen = tlen;
  return pHead;
}

void* MndTestStb::BuildAlterStbAddTagReq(const char* stbname, const char* tagname, int32_t* pContLen) {
  SMAltertbReq req = {0};
  strcpy(req.name, stbname);
  req.numOfFields = 1;
  req.pFields = taosArrayInit(1, sizeof(SField));
  req.alterType = TSDB_ALTER_TABLE_ADD_TAG;

  SField field = {0};
  field.bytes = 12;
  field.type = TSDB_DATA_TYPE_BINARY;
  strcpy(field.name, tagname);
  taosArrayPush(req.pFields, &field);

  int32_t contLen = tSerializeSMAlterStbReq(NULL, &req);
  void*   pHead = rpcMallocCont(contLen);
  void*   pBuf = pHead;
  tSerializeSMAlterStbReq(&pBuf, &req);

  *pContLen = contLen;
  return pHead;
}

void* MndTestStb::BuildAlterStbDropTagReq(const char* stbname, const char* tagname, int32_t* pContLen) {
  SMAltertbReq req = {0};
  strcpy(req.name, stbname);
  req.numOfFields = 1;
  req.pFields = taosArrayInit(1, sizeof(SField));
  req.alterType = TSDB_ALTER_TABLE_DROP_TAG;

  SField field = {0};
  field.bytes = 12;
  field.type = TSDB_DATA_TYPE_BINARY;
  strcpy(field.name, tagname);
  taosArrayPush(req.pFields, &field);

  int32_t contLen = tSerializeSMAlterStbReq(NULL, &req);
  void*   pHead = rpcMallocCont(contLen);
  void*   pBuf = pHead;
  tSerializeSMAlterStbReq(&pBuf, &req);

  *pContLen = contLen;
  return pHead;
}

void* MndTestStb::BuildAlterStbUpdateTagNameReq(const char* stbname, const char* tagname, const char* newtagname,
                                                int32_t* pContLen) {
  SMAltertbReq req = {0};
  strcpy(req.name, stbname);
  req.numOfFields = 2;
  req.pFields = taosArrayInit(2, sizeof(SField));
  req.alterType = TSDB_ALTER_TABLE_UPDATE_TAG_NAME;

  SField field = {0};
  field.bytes = 12;
  field.type = TSDB_DATA_TYPE_BINARY;
  strcpy(field.name, tagname);
  taosArrayPush(req.pFields, &field);

  SField field2 = {0};
  field2.bytes = 12;
  field2.type = TSDB_DATA_TYPE_BINARY;
  strcpy(field2.name, newtagname);
  taosArrayPush(req.pFields, &field2);

  int32_t contLen = tSerializeSMAlterStbReq(NULL, &req);
  void*   pHead = rpcMallocCont(contLen);
  void*   pBuf = pHead;
  tSerializeSMAlterStbReq(&pBuf, &req);

  *pContLen = contLen;
  return pHead;
}

void* MndTestStb::BuildAlterStbUpdateTagBytesReq(const char* stbname, const char* tagname, int32_t bytes,
                                                 int32_t* pContLen) {
  SMAltertbReq req = {0};
  strcpy(req.name, stbname);
  req.numOfFields = 1;
  req.pFields = taosArrayInit(1, sizeof(SField));
  req.alterType = TSDB_ALTER_TABLE_UPDATE_TAG_BYTES;

  SField field = {0};
  field.bytes = bytes;
  field.type = TSDB_DATA_TYPE_BINARY;
  strcpy(field.name, tagname);
  taosArrayPush(req.pFields, &field);

  int32_t contLen = tSerializeSMAlterStbReq(NULL, &req);
  void*   pHead = rpcMallocCont(contLen);
  void*   pBuf = pHead;
  tSerializeSMAlterStbReq(&pBuf, &req);

  *pContLen = contLen;
  return pHead;
}

void* MndTestStb::BuildAlterStbAddColumnReq(const char* stbname, const char* colname, int32_t* pContLen) {
  SMAltertbReq req = {0};
  strcpy(req.name, stbname);
  req.numOfFields = 1;
  req.pFields = taosArrayInit(1, sizeof(SField));
  req.alterType = TSDB_ALTER_TABLE_ADD_COLUMN;

  SField field = {0};
  field.bytes = 12;
  field.type = TSDB_DATA_TYPE_BINARY;
  strcpy(field.name, colname);
  taosArrayPush(req.pFields, &field);

  int32_t contLen = tSerializeSMAlterStbReq(NULL, &req);
  void*   pHead = rpcMallocCont(contLen);
  void*   pBuf = pHead;
  tSerializeSMAlterStbReq(&pBuf, &req);

  *pContLen = contLen;
  return pHead;
}

void* MndTestStb::BuildAlterStbDropColumnReq(const char* stbname, const char* colname, int32_t* pContLen) {
  SMAltertbReq req = {0};
  strcpy(req.name, stbname);
  req.numOfFields = 1;
  req.pFields = taosArrayInit(1, sizeof(SField));
  req.alterType = TSDB_ALTER_TABLE_DROP_COLUMN;

  SField field = {0};
  field.bytes = 12;
  field.type = TSDB_DATA_TYPE_BINARY;
  strcpy(field.name, colname);
  taosArrayPush(req.pFields, &field);

  int32_t contLen = tSerializeSMAlterStbReq(NULL, &req);
  void*   pHead = rpcMallocCont(contLen);
  void*   pBuf = pHead;
  tSerializeSMAlterStbReq(&pBuf, &req);

  *pContLen = contLen;
  return pHead;
}

void* MndTestStb::BuildAlterStbUpdateColumnBytesReq(const char* stbname, const char* colname, int32_t bytes,
                                                    int32_t* pContLen) {
  SMAltertbReq req = {0};
  strcpy(req.name, stbname);
  req.numOfFields = 1;
  req.pFields = taosArrayInit(1, sizeof(SField));
  req.alterType = TSDB_ALTER_TABLE_UPDATE_COLUMN_BYTES;

  SField field = {0};
  field.bytes = bytes;
  field.type = TSDB_DATA_TYPE_BINARY;
  strcpy(field.name, colname);
  taosArrayPush(req.pFields, &field);

  int32_t contLen = tSerializeSMAlterStbReq(NULL, &req);
  void*   pHead = rpcMallocCont(contLen);
  void*   pBuf = pHead;
  tSerializeSMAlterStbReq(&pBuf, &req);

  *pContLen = contLen;
  return pHead;
}

TEST_F(MndTestStb, 01_Create_Show_Meta_Drop_Restart_Stb) {
  const char* dbname = "1.d1";
  const char* stbname = "1.d1.stb";

  {
    int32_t  contLen = 0;
    void*    pReq = BuildCreateDbReq(dbname, &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_CREATE_DB, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, 0);
  }

  {
    int32_t  contLen = 0;
    void*    pReq = BuildCreateStbReq(stbname, &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_CREATE_STB, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, 0);
  }

  {
    test.SendShowMetaReq(TSDB_MGMT_TABLE_STB, dbname);
    CHECK_META("show stables", 4);
    CHECK_SCHEMA(0, TSDB_DATA_TYPE_BINARY, TSDB_TABLE_NAME_LEN + VARSTR_HEADER_SIZE, "name");
    CHECK_SCHEMA(1, TSDB_DATA_TYPE_TIMESTAMP, 8, "create_time");
    CHECK_SCHEMA(2, TSDB_DATA_TYPE_INT, 4, "columns");
    CHECK_SCHEMA(3, TSDB_DATA_TYPE_INT, 4, "tags");

    test.SendShowRetrieveReq();
    EXPECT_EQ(test.GetShowRows(), 1);
    CheckBinary("stb", TSDB_TABLE_NAME_LEN);
    CheckTimestamp();
    CheckInt32(2);
    CheckInt32(3);
  }

  // ----- meta ------
  {
    int32_t        contLen = sizeof(STableInfoReq);
    STableInfoReq* pReq = (STableInfoReq*)rpcMallocCont(contLen);
    strcpy(pReq->dbFName, dbname);
    strcpy(pReq->tbName, "stb");

    SRpcMsg* pMsg = test.SendReq(TDMT_MND_STB_META, pReq, contLen);
    ASSERT_NE(pMsg, nullptr);
    ASSERT_EQ(pMsg->code, 0);

    STableMetaRsp* pRsp = (STableMetaRsp*)pMsg->pCont;
    pRsp->numOfTags = htonl(pRsp->numOfTags);
    pRsp->numOfColumns = htonl(pRsp->numOfColumns);
    pRsp->sversion = htonl(pRsp->sversion);
    pRsp->tversion = htonl(pRsp->tversion);
    pRsp->suid = be64toh(pRsp->suid);
    pRsp->tuid = be64toh(pRsp->tuid);
    pRsp->vgId = be64toh(pRsp->vgId);
    for (int32_t i = 0; i < pRsp->numOfTags + pRsp->numOfColumns; ++i) {
      SSchema* pSchema = &pRsp->pSchema[i];
      pSchema->colId = htonl(pSchema->colId);
      pSchema->bytes = htonl(pSchema->bytes);
    }

    EXPECT_STREQ(pRsp->dbFName, dbname);
    EXPECT_STREQ(pRsp->tbName, "stb");
    EXPECT_STREQ(pRsp->stbName, "stb");
    EXPECT_EQ(pRsp->numOfColumns, 2);
    EXPECT_EQ(pRsp->numOfTags, 3);
    EXPECT_EQ(pRsp->precision, TSDB_TIME_PRECISION_MILLI);
    EXPECT_EQ(pRsp->tableType, TSDB_SUPER_TABLE);
    EXPECT_EQ(pRsp->update, 0);
    EXPECT_EQ(pRsp->sversion, 1);
    EXPECT_EQ(pRsp->tversion, 0);
    EXPECT_GT(pRsp->suid, 0);
    EXPECT_GT(pRsp->tuid, 0);
    EXPECT_EQ(pRsp->vgId, 0);

    {
      SSchema* pSchema = &pRsp->pSchema[0];
      EXPECT_EQ(pSchema->type, TSDB_DATA_TYPE_TIMESTAMP);
      EXPECT_EQ(pSchema->colId, 1);
      EXPECT_EQ(pSchema->bytes, 8);
      EXPECT_STREQ(pSchema->name, "ts");
    }

    {
      SSchema* pSchema = &pRsp->pSchema[1];
      EXPECT_EQ(pSchema->type, TSDB_DATA_TYPE_BINARY);
      EXPECT_EQ(pSchema->colId, 2);
      EXPECT_EQ(pSchema->bytes, 12);
      EXPECT_STREQ(pSchema->name, "col1");
    }

    {
      SSchema* pSchema = &pRsp->pSchema[2];
      EXPECT_EQ(pSchema->type, TSDB_DATA_TYPE_TINYINT);
      EXPECT_EQ(pSchema->colId, 3);
      EXPECT_EQ(pSchema->bytes, 2);
      EXPECT_STREQ(pSchema->name, "tag1");
    }

    {
      SSchema* pSchema = &pRsp->pSchema[3];
      EXPECT_EQ(pSchema->type, TSDB_DATA_TYPE_BIGINT);
      EXPECT_EQ(pSchema->colId, 4);
      EXPECT_EQ(pSchema->bytes, 8);
      EXPECT_STREQ(pSchema->name, "tag2");
    }

    {
      SSchema* pSchema = &pRsp->pSchema[4];
      EXPECT_EQ(pSchema->type, TSDB_DATA_TYPE_BINARY);
      EXPECT_EQ(pSchema->colId, 5);
      EXPECT_EQ(pSchema->bytes, 16);
      EXPECT_STREQ(pSchema->name, "tag3");
    }
  }

  // restart
  test.Restart();

  {
    test.SendShowMetaReq(TSDB_MGMT_TABLE_STB, dbname);
    CHECK_META("show stables", 4);
    test.SendShowRetrieveReq();
    EXPECT_EQ(test.GetShowRows(), 1);

    CheckBinary("stb", TSDB_TABLE_NAME_LEN);
    CheckTimestamp();
    CheckInt32(2);
    CheckInt32(3);
  }

  {
    SMDropStbReq dropReq = {0};
    strcpy(dropReq.name, stbname);

    int32_t contLen = tSerializeSMDropStbReq(NULL, &dropReq);
    void*   pHead = rpcMallocCont(contLen);
    void*   pBuf = pHead;
    tSerializeSMDropStbReq(&pBuf, &dropReq);

    SRpcMsg* pRsp = test.SendReq(TDMT_MND_DROP_STB, pHead, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, 0);
  }

  {
    test.SendShowMetaReq(TSDB_MGMT_TABLE_STB, dbname);
    CHECK_META("show stables", 4);
    test.SendShowRetrieveReq();
    EXPECT_EQ(test.GetShowRows(), 0);
  }

  {
    int32_t  contLen = 0;
    void*    pReq = BuildDropDbReq(dbname, &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_DROP_DB, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, 0);
  }
}

TEST_F(MndTestStb, 02_Alter_Stb_AddTag) {
  const char* dbname = "1.d2";
  const char* stbname = "1.d2.stb";
  int32_t     contLen = 0;

  {
    void*    pReq = BuildCreateDbReq(dbname, &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_CREATE_DB, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, 0);
  }

  {
    void*    pReq = BuildCreateStbReq(stbname, &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_CREATE_STB, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, 0);
  }

  {
    void*    pReq = BuildAlterStbAddTagReq("1.d3.stb", "tag4", &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_ALTER_STB, pReq, contLen);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_INVALID_DB);
  }

  {
    void*    pReq = BuildAlterStbAddTagReq("1.d2.stb3", "tag4", &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_ALTER_STB, pReq, contLen);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_STB_NOT_EXIST);
  }

  {
    void*    pReq = BuildAlterStbAddTagReq(stbname, "tag3", &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_ALTER_STB, pReq, contLen);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_TAG_ALREADY_EXIST);
  }

  {
    void*    pReq = BuildAlterStbAddTagReq(stbname, "col1", &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_ALTER_STB, pReq, contLen);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_COLUMN_ALREADY_EXIST);
  }

  {
    void*    pReq = BuildAlterStbAddTagReq(stbname, "tag4", &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_ALTER_STB, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, 0);

    test.SendShowMetaReq(TSDB_MGMT_TABLE_STB, dbname);
    test.SendShowRetrieveReq();
    EXPECT_EQ(test.GetShowRows(), 1);
    CheckBinary("stb", TSDB_TABLE_NAME_LEN);
    CheckTimestamp();
    CheckInt32(2);
    CheckInt32(4);
  }

  {
    void*    pReq = BuildDropDbReq(dbname, &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_DROP_DB, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, 0);
  }
}

TEST_F(MndTestStb, 03_Alter_Stb_DropTag) {
  const char* dbname = "1.d3";
  const char* stbname = "1.d3.stb";
  int32_t     contLen = 0;

  {
    void*    pReq = BuildCreateDbReq(dbname, &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_CREATE_DB, pReq, contLen);
    ASSERT_EQ(pRsp->code, 0);
  }

  {
    void*    pReq = BuildCreateStbReq(stbname, &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_CREATE_STB, pReq, contLen);
    ASSERT_EQ(pRsp->code, 0);
  }

  {
    void*    pReq = BuildAlterStbDropTagReq(stbname, "tag5", &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_ALTER_STB, pReq, contLen);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_TAG_NOT_EXIST);
  }

  {
    void*    pReq = BuildAlterStbDropTagReq(stbname, "tag3", &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_ALTER_STB, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, 0);

    test.SendShowMetaReq(TSDB_MGMT_TABLE_STB, dbname);
    test.SendShowRetrieveReq();
    EXPECT_EQ(test.GetShowRows(), 1);
    CheckBinary("stb", TSDB_TABLE_NAME_LEN);
    CheckTimestamp();
    CheckInt32(2);
    CheckInt32(2);
  }

  {
    void*    pReq = BuildDropDbReq(dbname, &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_DROP_DB, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, 0);
  }
}

TEST_F(MndTestStb, 04_Alter_Stb_AlterTagName) {
  const char* dbname = "1.d4";
  const char* stbname = "1.d4.stb";
  int32_t     contLen = 0;

  {
    void*    pReq = BuildCreateDbReq(dbname, &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_CREATE_DB, pReq, contLen);
    ASSERT_EQ(pRsp->code, 0);
  }

  {
    void*    pReq = BuildCreateStbReq(stbname, &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_CREATE_STB, pReq, contLen);
    ASSERT_EQ(pRsp->code, 0);
  }

  {
    void*    pReq = BuildAlterStbUpdateTagNameReq(stbname, "tag5", "tag6", &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_ALTER_STB, pReq, contLen);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_TAG_NOT_EXIST);
  }

  {
    void*    pReq = BuildAlterStbUpdateTagNameReq(stbname, "col1", "tag6", &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_ALTER_STB, pReq, contLen);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_TAG_NOT_EXIST);
  }

  {
    void*    pReq = BuildAlterStbUpdateTagNameReq(stbname, "tag3", "col1", &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_ALTER_STB, pReq, contLen);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_COLUMN_ALREADY_EXIST);
  }

  {
    void*    pReq = BuildAlterStbUpdateTagNameReq(stbname, "tag3", "tag2", &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_ALTER_STB, pReq, contLen);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_TAG_ALREADY_EXIST);
  }
  {
    void*    pReq = BuildAlterStbUpdateTagNameReq(stbname, "tag3", "tag2", &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_ALTER_STB, pReq, contLen);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_TAG_ALREADY_EXIST);
  }

  {
    void*    pReq = BuildAlterStbUpdateTagNameReq(stbname, "tag3", "tag4", &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_ALTER_STB, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, 0);

    test.SendShowMetaReq(TSDB_MGMT_TABLE_STB, dbname);
    test.SendShowRetrieveReq();
    EXPECT_EQ(test.GetShowRows(), 1);
    CheckBinary("stb", TSDB_TABLE_NAME_LEN);
    CheckTimestamp();
    CheckInt32(2);
    CheckInt32(3);
  }

  {
    void*    pReq = BuildDropDbReq(dbname, &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_DROP_DB, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, 0);
  }
}

TEST_F(MndTestStb, 05_Alter_Stb_AlterTagBytes) {
  const char* dbname = "1.d5";
  const char* stbname = "1.d5.stb";
  int32_t     contLen = 0;

  {
    void*    pReq = BuildCreateDbReq(dbname, &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_CREATE_DB, pReq, contLen);
    ASSERT_EQ(pRsp->code, 0);
  }

  {
    void*    pReq = BuildCreateStbReq(stbname, &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_CREATE_STB, pReq, contLen);
    ASSERT_EQ(pRsp->code, 0);
  }

  {
    void*    pReq = BuildAlterStbUpdateTagBytesReq(stbname, "tag5", 12, &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_ALTER_STB, pReq, contLen);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_TAG_NOT_EXIST);
  }

  {
    void*    pReq = BuildAlterStbUpdateTagBytesReq(stbname, "tag1", 13, &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_ALTER_STB, pReq, contLen);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_INVALID_STB_OPTION);
  }

  {
    void*    pReq = BuildAlterStbUpdateTagBytesReq(stbname, "tag3", 8, &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_ALTER_STB, pReq, contLen);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_INVALID_ROW_BYTES);
  }

  {
    void*    pReq = BuildAlterStbUpdateTagBytesReq(stbname, "tag3", 20, &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_ALTER_STB, pReq, contLen);
    ASSERT_EQ(pRsp->code, 0);

    test.SendShowMetaReq(TSDB_MGMT_TABLE_STB, dbname);
    test.SendShowRetrieveReq();
    EXPECT_EQ(test.GetShowRows(), 1);
    CheckBinary("stb", TSDB_TABLE_NAME_LEN);
    CheckTimestamp();
    CheckInt32(2);
    CheckInt32(3);
  }

  {
    void*    pReq = BuildDropDbReq(dbname, &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_DROP_DB, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, 0);
  }
}

TEST_F(MndTestStb, 06_Alter_Stb_AddColumn) {
  const char* dbname = "1.d6";
  const char* stbname = "1.d6.stb";
  int32_t     contLen = 0;

  {
    void*    pReq = BuildCreateDbReq(dbname, &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_CREATE_DB, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, 0);
  }

  {
    void*    pReq = BuildCreateStbReq(stbname, &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_CREATE_STB, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, 0);
  }

  {
    void*    pReq = BuildAlterStbAddColumnReq("1.d7.stb", "tag4", &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_ALTER_STB, pReq, contLen);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_INVALID_DB);
  }

  {
    void*    pReq = BuildAlterStbAddColumnReq("1.d6.stb3", "tag4", &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_ALTER_STB, pReq, contLen);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_STB_NOT_EXIST);
  }

  {
    void*    pReq = BuildAlterStbAddColumnReq(stbname, "tag3", &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_ALTER_STB, pReq, contLen);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_TAG_ALREADY_EXIST);
  }

  {
    void*    pReq = BuildAlterStbAddColumnReq(stbname, "col1", &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_ALTER_STB, pReq, contLen);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_COLUMN_ALREADY_EXIST);
  }

  {
    void*    pReq = BuildAlterStbAddColumnReq(stbname, "col2", &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_ALTER_STB, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, 0);

    test.SendShowMetaReq(TSDB_MGMT_TABLE_STB, dbname);
    test.SendShowRetrieveReq();
    EXPECT_EQ(test.GetShowRows(), 1);
    CheckBinary("stb", TSDB_TABLE_NAME_LEN);
    CheckTimestamp();
    CheckInt32(3);
    CheckInt32(3);
  }

  {
    void*    pReq = BuildDropDbReq(dbname, &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_DROP_DB, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, 0);
  }
}

TEST_F(MndTestStb, 07_Alter_Stb_DropColumn) {
  const char* dbname = "1.d7";
  const char* stbname = "1.d7.stb";
  int32_t     contLen = 0;

  {
    void*    pReq = BuildCreateDbReq(dbname, &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_CREATE_DB, pReq, contLen);
    ASSERT_EQ(pRsp->code, 0);
  }

  {
    void*    pReq = BuildCreateStbReq(stbname, &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_CREATE_STB, pReq, contLen);
    ASSERT_EQ(pRsp->code, 0);
  }

  {
    void*    pReq = BuildAlterStbDropColumnReq(stbname, "col4", &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_ALTER_STB, pReq, contLen);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_COLUMN_NOT_EXIST);
  }

  {
    void*    pReq = BuildAlterStbDropColumnReq(stbname, "col1", &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_ALTER_STB, pReq, contLen);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_INVALID_STB_ALTER_OPTION);
  }

  {
    void*    pReq = BuildAlterStbDropColumnReq(stbname, "ts", &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_ALTER_STB, pReq, contLen);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_INVALID_STB_ALTER_OPTION);
  }

  {
    void*    pReq = BuildAlterStbAddColumnReq(stbname, "col2", &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_ALTER_STB, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, 0);
  }

  {
    void*    pReq = BuildAlterStbDropColumnReq(stbname, "col1", &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_ALTER_STB, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, 0);

    test.SendShowMetaReq(TSDB_MGMT_TABLE_STB, dbname);
    test.SendShowRetrieveReq();
    EXPECT_EQ(test.GetShowRows(), 1);
    CheckBinary("stb", TSDB_TABLE_NAME_LEN);
    CheckTimestamp();
    CheckInt32(2);
    CheckInt32(3);
  }

  {
    void*    pReq = BuildDropDbReq(dbname, &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_DROP_DB, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, 0);
  }
}

TEST_F(MndTestStb, 08_Alter_Stb_AlterTagBytes) {
  const char* dbname = "1.d8";
  const char* stbname = "1.d8.stb";
  int32_t     contLen = 0;

  {
    void*    pReq = BuildCreateDbReq(dbname, &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_CREATE_DB, pReq, contLen);
    ASSERT_EQ(pRsp->code, 0);
  }

  {
    void*    pReq = BuildCreateStbReq(stbname, &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_CREATE_STB, pReq, contLen);
    ASSERT_EQ(pRsp->code, 0);
  }

  {
    void*    pReq = BuildAlterStbUpdateColumnBytesReq(stbname, "col5", 12, &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_ALTER_STB, pReq, contLen);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_COLUMN_NOT_EXIST);
  }

  {
    void*    pReq = BuildAlterStbUpdateColumnBytesReq(stbname, "ts", 8, &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_ALTER_STB, pReq, contLen);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_INVALID_STB_OPTION);
  }

  {
    void*    pReq = BuildAlterStbUpdateColumnBytesReq(stbname, "col1", 8, &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_ALTER_STB, pReq, contLen);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_INVALID_ROW_BYTES);
  }

  {
    void*    pReq = BuildAlterStbUpdateColumnBytesReq(stbname, "col1", TSDB_MAX_BYTES_PER_ROW, &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_ALTER_STB, pReq, contLen);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_INVALID_ROW_BYTES);
  }

  {
    void*    pReq = BuildAlterStbUpdateColumnBytesReq(stbname, "col1", 20, &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_ALTER_STB, pReq, contLen);
    ASSERT_EQ(pRsp->code, 0);

    test.SendShowMetaReq(TSDB_MGMT_TABLE_STB, dbname);
    test.SendShowRetrieveReq();
    EXPECT_EQ(test.GetShowRows(), 1);
    CheckBinary("stb", TSDB_TABLE_NAME_LEN);
    CheckTimestamp();
    CheckInt32(2);
    CheckInt32(3);
  }

  {
    void*    pReq = BuildDropDbReq(dbname, &contLen);
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_DROP_DB, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, 0);
  }
}
