#pragma once

#include <cmath>
#include <algorithm>
#include <vector>

#include "core/types.h"
#include "core/geo_utils.h"

namespace ba::loss {

/**
 * \brief Calculates the loss of a single edge based on target length.
 */
double get_edge_loss(const Mesh& mesh, Edge e, double target_length);

/**
 * \brief Calculates the total edge loss for the mesh.
 */
double calc_total_edge_loss(const Mesh& mesh, double target_length);

/**
 * \brief Calculates the average edge loss for each vertex.
 * \return A vector of losses corresponding to each vertex by index.
 */
std::vector<double> get_vertex_losses(const Mesh& mesh, double target_length);

} // namespace ba::loss