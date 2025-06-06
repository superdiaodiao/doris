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

#pragma once

#include <gen_cpp/Types_types.h>
#include <gen_cpp/types.pb.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "common/status.h"
#include "pipeline/local_exchange/local_exchanger.h"
#include "pipeline/pipeline.h"
#include "pipeline/pipeline_fragment_context.h"
#include "pipeline/pipeline_task.h"
#include "runtime/query_context.h"
#include "runtime/runtime_state.h"
#include "runtime/task_execution_context.h"
#include "util/runtime_profile.h"
#include "util/stopwatch.hpp"

namespace doris {
struct ReportStatusRequest;
class ExecEnv;
class RuntimeFilterMergeControllerEntity;
class TDataSink;
class TPipelineFragmentParams;

namespace pipeline {

class Dependency;

class PipelineFragmentContext : public TaskExecutionContext {
public:
    ENABLE_FACTORY_CREATOR(PipelineFragmentContext);
    // Callback to report execution status of plan fragment.
    // 'profile' is the cumulative profile, 'done' indicates whether the execution
    // is done or still continuing.
    // Note: this does not take a const RuntimeProfile&, because it might need to call
    // functions like PrettyPrint() or to_thrift(), neither of which is const
    // because they take locks.
    using report_status_callback = std::function<Status(
            const ReportStatusRequest, std::shared_ptr<pipeline::PipelineFragmentContext>&&)>;
    PipelineFragmentContext(const TUniqueId& query_id, const int fragment_id,
                            std::shared_ptr<QueryContext> query_ctx, ExecEnv* exec_env,
                            const std::function<void(RuntimeState*, Status*)>& call_back,
                            const report_status_callback& report_status_cb);

    ~PipelineFragmentContext();

    void print_profile(const std::string& extra_info);

    std::vector<std::shared_ptr<TRuntimeProfileTree>> collect_realtime_profile() const;
    std::shared_ptr<TRuntimeProfileTree> collect_realtime_load_channel_profile() const;

    bool is_timeout(timespec now) const;

    uint64_t elapsed_time() const { return _fragment_watcher.elapsed_time(); }

    int timeout_second() const { return _timeout; }

    PipelinePtr add_pipeline(PipelinePtr parent = nullptr, int idx = -1);

    QueryContext* get_query_ctx() { return _query_ctx.get(); }
    [[nodiscard]] bool is_canceled() const { return _query_ctx->is_cancelled(); }

    Status prepare(const doris::TPipelineFragmentParams& request, ThreadPool* thread_pool);

    Status submit();

    void set_is_report_success(bool is_report_success) { _is_report_success = is_report_success; }

    void cancel(const Status reason);

    TUniqueId get_query_id() const { return _query_id; }

    [[nodiscard]] int get_fragment_id() const { return _fragment_id; }

    void decrement_running_task(PipelineId pipeline_id);

    Status send_report(bool);

    void trigger_report_if_necessary();
    void refresh_next_report_time();

    std::string debug_string();

    [[nodiscard]] int next_operator_id() { return _operator_id--; }

    [[nodiscard]] int max_operator_id() const { return _operator_id; }

    [[nodiscard]] int next_sink_operator_id() { return _sink_operator_id--; }

    [[nodiscard]] size_t get_revocable_size(bool* has_running_task) const;

    [[nodiscard]] std::vector<PipelineTask*> get_revocable_tasks() const;

    void clear_finished_tasks() {
        for (size_t j = 0; j < _tasks.size(); j++) {
            for (size_t i = 0; i < _tasks[j].size(); i++) {
                _tasks[j][i]->stop_if_finished();
            }
        }
    }

    std::string get_load_error_url();

private:
    Status _build_pipelines(ObjectPool* pool, const doris::TPipelineFragmentParams& request,
                            const DescriptorTbl& descs, OperatorPtr* root, PipelinePtr cur_pipe);
    Status _create_tree_helper(ObjectPool* pool, const std::vector<TPlanNode>& tnodes,
                               const doris::TPipelineFragmentParams& request,
                               const DescriptorTbl& descs, OperatorPtr parent, int* node_idx,
                               OperatorPtr* root, PipelinePtr& cur_pipe, int child_idx,
                               const bool followed_by_shuffled_join);

