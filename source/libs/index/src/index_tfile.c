/*
 * Copyright (c) 2019 TAOS Data, Inc. <jhtao@taosdata.com>
p *
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

#include "index_tfile.h"
#include "index.h"
#include "index_comm.h"
#include "index_fst.h"
#include "index_fst_counting_writer.h"
#include "index_util.h"
#include "taosdef.h"
#include "tcoding.h"
#include "tcompare.h"

const static uint64_t tfileMagicNumber = 0xdb4775248b80fb57ull;

typedef struct TFileFstIter {
  FstStreamBuilder* fb;
  StreamWithState*  st;
  AutomationCtx*    ctx;
  TFileReader*      rdr;
} TFileFstIter;

#define TF_TABLE_TATOAL_SIZE(sz) (sizeof(sz) + sz * sizeof(uint64_t))

static int  tfileUidCompare(const void* a, const void* b);
static int  tfileStrCompare(const void* a, const void* b);
static int  tfileValueCompare(const void* a, const void* b, const void* param);
static void tfileSerialTableIdsToBuf(char* buf, SArray* tableIds);

static int tfileWriteHeader(TFileWriter* writer);
static int tfileWriteFstOffset(TFileWriter* tw, int32_t offset);
static int tfileWriteData(TFileWriter* write, TFileValue* tval);
static int tfileWriteFooter(TFileWriter* write);

// handle file corrupt later
static int tfileReaderLoadHeader(TFileReader* reader);
static int tfileReaderLoadFst(TFileReader* reader);
static int tfileReaderVerify(TFileReader* reader);
static int tfileReaderLoadTableIds(TFileReader* reader, int32_t offset, SArray* result);

static SArray* tfileGetFileList(const char* path);
static int     tfileRmExpireFile(SArray* result);
static void    tfileDestroyFileName(void* elem);
static int     tfileCompare(const void* a, const void* b);
static int     tfileParseFileName(const char* filename, uint64_t* suid, char* col, int* version);
static void    tfileGenFileName(char* filename, uint64_t suid, const char* col, int version);
static void    tfileGenFileFullName(char* fullname, const char* path, uint64_t suid, const char* col, int32_t version);

TFileCache* tfileCacheCreate(const char* path) {
  TFileCache* tcache = calloc(1, sizeof(TFileCache));
  if (tcache == NULL) {
    return NULL;
  }

  tcache->tableCache = taosHashInit(8, taosGetDefaultHashFunction(TSDB_DATA_TYPE_BINARY), true, HASH_ENTRY_LOCK);
  tcache->capacity = 64;

  SArray* files = tfileGetFileList(path);
  for (size_t i = 0; i < taosArrayGetSize(files); i++) {
    char* file = taosArrayGetP(files, i);

    WriterCtx* wc = writerCtxCreate(TFile, file, true, 1024 * 1024 * 64);
    if (wc == NULL) {
      indexError("failed to open index:%s", file);
      goto End;
    }

    TFileReader* reader = tfileReaderCreate(wc);
    if (reader == NULL) {
      indexInfo("skip invalid file: %s", file);
      continue;
    }
    TFileHeader* header = &reader->header;
    ICacheKey    key = {.suid = header->suid, .colName = header->colName, .nColName = strlen(header->colName)};

    char    buf[128] = {0};
    int32_t sz = indexSerialCacheKey(&key, buf);
    assert(sz < sizeof(buf));
    taosHashPut(tcache->tableCache, buf, sz, &reader, sizeof(void*));
    tfileReaderRef(reader);
  }
  taosArrayDestroyEx(files, tfileDestroyFileName);
  return tcache;
End:
  tfileCacheDestroy(tcache);
  taosArrayDestroyEx(files, tfileDestroyFileName);
  return NULL;
}
void tfileCacheDestroy(TFileCache* tcache) {
  if (tcache == NULL) {
    return;
  }
  // free table cache
  TFileReader** reader = taosHashIterate(tcache->tableCache, NULL);
  while (reader) {
    TFileReader* p = *reader;
    indexInfo("drop table cache suid: %" PRIu64 ", colName: %s, colType: %d", p->header.suid, p->header.colName,
              p->header.colType);

    tfileReaderUnRef(p);
    reader = taosHashIterate(tcache->tableCache, reader);
  }
  taosHashCleanup(tcache->tableCache);
  free(tcache);
}

TFileReader* tfileCacheGet(TFileCache* tcache, ICacheKey* key) {
  char    buf[128] = {0};
  int32_t sz = indexSerialCacheKey(key, buf);
  assert(sz < sizeof(buf));
  TFileReader** reader = taosHashGet(tcache->tableCache, buf, sz);
  if (reader == NULL) {
    return NULL;
  }
  tfileReaderRef(*reader);

  return *reader;
}
void tfileCachePut(TFileCache* tcache, ICacheKey* key, TFileReader* reader) {
  char    buf[128] = {0};
  int32_t sz = indexSerialCacheKey(key, buf);
  // remove last version index reader
  TFileReader** p = taosHashGet(tcache->tableCache, buf, sz);
  if (p != NULL) {
    TFileReader* oldReader = *p;
    taosHashRemove(tcache->tableCache, buf, sz);
    oldReader->remove = true;
    tfileReaderUnRef(oldReader);
  }

  taosHashPut(tcache->tableCache, buf, sz, &reader, sizeof(void*));
  tfileReaderRef(reader);
  return;
}
TFileReader* tfileReaderCreate(WriterCtx* ctx) {
  TFileReader* reader = calloc(1, sizeof(TFileReader));
  if (reader == NULL) {
    return NULL;
  }

  reader->ctx = ctx;

  if (0 != tfileReaderVerify(reader)) {
    indexError("invalid tfile, suid: %" PRIu64 ", colName: %s", reader->header.suid, reader->header.colName);
    tfileReaderDestroy(reader);
    return NULL;
  }
  // T_REF_INC(reader);
  if (0 != tfileReaderLoadHeader(reader)) {
    indexError("failed to load index header, suid: %" PRIu64 ", colName: %s", reader->header.suid,
               reader->header.colName);
    tfileReaderDestroy(reader);
    return NULL;
  }

  if (0 != tfileReaderLoadFst(reader)) {
    indexError("failed to load index fst, suid: %" PRIu64 ", colName: %s, errno: %d", reader->header.suid,
               reader->header.colName, errno);
    tfileReaderDestroy(reader);
    return NULL;
  }

  return reader;
}
void tfileReaderDestroy(TFileReader* reader) {
  if (reader == NULL) {
    return;
  }
  // T_REF_INC(reader);
  fstDestroy(reader->fst);
  writerCtxDestroy(reader->ctx, reader->remove);
  free(reader);
}

int tfileReaderSearch(TFileReader* reader, SIndexTermQuery* query, SIdxTempResult* tr) {
  SIndexTerm*     term = query->term;
  bool            hasJson = INDEX_TYPE_CONTAIN_EXTERN_TYPE(term->colType, TSDB_DATA_TYPE_JSON);
  EIndexQueryType qtype = query->qType;

  // SArray* result = taosArrayInit(16, sizeof(uint64_t));
  int ret = -1;
  // refactor to callback later
  if (qtype == QUERY_TERM) {
    uint64_t offset;
    char*    p = term->colVal;
    uint64_t sz = term->nColVal;
    if (hasJson) {
      p = indexPackJsonData(term);
      sz = strlen(p);
    }
    int64_t  st = taosGetTimestampUs();
    FstSlice key = fstSliceCreate(p, sz);
    if (fstGet(reader->fst, &key, &offset)) {
      int64_t et = taosGetTimestampUs();
      int64_t cost = et - st;
      indexInfo("index: %" PRIu64 ", col: %s, colVal: %s, found table info in tindex, time cost: %" PRIu64 "us",
                term->suid, term->colName, term->colVal, cost);

      ret = tfileReaderLoadTableIds(reader, offset, tr->total);
      cost = taosGetTimestampUs() - et;
      indexInfo("index: %" PRIu64 ", col: %s, colVal: %s, load all table info, time cost: %" PRIu64 "us", term->suid,
                term->colName, term->colVal, cost);
    } else {
      indexInfo("index: %" PRIu64 ", col: %s, colVal: %s, not found table info in tindex", term->suid, term->colName,
                term->colVal);
    }
    fstSliceDestroy(&key);
    if (hasJson) {
      free(p);
    }
  } else if (qtype == QUERY_PREFIX) {
    // handle later
    //
  } else if (qtype == QUERY_SUFFIX) {
    // handle later
  } else if (qtype == QUERY_REGEX) {
    // handle later
  } else if (qtype == QUERY_RANGE) {
    // handle later
  }
  tfileReaderUnRef(reader);

  // taosArrayAddAll(tr->total, result);
  // taosArrayDestroy(result);

  return ret;
}

TFileWriter* tfileWriterOpen(char* path, uint64_t suid, int32_t version, const char* colName, uint8_t colType) {
  char fullname[256] = {0};
  tfileGenFileFullName(fullname, path, suid, colName, version);
  // indexInfo("open write file name %s", fullname);
  WriterCtx* wcx = writerCtxCreate(TFile, fullname, false, 1024 * 1024 * 64);
  if (wcx == NULL) {
    return NULL;
  }

  TFileHeader tfh = {0};
  tfh.suid = suid;
  tfh.version = version;
  memcpy(tfh.colName, colName, strlen(colName));
  tfh.colType = colType;

  return tfileWriterCreate(wcx, &tfh);
}
TFileReader* tfileReaderOpen(char* path, uint64_t suid, int32_t version, const char* colName) {
  char fullname[256] = {0};
  tfileGenFileFullName(fullname, path, suid, colName, version);

  WriterCtx* wc = writerCtxCreate(TFile, fullname, true, 1024 * 1024 * 1024);
  indexInfo("open read file name:%s, file size: %d", wc->file.buf, wc->file.size);
  if (wc == NULL) {
    return NULL;
  }

  TFileReader* reader = tfileReaderCreate(wc);
  return reader;
}
TFileWriter* tfileWriterCreate(WriterCtx* ctx, TFileHeader* header) {
  TFileWriter* tw = calloc(1, sizeof(TFileWriter));
  if (tw == NULL) {
    indexError("index: %" PRIu64 " failed to alloc TFilerWriter", header->suid);
    return NULL;
  }
  tw->ctx = ctx;
  tw->header = *header;
  tfileWriteHeader(tw);
  return tw;
}

int tfileWriterPut(TFileWriter* tw, void* data, bool order) {
  // sort by coltype and write to tindex
  if (order == false) {
    __compar_fn_t fn;

    int8_t colType = tw->header.colType;
    colType = INDEX_TYPE_GET_TYPE(colType);
    if (colType == TSDB_DATA_TYPE_BINARY || colType == TSDB_DATA_TYPE_NCHAR) {
      fn = tfileStrCompare;
    } else {
      fn = getComparFunc(colType, 0);
    }
    taosArraySortPWithExt((SArray*)(data), tfileValueCompare, &fn);
  }

  int32_t bufLimit = 64 * 4096, offset = 0;
  // char*   buf = calloc(1, sizeof(char) * bufLimit);
  // char*   p = buf;
  int32_t sz = taosArrayGetSize((SArray*)data);
  int32_t fstOffset = tw->offset;

  // ugly code, refactor later
  for (size_t i = 0; i < sz; i++) {
    TFileValue* v = taosArrayGetP((SArray*)data, i);
    taosArraySort(v->tableId, tfileUidCompare);
    taosArrayRemoveDuplicate(v->tableId, tfileUidCompare, NULL);
    int32_t tbsz = taosArrayGetSize(v->tableId);
    fstOffset += TF_TABLE_TATOAL_SIZE(tbsz);
  }
  tfileWriteFstOffset(tw, fstOffset);

  for (size_t i = 0; i < sz; i++) {
    TFileValue* v = taosArrayGetP((SArray*)data, i);

    int32_t tbsz = taosArrayGetSize(v->tableId);
    // check buf has enough space or not
    int32_t ttsz = TF_TABLE_TATOAL_SIZE(tbsz);

    char* buf = calloc(1, ttsz * sizeof(char));
    char* p = buf;
    tfileSerialTableIdsToBuf(p, v->tableId);
    tw->ctx->write(tw->ctx, buf, ttsz);
    v->offset = tw->offset;
    tw->offset += ttsz;
    free(buf);
  }

  tw->fb = fstBuilderCreate(tw->ctx, 0);
  if (tw->fb == NULL) {
    tfileWriterClose(tw);
    return -1;
  }

  // write data
  for (size_t i = 0; i < sz; i++) {
    // TODO, fst batch write later
    TFileValue* v = taosArrayGetP((SArray*)data, i);
    if (tfileWriteData(tw, v) != 0) {
      indexError("failed to write data: %s, offset: %d len: %d", v->colVal, v->offset,
                 (int)taosArrayGetSize(v->tableId));
    } else {
      // indexInfo("success to write data: %s, offset: %d len: %d", v->colVal, v->offset,
      //          (int)taosArrayGetSize(v->tableId));

      // indexInfo("tfile write data size: %d", tw->ctx->size(tw->ctx));
    }
  }
  fstBuilderFinish(tw->fb);
  fstBuilderDestroy(tw->fb);
  tw->fb = NULL;

  tfileWriteFooter(tw);
  return 0;
}
void tfileWriterClose(TFileWriter* tw) {
  if (tw == NULL) {
    return;
  }
  writerCtxDestroy(tw->ctx, false);
  free(tw);
}
void tfileWriterDestroy(TFileWriter* tw) {
  if (tw == NULL) {
    return;
  }
  writerCtxDestroy(tw->ctx, false);
  free(tw);
}

IndexTFile* indexTFileCreate(const char* path) {
  TFileCache* cache = tfileCacheCreate(path);
  if (cache == NULL) {
    return NULL;
  }

  IndexTFile* tfile = calloc(1, sizeof(IndexTFile));
  if (tfile == NULL) {
    tfileCacheDestroy(cache);
    return NULL;
  }

  tfile->cache = cache;
  return tfile;
}
void indexTFileDestroy(IndexTFile* tfile) {
  if (tfile == NULL) {
    return;
  }
  tfileCacheDestroy(tfile->cache);
  free(tfile);
}

int indexTFileSearch(void* tfile, SIndexTermQuery* query, SIdxTempResult* result) {
  int ret = -1;
  if (tfile == NULL) {
    return ret;
  }

  int64_t     st = taosGetTimestampUs();
  IndexTFile* pTfile = tfile;

  SIndexTerm* term = query->term;
  ICacheKey key = {.suid = term->suid, .colType = term->colType, .colName = term->colName, .nColName = term->nColName};
  TFileReader* reader = tfileCacheGet(pTfile->cache, &key);
  if (reader == NULL) {
    return 0;
  }
  int64_t cost = taosGetTimestampUs() - st;
  indexInfo("index tfile stage 1 cost: %" PRId64 "", cost);

  return tfileReaderSearch(reader, query, result);
}
int indexTFilePut(void* tfile, SIndexTerm* term, uint64_t uid) {
  // TFileWriterOpt wOpt = {.suid = term->suid, .colType = term->colType, .colName = term->colName, .nColName =
  // term->nColName, .version = 1};

  return 0;
}
static bool tfileIteratorNext(Iterate* iiter) {
  IterateValue* iv = &iiter->val;
  iterateValueDestroy(iv, false);

  char*    colVal = NULL;
  uint64_t offset = 0;

  TFileFstIter*          tIter = iiter->iter;
  StreamWithStateResult* rt = streamWithStateNextWith(tIter->st, NULL);
  if (rt == NULL) {
    return false;
  }

  int32_t sz = 0;
  char*   ch = (char*)fstSliceData(&rt->data, &sz);
  colVal = calloc(1, sz + 1);
  memcpy(colVal, ch, sz);

  offset = (uint64_t)(rt->out.out);
  swsResultDestroy(rt);
  // set up iterate value
  if (tfileReaderLoadTableIds(tIter->rdr, offset, iv->val) != 0) {
    return false;
  }

  iv->ver = 0;
  iv->type = ADD_VALUE;  // value in tfile always ADD_VALUE
  iv->colVal = colVal;
  return true;
  // std::string key(ch, sz);
}

static IterateValue* tifileIterateGetValue(Iterate* iter) { return &iter->val; }

static TFileFstIter* tfileFstIteratorCreate(TFileReader* reader) {
  TFileFstIter* tIter = calloc(1, sizeof(TFileFstIter));
  if (tIter == NULL) {
    return NULL;
  }

  tIter->ctx = automCtxCreate(NULL, AUTOMATION_ALWAYS);
  tIter->fb = fstSearch(reader->fst, tIter->ctx);
  tIter->st = streamBuilderIntoStream(tIter->fb);
  tIter->rdr = reader;
  return tIter;
}

Iterate* tfileIteratorCreate(TFileReader* reader) {
  if (reader == NULL) {
    return NULL;
  }

  Iterate* iter = calloc(1, sizeof(Iterate));
  iter->iter = tfileFstIteratorCreate(reader);
  if (iter->iter == NULL) {
    free(iter);
    return NULL;
  }
  iter->next = tfileIteratorNext;
  iter->getValue = tifileIterateGetValue;
  iter->val.val = taosArrayInit(1, sizeof(uint64_t));
  iter->val.colVal = NULL;
  return iter;
}
void tfileIteratorDestroy(Iterate* iter) {
  if (iter == NULL) {
    return;
  }

  IterateValue* iv = &iter->val;
  iterateValueDestroy(iv, true);

  TFileFstIter* tIter = iter->iter;
  streamWithStateDestroy(tIter->st);
  fstStreamBuilderDestroy(tIter->fb);
  automCtxDestroy(tIter->ctx);
  free(tIter);

  free(iter);
}

TFileReader* tfileGetReaderByCol(IndexTFile* tf, uint64_t suid, char* colName) {
  if (tf == NULL) {
    return NULL;
  }
  ICacheKey key = {.suid = suid, .colType = TSDB_DATA_TYPE_BINARY, .colName = colName, .nColName = strlen(colName)};
  return tfileCacheGet(tf->cache, &key);
}

static int tfileUidCompare(const void* a, const void* b) {
  uint64_t l = *(uint64_t*)a;
  uint64_t r = *(uint64_t*)b;
  return l - r;
}
static int tfileStrCompare(const void* a, const void* b) {
  int ret = strcmp((char*)a, (char*)b);
  if (ret == 0) {
    return ret;
  }
  return ret < 0 ? -1 : 1;
}

static int tfileValueCompare(const void* a, const void* b, const void* param) {
  __compar_fn_t fn = *(__compar_fn_t*)param;

  TFileValue* av = (TFileValue*)a;
  TFileValue* bv = (TFileValue*)b;

  return fn(av->colVal, bv->colVal);
}

TFileValue* tfileValueCreate(char* val) {
  TFileValue* tf = calloc(1, sizeof(TFileValue));
  if (tf == NULL) {
    return NULL;
  }
  tf->colVal = tstrdup(val);
  tf->tableId = taosArrayInit(32, sizeof(uint64_t));
  return tf;
}
int tfileValuePush(TFileValue* tf, uint64_t val) {
  if (tf == NULL) {
    return -1;
  }
  taosArrayPush(tf->tableId, &val);
  return 0;
}
void tfileValueDestroy(TFileValue* tf) {
  taosArrayDestroy(tf->tableId);
  free(tf->colVal);
  free(tf);
}
static void tfileSerialTableIdsToBuf(char* buf, SArray* ids) {
  int sz = taosArrayGetSize(ids);
  SERIALIZE_VAR_TO_BUF(buf, sz, int32_t);
  for (size_t i = 0; i < sz; i++) {
    uint64_t* v = taosArrayGet(ids, i);
    SERIALIZE_VAR_TO_BUF(buf, *v, uint64_t);
  }
}

static int tfileWriteFstOffset(TFileWriter* tw, int32_t offset) {
  int32_t fstOffset = offset + sizeof(tw->header.fstOffset);
  tw->header.fstOffset = fstOffset;

  if (sizeof(fstOffset) != tw->ctx->write(tw->ctx, (char*)&fstOffset, sizeof(fstOffset))) {
    return -1;
  }
  indexInfo("tfile write fst offset: %d", tw->ctx->size(tw->ctx));
  tw->offset += sizeof(fstOffset);
  return 0;
}
static int tfileWriteHeader(TFileWriter* writer) {
  char buf[TFILE_HEADER_NO_FST] = {0};

  TFileHeader* header = &writer->header;
  memcpy(buf, (char*)header, sizeof(buf));

  indexInfo("tfile pre write header size: %d", writer->ctx->size(writer->ctx));
  int nwrite = writer->ctx->write(writer->ctx, buf, sizeof(buf));
  if (sizeof(buf) != nwrite) {
    return -1;
  }

  indexInfo("tfile after write header size: %d", writer->ctx->size(writer->ctx));
  writer->offset = nwrite;
  return 0;
}
static int tfileWriteData(TFileWriter* write, TFileValue* tval) {
  TFileHeader* header = &write->header;
  uint8_t      colType = header->colType;

  colType = INDEX_TYPE_GET_TYPE(colType);
  if (colType == TSDB_DATA_TYPE_BINARY || colType == TSDB_DATA_TYPE_NCHAR) {
    FstSlice key = fstSliceCreate((uint8_t*)(tval->colVal), (size_t)strlen(tval->colVal));
    if (fstBuilderInsert(write->fb, key, tval->offset)) {
      fstSliceDestroy(&key);
      return 0;
    }
    fstSliceDestroy(&key);
    return -1;
  } else {
    // handle other type later
  }
  return 0;
}
static int tfileWriteFooter(TFileWriter* write) {
  char  buf[sizeof(tfileMagicNumber) + 1] = {0};
  void* pBuf = (void*)buf;
  taosEncodeFixedU64((void**)(void*)&pBuf, tfileMagicNumber);
  int nwrite = write->ctx->write(write->ctx, buf, strlen(buf));

  indexInfo("tfile write footer size: %d", write->ctx->size(write->ctx));
  assert(nwrite == sizeof(tfileMagicNumber));
  return nwrite;
}
static int tfileReaderLoadHeader(TFileReader* reader) {
  // TODO simple tfile header later
  char buf[TFILE_HEADER_SIZE] = {0};

  int64_t nread = reader->ctx->readFrom(reader->ctx, buf, sizeof(buf), 0);
  if (nread == -1) {
    indexError("actual Read: %d, to read: %d, errno: %d, filename: %s", (int)(nread), (int)sizeof(buf), errno,
               reader->ctx->file.buf);
  } else {
    indexInfo("actual Read: %d, to read: %d, filename: %s", (int)(nread), (int)sizeof(buf), reader->ctx->file.buf);
  }
  // assert(nread == sizeof(buf));
  memcpy(&reader->header, buf, sizeof(buf));

  return 0;
}
static int tfileReaderLoadFst(TFileReader* reader) {
  WriterCtx* ctx = reader->ctx;
  int        size = ctx->size(ctx);

  // current load fst into memory, refactor it later
  int   fstSize = size - reader->header.fstOffset - sizeof(tfileMagicNumber);
  char* buf = calloc(1, fstSize);
  if (buf == NULL) {
    return -1;
  }

  int64_t ts = taosGetTimestampUs();
  int32_t nread = ctx->readFrom(ctx, buf, fstSize, reader->header.fstOffset);
  int64_t cost = taosGetTimestampUs() - ts;
  indexInfo("nread = %d, and fst offset=%d, fst size: %d, filename: %s, file size: %d, time cost: %" PRId64 "us", nread,
            reader->header.fstOffset, fstSize, ctx->file.buf, ctx->file.size, cost);
  // we assuse fst size less than FST_MAX_SIZE
  assert(nread > 0 && nread <= fstSize);

  FstSlice st = fstSliceCreate((uint8_t*)buf, nread);
  reader->fst = fstCreate(&st);
  free(buf);
  fstSliceDestroy(&st);

  return reader->fst != NULL ? 0 : -1;
}
static int tfileReaderLoadTableIds(TFileReader* reader, int32_t offset, SArray* result) {
  // TODO(yihao): opt later
  WriterCtx* ctx = reader->ctx;
  char       block[1024] = {0};
  int32_t    nread = ctx->readFrom(ctx, block, sizeof(block), offset);
  assert(nread >= sizeof(uint32_t));

  char*   p = block;
  int32_t nid = *(int32_t*)p;
  p += sizeof(nid);

  while (nid > 0) {
    int32_t left = block + sizeof(block) - p;
    if (left >= sizeof(uint64_t)) {
      taosArrayPush(result, (uint64_t*)p);
      p += sizeof(uint64_t);
    } else {
      char buf[sizeof(uint64_t)] = {0};
      memcpy(buf, p, left);

      memset(block, 0, sizeof(block));
      offset += sizeof(block);
      nread = ctx->readFrom(ctx, block, sizeof(block), offset);
      memcpy(buf + left, block, sizeof(uint64_t) - left);

      taosArrayPush(result, (uint64_t*)buf);
      p = block + sizeof(uint64_t) - left;
    }
    nid -= 1;
  }
  return 0;
}
static int tfileReaderVerify(TFileReader* reader) {
  // just validate header and Footer, file corrupted also shuild be verified later
  WriterCtx* ctx = reader->ctx;

  uint64_t tMagicNumber = 0;

  char buf[sizeof(tMagicNumber) + 1] = {0};
  int  size = ctx->size(ctx);

  if (size < sizeof(tMagicNumber) || size <= sizeof(reader->header)) {
    return -1;
  } else if (ctx->readFrom(ctx, buf, sizeof(tMagicNumber), size - sizeof(tMagicNumber)) != sizeof(tMagicNumber)) {
    return -1;
  }

  taosDecodeFixedU64(buf, &tMagicNumber);
  return tMagicNumber == tfileMagicNumber ? 0 : -1;
}

void tfileReaderRef(TFileReader* reader) {
  if (reader == NULL) {
    return;
  }
  int ref = T_REF_INC(reader);
  UNUSED(ref);
}

void tfileReaderUnRef(TFileReader* reader) {
  if (reader == NULL) {
    return;
  }
  int ref = T_REF_DEC(reader);
  if (ref == 0) {
    // do nothing
    tfileReaderDestroy(reader);
  }
}

static SArray* tfileGetFileList(const char* path) {
  char     buf[128] = {0};
  uint64_t suid;
  uint32_t version;
  SArray*  files = taosArrayInit(4, sizeof(void*));

  TdDirPtr pDir = taosOpenDir(path);
  if (NULL == pDir) {
    return NULL;
  }
  TdDirEntryPtr pDirEntry;
  while ((pDirEntry = taosReadDir(pDir)) != NULL) {
    char* file = taosGetDirEntryName(pDirEntry);
    if (0 != tfileParseFileName(file, &suid, buf, &version)) {
      continue;
    }

    size_t len = strlen(path) + 1 + strlen(file) + 1;
    char*  buf = calloc(1, len);
    sprintf(buf, "%s/%s", path, file);
    taosArrayPush(files, &buf);
  }
  taosCloseDir(pDir);

  taosArraySort(files, tfileCompare);
  tfileRmExpireFile(files);

  return files;
}
static int tfileRmExpireFile(SArray* result) {
  // TODO(yihao): remove expire tindex after restart
  return 0;
}
static void tfileDestroyFileName(void* elem) {
  char* p = *(char**)elem;
  free(p);
}
static int tfileCompare(const void* a, const void* b) {
  const char* as = *(char**)a;
  const char* bs = *(char**)b;
  return strcmp(as, bs);
}

static int tfileParseFileName(const char* filename, uint64_t* suid, char* col, int* version) {
  if (3 == sscanf(filename, "%" PRIu64 "-%[^-]-%d.tindex", suid, col, version)) {
    // read suid & colid & version  success
    return 0;
  }
  return -1;
}
// tfile name suid-colId-version.tindex
static void tfileGenFileName(char* filename, uint64_t suid, const char* col, int version) {
  sprintf(filename, "%" PRIu64 "-%s-%d.tindex", suid, col, version);
  return;
}
static void tfileGenFileFullName(char* fullname, const char* path, uint64_t suid, const char* col, int32_t version) {
  char filename[128] = {0};
  tfileGenFileName(filename, suid, col, version);
  sprintf(fullname, "%s/%s", path, filename);
}
