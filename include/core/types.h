#pragma once

#include <pmp/surface_mesh.h>
#include <polyscope/polyscope.h>
#include <polyscope/surface_mesh.h>
#include <queue>
#include <mutex>

#define ps polyscope

#define OUT_VTK_DIR "../../out/vtk/"
#define OUT_LOG_DIR "../../out/logs/"
#define DATA_DIR "../../data/"

#define L_MAX 1.3333
#define L_MIN 0.8

namespace ba {

enum RemesherType { BASE, PRIORITY_LOCAL, PRIORITY_GLOBAL };
enum SplitMode { SUM, MAX, AVG };
enum CollapseMode { LOSS };
enum FlipMode { VALENCE, EDGE_LENGTH };

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

struct RemesherSettings {
    SplitMode split = SplitMode::SUM;
    CollapseMode collapse = CollapseMode::LOSS;
    FlipMode flip = FlipMode::VALENCE;
    float target_length = 0.1f;
    float op_gain_threshold = 1e-5f;
    int flip_frequency = 5;
    int log_frequency = 0;
    int progress_frequency = 100;
    int iterations = 5;
    bool separate_flip_queue = true;
    bool show_vertex_loss = false;
};

struct Metrics {
    int operations = 0;
    double time_ms = -1;
    double total_edge_loss = -1;
    double volume_ratio = 1;

    // Geometry counts
    int vertex_count = 0;
    int edge_count = 0;
    int face_count = 0;
    
    // Operation counts
    int split_count = 0;
    int collapse_count = 0;
    int flip_count = 0;
    int smooth_count = 0;
};

struct LoggingState {
    bool logging = true;
    bool vtk_export = false;
    int logs = 0;
};

struct ProgressState {
    bool is_remeshing = false;
    int current_queue_size = 0;
    double current_progress_loss = 0.0;
    Metrics metrics;
};

// Thread-safe state wrapper. Provides atomic read-modify-write via update()
template<typename T>
class SyncState {
    mutable std::mutex mtx;
    T state;
public:
    SyncState() = default;
    explicit SyncState(T initial) : state(std::move(initial)) {}

    T load() const {
        std::lock_guard<std::mutex> lock(mtx);
        return state;
    }

    void store(T new_state) {
        std::lock_guard<std::mutex> lock(mtx);
        state = std::move(new_state);
    }

    // Atomic read-modify-write: fn receives a mutable reference to the state
    template<typename F>
    void update(F&& fn) {
        std::lock_guard<std::mutex> lock(mtx);
        fn(state);
    }
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