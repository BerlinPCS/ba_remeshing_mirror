#pragma once

#include "core/types.h"

namespace ba {

/**
 * \brief Gets the length of a single edge
 *
 * \return The length value as a double.
 */
double edge_length(const Mesh &mesh, Edge e);

/**
 * \brief Calculates the average edge length in the mesh
 *
 * \return The average length value as a double.
 */
double avg_edge_length(const Mesh &mesh);

/**
 * \brief Gets the unnormalized normal of a face.
 *
 * \return The area-weighted face normal.
 */
Normal face_normal(const Mesh &mesh, Face f);

/**
 * \brief Gets the normal of a Vertex v based on the surrounding face normals (weighted).
 *
 * \return The normal as a Normal.
 */
Normal vertex_normal(const Mesh &mesh, Vertex v);

/**
 * \brief Gets the ideal valence of a Vertex v.
 *
 * This is 6 for interior vertices and 4 for boundary vertices.
 *
 * \return The ideal valence as an integer.
 */
int ideal_valence(const Mesh &mesh, Vertex v);

/**
 * \brief Gets the volume of a Mesh.
 *
 * \return The volume as a double.
 */
double get_mesh_volume(const Mesh &mesh);

/**
 * \brief Calculates the ratio of volumes between two meshes.
 * mesh2 / mesh1
 * \return The volume ratio as a double.
 */
double volume_ratio(const Mesh &mesh1, const Mesh &mesh2);

/**
 * \brief Get the halfedge and new position for a collapse operation
 *         Collapses into a boundary vertex if possible, otherwise into midpoint of edge
 */
void get_collapse_info(const Mesh &mesh, Edge e, Halfedge &h, Point &new_pos);

/**
 * \brief Checks if a collapse is valid and would not create a long edge
 */
bool is_collapse_valid(const Mesh &mesh, Edge e, Halfedge &h, Point &new_pos, double target_length);

/**
 * \brief Computed the Vector which a vertex is moved by in the smoothing step
 * \return The update vector
 */
vec3 compute_smooth_step(const Mesh &mesh, Vertex v);

} // namespace ba
