#include <gtest/gtest.h>
#include <cassert>
#include <iostream>

#include "taos.h"
#include "tpagedbuf.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"

namespace {
// simple test
void simpleTest() {
  SDiskbasedBuf* pResultBuf = NULL;
  int32_t ret = createDiskbasedBuffer(&pResultBuf, 1024, 4096, 1, "/tmp/");
  
  int32_t pageId = 0;
  int32_t groupId = 0;
  
  SFilePage* pBufPage = getNewDataBuf(pResultBuf, groupId, &pageId);
  ASSERT_TRUE(pBufPage != NULL);
  
  ASSERT_EQ(getTotalBufSize(pResultBuf), 1024);
  
  SIDList list = getDataBufPagesIdList(pResultBuf, groupId);
  ASSERT_EQ(taosArrayGetSize(list), 1);
  ASSERT_EQ(getNumOfResultBufGroupId(pResultBuf), 1);

  releaseBufPage(pResultBuf, pBufPage);

  SFilePage* pBufPage1 = getNewDataBuf(pResultBuf, groupId, &pageId);

  SFilePage* t = getBufPage(pResultBuf, pageId);
  ASSERT_TRUE(t == pBufPage1);

  SFilePage* pBufPage2 = getNewDataBuf(pResultBuf, groupId, &pageId);
  SFilePage* t1 = getBufPage(pResultBuf, pageId);
  ASSERT_TRUE(t1 == pBufPage2);

  SFilePage* pBufPage3 = getNewDataBuf(pResultBuf, groupId, &pageId);
  SFilePage* t2 = getBufPage(pResultBuf, pageId);
  ASSERT_TRUE(t2 == pBufPage3);

  SFilePage* pBufPage4 = getNewDataBuf(pResultBuf, groupId, &pageId);
  SFilePage* t3 = getBufPage(pResultBuf, pageId);
  ASSERT_TRUE(t3 == pBufPage4);

  SFilePage* pBufPage5 = getNewDataBuf(pResultBuf, groupId, &pageId);
  SFilePage* t4 = getBufPage(pResultBuf, pageId);
  ASSERT_TRUE(t4 == pBufPage5);

  destroyResultBuf(pResultBuf);
}

void writeDownTest() {
  SDiskbasedBuf* pResultBuf = NULL;
  int32_t ret = createDiskbasedBuffer(&pResultBuf, 1024, 4*1024, 1, "/tmp/");

  int32_t pageId = 0;
  int32_t writePageId = 0;
  int32_t groupId = 0;
  int32_t nx = 12345;

  SFilePage* pBufPage = getNewDataBuf(pResultBuf, groupId, &pageId);
  ASSERT_TRUE(pBufPage != NULL);

  *(int32_t*)(pBufPage->data) = nx;
  writePageId = pageId;
  releaseBufPage(pResultBuf, pBufPage);

  SFilePage* pBufPage1 = getNewDataBuf(pResultBuf, groupId, &pageId);
  SFilePage* t1 = getBufPage(pResultBuf, pageId);
  ASSERT_TRUE(t1 == pBufPage1);
  ASSERT_TRUE(pageId == 1);

  SFilePage* pBufPage2 = getNewDataBuf(pResultBuf, groupId, &pageId);
  SFilePage* t2 = getBufPage(pResultBuf, pageId);
  ASSERT_TRUE(t2 == pBufPage2);
  ASSERT_TRUE(pageId == 2);

  SFilePage* pBufPage3 = getNewDataBuf(pResultBuf, groupId, &pageId);
  SFilePage* t3 = getBufPage(pResultBuf, pageId);
  ASSERT_TRUE(t3 == pBufPage3);
  ASSERT_TRUE(pageId == 3);

  SFilePage* pBufPage4 = getNewDataBuf(pResultBuf, groupId, &pageId);
  SFilePage* t4 = getBufPage(pResultBuf, pageId);
  ASSERT_TRUE(t4 == pBufPage4);
  ASSERT_TRUE(pageId == 4);
  releaseBufPage(pResultBuf, t4);

  // flush the written page to disk, and read it out again
  SFilePage* pBufPagex = getBufPage(pResultBuf, writePageId);
  ASSERT_EQ(*(int32_t*)pBufPagex->data, nx);

  SArray* pa = getDataBufPagesIdList(pResultBuf, groupId);
  ASSERT_EQ(taosArrayGetSize(pa), 5);

  destroyResultBuf(pResultBuf);
}

void recyclePageTest() {
  SDiskbasedBuf* pResultBuf = NULL;
  int32_t ret = createDiskbasedBuffer(&pResultBuf, 1024, 4*1024, 1, "/tmp/");

  int32_t pageId = 0;
  int32_t writePageId = 0;
  int32_t groupId = 0;
  int32_t nx = 12345;

  SFilePage* pBufPage = getNewDataBuf(pResultBuf, groupId, &pageId);
  ASSERT_TRUE(pBufPage != NULL);
  releaseBufPage(pResultBuf, pBufPage);

  SFilePage* pBufPage1 = getNewDataBuf(pResultBuf, groupId, &pageId);
  SFilePage* t1 = getBufPage(pResultBuf, pageId);
  ASSERT_TRUE(t1 == pBufPage1);
  ASSERT_TRUE(pageId == 1);

  SFilePage* pBufPage2 = getNewDataBuf(pResultBuf, groupId, &pageId);
  SFilePage* t2 = getBufPage(pResultBuf, pageId);
  ASSERT_TRUE(t2 == pBufPage2);
  ASSERT_TRUE(pageId == 2);

  SFilePage* pBufPage3 = getNewDataBuf(pResultBuf, groupId, &pageId);
  SFilePage* t3 = getBufPage(pResultBuf, pageId);
  ASSERT_TRUE(t3 == pBufPage3);
  ASSERT_TRUE(pageId == 3);

  SFilePage* pBufPage4 = getNewDataBuf(pResultBuf, groupId, &pageId);
  SFilePage* t4 = getBufPage(pResultBuf, pageId);
  ASSERT_TRUE(t4 == pBufPage4);
  ASSERT_TRUE(pageId == 4);
  releaseBufPage(pResultBuf, t4);

  SFilePage* pBufPage5 = getNewDataBuf(pResultBuf, groupId, &pageId);
  SFilePage* t5 = getBufPage(pResultBuf, pageId);
  ASSERT_TRUE(t5 == pBufPage5);
  ASSERT_TRUE(pageId == 5);

  // flush the written page to disk, and read it out again
  SFilePage* pBufPagex = getBufPage(pResultBuf, writePageId);
  *(int32_t*)(pBufPagex->data) = nx;
  writePageId = pageId;   // update the data
  releaseBufPage(pResultBuf, pBufPagex);

  SFilePage* pBufPagex1 = getBufPage(pResultBuf, 1);

  SArray* pa = getDataBufPagesIdList(pResultBuf, groupId);
  ASSERT_EQ(taosArrayGetSize(pa), 6);

  destroyResultBuf(pResultBuf);
}
} // namespace


TEST(testCase, resultBufferTest) {
  srand(time(NULL));
  simpleTest();
  writeDownTest();
  recyclePageTest();
}

#pragma GCC diagnostic pop