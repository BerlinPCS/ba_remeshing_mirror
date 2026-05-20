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
private: 
    Mesh& mesh;
    const Mesh original_mesh;
    double target_length;
    const double l_max = 1.3333;
    const double l_min = 0.8;
    int iterations = 5;
    IterationMetrics metrics = IterationMetrics();
    
    // Callback for progress updates in ui thread
    std::function<void(int, IterationMetrics)> progress_callback = nullptr;

    void set_metrics();

public:

    //Getters
    int get_iterations() const { return iterations; }
    double get_target_length() { return target_length; }
    IterationMetrics get_metrics() { return metrics; }

    //Setters
    void set_iterations(int i) { iterations = i; }
    void set_target_length(double t) { target_length = t; }
    void set_progress_callback(std::function<void(int, IterationMetrics)> cb) { progress_callback = cb; }

    /**
     * \brief Splits long edges that exceed 4/3rds of the target length.
     */
    int split_long_edges();

    /**
     * \brief Splits short edges that are shorter than 4/5ths of the target length.
     */
    int collapse_short_edges();

    /**
     * \brief Flips edges to improve valence of vertices.
     * 
     * Currently penalizes large deviations heavier with a squared error.
     * Could also use abs instead
     */
    int flip_edges();

    /**
     * \brief Smoothes the vertices.
     */
    int smooth_vertices();

    /**
     * \brief Performs a single iteration of the isotropic remeshing algorithm
     * 
     * This includes:
     * 1. Splitting edges that are longer than 4/3rds of the target length
     * 2. Collapsing edges that are shorter than 4/5ths of the target length
     * 3. Flipping edges to improve valence of vertices
     * 4. Tangential Smoothing to improve triangle area
     */
    void single_iteration();

    /**
     * \brief Performs isotropic remeshing on the surface mesh.
     * 
     * If the class was initialized with a set amount of iterations, 
     * the single_iteration() function will be called that many times. Otherwise 
     * it will continue untill the loss function has converged.
     */
    void remesh(bool run_until_converged);
    
    Remesher(Mesh& m) : mesh(m), original_mesh(m) { 
        target_length = avg_edge_length(m); 
        set_metrics();
    }
};

} //namespace ba