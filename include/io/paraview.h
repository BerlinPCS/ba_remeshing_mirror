#pragma once

#include <pmp/surface_mesh.h>
#include <string>
#include <vector>

namespace ba::io {

// Removes all vtks in OUT_VTK_DIR
void remove_vtks();

/**
 * \brief Exports a surface mesh to a VTK file.
 * \param filename The name of the file to export to.
 * \param mesh The surface mesh to export.
 * \param vertex_losses The losses for each vertex.
 * \param current_iteration The current iteration number. Optional. If included, create vtk bundle for paraview
 */
void export_mesh_vtk(const std::string &filename, pmp::SurfaceMesh &mesh, const std::vector<double> &vertex_losses,
					 int current_iteration = -1);

} // namespace ba::io
