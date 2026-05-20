#pragma once

#include <pmp/surface_mesh.h>

#include "polyscope/polyscope.h"
#include "polyscope/surface_mesh.h"

#define ps polyscope
#define OUT_VTK_DIR "../../out/vtk/"
#define OUT_LOG_DIR "../../out/logs/"
#define DATA_DIR "../../data/"

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
    int iteration_num = 0;
    double time_ms = -1;
    double total_edge_loss = -1;
    double volume_ratio = 1;

    // Geometry counts
    int vertex_count;
    int edge_count;
    int face_count;
    
    // Operation counts
    int split_count = -1;
    int collapse_count = -1;
    int flip_count = -1;
    int smooth_count = -1;
};

} // namespace ba