#pragma once

#include <pmp/surface_mesh.h>
#include <polyscope/polyscope.h>
#include <polyscope/surface_mesh.h>
#include <queue>

#define ps polyscope

#define OUT_VTK_DIR "../../out/vtk/"
#define OUT_LOG_DIR "../../out/logs/"
#define DATA_DIR "../../data/"

#define BASE 0
#define PRIORITY_LOCAL 1
#define PRIORITY_GLOBAL 2

#define SPLIT_SUM 0
#define SPLIT_MAX 1
#define SPLIT_AVG 2

#define L_MAX 1.3333
#define L_MIN 0.8

namespace ba {

// PMP types
using Mesh = pmp::SurfaceMesh;
using Vertex = pmp::Vertex;
template <typename T>
using VertexProperty = pmp::VertexProperty<T>;
using Edge = pmp::Edge;
template <typename T>
using EdgeProperty = pmp::EdgeProperty<T>;
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
struct Metrics {
    int operations = 0;
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

// Types for Operation Ordering
enum class OpType { Split, Collapse, Flip, Smooth };
class OpCandidate {
public:
    OpCandidate(OpType t, Vertex v) : type(t), v(v) { validate(); }
    OpCandidate(OpType t, Edge e) : type(t), e(e) { validate(); }
    OpCandidate() = default;

    OpType type;
    Edge e = Edge();
    Vertex v = Vertex();
    double score;
    int version = 0;
private:
    void validate() {
        switch (type) {
            case OpType::Split:
                if(!e.is_valid()) throw std::invalid_argument("Invalid edge for split operation");
                break;
            case OpType::Collapse:
                if(!e.is_valid()) throw std::invalid_argument("Invalid edge for collapse operation");
                break;
            case OpType::Flip:
                if(!e.is_valid()) throw std::invalid_argument("Invalid edge for flip operation");
                break;
            case OpType::Smooth:
                if(!v.is_valid()) throw std::invalid_argument("Invalid vertex for smooth operation");
                break;
        }
    }
};
struct OpCompare {
    bool operator()(const OpCandidate& a, const OpCandidate& b) const {
        return a.score < b.score;
    }
};
using OpQueue = std::priority_queue<OpCandidate, std::vector<OpCandidate>, OpCompare>;

} // namespace ba