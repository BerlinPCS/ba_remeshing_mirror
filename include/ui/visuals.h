#pragma once

#include "core/types.h"
#include <polyscope/surface_mesh.h>

namespace ba::ui {

/**
 * \brief Draws the given Mesh.
 * Converts the halfedge data structure to the format expected by Polyscope.
 * Adds vertex loss information as a scalar quantity.
 * 
 * \param mesh_name The name to be given to the mesh in Polyscope
 * \returns A pointer to the registered Polyscope mesh for inline operations
 */
polyscope::SurfaceMesh* draw_surface_mesh(Mesh const& mesh, float target_length);

} // namespace ba::ui
