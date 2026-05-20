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

/**
 * \brief A struct to hold metrics for a single iteration.
 */
struct IterationMetrics {
    int iteration_num;
    double time_ms;
    double total_edge_loss;
    double volume_ratio;

    // Geometry counts
    int vertex_count;
    int edge_count;
    int face_count;
    
    // Operation counts
    int split_count;
    int collapse_count;
    int flip_count;
    int smooth_count;
};

} // namespace ba