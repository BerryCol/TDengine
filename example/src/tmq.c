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

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "taos.h"

static int running = 1;
static void msg_process(tmq_message_t* message) {
  tmqShowMsg(message);
}

int32_t init_env() {
  TAOS* pConn = taos_connect("localhost", "root", "taosdata", NULL, 0);
  if (pConn == NULL) {
    return -1;
  }

  TAOS_RES* pRes = taos_query(pConn, "create database if not exists abc1 vgroups 1");
  if (taos_errno(pRes) != 0) {
    printf("error in create db, reason:%s\n", taos_errstr(pRes));
    return -1;
  }
  taos_free_result(pRes);

  pRes = taos_query(pConn, "use abc1");
  if (taos_errno(pRes) != 0) {
    printf("error in use db, reason:%s\n", taos_errstr(pRes));
    return -1;
  }
  taos_free_result(pRes);

  /*pRes = taos_query(pConn, "create stable if not exists st1 (ts timestamp, k int) tags(a int)");*/
  /*if (taos_errno(pRes) != 0) {*/
    /*printf("failed to create super table 123_$^), reason:%s\n", taos_errstr(pRes));*/
    /*return -1;*/
  /*}*/
  /*taos_free_result(pRes);*/

  /*pRes = taos_query(pConn, "create table if not exists tu using st1 tags(1)");*/
  /*if (taos_errno(pRes) != 0) {*/
    /*printf("failed to create child table tu, reason:%s\n", taos_errstr(pRes));*/
    /*return -1;*/
  /*}*/
  /*taos_free_result(pRes);*/

  /*pRes = taos_query(pConn, "create table if not exists tu2 using st1 tags(2)");*/
  /*if (taos_errno(pRes) != 0) {*/
    /*printf("failed to create child table tu2, reason:%s\n", taos_errstr(pRes));*/
    /*return -1;*/
  /*}*/
  /*taos_free_result(pRes);*/


  const char* sql = "select * from st1";
  pRes = tmq_create_topic(pConn, "test_stb_topic_1", sql, strlen(sql));
  if (taos_errno(pRes) != 0) {
    printf("failed to create topic test_stb_topic_1, reason:%s\n", taos_errstr(pRes));
    return -1;
  }
  taos_free_result(pRes);
  taos_close(pConn);
  return 0;
}

tmq_t* build_consumer() {
  TAOS* pConn = taos_connect("localhost", "root", "taosdata", NULL, 0);
  assert(pConn != NULL);

  TAOS_RES* pRes = taos_query(pConn, "use abc1");
  if (taos_errno(pRes) != 0) {
    printf("error in use db, reason:%s\n", taos_errstr(pRes));
  }
  taos_free_result(pRes);

  tmq_conf_t* conf = tmq_conf_new();
  tmq_conf_set(conf, "group.id", "tg2");
  tmq_t* tmq = tmq_consumer_new(pConn, conf, NULL, 0);
  return tmq;

  tmq_list_t* topic_list = tmq_list_new();
  tmq_list_append(topic_list, "test_stb_topic_1");
  tmq_subscribe(tmq, topic_list);
  return NULL; 
}

tmq_list_t* build_topic_list() {
  tmq_list_t* topic_list = tmq_list_new();
  tmq_list_append(topic_list, "test_stb_topic_1");
  return topic_list;
}

void basic_consume_loop(tmq_t *tmq,
                        tmq_list_t *topics) {
  tmq_resp_err_t err;

  if ((err = tmq_subscribe(tmq, topics))) {
    fprintf(stderr, "%% Failed to start consuming topics: %s\n", tmq_err2str(err));
    printf("subscribe err\n");
    return;
  }
  int32_t cnt = 0;
  /*clock_t startTime = clock();*/
  while (running) {
    tmq_message_t *tmqmessage = tmq_consumer_poll(tmq, 500);
    if (tmqmessage) {
      cnt++;
      msg_process(tmqmessage);
      tmq_message_destroy(tmqmessage);
    /*} else {*/
      /*break;*/
    }
  }
  /*clock_t endTime = clock();*/
  /*printf("log cnt: %d %f s\n", cnt, (double)(endTime - startTime) / CLOCKS_PER_SEC);*/

  err = tmq_consumer_close(tmq);
  if (err)
    fprintf(stderr, "%% Failed to close consumer: %s\n", tmq_err2str(err));
  else
    fprintf(stderr, "%% Consumer closed\n");
}

void sync_consume_loop(tmq_t *tmq,
                  tmq_list_t *topics) {
  static const int MIN_COMMIT_COUNT = 1000;

  int msg_count = 0;
  tmq_resp_err_t err;

  if ((err = tmq_subscribe(tmq, topics))) {
    fprintf(stderr, "%% Failed to start consuming topics: %s\n", tmq_err2str(err));
    return;
  }

  while (running) {
    tmq_message_t *tmqmessage = tmq_consumer_poll(tmq, 500);
    if (tmqmessage) {
      msg_process(tmqmessage);
      tmq_message_destroy(tmqmessage);

      if ((++msg_count % MIN_COMMIT_COUNT) == 0)
        tmq_commit(tmq, NULL, 0);
    }
 }

  err = tmq_consumer_close(tmq);
  if (err)
    fprintf(stderr, "%% Failed to close consumer: %s\n", tmq_err2str(err));
  else
    fprintf(stderr, "%% Consumer closed\n");
}

int main() {
  int code;
  code = init_env();
  tmq_t* tmq = build_consumer();
  tmq_list_t* topic_list = build_topic_list();
  basic_consume_loop(tmq, topic_list);
  /*sync_consume_loop(tmq, topic_list);*/
}
