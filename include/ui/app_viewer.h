#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_set>
#include <atomic>
#include <filesystem>
#include <thread>

#include "core/types.h"
#include "remesher/remesher.h"

#include <pmp/io/io.h>
#include "polyscope/polyscope.h"
#include "portable-file-dialogs.h"

namespace ba::ui {

    class AppViewer {
    private:
        // Application State 
        Mesh mesh;
        ps::SurfaceMesh* mesh_ps;
        std::unique_ptr<Remesher> remesher;
        std::string path_to_data;
        std::vector<std::string> file_paths;
        std::vector<std::string> mesh_names;
        int selected_mesh = 0;
        bool show_vertex_loss = false, has_vertex_loss = false;
        
        std::atomic<bool> is_remeshing{false};
        std::atomic<bool> remesh_finished{false};
        std::atomic<int> current_progress_iter{0};
        std::atomic<int> total_progress_iters{100};
        std::atomic<double> current_progress_loss{0.0};

        /**
         * \brief Loads a mesh based on a .obj, .stl, or .off file. 
         * Creates a new remesher instance for the loaded mesh.
         * 
         * \param filepath The path to the mesh file.
         * \returns A pointer to the remesher instance for inline operations
         */
        void load_mesh(const std::string& file_path);

        /**
         * \brief Loads the given pmp::SurfaceMesh into Polyscope.
         * Converts the halfedge data structure to the format expected by Polyscope.
         * 
         * \param mesh_name The name to be given to the mesh in Polyscope
         * \returns A pointer to the registered Polyscope mesh for inline operations
         */
        ps::SurfaceMesh* update_polyscope();

        /**
         * Adds a scalar quantity to the mesh representing the loss associated with each vertex.
         * This is averaged over the edge loss of all edges incident to the vertex.
         */
        void add_vertex_loss();

        /**
         * \brief Callback function for the ImGui UI. 
         * This is where all Elements get drawn
         */
        void draw_ui();

    public:
        /**
         * \brief Constructor for UI
         * \param path_to_data Path to the directory containing .off files. Defaults to "./data"
         */
        AppViewer(std::string path_to_data = "../../data") : path_to_data(path_to_data) {}

        /**
         * \brief Initializes Polyscope, registers the point cloud, and sets the callback
         */
        void init();

        /**
         * \brief Starts main loop
         */
        void run() { polyscope::show(); };
    };

} // namespace ui