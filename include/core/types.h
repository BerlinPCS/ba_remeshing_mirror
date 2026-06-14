#pragma once

#include <pmp/surface_mesh.h>
#include <queue>
#include <mutex>

#define OUT_VTK_DIR "../../out/vtk/"
#define OUT_LOG_DIR "../../out/logs/"
#define DATA_DIR "../../data/"

namespace ba {

inline constexpr double L_MAX = 4.0 / 3.0;
inline constexpr double L_MIN = 0.8;

enum RemesherType { BASE, PRIORITY_LOCAL, PRIORITY_GLOBAL };
enum SplitMode { SUM, MAX, AVG };
enum CollapseMode { LOSS };
enum FlipMode { VALENCE, EDGE_LOSS };

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
    int operation_limit = 0;
};

struct LoggingState {
    bool logging = true;
    bool vtk_export = false;
    int logs = 0;
};

// Progress and Statistics
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

enum class TerminationReason { None, Converged, EmptyQueues, OperationLimit };

inline const char* termination_reason_name(TerminationReason reason) {
    switch (reason) {
        case TerminationReason::Converged: return "Converged";
        case TerminationReason::EmptyQueues: return "Empty queues";
        case TerminationReason::OperationLimit: return "Operation limit";
        case TerminationReason::None: return "Running";
    }
    return "Unknown";
}

struct QueueStats {
    int queued = 0;
    int popped = 0;
    int stale = 0;
    int rejected = 0;
    int executed = 0;
    int rebuilt = 0;
};

struct ProgressState {
    bool is_remeshing = false;
    int current_queue_size = 0;
    int processed_candidates = 0;
    int current_iteration = 0;
    int total_iterations = 0;
    TerminationReason termination_reason = TerminationReason::None;
    QueueStats queue_stats;
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

// Operation Ordering and Evaluation
enum class OpType { Split, Collapse, Flip, Smooth };

struct OperationEvaluation {
    double priority = 0.0;
    double edge_loss_gain = 0.0;
    bool valid = false; // if operation is topologically valid
    bool accepted = false; // if operation has positive effect
};

class OpCandidate {
public:
    OpCandidate(OpType t, Vertex v) : type(t), v(v) { validate(); }
    OpCandidate(OpType t, Edge e) : type(t), e(e) { validate(); }
    OpCandidate() = default;

    OpType type;
    Edge e = Edge();
    Vertex v = Vertex();
    double score = 0.0;
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
