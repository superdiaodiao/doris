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

#include "olap/schema_cache.h"

#include <butil/logging.h>
#include <fmt/core.h>
#include <gen_cpp/Descriptors_types.h>
#include <glog/logging.h>

#include <memory>
#include <mutex>
#include <vector>

#include "common/config.h"
#include "olap/schema.h"
#include "olap/tablet.h"
#include "olap/tablet_schema.h"
#include "util/defer_op.h"
#include "util/time.h"

namespace doris {

SchemaCache* SchemaCache::instance() {
    return ExecEnv::GetInstance()->schema_cache();
}

// format: tabletId-unique_id1-uniqueid2...-version-type
std::string SchemaCache::get_schema_key(int64_t tablet_id, const std::vector<TColumn>& columns,
                                        int32_t version) {
    if (columns.empty() || columns[0].col_unique_id < 0) {
        return "";
    }
    std::string key = fmt::format("{}-", tablet_id);
    std::for_each(columns.begin(), columns.end(), [&](const TColumn& col) {
        key.append(fmt::format("{}-", col.col_unique_id));
        // Remove the check after we del old impl of topn materialize code
        if (col.column_name.find("ROWID") != std::string::npos) {
            key.append(fmt::format("{}-", col.column_name));
        }
    });
    key.append(fmt::format("{}", version));
    return key;
}

} // namespace doris