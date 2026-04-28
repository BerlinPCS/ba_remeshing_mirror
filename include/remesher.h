#pragma once
#include <pmp/surface_mesh.h>

namespace ba 
{

class Remesher {
private: 
    pmp::SurfaceMesh& mesh;
    double target_length;
    double l_max = 1.3333;
    double l_min = 0.8;
    double loss;
    int iterations;

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
     */
    void flip_edges();

    /**
     * \brief Smoothes the vertices.
     */
    void smooth_vertices();

    /**
     * \brief Gets the loss of a single edge 
     *
     * \return The loss value as a double.
     */
    double get_edge_loss(pmp::Edge e);

    /**
     * \brief Gets the ideal valence of a Vertex v. 
     * 
     * This is 6 for interior vertices and 4 for boundary vertices.
     *
     * \return The ideal valence as an integer.
     */
    int ideal_valence(pmp::Vertex v);

public:
    /**
     * \brief Gets the current total mesh loss / entropy. 
     *
     * \return The current loss value as a double.
     */
    double get_current_loss() const { return loss; }

    /**
     * \brief Performs isotropic remeshing on the surface mesh.
     * 
     * If the class was initialized with a set amount of iterations, 
     * the single_iteration() function will be called that many times. Otherwise 
     * it will continue untill the loss function has converged.
     */
    void remesh();

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


    Remesher(pmp::SurfaceMesh& m, double t, int i) : mesh(m), target_length(t), iterations(i) {}
    Remesher(pmp::SurfaceMesh& m, double t) : mesh(m), target_length(t), iterations(0) {}
};

}