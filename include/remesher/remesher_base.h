#pragma once
#include <pmp/surface_mesh.h>
#include <iostream>
#include <functional>
#include <chrono>
#include <atomic>
#include "core/types.h"
#include "core/geo_utils.h"
#include "io/paraview.h"
#include "io/logger.h"
#include "remesher/loss.h"

namespace ba
{

class Remesher {
protected: 
    Mesh& mesh;
    const Mesh original_mesh;
    const double l_max = 1.3333;
    const double l_min = 0.8;
    double target_length;
    int iterations = 5;
    double op_gain_threshold = 1e-5;
    IterationMetrics metrics = IterationMetrics();
    
    // Callback for progress updates in ui thread
    std::function<void(int, IterationMetrics)> progress_callback = nullptr;

    // Sets base mesh metrics (eg. vertex count)
    void set_metrics();

    // Computed the Vector which a vertex is moved by in the smoothing step
    vec3 compute_smooth_step(Vertex v) const;

    // Find collapse point and relevant halfedge
    bool get_collapse_info(Edge e, Halfedge& collapse_h, Point& new_pos);

    // Score Split (loss before - after)
    double split_score(Edge e);
    // Score Collapse (loss before - after)
	double collapse_score(Edge e);
    // Score Flip (valence difference before - after)
	int flip_score(Edge e);
	// Score Smooth (loss before - after)
	double smooth_score(Vertex v);

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

    //Getters
    int get_iterations() const { return iterations; }
    double get_target_length() const { return target_length; }
    IterationMetrics get_metrics() const { return metrics; }
	double get_op_gain_threshold() const { return op_gain_threshold; }

    //Setters
    void set_iterations(int i) { iterations = i; }
    void set_target_length(double t) { target_length = t; }
    void set_progress_callback(std::function<void(int, IterationMetrics)> cb) { progress_callback = cb; }
    void set_op_gain_threshold(double t) { op_gain_threshold = t; }

    //Split for subclasses to implement using their own edge collection logic
    virtual int split_long_edges() = 0;
    //Collapse for subclasses to implement using their own edge collection logic
    virtual int collapse_short_edges() = 0;
    //Flip for subclasses to implement using their own edge collection logic
    virtual int flip_edges() = 0;
    //Smooth for subclasses to implement using their own vertex collection logic
    virtual int smooth_vertices() = 0;

    /**
     * Overridable function for performing a single iteration of the remeshing algorithm.
     */
    virtual void single_iteration();

    /**
     * Performs single_iteration with timing information.
     */
    void timed_iteration();

    /**
    * \brief Checks if the remeshing algorithm has converged (for void remesh())
    * Can be implemented by subclasses if needed
    */
    virtual bool converged(double prev_loss);

    /**
     * \brief Performs isotropic remeshing on the surface mesh.
     * 
     * \param run_until_converged If the remesher should try to converge or not
     * Either way it will stop at 100 iterations
     */
    void remesh(bool run_until_converged);
    virtual ~Remesher() = default;
    
    Remesher(Mesh& m) : mesh(m), original_mesh(m) { 
        target_length = avg_edge_length(m); 
        set_metrics();
    }
};

} //namespace ba