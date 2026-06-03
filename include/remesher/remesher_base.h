#pragma once
#include "core/geo_utils.h"
#include "core/types.h"
#include <functional>
#include <pmp/surface_mesh.h>

#include "remesher/evaluation_strategy.h"
#include <memory>

namespace ba {

class Remesher {
protected:
    Mesh& mesh;
    const Mesh original_mesh;
    double target_length;
    int iterations = 5;
    double op_gain_threshold = 1e-5;
    int progress_frequency = 100; // how often to report to ui thread
    int log_frequency = 0; // how often to log to csv (set to 0 for automatic)
    Metrics metrics = Metrics();
    std::shared_ptr<EvaluationStrategy> evaluator;
    
    int current_tick = 0;
    EdgeProperty<int> split_versions;
    EdgeProperty<int> collapse_versions;
    EdgeProperty<int> flip_versions;
    VertexProperty<int> smooth_versions;

    // Callback for progress updates in ui thread (queue_size, metrics, is_log_tick)
    std::function<void(int, Metrics, bool)> progress_callback = nullptr;

    inline void report_progress(int queue_size = 0) {
        metrics.operations++;
        bool is_log_tick = (metrics.operations % log_frequency == 0);
        bool is_ui_tick = (metrics.operations % progress_frequency == 0);

        if (progress_callback && (is_ui_tick || is_log_tick)) {
            if (is_log_tick) set_metrics();
            progress_callback(queue_size, metrics, is_log_tick);
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
    // Getters
    int get_iterations() const { return iterations; }
    double get_target_length() const { return target_length; }
    double get_op_gain_threshold() const { return op_gain_threshold; }
    int get_log_frequency() const { return log_frequency; }
    Metrics get_metrics() const { return metrics; }

    // Setters
    void set_iterations(int i) { iterations = i; }
    void set_target_length(double t) { target_length = t; }
    void set_op_gain_threshold(double t) { op_gain_threshold = t; }
    void set_log_frequency(int f) { log_frequency = f; }
    void set_progress_callback(std::function<void(int, Metrics, bool)> cb) { progress_callback = cb; }

    // Split for subclasses to implement using their own edge collection logic
    virtual void split_long_edges() = 0;
    // Collapse for subclasses to implement using their own edge collection logic
    virtual void collapse_short_edges() = 0;
    // Flip for subclasses to implement using their own edge collection logic
    virtual void flip_edges() = 0;
    // Smooth for subclasses to implement using their own vertex collection logic
    virtual void smooth_vertices() = 0;

    /**
     * Overridable function for performing a single iteration of the remeshing algorithm.
     */
    virtual void single_iteration();

    /**
     * \brief Performs isotropic remeshing on the surface mesh.
     */
    virtual void remesh();
    virtual ~Remesher() = default;

    Remesher(Mesh& m, std::shared_ptr<EvaluationStrategy> evaluator)  : mesh(m), original_mesh(m), evaluator(evaluator) {
        target_length = avg_edge_length(m);
        log_frequency = std::max(1, (int)m.n_edges() / 8);
        set_metrics();
        evaluator->set_params(target_length, op_gain_threshold);
        split_versions = mesh.add_edge_property<int>("e:split_version", 0);
        collapse_versions = mesh.add_edge_property<int>("e:collapse_version", 0);
        flip_versions = mesh.add_edge_property<int>("e:flip_version", 0);
        smooth_versions = mesh.add_vertex_property<int>("v:smooth_version", 0);
    }
};

} // namespace ba