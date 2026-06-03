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
        int iterations = 5;
        int flip_frequency = 5;
        bool separate_flip_queue = true;
        float op_gain_threshold = 1e-5f;

        // Logging & Progress
        bool logging = true, vtk_export = false;
        int logs = 0;
        std::unique_ptr<io::Logger> logger;
        std::atomic<bool> is_remeshing{false};
        std::atomic<int> current_progress_ops{0};
        std::atomic<int> current_queue_size{0};
        std::atomic<double> current_progress_loss{0.0};
        int log_frequency = 0;
        Metrics metrics;

        /**
         * Reset UI state and load a mesh 
         */
        void reset(const std::string& filepath = "");

        void log(Metrics metrics, bool initial_log = false);

        /**
         * \brief Callback function for the ImGui UI. 
         */
        void draw_ui();
        void draw_mesh_control();
        void draw_remesh_control();
        void draw_prio_control();
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