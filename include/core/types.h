#pragma once

#include <pmp/surface_mesh.h>

#include "polyscope/polyscope.h"
#include "polyscope/surface_mesh.h"

#define ps polyscope

namespace ba {

// PMP types
using Mesh = pmp::SurfaceMesh;
using Vertex = pmp::Vertex;
using Edge = pmp::Edge;
using Face = pmp::Face;
using Halfedge = pmp::Halfedge;
using Point = pmp::Point;
using Normal = pmp::Normal;
using Scalar = pmp::Scalar;
using vec3 = pmp::vec3;
using vec2 = pmp::vec2;

} // namespace ba