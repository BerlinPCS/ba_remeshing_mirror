#pragma once

#include <fstream>
#include <filesystem>
#include <cmath>
#include <iostream>
#include <iomanip>

#include "core/types.h"

namespace ba::io {

void remove_vtks();

void export_mesh_vtk(const std::string& filename, 
                 pmp::SurfaceMesh& mesh, 
                 const std::vector<double>& vertex_losses,
                 int current_iteration = -1);

}
