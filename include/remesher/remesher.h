#pragma once
#include <pmp/surface_mesh.h>
#include <iostream>
#include <functional>
#include <chrono>
#include "core/types.h"
#include "core/geo_utils.h"
#include "io/logger.h"

namespace ba
{

class Remesher {
private: 
    Mesh& mesh;
    const Mesh original_mesh;
    double target_length;
    const double l_max = 1.3333;
    const double l_min = 0.8;
    double edge_loss = 0;
    int iterations = 5;
    
    // Callback for progress updates in ui thread
    std::function<void(int, int, double)> progress_callback = nullptr;

    /**
     * \brief Calculates the total edge loss for the mesh.
     *
     * \return The total loss value as a double.
     */
    double calc_total_edge_loss();

public:

    /**
     * \brief Gets the loss of a single edge 
     *
     * \return The loss value as a double.
     */
    double get_edge_loss(Edge e);

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
     * \brief Gets the current total mesh loss / entropy. 
     *
     * \return The current loss value as a double.
     */
    double get_total_edge_loss() const { return edge_loss; }

    int get_iterations() const { return iterations; }
    void set_iterations(int i) { iterations = i; }
    double get_target_length() { return target_length; }
    void set_target_length(double t) { target_length = t; }
    
    void set_progress_callback(std::function<void(int, int, double)> cb) { progress_callback = cb; }

    /**
     * \brief Performs a single iteration of the isotropic remeshing algorithm
     * 
     * This includes:
     * 1. Splitting edges that are longer than 4/3rds of the target length
     * 2. Collapsing edges that are shorter than 4/5ths of the target length
     * 3. Flipping edges to improve valence of vertices
     * 4. Tangential Smoothing to improve triangle area
     */
    void single_iteration(IterationMetrics &metrics);

    /**
     * \brief Performs a single iteration of the isotropic remeshing algorithm without recording metrics.
     */
    void single_iteration();

    /**
     * \brief Performs isotropic remeshing on the surface mesh.
     * 
     * If the class was initialized with a set amount of iterations, 
     * the single_iteration() function will be called that many times. Otherwise 
     * it will continue untill the loss function has converged.
     */
    void remesh(bool log_metrics = false, bool run_until_converged = false);
    
    Remesher(Mesh& m) : mesh(m), original_mesh(m) { target_length = avg_edge_length(m); }
};

} //namespace ba