#pragma once

#include "core/types.h"
#include <vector>

namespace ba::loss {

/**
 * \brief Calculates the loss of a single edge based on target length.
 */
double get_edge_loss(const Mesh &mesh, Edge e, double target_length);

/**
 * \brief Calculates the loss for a given edge length.
 */
double get_edge_loss_from_length(double length, double target_length);

/**
 * \brief Calculates the total edge loss for the mesh.
 */
double calc_total_edge_loss(const Mesh &mesh, double target_length);

/**
 * \brief Gets the average edge loss for a single vertex
 */
double single_vertex_loss(const Mesh &mesh, double target_length, Vertex v);

/**
 * \brief Calculates the average edge loss for each vertex.
 * \return A vector of losses corresponding to each vertex by index.
 */
std::vector<double> get_vertex_losses(const Mesh &mesh, double target_length);

} // namespace ba::loss