    Status _create_operator(ObjectPool* pool, const TPlanNode& tnode,
                            const doris::TPipelineFragmentParams& request,
                            const DescriptorTbl& descs, OperatorPtr& op, PipelinePtr& cur_pipe,
                            int parent_idx, int child_idx, const bool followed_by_shuffled_join);
    template <bool is_intersect>
    Status _build_operators_for_set_operation_node(ObjectPool* pool, const TPlanNode& tnode,
                                                   const DescriptorTbl& descs, OperatorPtr& op,
                                                   PipelinePtr& cur_pipe, int parent_idx,
                                                   int child_idx,
                                                   const doris::TPipelineFragmentParams& request);

    Status _create_data_sink(ObjectPool* pool, const TDataSink& thrift_sink,
                             const std::vector<TExpr>& output_exprs,
                             const TPipelineFragmentParams& params, const RowDescriptor& row_desc,
                             RuntimeState* state, DescriptorTbl& desc_tbl,
                             PipelineId cur_pipeline_id);
    Status _plan_local_exchange(int num_buckets,
                                const std::map<int, int>& bucket_seq_to_instance_idx,
                                const std::map<int, int>& shuffle_idx_to_instance_idx);
    Status _plan_local_exchange(int num_buckets, int pip_idx, PipelinePtr pip,
                                const std::map<int, int>& bucket_seq_to_instance_idx,
                                const std::map<int, int>& shuffle_idx_to_instance_idx);
    void _inherit_pipeline_properties(const DataDistribution& data_distribution,
                                      PipelinePtr pipe_with_source, PipelinePtr pipe_with_sink);
    Status _add_local_exchange(int pip_idx, int idx, int node_id, ObjectPool* pool,
                               PipelinePtr cur_pipe, DataDistribution data_distribution,
                               bool* do_local_exchange, int num_buckets,
                               const std::map<int, int>& bucket_seq_to_instance_idx,
                               const std::map<int, int>& shuffle_idx_to_instance_idx);
    Status _add_local_exchange_impl(int idx, ObjectPool* pool, PipelinePtr cur_pipe,
                                    PipelinePtr new_pip, DataDistribution data_distribution,
                                    bool* do_local_exchange, int num_buckets,
                                    const std::map<int, int>& bucket_seq_to_instance_idx,
                                    const std::map<int, int>& shuffle_idx_to_instance_idx);

    Status _build_pipeline_tasks(const doris::TPipelineFragmentParams& request,
                                 ThreadPool* thread_pool);
    void _close_fragment_instance();
    void _init_next_report_time();

    // Id of this query
    TUniqueId _query_id;
    int _fragment_id;

    ExecEnv* _exec_env = nullptr;

    std::atomic_bool _prepared = false;
    bool _submitted = false;

    Pipelines _pipelines;
    PipelineId _next_pipeline_id = 0;
    std::mutex _task_mutex;
    int _closed_tasks = 0;
    // After prepared, `_total_tasks` is equal to the size of `_tasks`.
    // When submit fail, `_total_tasks` is equal to the number of tasks submitted.
    std::atomic<int> _total_tasks = 0;

    std::unique_ptr<RuntimeProfile> _fragment_level_profile;
    bool _is_report_success = false;

    std::unique_ptr<RuntimeState> _runtime_state;

    std::shared_ptr<QueryContext> _query_ctx;

    MonotonicStopWatch _fragment_watcher;
    RuntimeProfile::Counter* _prepare_timer = nullptr;
    RuntimeProfile::Counter* _init_context_timer = nullptr;
    RuntimeProfile::Counter* _build_pipelines_timer = nullptr;
    RuntimeProfile::Counter* _plan_local_exchanger_timer = nullptr;
    RuntimeProfile::Counter* _prepare_all_pipelines_timer = nullptr;
    RuntimeProfile::Counter* _build_tasks_timer = nullptr;

    std::function<void(RuntimeState*, Status*)> _call_back;
    bool _is_fragment_instance_closed = false;

    // If this is set to false, and '_is_report_success' is false as well,
    // This executor will not report status to FE on being cancelled.
    bool _is_report_on_cancel;

    // 0 indicates reporting is in progress or not required
    std::atomic_bool _disable_period_report = true;
    std::atomic_uint64_t _previous_report_time = 0;

