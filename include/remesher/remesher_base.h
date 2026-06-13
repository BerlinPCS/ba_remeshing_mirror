#pragma once
#include "core/geo_utils.h"
#include "core/types.h"
#include <functional>
#include <memory>
#include <pmp/surface_mesh.h>

#include "remesher/evaluation_strategy.h"

namespace ba {

class Remesher {
protected:
    Mesh& mesh;
    const Mesh original_mesh;
    RemesherSettings& r_ctx;
    SyncState<ProgressState>& p_ctx;
    std::shared_ptr<EvaluationStrategy> evaluator;

    int current_tick = 0;
    EdgeProperty<int> split_versions;
    EdgeProperty<int> collapse_versions;
    EdgeProperty<int> flip_versions;
    VertexProperty<int> smooth_versions;

    std::function<void(bool)> progress_callback = nullptr;

    inline void report_progress(OpType op_type, int queue_size = -1) {
        int ops;
        p_ctx.update([&](ProgressState& state) {
            state.metrics.operations++;
            ops = state.metrics.operations;
            switch(op_type) {
                case OpType::Split: state.metrics.split_count++; break;
                case OpType::Collapse: state.metrics.collapse_count++; break;
                case OpType::Flip: state.metrics.flip_count++; break;
                case OpType::Smooth: state.metrics.smooth_count++; break;
            }
            if (queue_size >= 0) state.current_queue_size = queue_size;
        });

        bool is_log_tick = r_ctx.log_frequency > 0 && (ops % r_ctx.log_frequency == 0);
        bool is_ui_tick = r_ctx.progress_frequency > 0 && (ops % r_ctx.progress_frequency == 0);

        if (progress_callback && (is_ui_tick || is_log_tick)) {
            if (is_log_tick) set_metrics();
            progress_callback(is_log_tick);
        }
    } 

    // Sets base mesh metrics (eg. vertex count)
    void set_metrics();

    // Base split for a single edge - returns if split occured or not
    bool split_edge(Edge e);
    // Base collapse for a single edge - returns if collapse occured or not
    bool collapse_edge(Edge e);
    // Base flip for a single edge - returns if flip occured or not
    bool flip_edge(Edge e);
    // Base smooth for a single vertex - returns if smooth occured or not
    bool smooth_vertex(Vertex v);

    void enqueue_candidate(OpQueue& pq, OpCandidate cand);

public:
    void set_progress_callback(std::function<void(bool)> cb) { progress_callback = cb; }

    // Methods to be implemented by subclasses
    virtual void split_long_edges() = 0;
    virtual void collapse_short_edges() = 0;
    virtual void flip_edges() = 0;
    virtual void smooth_vertices() = 0;
    virtual void single_iteration();
    virtual void remesh();

    virtual ~Remesher() = default;

    Remesher(Mesh& m, RemesherSettings& r_ctx, SyncState<ProgressState>& p_ctx) : mesh(m), original_mesh(m), r_ctx(r_ctx), p_ctx(p_ctx) {
        evaluator = std::make_shared<EvaluationStrategy>(r_ctx);
        set_metrics();
        split_versions = mesh.add_edge_property<int>("e:split_version", 0);
        collapse_versions = mesh.add_edge_property<int>("e:collapse_version", 0);
        flip_versions = mesh.add_edge_property<int>("e:flip_version", 0);
        smooth_versions = mesh.add_vertex_property<int>("v:smooth_version", 0);
    }
};

} // namespace ba