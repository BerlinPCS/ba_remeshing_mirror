#pragma once

#include <memory>
#include <algorithm>
#include <string>
#include <sstream>
#include <vector>
#include <unordered_set>
#include <atomic>
#include <filesystem>
#include <thread>

#include "core/types.h"
#include "remesher/remesher.h"
#include "io/paraview.h"
#include "io/logger.h"
#include "remesher/loss.h"

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
        std::string current_file_name;
        int selected_mesh = 0;
        bool show_vertex_loss = false;
        bool logging = true, run_until_converged = false;
        std::unique_ptr<io::Logger> logger;
        std::atomic<int> current_total_iters{0};
        
        std::atomic<bool> is_remeshing{false};
        std::atomic<bool> remesh_finished{false};
        std::atomic<int> current_progress_iter{0};
        std::atomic<int> total_progress_iters{100};
        std::atomic<double> current_progress_loss{0.0};
        IterationMetrics metrics;

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
         * Reset UI state and load a mesh 
         */
        void reset(const std::string& filepath = "");

        void log(bool initial_log = false);

        /**
         * \brief Callback function for the ImGui UI. 
         */
        void draw_ui();
        void draw_mesh_control();
        void draw_remesh_control();
        void draw_visualization_control();
        void condition_updates(); // update output based on current ui state


    public:
        /**
         * \brief Constructor for UI
         * \param path_to_data Path to the directory containing .off files. Defaults to "./data"
         */
        AppViewer(std::string path_to_data = DATA_DIR) : path_to_data(path_to_data) {}

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