    // This callback is used to notify the FE of the status of the fragment.
    // For example:
    // 1. when the fragment is cancelled, it will be called.
    // 2. when the fragment is finished, it will be called. especially, when the fragment is
    // a insert into select statement, it should notfiy FE every fragment's status.
    // And also, this callback is called periodly to notify FE the load process.
    report_status_callback _report_status_cb;

    DescriptorTbl* _desc_tbl = nullptr;
    int _num_instances = 1;

    int _timeout = -1;
    bool _use_serial_source = false;

    OperatorPtr _root_op = nullptr;
    // this is a [n * m] matrix. n is parallelism of pipeline engine and m is the number of pipelines.
    std::vector<std::vector<std::shared_ptr<PipelineTask>>> _tasks;

    // TODO: remove the _sink and _multi_cast_stream_sink_senders to set both
    // of it in pipeline task not the fragment_context
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow-field"
#endif
    DataSinkOperatorPtr _sink = nullptr;
#ifdef __clang__
#pragma clang diagnostic pop
#endif

    // `_dag` manage dependencies between pipelines by pipeline ID. the indices will be blocked by members
    std::map<PipelineId, std::vector<PipelineId>> _dag;

    // We use preorder traversal to create an operator tree. When we meet a join node, we should
    // build probe operator and build operator in separate pipelines. To do this, we should build
    // ProbeSide first, and use `_pipelines_to_build` to store which pipeline the build operator
    // is in, so we can build BuildSide once we complete probe side.
    struct pipeline_parent_map {
        std::map<int, std::vector<PipelinePtr>> _build_side_pipelines;
        void push(int parent_node_id, PipelinePtr pipeline) {
            if (!_build_side_pipelines.contains(parent_node_id)) {
                _build_side_pipelines.insert({parent_node_id, {pipeline}});
            } else {
                _build_side_pipelines[parent_node_id].push_back(pipeline);
            }
        }
        void pop(PipelinePtr& cur_pipe, int parent_node_id, int child_idx) {
            if (!_build_side_pipelines.contains(parent_node_id)) {
                return;
            }
            DCHECK(_build_side_pipelines.contains(parent_node_id));
            auto& child_pipeline = _build_side_pipelines[parent_node_id];
            DCHECK(child_idx < child_pipeline.size());
            cur_pipe = child_pipeline[child_idx];
        }
        void clear() { _build_side_pipelines.clear(); }
    } _pipeline_parent_map;

    std::mutex _state_map_lock;

    int _operator_id = 0;
    int _sink_operator_id = 0;
    /**
     * Some states are shared by tasks in different pipeline task (e.g. local exchange , broadcast join).
     *
     * local exchange sink 0 ->                               -> local exchange source 0
     *                            LocalExchangeSharedState
     * local exchange sink 1 ->                               -> local exchange source 1
     *
     * hash join build sink 0 ->                               -> hash join build source 0
     *                              HashJoinSharedState
     * hash join build sink 1 ->                               -> hash join build source 1
     *
     * So we should keep states here.
     */
    std::map<int,
             std::pair<std::shared_ptr<BasicSharedState>, std::vector<std::shared_ptr<Dependency>>>>
            _op_id_to_shared_state;

    std::map<PipelineId, Pipeline*> _pip_id_to_pipeline;
    std::vector<std::unique_ptr<RuntimeFilterMgr>> _runtime_filter_mgr_map;

    //Here are two types of runtime states:
    //    - _runtime state is at the Fragment level.
    //    - _task_runtime_states is at the task level, unique to each task.

    std::vector<TUniqueId> _fragment_instance_ids;
    /**
     * Local runtime states for each task.
     *
     * 2-D matrix:
     * +-------------------------+------------+-------+
     * |            | Instance 0 | Instance 1 |  ...  |
     * +------------+------------+------------+-------+
     * | Pipeline 0 |  task 0-0  |  task 0-1  |  ...  |
     * +------------+------------+------------+-------+
     * | Pipeline 1 |  task 1-0  |  task 1-1  |  ...  |
     * +------------+------------+------------+-------+
     * | ...                                          |
     * +--------------------------------------+-------+
     */
    std::vector<std::vector<std::unique_ptr<RuntimeState>>> _task_runtime_states;

    // Total instance num running on all BEs
    int _total_instances = -1;
    bool _require_bucket_distribution = false;
};
} // namespace pipeline
} // namespace doris