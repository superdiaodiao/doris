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
suite("q10_spill") {
  sql """
    set enable_force_spill=true;
  """
  sql """
    set spill_min_revocable_mem=1;
  """
  sql """
    use regression_test_tpcds_sf1_unique_p1;
  """
/*
  qt_q10 """
SELECT
  cd_gender
, cd_marital_status
, cd_education_status
, count(*) cnt1
, cd_purchase_estimate
, count(*) cnt2
, cd_credit_rating
, count(*) cnt3
, cd_dep_count
, count(*) cnt4
, cd_dep_employed_count
, count(*) cnt5
, cd_dep_college_count
, count(*) cnt6
FROM
  customer c
, customer_address ca
, customer_demographics
WHERE (c.c_current_addr_sk = ca.ca_address_sk)
   AND (ca_county IN ('Rush County', 'Toole County', 'Jefferson County', 'Dona Ana County', 'La Porte County'))
   AND (cd_demo_sk = c.c_current_cdemo_sk)
   AND (EXISTS (
   SELECT *
   FROM
     store_sales
   , date_dim
   WHERE (c.c_customer_sk = ss_customer_sk)
      AND (ss_sold_date_sk = d_date_sk)
      AND (d_year = 2002)
      AND (d_moy BETWEEN 1 AND (1 + 3))
))
   AND ((EXISTS (
      SELECT *
      FROM
        web_sales
      , date_dim
      WHERE (c.c_customer_sk = ws_bill_customer_sk)
         AND (ws_sold_date_sk = d_date_sk)
         AND (d_year = 2002)
         AND (d_moy BETWEEN 1 AND (1 + 3))
   ))
      OR (EXISTS (
      SELECT *
      FROM
        catalog_sales
      , date_dim
      WHERE (c.c_customer_sk = cs_ship_customer_sk)
         AND (cs_sold_date_sk = d_date_sk)
         AND (d_year = 2002)
         AND (d_moy BETWEEN 1 AND (1 + 3))
   )))
GROUP BY cd_gender, cd_marital_status, cd_education_status, cd_purchase_estimate, cd_credit_rating, cd_dep_count, cd_dep_employed_count, cd_dep_college_count
ORDER BY cd_gender ASC, cd_marital_status ASC, cd_education_status ASC, cd_purchase_estimate ASC, cd_credit_rating ASC, cd_dep_count ASC, cd_dep_employed_count ASC, cd_dep_college_count ASC
LIMIT 100
"""
*/
}
