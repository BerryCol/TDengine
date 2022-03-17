Since TDengine was open sourced in July 2019, it has gained a lot of popularity among time-series database developers with its innovative data modeling design, simple installation method, easy programming interface, and powerful data insertion and query performance. The insertion and querying performance is often astonishing to users who are new to TDengine. In order to help users to experience the high performance and functions of TDengine in the shortest time, we developed an application called `taosBenchmark` (was named `taosdemo`) for insertion and querying performance testing of TDengine. Then user can easily simulate the scenario of a large number of devices generating a very large amount of data. User can easily manipulate the number of tables, columns, data types, disorder ratio, and number of concurrent threads with taosBenchmark customized parameters.

Running taosBenchmark is very simple. Just download the [TDengine installation package](https://www.taosdata.com/cn/all-downloads/) or compiling the [TDengine code](https://github.com/taosdata/TDengine). It can be found and run in the installation directory or in the compiled results directory.

# To run an insertion test with taosBenchmark

Executing taosBenchmark without any parameters results in the following output.

```
$ taosBenchmark

taosBenchmark is simulating data generated by power equipment monitoring...

host:                       127.0.0.1:6030
user:                       root
password:                   taosdata
configDir:
resultFile:                 ./output.txt
thread num of insert data:  8
thread num of create table: 8
top insert interval:        0
number of records per req:  30000
max sql length:             1048576
database count:             1
database[0]:
  database[0] name:      test
  drop:                  yes
  replica:               1
  precision:             ms
  super table count:     1
  super table[0]:
      stbName:           meters
      autoCreateTable:   no
      childTblExists:    no
      childTblCount:     10000
      childTblPrefix:    d
      dataSource:        rand
      iface:             taosc
      insertRows:        10000
      interlaceRows:     0
      disorderRange:     1000
      disorderRatio:     0
      maxSqlLen:         1048576
      timeStampStep:     1
      startTimestamp:    2017-07-14 10:40:00.000
      sampleFormat:
      sampleFile:
      tagsFile:
      columnCount:       3
column[0]:FLOAT column[1]:INT column[2]:FLOAT
      tagCount:            2
        tag[0]:INT tag[1]:BINARY(16)

         Press enter key to continue or Ctrl-C to stop
```

The parameters here shows for what taosBenchmark will use for data insertion. By default, taosBenchmark  without entering any command line arguments will simulate a city power grid system's meter data collection scenario as a typical application in the power industry. That is, a database named test will be created, and a super table named meters will be created, where the super table schema is following:

```
taos> describe test.meters;
             Field              |         Type         |   Length    |   Note   |
=================================================================================
 ts                             | TIMESTAMP            |           8 |          |
 current                        | FLOAT                |           4 |          |
 voltage                        | INT                  |           4 |          |
 phase                          | FLOAT                |           4 |          |
 groupid                        | INT                  |           4 | TAG      |
 location                       | BINARY               |          64 | TAG      |
Query OK, 6 row(s) in set (0.002972s)
```

After pressing any key taosBenchmark will create the database test and super table meters and generate 10,000 sub-tables representing 10,000 individual meter devices that report data. That means they independently using the super table meters as a template according to TDengine data modeling best practices.

```
taos> use test;
Database changed.

taos> show stables;
              name              |      created_time       | columns |  tags  |   tables    |
============================================================================================
 meters                         | 2021-08-27 11:21:01.209 |       4 |      2 |       10000 |
Query OK, 1 row(s) in set (0.001740s)
```

```
taos> use test;
Database changed.

taos> show stables;
              name              |      created_time       | columns |  tags  |   tables    |
============================================================================================
 meters                         | 2021-08-27 11:21:01.209 |       4 |      2 |       10000 |
Query OK, 1 row(s) in set (0.001740s)
```

Then taosBenchmark generates 10,000 records for each meter device.

```
...
====thread[3] completed total inserted rows: 6250000, total affected rows: 6250000. 347626.22 records/second====
[1]:100%
====thread[1] completed total inserted rows: 6250000, total affected rows: 6250000. 347481.98 records/second====
[4]:100%
====thread[4] completed total inserted rows: 6250000, total affected rows: 6250000. 347149.44 records/second====
[8]:100%
====thread[8] completed total inserted rows: 6250000, total affected rows: 6250000. 347082.43 records/second====
[6]:99%
[6]:100%
====thread[6] completed total inserted rows: 6250000, total affected rows: 6250000. 345586.35 records/second====
Spent 18.0863 seconds to insert rows: 100000000, affected rows: 100000000 with 16 thread(s) into test.meters. 5529049.90 records/second

insert delay, avg:      28.64ms, max:     112.92ms, min:       9.35ms
```

The above information is the result of a real test on a normal PC server with 8 CPUs and 64G RAM. It shows that taosBenchmark inserted 100,000,000 (no need to count, 100 million) records in 18 seconds, or an average of 552,909,049 records per second.

TDengine also offers a parameter-bind interface for better performance, and using the parameter-bind interface (taosBenchmark -I stmt) on the same hardware for the same amount of data writes, the results are as follows.

```
...

====thread[14] completed total inserted rows: 6250000, total affected rows: 6250000. 1097331.55 records/second====
[9]:97%
[4]:97%
[3]:97%
[3]:98%
[4]:98%
[9]:98%
[3]:99%
[4]:99%
[3]:100%
====thread[3] completed total inserted rows: 6250000, total affected rows: 6250000. 1089038.19 records/second====
[9]:99%
[4]:100%
====thread[4] completed total inserted rows: 6250000, total affected rows: 6250000. 1087123.09 records/second====
[9]:100%
====thread[9] completed total inserted rows: 6250000, total affected rows: 6250000. 1085689.38 records/second====
[11]:91%
[11]:92%
[11]:93%
[11]:94%
[11]:95%
[11]:96%
[11]:97%
[11]:98%
[11]:99%
[11]:100%
====thread[11] completed total inserted rows: 6250000, total affected rows: 6250000. 1039087.65 records/second====
Spent 6.0257 seconds to insert rows: 100000000, affected rows: 100000000 with 16 thread(s) into test.meters. 16595590.52 records/second

insert delay, avg:       8.31ms, max:     860.12ms, min:       2.00ms
```

It shows that taosBenchmark inserted 100 million records in 6 seconds, with a much more higher insertion performance, 1,659,590 records was inserted per second.

Because taosBenchmark is so easy to use, so we have extended it with more features to support more complex parameter settings for sample data preparation and validation for rapid prototyping.

The complete list of taosBenchmark command-line arguments can be displayed via taosBenchmark --help as follows.

```
$ taosBenchmark --help

-f, --file=FILE The JSON configuration file to the execution procedure. Currently, we support standard UTF-8 (without BOM) encoded files only.
-u, --user=USER The user name to use when connecting to the server.
-p, --password The password to use when connecting to the server.
-c, --config-dir=CONFIG_DIR Configuration directory.
-h, --host=HOST TDengine server FQDN to connect. The default host is localhost.
-P, --port=PORT The TCP/IP port number to use for the connection.
-I, --interface=INTERFACE The interface (taosc, rest, and stmt) taosBenchmark uses. By default use 'taosc'.
-d, --database=DATABASE Destination database. By default is 'test'.
-a, --replica=REPLICA Set the replica parameters of the database, By default use 1, min: 1, max: 3.
-m, --table-prefix=TABLEPREFIX Table prefix name. By default use 'd'.
-s, --sql-file=FILE The select SQL file.
-N, --normal-table Use normal table flag.
-o, --output=FILE Direct output to the named file. By default use './output.txt'.
-q, --query-mode=MODE Query mode -- 0: SYNC, 1: ASYNC. By default use SYNC.
-b, --data-type=DATATYPE The data_type of columns, By default use: FLOAT, INT, FLOAT.
-w, --binwidth=WIDTH The width of data_type 'BINARY' or 'NCHAR'. By default use 64
-l, --columns=COLUMNS The number of columns per record. Demo mode by default is 1 (float, int, float). Max values is 4095
All of the new column(s) type is INT. If use -b to specify column type, -l will be ignored.
-T, --threads=NUMBER The number of threads. By default use 8.
-i, --insert-interval=NUMBER The sleep time (ms) between insertion. By default is 0.
-S, --time-step=TIME_STEP The timestamp step between insertion. By default is 1.
-B, --interlace-rows=NUMBER The interlace rows of insertion. By default is 0.
-r, --rec-per-req=NUMBER The number of records per request. By default is 30000.
-t, --tables=NUMBER The number of tables. By default is 10000.
-n, --records=NUMBER The number of records per table. By default is 10000.
-M, --random The value of records generated are totally random.
By default to simulate power equipment scenario.
-x, --aggr-func Test aggregation functions after insertion.
-y, --answer-yes Input yes for prompt.
-O, --disorder=NUMBER Insert order mode--0: In order, 1 ~ 50: disorder ratio. By default is in order.
-R, --disorder-range=NUMBER Out of order data's range. Unit is ms. By default is 1000.
-g, --debug Print debug info.
-?, --help Give this help list
--usage Give a short usage message
-V, --version Print program version.

Mandatory or optional arguments to long options are also mandatory or optional
for any corresponding short options.

Report bugs to <support@taosdata.com>.
```

taosBenchmark's parameters are designed to meet the needs of data simulation. A few commonly used parameters are described below.

```
-I, --interface=INTERFACE     The interface (taosc, rest, and stmt) taosBenchmark uses. Default is 'taosc'.
```

The performance difference between different interfaces of taosBenchmark has been mentioned earlier, the -I parameter is used to select different interfaces, currently taosc, stmt and rest are supported. The -I parameter is used to select different interfaces, currently taosc, stmt and rest are supported. taosc uses SQL statements to write data, stmt uses parameter binding interface to write data, and rest uses RESTful protocol to write data.

```
-T, --threads=NUMBER          The number of threads. Default is 8.
```

The -T parameter sets how many threads taosBenchmark uses to synchronize data writes, so that multiple threads can squeeze as much processing power out of the hardware as possible.

```
-b, --data-type=DATATYPE      The data_type of columns, default: FLOAT, INT, FLOAT.

-w, --binwidth=WIDTH          The width of data_type 'BINARY' or 'NCHAR'. Default is 64

-l, --columns=COLUMNS         The number of columns per record. Demo mode by default is 3 (float, int, float). Max values is 4095
```

As mentioned earlier, taosdemo creates a typical meter data reporting scenario by default, with each device containing three columns. They are current, voltage and phases. TDengine supports BOOL, TINYINT, SMALLINT, INT, BIGINT, FLOAT, DOUBLE, BINARY, NCHAR, TIMESTAMP data types. By using -b with a list of types allows you to specify the column list with customized data type. Using -w to specify the width of the columns of the BINARY and NCHAR data types (default is 64). The -l parameter can be added to the columns of the data type specified by the -b parameter with the total number of columns of the INT type, which reduces the manual input process in case of a particularly large number of columns, up to 4095 columns.

```
-r, --rec-per-req=NUMBER      The number of records per request. Default is 30000.
```

To reach TDengine performance limits, data insertion can be executed by using multiple clients, multiple threads, and batch data insertions at once. The -r parameter sets the number of records batch that can be stitched together in a single write request, the default is 30,000. The effective number of spliced records is also related to the client buffer size, which is currently 1M Bytes. If the record column width is large, the maximum number of spliced records can be calculated by dividing 1M by the column width (in bytes).

```
-t, --tables=NUMBER           The number of tables. Default is 10000.
-n, --records=NUMBER          The number of records per table. Default is 10000.
-M, --random                  The value of records generated are totally random. The default is to simulate power equipment scenario.
```

As mentioned earlier, taosBenchmark creates 10,000 tables by default, and each table writes 10,000 records. taosBenchmark can set the number of tables and the number of records in each table by -t and -n. The data generated by default without parameters are simulated real scenarios, and the simulated data are current and voltage phase values with certain jitter, which can more realistically show TDengine's efficient data compression ability. If you need to simulate the generation of completely random data, you can pass the -M parameter.

```
-y, --answer-yes              Default input yes for prompt.
```

As we can see above, taosBenchmark outputs a list of parameters for the upcoming operation by default before creating a database or inserting data, so that the user can know what data is about to be written before inserting. To facilitate automatic testing, the -y parameter allows taosBenchmark to write data immediately after outputting the parameters.

```
-O, --disorder=NUMBER         Insert order mode--0: In order, 1 ~ 50: disorder ratio. Default is in order.
-R, --disorder-range=NUMBER   Out of order data's range, ms, default is 1000.
```

In some scenarios, the received data does not arrive in exact order, but contains a certain percentage of out-of-order data, which TDengine can also handle very well. In order to simulate the writing of out-of-order data, taosdemo provides -O and -R parameters to be set. The -O parameter is the same as the -O parameter for fully ordered data writes. 1 to 50 is the percentage of data that contains out-of-order data. The -R parameter is the range of the timestamp offset of the out-of-order data, default is 1000 milliseconds. Also note that temporal data is uniquely identified by a timestamp, so garbled data may generate the exact same timestamp as previously written data, and such data may either be discarded (update 0) or overwrite existing data (update 1 or 2) depending on the update value created by the database, and the total number of data entries may not match the expected number of entries.

```
 -g, --debug                   Print debug info.
```

If you are interested in the taosBenchmark insertion process or if the data insertion result is not as expected, you can use the -g parameter to make taosBenchmark print the debugging information in the process of the execution to the screen or import it to another file with the Linux redirect command to easily find the cause of the problem. In addition, taosBenchmark will also output the corresponding executed statements and debugging reasons to the screen after the execution fails. You can search the word "reason" to find the error reason information returned by the TDengine server.

```
-x, --aggr-func               Test aggregation functions after insertion.
```

TDengine is not only very powerful in insertion performance, but also in query performance due to its advanced database engine design. taosdemo provides a -x function that performs the usual query operations and outputs the query consumption time after the insertion of data. The following is the result of a common query after inserting 100 million rows on the aforementioned server.

You can see that the select * fetch 100 million rows (not output to the screen) operation consumes only 1.26 seconds. The most of normal aggregation function for 100 million records usually takes only about 20 milliseconds, and even the longest count function takes less than 40 milliseconds.

```
taosBenchmark -I stmt -T 48 -y -x
...
...
select          * took 1.266835 second(s)
...
select   count(*) took 0.039684 second(s)
...
Where condition: groupid = 1
select avg(current) took 0.025897 second(s)
...
select sum(current) took 0.025622 second(s)
...
select max(current) took 0.026124 second(s)
...
...
select min(current) took 0.025812 second(s)
...
select first(current) took 0.024105 second(s)
...
```

In addition to the command line approach, taosBenchmark also supports take a JSON file as an incoming parameter to provide a richer set of settings. A typical JSON file would look like this.

```
{
    "filetype": "insert",
    "cfgdir": "/etc/taos",
    "host": "127.0.0.1",
    "port": 6030,
    "user": "root",
    "password": "taosdata",
    "thread_count": 4,
    "thread_count_create_tbl": 4,
    "result_file": "./insert_res.txt",
    "confirm_parameter_prompt": "no",
    "insert_interval": 0,
    "interlace_rows": 100,
    "num_of_records_per_req": 100,
    "databases": [{
        "dbinfo": {
            "name": "db",
            "drop": "yes",
            "replica": 1,
            "days": 10,
            "cache": 16,
            "blocks": 8,
            "precision": "ms",
            "keep": 3650,
            "minRows": 100,
            "maxRows": 4096,
            "comp":2,
            "walLevel":1,
            "cachelast":0,
            "quorum":1,
            "fsync":3000,
            "update": 0
        },
        "super_tables": [{
            "name": "stb",
            "child_table_exists":"no",
            "childtable_count": 100,
            "childtable_prefix": "stb_",
            "auto_create_table": "no",
            "batch_create_tbl_num": 5,
            "data_source": "rand",
            "insert_mode": "taosc",
            "insert_rows": 100000,
            "childtable_limit": 10,
            "childtable_offset":100,
            "interlace_rows": 0,
            "insert_interval":0,
            "max_sql_len": 1024000,
            "disorder_ratio": 0,
            "disorder_range": 1000,
            "timestamp_step": 10,
            "start_timestamp": "2020-10-01 00:00:00.000",
            "sample_format": "csv",
            "sample_file": "./sample.csv",
            "use_sample_ts": "no",
            "tags_file": "",
            "columns": [{"type": "INT"}, {"type": "DOUBLE", "count":10}, {"type": "BINARY", "len": 16, "count":3}, {"type": "BINARY", "len": 32, "count":6}],
            "tags": [{"type": "TINYINT", "count":2}, {"type": "BINARY", "len": 16, "count":5}]
        }]
    }]
}
```

For example, we can specify different number of threads for table creation and data insertion with `thread_count` and `thread_count_create_tbl`. You can use a combination of `child_table_exists`, `childtable_limit` and `childtable_offset` to use multiple taosBenchmark processes (even on different computers) to write to different ranges of child tables of the same super table at the same time. You can also import existing data by specifying the data source as a CSV file with `data_source` and `sample_file`. The argument `use_sample_ts` indicate whether the first column, timestamp in TDengine would use the data of the specified CSV file too.

CSV file is a plain text format and use comma signs as separators between two columns. The number of columns must is same as the number of columns or tags of the table you intend to insert.

# Use taosBenchmark for query and subscription testing

taosBenchmark can not only write data, but also perform query and subscription functions. However, a taosBenchmark instance can only support one of these functions, not all three, and the configuration file is used to specify which function to test.

The following is the content of a typical query JSON example file.

```
{
  "filetype": "query",
  "cfgdir": "/etc/taos",
  "host": "127.0.0.1",
  "port": 6030,
  "user": "root",
  "password": "taosdata",
  "confirm_parameter_prompt": "no",
  "databases": "db",
  "query_times": 2,
  "query_mode": "taosc",
  "specified_table_query": {
    "query_interval": 1,
    "concurrent": 3,
    "sqls": [
      {
        "sql": "select last_row(*) from stb0 ",
        "result": "./query_res0.txt"
      },
      {
        "sql": "select count(*) from stb00_1",
        "result": "./query_res1.txt"
      }
    ]
  },
  "super_table_query": {
    "stblname": "stb1",
    "query_interval": 1,
    "threads": 3,
    "sqls": [
      {
        "sql": "select last_row(ts) from xxxx",
        "result": "./query_res2.txt"
      }
    ]
  }
}
```

The following parameters are specific to the query in the JSON file.

```
"query_times": the number of queries per query type
"query_mode": query data interface, "taosc": call TDengine's c interface; "restful": use RESTful interface. Options are available. Default is "taosc".
"specified_table_query": { query for the specified table
"query_interval": interval to execute sqls, in seconds. Optional, default is 0.
"concurrent": the number of threads to execute sqls concurrently, optional, default is 1. Each thread executes all sqls.
"sqls": multiple SQL statements can be added, support up to 100 statements.
"sql": query statement. Mandatory.
"result": the name of the file where the query result will be written. Optional, default is null, means the query result will not be written to the file.
"super_table_query": { query for all sub-tables in the super table
"stblname": the name of the super table. Mandatory.
"query_interval": interval to execute sqls, in seconds. Optional, default is 0.
"threads": the number of threads to execute sqls concurrently, optional, default is 1. Each thread is responsible for a part of sub-tables and executes all sqls.
"sql": "select count(*) from xxxx". Query statement for all sub-tables in the super table, where the table name must be written as "xxxx" and the instance will be replaced with the sub-table name automatically.
"result": the name of the file to which the query result is written. Optional, the default is null, which means the query results are not written to a file.
```

The following is a typical subscription JSON example file content.

```
{
    "filetype":"subscribe",
    "cfgdir": "/etc/taos",
    "host": "127.0.0.1",
    "port": 6030,
    "user": "root",
    "password": "taosdata",
    "databases": "db",
    "confirm_parameter_prompt": "no",
    "specified_table_query":
      {
       "concurrent":1,
       "mode":"sync",
       "interval":0,
       "restart":"yes",
       "keepProgress":"yes",
       "sqls": [
        {
          "sql": "select * from stb00_0 ;",
          "result": "./subscribe_res0.txt"
        }]
      },
    "super_table_query":
      {
       "stblname": "stb0",
       "threads":1,
       "mode":"sync",
       "interval":10000,
       "restart":"yes",
       "keepProgress":"yes",
       "sqls": [
        {
          "sql": "select * from xxxx where ts > '2021-02-25 11:35:00.000' ;",
          "result": "./subscribe_res1.txt"
        }]
      }
  }
```

The following are the meanings of the parameters specific to the subscription function.

```
"interval": interval for executing subscriptions, in seconds. Optional, default is 0.
"restart": subscription restart." yes": restart the subscription if it already exists, "no": continue the previous subscription. (Please note that the executing user needs to have read/write access to the dataDir directory)
"keepProgress": keep the progress of the subscription information. yes means keep the subscription information, no means don't keep it. The value is yes and restart is no to continue the previous subscriptions.
"resubAfterConsume": Used in conjunction with keepProgress to call unsubscribe after the subscription has been consumed the appropriate number of times and to subscribe again.
"result": the name of the file to which the query result is written. Optional, default is null, means the query result will not be written to the file. Note: The file to save the result after each SQL statement cannot be renamed, and the file name will be appended with the thread number when generating the result file.
```

# Conclusion

TDengine is a big data platform designed and optimized for IoT, Telemetric, Industrial Internet, DevOps, etc. TDengine shows a high performance that far exceeds similar products due to the innovative data storage and query engine design in the database kernel. And with SQL syntax support and connectors for multiple programming languages (currently Java, Python, Go, C#, Node.js, Rust, etc. are supported), it is extremely easy to use and has zero learning cost. To facilitate the operation and maintenance needs, we also provide data migration and monitoring functions and other related ecological tools and software.

For users who are new to TDengine, we have developed rich features for taosBenchmark to facilitate technical evaluation and stress testing. This article is a brief introduction to taosBenchmark, which will continue to evolve and improve as new features are added to TDengine.

 As part of TDengine, taosBenchmark's source code is fully open on the GitHub. Suggestions or advice about the use or implementation of taosBenchmark or TDengine are welcomed on GitHub or in the Taos Data user group.
