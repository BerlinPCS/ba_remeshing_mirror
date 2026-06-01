#include "remesher/remesher_base.h"
#include <algorithm>

namespace ba {

// Scoring of Singular Operations
double Remesher::split_score(Edge e) {
    if (mesh.is_deleted(e)) return -1.0;
    if (edge_length(mesh, e) <= target_length * l_max) return -1.0;

	Vertex v0 = mesh.vertex(e, 0);
	Vertex v1 = mesh.vertex(e, 1);
    Point p_mid = 0.5 * (mesh.position(v0) + mesh.position(v1)); // Mid is where the split woud happen

    // The two points opposite of the edge e
    Halfedge h0 = mesh.next_halfedge(mesh.halfedge(e, 0));
	Halfedge h1 = mesh.next_halfedge(mesh.halfedge(e, 1));
	Vertex v2 = mesh.is_boundary(h0) ? Vertex() : mesh.to_vertex(h0);
	Vertex v3 = mesh.is_boundary(h1) ? Vertex() : mesh.to_vertex(h1);
    Point p2 = (v2.is_valid()) ? mesh.position(v2) : p_mid;
    Point p3 = (v3.is_valid()) ? mesh.position(v3) : p_mid;    

    double length = edge_length(mesh, e);
    // Loss of the single edge within the two trianges
	double before = loss::get_edge_loss_from_length(length, target_length);
    // Loss of all 4 edges within the two triangles
	double after = 2.0 * loss::get_edge_loss_from_length(0.5 * length, target_length) + 
                   loss::get_edge_loss_from_length(pmp::distance(p2, p_mid), target_length) + 
                   loss::get_edge_loss_from_length(pmp::distance(p3, p_mid), target_length);
    double score = before - after;
    if(score < op_gain_threshold) return -1.0;
	return score;
}

double Remesher::collapse_score(Edge e) {
    if (mesh.is_deleted(e)) return -1.0;
    if (edge_length(mesh, e) >= target_length * l_min) return -1.0;

    Halfedge h;
    Point new_pos;
    if (!get_collapse_info(e, h, new_pos)) return -1.0;

    Vertex v_from = mesh.from_vertex(h);
    Vertex v_to = mesh.to_vertex(h);
    Point p_from = mesh.position(v_from);
    Point p_to = mesh.position(v_to);

    double before = loss::get_edge_loss(mesh, e, target_length);
    double after = 0.0;

    // We have to evaluate all edges that are one step away, since they would be affected by the operation
    std::vector<Vertex> neighbors;
    // Evaluate edges around v_from
    for (auto v_n : mesh.vertices(v_from)) {
        if (v_n == v_to) continue;
        neighbors.push_back(v_n);
        before += loss::get_edge_loss_from_length(pmp::distance(p_from, mesh.position(v_n)), target_length);
    }
    // Evaluate edges around v_to
    for (auto v_n : mesh.vertices(v_to)) {
        if (v_n == v_from) continue;
        before += loss::get_edge_loss_from_length(pmp::distance(p_to, mesh.position(v_n)), target_length);
        if (std::find(neighbors.begin(), neighbors.end(), v_n) == neighbors.end()) {
            neighbors.push_back(v_n);
        }
    }
    // Evaluate the new edges connecting to the merged vertex
    for (auto v_n : neighbors) {
        after += loss::get_edge_loss_from_length(pmp::distance(new_pos, mesh.position(v_n)), target_length);
    }
    double score = before - after;
    if(score < op_gain_threshold) return -1.0;
	return score;
}

// Ignore for Global Prio Queue - unable to compare since this is valence loss
int Remesher::flip_score(Edge e) {
    if (mesh.is_deleted(e)) return -1;
	if (!mesh.is_flip_ok(e)) return -1;
	Vertex v1 = mesh.vertex(e, 0);
	Vertex v2 = mesh.vertex(e, 1);
	Vertex w1 = mesh.to_vertex(mesh.next_halfedge(mesh.halfedge(e, 0)));
	Vertex w2 = mesh.to_vertex(mesh.next_halfedge(mesh.halfedge(e, 1)));

	int v1_v = mesh.valence(v1);
	int v2_v = mesh.valence(v2);
	int w1_v = mesh.valence(w1);
	int w2_v = mesh.valence(w2);

	int iv1 = ideal_valence(mesh, v1);
	int iv2 = ideal_valence(mesh, v2);
	int iw1 = ideal_valence(mesh, w1);
	int iw2 = ideal_valence(mesh, w2);

    // Calculate difference from the ideal valence
	int before = (v1_v - iv1) * (v1_v - iv1) + 
			(v2_v - iv2) * (v2_v - iv2) + 
			(w1_v - iw1) * (w1_v - iw1) + 
			(w2_v - iw2) * (w2_v - iw2);

    // And for the flipped edge
	int after = (v1_v - 1 - iv1) * (v1_v - 1 - iv1) + 
			 (v2_v - 1 - iv2) * (v2_v - 1 - iv2) + 
			 (w1_v + 1 - iw1) * (w1_v + 1 - iw1) + 
			 (w2_v + 1 - iw2) * (w2_v + 1 - iw2);

    // And compare
	return before - after;
}

double Remesher::smooth_score(Vertex v) {
    if (mesh.is_deleted(v)) return -1.0;
	if (mesh.is_boundary(v)) return -1.0;

	vec3 step = compute_smooth_step(v);
    if (pmp::norm(step) == 0.0) return -1.0;
	Point p_new = mesh.position(v) + step;

    double before = loss::single_vertex_loss(mesh, target_length, v);
    double after = 0.0;
    for (auto v_n : mesh.vertices(v)) {
        Point p_n = mesh.position(v_n);

        after += loss::get_edge_loss_from_length(pmp::distance(p_new, p_n), target_length);
    }
    after /= mesh.valence(v);
	double score = before - after;
    if(score < op_gain_threshold) return -1.0;
	return score;
}

// Base splitting operation
bool Remesher::split_edge(Edge e) {
    if (mesh.is_deleted(e)) return false;
    if (edge_length(mesh, e) <= target_length * l_max) return false;

    Vertex v0 = mesh.vertex(e, 0);
    Vertex v1 = mesh.vertex(e, 1);

    Point p0 = mesh.position(v0);
    Point p1 = mesh.position(v1);

    auto p_mid = 0.5 * (p0 + p1);
    auto v_mid = mesh.add_vertex(p_mid);

    mesh.split(e, v_mid);
    return true;
}

// If one of the vertices is on the boundary, we collapse into the boundary vertex.
// Otherwise, we would be pulling the boundary inwards, changing the shape drastically.
bool Remesher::get_collapse_info(Edge e, Halfedge& h, Point& new_pos) {
    h = mesh.halfedge(e, 0);
    if (mesh.is_boundary(mesh.from_vertex(h)) && !mesh.is_boundary(mesh.to_vertex(h))) {
        h = mesh.halfedge(e, 1);
    }
    if (!mesh.is_collapse_ok(h)) return false;

    Vertex v_from = mesh.from_vertex(h);
    Vertex v_to = mesh.to_vertex(h);
    new_pos = (mesh.is_boundary(v_to)) ? mesh.position(v_to) : 0.5 * (mesh.position(v_from) + mesh.position(v_to));
    return true;
}

// base collapsing operation
bool Remesher::collapse_edge(Edge e) {
    if (mesh.is_deleted(e)) return false;
    if (edge_length(mesh, e) >= target_length * l_min) return false;

    Halfedge h; Point new_pos;
    if (!get_collapse_info(e, h, new_pos)) return false;

    Vertex v_from = mesh.from_vertex(h);
    Vertex v_to = mesh.to_vertex(h);

    // Check if the collapse would create a long edge, which would result in a loop
    bool creates_long_edge = false;
    for(auto v_n : mesh.vertices(v_from)) {
        if (v_n == v_to) continue;
        if (pmp::distance(new_pos, mesh.position(v_n)) > target_length * l_max) {
            creates_long_edge = true;
            break;
        }
    }

    if(!creates_long_edge) {
        for(auto v_n : mesh.vertices(v_to)) {
            if (v_n == v_from) continue;
            if (pmp::distance(new_pos, mesh.position(v_n)) > target_length * l_max) {
                creates_long_edge = true;
                break;
            }
        }
    }

    if (creates_long_edge) return false;
    mesh.position(v_to) = new_pos;
    mesh.collapse(h);
    return true;
}

// Base flipping operation
bool Remesher::flip_edge(Edge e) {
    if(flip_score(e) > 0.0) {
        mesh.flip(e);
        return true;
    }
    return false;
}

//Based off https://stanford-cs248.github.io/Cardinal3D/meshedit/global/remesh/
//Didn't add the part where the vertexes get moved gently
vec3 Remesher::compute_smooth_step(Vertex v) const {
    if (mesh.is_deleted(v)) return vec3(0, 0, 0);
    if (mesh.is_boundary(v)) return vec3(0, 0, 0);
    if (mesh.valence(v) == 0) return vec3(0, 0, 0);

    Point center(0, 0, 0);
    for(auto vn : mesh.vertices(v)){
        center += mesh.position(vn);
    }
    center /= mesh.valence(v);

    vec3 dir = center - mesh.position(v);
    Normal normal = vertex_normal(mesh, v);
    vec3 tangential = dir - pmp::dot(dir, normal) * normal;

    double scale = 1.0; // Implement gentle moving at some point?
    return tangential * scale;
}

// Base smoothing operation (for single vertex, not entire mesh)
bool Remesher::smooth_vertex(Vertex v) {
    vec3 step = compute_smooth_step(v);
    if (pmp::norm(step) == 0.0) return false;
    mesh.position(v) += step;
    return true;
}

void Remesher::enqueue_candidate(OpQueue& pq, OpCandidate cand) {
	switch (cand.type) {
		case OpType::Split:
			cand.score = split_score(cand.e);
			break;
		case OpType::Collapse:
			cand.score = collapse_score(cand.e);
			break;
		case OpType::Flip:
			cand.score = flip_score(cand.e);
			break;
		case OpType::Smooth:
			cand.score = smooth_score(cand.v);
			break;
	}
	if (cand.score >= op_gain_threshold) pq.push(cand);
}

void Remesher::single_iteration() {
    metrics.split_count = split_long_edges();
	metrics.collapse_count = collapse_short_edges();
	metrics.flip_count = flip_edges();
	metrics.smooth_count = smooth_vertices();
}

void Remesher::set_metrics() {
    metrics.total_edge_loss = loss::calc_total_edge_loss(mesh, target_length);
    metrics.volume_ratio = volume_ratio(original_mesh, mesh);
    metrics.vertex_count = mesh.n_vertices();
    metrics.edge_count = mesh.n_edges();
    metrics.face_count = mesh.n_faces();
}

void Remesher::timed_iteration(){
    auto start = std::chrono::high_resolution_clock::now();
    single_iteration();
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    metrics.time_ms = elapsed.count() * 1000;
    set_metrics();
    mesh.garbage_collection();
}

bool Remesher::converged(double prev_loss) {
    return prev_loss >= 0.0 && std::abs(prev_loss - metrics.total_edge_loss) < op_gain_threshold;
}

void Remesher::remesh(bool run_until_converged){
    int max_iters = (run_until_converged ? 100 : iterations);
    double prev_loss = -1.0;
    for(int i = 0; i < max_iters; i++) {
        timed_iteration();
        if (progress_callback) progress_callback(i + 1, metrics);
        if (run_until_converged) {
            if (converged(prev_loss)) {
                break; // Loss stabilized, exit early
            }
            prev_loss = metrics.total_edge_loss;
        }
    }
}

} //namespace ba