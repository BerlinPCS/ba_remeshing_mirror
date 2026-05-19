#include "io/paraview.h"

namespace ba::io {

void remove_vtks(const std::string& path) {
    if (!std::filesystem::exists(path)) {
        std::filesystem::create_directories(path);
    }
    for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
        if (entry.path().extension() == ".vtk") {
            std::filesystem::remove(entry.path());
        }
    }
}

void export_mesh_vtk(const std::string& filepath, 
                     pmp::SurfaceMesh& mesh, 
                     const std::vector<double>& vertex_losses) {
                                  
    std::filesystem::path path(filepath);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream out(filepath);
    if (!out.is_open()) {
        std::cerr << "[VTK Exporter] ERROR: Could not open file " << filepath << std::endl;
        return;
    }

    // --- 1. Write VTK Header ---
    out << "# vtk DataFile Version 3.0\n";
    out << "Isotropic Remeshing Output\n";
    out << "ASCII\n";
    out << "DATASET POLYDATA\n";

    // --- 2. Write Vertices ---
    out << "POINTS " << mesh.n_vertices() << " float\n";
    
    // Set precision for writing floats to avoid truncating geometry
    out << std::fixed << std::setprecision(6);
    
    // Create a map from the mesh's vertex indices (which can have gaps)
    // to the compact 0..N-1 indices that VTK requires.
    std::vector<int> vtk_indices(mesh.vertices_size(), -1);
    int vtk_idx = 0;
    for (auto v : mesh.vertices()) {
        auto p = mesh.position(v);
        out << p[0] << " " << p[1] << " " << p[2] << "\n";
        vtk_indices[v.idx()] = vtk_idx++;
    }

    // --- 3. Write Faces ---
    // For triangles: "3 vertex1 vertex2 vertex3"
    size_t total_integers = 0;
    for (auto f : mesh.faces()) {
        total_integers += mesh.valence(f) + 1;
    }
    out << "\nPOLYGONS " << mesh.n_faces() << " " << total_integers << "\n";
    
    for (auto f : mesh.faces()) {
        out << mesh.valence(f);
        for (auto v : mesh.vertices(f)) {
            out << " " << vtk_indices[v.idx()];
        }
        out << "\n";
    }

    // --- 4. Write Scalar Data ---
    if (!vertex_losses.empty()) {
        out << "\nPOINT_DATA " << mesh.n_vertices() << "\n";
        out << "SCALARS Edge_Loss_Entropy float 1\n";
        out << "LOOKUP_TABLE default\n";
        
        // Iterate through vertices in the same order they were written to ensure data aligns
        for (auto v : mesh.vertices()) {
            double loss = vertex_losses[v.idx()];
            if (std::isnan(loss) || std::isinf(loss)) {
                out << 0.0 << "\n";
            } else {
                out << loss << "\n";
            }
        }
    }

    out.close();
    std::cout << "[VTK Exporter] Successfully wrote: " << filepath << std::endl;
}

} // namespace ba