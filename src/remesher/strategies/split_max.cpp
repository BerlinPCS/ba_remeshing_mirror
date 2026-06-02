#include "remesher/strategies/split_max.h"
#include "remesher/loss.h"
#include "core/geo_utils.h"
#include "core/types.h"

namespace ba {

double SplitMaxEvaluation::split_score(const Mesh& mesh, Edge e) {
    if (mesh.is_deleted(e)) return -1.0;
    if (edge_length(mesh, e) <= target_length * L_MAX) return -1.0;

    // The new vertex v_mid would be created at the center of the edge e
	Vertex v0 = mesh.vertex(e, 0);
	Vertex v1 = mesh.vertex(e, 1);
    Point p_mid = 0.5 * (mesh.position(v0) + mesh.position(v1));

    // The two vertices opposite of the edge e
    Halfedge h0 = mesh.halfedge(e, 0);
	Halfedge h1 = mesh.halfedge(e, 1);
    Point p2 = mesh.position(mesh.to_vertex(mesh.next_halfedge(h0)));
    Point p3 = mesh.position(mesh.to_vertex(mesh.next_halfedge(h1)));

    double length = edge_length(mesh, e);
    // Loss of the single edge within the two trianges
	double before = loss::get_edge_loss_from_length(length, target_length);
    // Maximum loss of the 4 new edges
	double max_after = loss::get_edge_loss_from_length(0.5 * length, target_length);
    if (!mesh.is_boundary(h0)) max_after = std::max(max_after, loss::get_edge_loss_from_length(pmp::distance(p2, p_mid), target_length));
    if (!mesh.is_boundary(h1)) max_after = std::max(max_after, loss::get_edge_loss_from_length(pmp::distance(p3, p_mid), target_length));

    double score = before - max_after;
    return (score >= op_gain_threshold) ? score : -1.0;
}

} // namespace ba