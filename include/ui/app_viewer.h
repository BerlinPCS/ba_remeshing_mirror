#pragma once

#include <memory>
#include <string>
#include <vector>

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
        const std::vector<std::string> split_mode_names = {"Sum", "Max", "Avg"};
        const std::vector<std::string> collapse_mode_names = {"Loss"};
        const std::vector<std::string> flip_mode_names = {"Valence", "Edge Length"};
        std::vector<std::string> file_paths;
        std::vector<std::string> mesh_names;
        std::string current_file_name;
        int selected_mesh = 0;

        // Remesher Settings
        RemesherSettings r_ctx;
        RemesherType r_type = RemesherType::BASE;

        // Logging & Progress
        LoggingState l_ctx;
        std::unique_ptr<io::Logger> logger;
        SyncState<ProgressState> p_ctx;
        void log(bool initial_log = false);

        // Reset Polyscope State
        void reset(const std::string& filepath = "");

        // UI Rendering
        void draw_ui();
        void draw_mesh_control();
        void draw_remesh_control();
        void draw_prio_control();
        void draw_visualization_control();
        void condition_updates();


    public:
        void init();
        void run() { polyscope::show(); };
    };

} // namespace ui