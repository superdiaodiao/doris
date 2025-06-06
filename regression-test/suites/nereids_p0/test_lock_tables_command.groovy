// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

suite("test_lock_tables_command", "nereids_p0") {
    def table1 = "test_lock_table_1"
    def table2 = "test_lock_table_2"

    try {
        sql """
            CREATE TABLE IF NOT EXISTS ${table1} (
                id INT,
                name STRING
            )
            DISTRIBUTED BY HASH(id) BUCKETS 3
            PROPERTIES ("replication_num" = "1");
        """

        sql """
            CREATE TABLE IF NOT EXISTS ${table2} (
                id INT,
                name STRING
            )
            DISTRIBUTED BY HASH(id) BUCKETS 3
            PROPERTIES ("replication_num" = "1");
        """

        checkNereidsExecute("""
            LOCK TABLES ${table1} READ, ${table2} WRITE
        """)

        sql """ UNLOCK TABLES """
    } finally {
        sql "DROP TABLE IF EXISTS ${table1}"
        sql "DROP TABLE IF EXISTS ${table2}"
    }
}

