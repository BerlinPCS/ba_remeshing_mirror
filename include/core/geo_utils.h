#pragma once

#include <vector>
#include "core/types.h"

namespace ba {

/**
 * \brief Gets the length of a single edge 
 *
 * \return The length value as a double.
 */
double edge_length(const Mesh& mesh, Edge e);

/**
 * \brief Calculates the average edge length in the mesh
 *
 * \return The average length value as a double.
 */
double avg_edge_length(const Mesh& mesh);

/**
 * \brief Gets the area of a Face f. 
 *
 * \return The area as a double.
 */
Normal face_normal(const Mesh& mesh, Face f);

/**
 * \brief Gets the normal of a Vertex v based on the surrounding face normals (weighted). 
 *
 * \return The normal as a Normal.
 */
Normal vertex_normal(const Mesh& mesh, Vertex v);

/**
 * \brief Gets the ideal valence of a Vertex v. 
 * 
 * This is 6 for interior vertices and 4 for boundary vertices.
 *
 * \return The ideal valence as an integer.
 */
int ideal_valence(const Mesh& mesh, Vertex v);

/**
 * \brief Gets the volume of a Mesh.
 *
 * \return The volume as a double.
 */
double get_mesh_volume(const Mesh& mesh);

/**
 * \brief Calculates the ratio of volumes between two meshes.
 * mesh2 / mesh1
 * \return The volume ratio as a double.
 */
double volume_ratio(const Mesh& mesh1, const Mesh& mesh2);

} // namespace ba::core