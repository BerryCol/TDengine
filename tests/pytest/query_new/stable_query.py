###################################################################
#           Copyright (c) 2016 by TAOS Technologies, Inc.
#                     All rights reserved.
#
#  This file is proprietary and confidential to TAOS Technologies.
#  No part of this file may be reproduced, stored, transmitted,
#  disclosed or used in any form or by any means other than as
#  expressly provided by the written permission from Jianhui Tao
#
###################################################################

# -*- coding: utf-8 -*-

import random
import string
import os
import sys
import time
import taos
from util.log import tdLog
from util.cases import tdCases
from util.sql import tdSql
from util.dnodes import tdDnodes
from util.dnodes import *
from util.createdata import *
from util.where import *
import itertools
from itertools import product
from itertools import combinations
import subprocess

class TDTestCase:
    def caseDescription(self):
        '''
        case1<xyguo>:select * from stable where condition && select * from ( select front )
        case2<xyguo>:select * from stable where condition order by ts asc | desc && select * from ( select front )
        case3<xyguo>:select * from stable where condition order by ts limit && select * from ( select front )
        case4<xyguo>:select * from stable where condition order by ts limit offset && select * from ( select front )
        case5<xyguo>:
        case6<xyguo>:
        case7<xyguo>:
        case8<xyguo>:
        case9<xyguo>:
        case10<xyguo>:
        ''' 
        return

    #basic_param
    db = "stable_all"
    table_list = ['stable_1','stable_2',]
    table = str(random.sample(table_list,1)).replace("[","").replace("]","").replace("'","")
    table_null_list = ['stable_null_data','stable_null_childtable']
    table_null = str(random.sample(table_null_list,1)).replace("[","").replace("]","").replace("'","")
    testcaseFilename = os.path.split(__file__)[-1]

    def init(self, conn, logSql):
        tdLog.debug("start to execute %s" % __file__)
        tdSql.init(conn.cursor(), logSql)

    def case_common(self):
        os.system("rm -rf query_new/%s.sql" % self.testcaseFilename )    
        tdCreateData.dropandcreateDB_random("%s" %self.db,1) 

        conn1 = taos.connect(host="127.0.0.1", user="root", password="taosdata", config="/etc/taos/")
        cur1 = conn1.cursor()
        tdSql.init(cur1, True)        
        cur1.execute('use "%s";' %self.db)
        sql = 'select * from stable_1 limit 5;'
        cur1.execute(sql)

        return(conn1,cur1)

    def right_case1(self):
        print("case1:select * from stable where condition && select * from ( select front )")
        print("\n\n\n=========================================case1=========================================\n\n\n")
        
        case_common = self.case_common()
        conn1 = case_common[0]
        cur1 = case_common[1]
        sql = 'Count the number of sqls'

        for i in range(2):
            try:
                taos_cmd1 = "taos -f query_new/%s.sql" % self.testcaseFilename
                _ = subprocess.check_output(taos_cmd1, shell=True).decode("utf-8")
                print(conn1)
                cur1.execute('use "%s";' %self.db)                 

                stable_where = tdWhere.stable_where()
                sql1 = 'select * from %s;' % self.table
                for i in range(2,len(stable_where[2])+1):
                    qt_where = list(combinations(stable_where[2],i))
                    for qt_where in qt_where:
                        qt_where = str(qt_where).replace("(","").replace(")","").replace("'","").replace("\"","").replace(",","")
                        qt_like_match = stable_where[3]
                        qt_in_where = stable_where[4]

                        sql2 = "select * from %s where %s %s %s " %(self.table,qt_where,qt_like_match,qt_in_where)
                        tdCreateData.dataequal('%s' %sql1 ,10,10,'%s' %sql2 ,10,10)
                        cur1.execute(sql2)
                        sql= sql + sql2

                        sql2 = "select * from (select * from %s where %s %s %s ) " %(self.table,qt_where,qt_like_match,qt_in_where)
                        tdCreateData.dataequal('%s' %sql1 ,10,10,'%s' %sql2 ,10,10)
                        cur1.execute(sql2)
                        sql= sql + sql2

                        sql2 = "select * from (select * from %s ) where %s %s %s " %(self.table,qt_where,qt_like_match,qt_in_where)
                        tdCreateData.dataequal('%s' %sql1 ,10,10,'%s' %sql2 ,10,10)
                        cur1.execute(sql2)
                        sql= sql + sql2

                        sql2 = "select * from (select * from %s where %s %s %s ) where %s %s %s " %(self.table,qt_where,qt_like_match,qt_in_where,qt_where,qt_like_match,qt_in_where)
                        tdCreateData.dataequal('%s' %sql1 ,10,10,'%s' %sql2 ,10,10)
                        cur1.execute(sql2)
                        sql= sql + sql2

            except Exception as e:
                raise e 

        num1 = sql.count('where')
        print("sqlnum1 %d" % num1) 

    def right_case2(self):
        print("case2:select * from stable where condition order by ts asc | desc && select * from ( select front )")
        print("\n\n\n=========================================case2=========================================\n\n\n")
        
        case_common = self.case_common()
        conn1 = case_common[0]
        cur1 = case_common[1]
        sql = 'Count the number of sqls'

        for i in range(2):
            try:
                taos_cmd1 = "taos -f query_new/%s.sql" % self.testcaseFilename
                _ = subprocess.check_output(taos_cmd1, shell=True).decode("utf-8")
                print(conn1)
                cur1.execute('use "%s";' %self.db)                 
                
                stable_where = tdWhere.stable_where()
                sql1 = 'select * from %s;' % self.table 
                for i in range(2,len(stable_where[2])+1):
                    qt_where = list(combinations(stable_where[2],i))
                    for qt_where in qt_where:
                        qt_where = str(qt_where).replace("(","").replace(")","").replace("'","").replace("\"","").replace(",","")
                        qt_like_match = stable_where[3]
                        qt_in_where = stable_where[4]

                        sql2 = "select * from %s where tbname in ('%s_1') and %s %s %s order by ts" %(self.table,self.table,qt_where,qt_like_match,qt_in_where)
                        tdCreateData.dataequal('%s' %sql1 ,10,10,'%s' %sql2 ,10,10)
                        cur1.execute(sql2)
                        sql= sql + sql2

                        sql2 = "select * from (select * from %s where tbname in ('%s_1') and %s %s %s order by ts ) " %(self.table,self.table,qt_where,qt_like_match,qt_in_where)
                        tdCreateData.dataequal('%s' %sql1 ,10,10,'%s' %sql2 ,10,10)
                        cur1.execute(sql2)
                        sql= sql + sql2

                        sql2 = "select * from (select * from %s  where tbname in ('%s_1')) where %s %s %s order by ts " %(self.table,self.table,qt_where,qt_like_match,qt_in_where)
                        tdCreateData.dataequal('%s' %sql1 ,10,10,'%s' %sql2 ,10,10)
                        cur1.execute(sql2)
                        sql= sql + sql2

                        sql2 = "select * from (select * from %s  where tbname in ('%s_1') order by ts ) where %s %s %s " %(self.table,self.table,qt_where,qt_like_match,qt_in_where)
                        tdCreateData.dataequal('%s' %sql1 ,10,10,'%s' %sql2 ,10,10)
                        cur1.execute(sql2)
                        sql= sql + sql2

                        sql2 = "select * from (select * from %s  where tbname in ('%s_1') order by ts ) where %s %s %s order by ts " %(self.table,self.table,qt_where,qt_like_match,qt_in_where)
                        tdCreateData.dataequal('%s' %sql1 ,10,10,'%s' %sql2 ,10,10)
                        cur1.execute(sql2)
                        sql= sql + sql2

                        sql2 = "select * from (select * from %s ) where tbname in ('%s_1') and %s %s %s order by ts " %(self.table,self.table,qt_where,qt_like_match,qt_in_where)
                        tdSql.error(sql2)
                        sql= sql + sql2

            except Exception as e:
                raise e 

        num2 = sql.count('where')
        print("sqlnum2 %d" % num2) 

    def right_case3(self):
        print("case3:select * from stable where condition order by ts limit && select * from ( select front ) ")
        print("\n\n\n=========================================case3=========================================\n\n\n")
        
        case_common = self.case_common()
        conn1 = case_common[0]
        cur1 = case_common[1]
        sql = 'Count the number of sqls'

        for i in range(2):
            try:
                taos_cmd1 = "taos -f query_new/%s.sql" % self.testcaseFilename
                _ = subprocess.check_output(taos_cmd1, shell=True).decode("utf-8")
                print(conn1)
                cur1.execute('use "%s";' %self.db)                                

                stable_where = tdWhere.stable_where()
                sql1 = 'select * from %s;' % self.table 
                for i in range(2,len(stable_where[2])+1):
                    qt_where = list(combinations(stable_where[2],i))
                    for qt_where in qt_where:
                        qt_where = str(qt_where).replace("(","").replace(")","").replace("'","").replace("\"","").replace(",","")
                        qt_like_match = stable_where[3]
                        qt_in_where = stable_where[4]

                        sql2 = "select * from %s where tbname in ('%s_1') and %s %s %s order by ts limit 10" %(self.table,self.table,qt_where,qt_like_match,qt_in_where)
                        tdCreateData.dataequal('%s' %sql1 ,10,10,'%s' %sql2 ,10,10)
                        cur1.execute(sql2)
                        sql= sql + sql2

                        sql2 = "select * from (select * from %s where tbname in ('%s_1') and %s %s %s order by ts limit 10)" %(self.table,self.table,qt_where,qt_like_match,qt_in_where)
                        tdCreateData.dataequal('%s' %sql1 ,10,10,'%s' %sql2 ,10,10)
                        cur1.execute(sql2)
                        sql= sql + sql2

                        sql2 = "select * from (select * from %s where tbname in ('%s_1')) where %s %s %s order by ts limit 10" %(self.table,self.table,qt_where,qt_like_match,qt_in_where)
                        tdCreateData.dataequal('%s' %sql1 ,10,10,'%s' %sql2 ,10,10)
                        cur1.execute(sql2)
                        sql= sql + sql2

                        sql2 = "select * from (select * from %s) where tbname in ('%s_1') and %s %s %s order by ts limit 10" %(self.table,self.table,qt_where,qt_like_match,qt_in_where)
                        tdSql.error(sql2)
                        sql= sql + sql2

            except Exception as e:
                raise e 

        num3 = sql.count('where')
        print("sqlnum3 %d" % num3) 

    def right_case4(self):
        print("case4:select * from stable where condition order by ts limit offset && select * from ( select front )")
        print("\n\n\n=========================================case4=========================================\n\n\n")
        
        case_common = self.case_common()
        conn1 = case_common[0]
        cur1 = case_common[1]
        sql = 'Count the number of sqls'

        for i in range(2):
            try:
                taos_cmd1 = "taos -f query_new/%s.sql" % self.testcaseFilename
                _ = subprocess.check_output(taos_cmd1, shell=True).decode("utf-8")
                print(conn1)
                cur1.execute('use "%s";' %self.db)                                 

                stable_where = tdWhere.stable_where()
                sql1 = 'select * from  %s limit 10 offset 5;'  % self.table
                for i in range(2,len(stable_where[2])+1):
                    qt_where = list(combinations(stable_where[2],i))
                    for qt_where in qt_where:
                        qt_where = str(qt_where).replace("(","").replace(")","").replace("'","").replace("\"","").replace(",","")
                        qt_like_match = stable_where[3]
                        qt_in_where = stable_where[4]

                        sql2 = "select * from %s where tbname in ('%s_1') and %s %s %s order by ts limit 10 offset 5" %(self.table,self.table,qt_where,qt_like_match,qt_in_where)
                        tdCreateData.dataequal('%s' %sql1 ,10,10,'%s' %sql2 ,10,10)
                        cur1.execute(sql2)
                        sql= sql + sql2

                        sql2 = "select * from (select * from %s where tbname in ('%s_1') and %s %s %s order by ts limit 10 offset 5)" %(self.table,self.table,qt_where,qt_like_match,qt_in_where)
                        tdCreateData.dataequal('%s' %sql1 ,10,10,'%s' %sql2 ,10,10)
                        cur1.execute(sql2)
                        sql= sql + sql2

                        sql2 = "select * from (select * from %s where tbname in ('%s_1')) where %s %s %s order by ts limit 10 offset 5" %(self.table,self.table,qt_where,qt_like_match,qt_in_where)
                        tdCreateData.dataequal('%s' %sql1 ,10,10,'%s' %sql2 ,10,10)
                        cur1.execute(sql2)
                        sql= sql + sql2

                        sql2 = "select * from (select * from %s ) where tbname in ('%s_1') and %s %s %s order by ts limit 10 offset 5" %(self.table,self.table,qt_where,qt_like_match,qt_in_where)
                        tdSql.error(sql2)   
                        sql= sql + sql2 

            except Exception as e:
                raise e 

        num4 = sql.count('where')
        print("sqlnum4 %d" % num4) 


    def false_case1(self):
        print("\n\n\n=======================================error case=======================================\n\n\n")
        print("case1:select * from stable where condition interval | sliding | Fill && select * from ( select front )")
        print("\n\n\n=========================================case1=========================================\n\n\n")
        
        case_common = self.case_common()
        conn1 = case_common[0]
        cur1 = case_common[1]

        for i in range(2):
            try:
                taos_cmd1 = "taos -f query_new/%s.sql" % self.testcaseFilename
                _ = subprocess.check_output(taos_cmd1, shell=True).decode("utf-8")
                print(conn1)
                cur1.execute('use "%s";' %self.db)                                 

                stable_where = tdWhere.stable_where()
                sql1 = 'select * from stable_1 interval(3s) sliding(3n) Fill(NEXT);'  
                for i in range(2,len(stable_where[2])+1):
                    qt_where = list(combinations(stable_where[2],i))
                    for qt_where in qt_where:
                        qt_where = str(qt_where).replace("(","").replace(")","").replace("'","").replace("\"","").replace(",","")
                        qt_like_match = stable_where[3]
                        qt_in_where = stable_where[4]
                        time_window = stable_where[5]
                        og_by = stable_where[6]
                        groupby = tdWhere.groupby()

                        sql2 = "select * from %s where %s %s %s %s" %(self.table,qt_where,qt_like_match,qt_in_where,time_window)
                        tdSql.error(sql2)

                        sql2 = "select * from (select * from %s where %s %s %s %s)" %(self.table,qt_where,qt_like_match,qt_in_where,time_window)
                        tdSql.error(sql2)

                        sql2 = "select * from (select * from %s) where %s %s %s %s" %(self.table,qt_where,qt_like_match,qt_in_where,time_window)
                        tdSql.error(sql2)

                        sql2 = "select distinct(*) from %s where %s %s %s" %(self.table,qt_where,qt_like_match,qt_in_where)
                        tdSql.error(sql2)

                        sql2 = "select * from %s where %s %s %s %s" %(self.table,qt_where,qt_like_match,qt_in_where,groupby)
                        tdSql.error(sql2)

            except Exception as e:
                raise e 

    def run(self):
        startTime = time.time() 

        self.false_case1()

        # self.right_case1()
        # self.right_case2()
        # self.right_case3()
        # self.right_case4()

        startTime1 = time.time()
        self.right_case1()
        endTime1 = time.time()       
        print("total time1 %d s" % (endTime1 - startTime1))

        startTime2 = time.time()
        self.right_case2()
        endTime2 = time.time()
        print("total time2 %d s" % (endTime2 - startTime2))

        startTime3 = time.time()
        self.right_case3()
        endTime3 = time.time()
        print("total time3 %ds" % (endTime3 - startTime3))

        startTime4 = time.time()
        self.right_case4()  
        endTime4 = time.time()
        print("total time4 %ds" % (endTime4 - startTime4))

        endTime = time.time()
        print("total time %ds" % (endTime - startTime))

    def stop(self):
        tdSql.close()
        tdLog.success("%s successfully executed" % __file__)


tdCases.addWindows(__file__, TDTestCase())
tdCases.addLinux(__file__, TDTestCase())