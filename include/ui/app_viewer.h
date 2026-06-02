#pragma once

#include <memory>
#include <string>
#include <vector>
#include <atomic>

#include "core/types.h"
#include "remesher/remesher.h"
#include "io/logger.h"


#include <pmp/io/io.h>
#include "polyscope/polyscope.h"

namespace ba::ui {

    class AppViewer {
    private:
        // Application State 
        Mesh mesh;
        std::unique_ptr<Remesher> remesher;
        const std::vector<std::string> remesher_names = {"Standard", "Priority Local", "Priority Global"};
        const std::vector<std::string> strategy_names = {"Split Sum", "Split Max", "Split Avg"};
        std::string path_to_data;
        std::vector<std::string> file_paths;
        std::vector<std::string> mesh_names;
        std::string current_file_name;

        // Remesher State
        int selected_mesh = 0;
        int remesher_type = BASE;
        int split_strategy = SPLIT_SUM;
        bool show_vertex_loss = false;
        bool run_until_converged = false;
        int iterations = 5;
        int flip_frequency = 5;

        // Logging
        bool logging = true, vtk_export = false;
        std::unique_ptr<io::Logger> logger;
        std::atomic<int> current_total_iters{0};
        std::atomic<bool> is_remeshing{false};
        std::atomic<int> current_progress_iter{0};
        std::atomic<int> total_progress_iters{100};
        std::atomic<double> current_progress_loss{0.0};
        IterationMetrics metrics;

        /**
         * Reset UI state and load a mesh 
         */
        void reset(const std::string& filepath = "");

        void log(IterationMetrics metrics, bool initial_log = false);

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