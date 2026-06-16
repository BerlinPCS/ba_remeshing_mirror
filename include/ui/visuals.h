#pragma once

#include "core/types.h"
#include <polyscope/surface_mesh.h>
#include <utility>
#include <vector>

namespace ba::ui {

/**
 * \brief Draws the given Mesh.
 * Converts the halfedge data structure to the format expected by Polyscope.
 * Adds vertex loss information as a scalar quantity.
 *
 * \param mesh_name The name to be given to the mesh in Polyscope
 * \returns A pointer to the registered Polyscope mesh for inline operations
 */
std::pair<double, double> get_loss_range(std::vector<double> const &losses, float threshold = 0.9f);
polyscope::SurfaceMesh *draw_surface_mesh(Mesh const &mesh, float target_length,
                                          std::pair<double, double> const &map_range);

} // namespace ba::ui
