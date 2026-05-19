#pragma once
#include <pmp/surface_mesh.h>
#include <iostream>
#include "core/types.h"
#include "core/geo_utils.h"

namespace ba
{

class Remesher {
private: 
    Mesh& mesh;
    double target_length;
    const double l_max = 1.3333;
    const double l_min = 0.8;
    double loss;
    int iterations = 5;

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
    void split_long_edges();

    /**
     * \brief Splits short edges that are shorter than 4/5ths of the target length.
     */
    void collapse_short_edges();

    /**
     * \brief Flips edges to improve valence of vertices.
     * 
     * Currently penalizes large deviations heavier with a squared error.
     * Could also use abs instead
     */
    void flip_edges();

    /**
     * \brief Smoothes the vertices.
     */
    void smooth_vertices();

    /**
     * \brief Gets the current total mesh loss / entropy. 
     *
     * \return The current loss value as a double.
     */
    double get_current_loss() const { return loss; }

    /**
     * \brief Gets the current iteration count for the remeshing algorithm 
     *
     * \return The current iteration value as an integer.
     */
    int get_iterations() const { return iterations; }
    
    /**
     * \brief Set the iteration count for the remeshing algortihm
     * 0 means run until converged.
     */
    void set_iterations(double i) { iterations = i; }

    double get_target_length() { return target_length; }
    void set_target_length(double t) { target_length = t; }

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
    void remesh();
    
    Remesher(Mesh& m) : mesh(m) { target_length = avg_edge_length(m); }
};

} //namespace ba