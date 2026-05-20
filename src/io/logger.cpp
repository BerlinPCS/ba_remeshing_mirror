#include "io/logger.h"

namespace ba::io {

Logger::Logger(const std::string& filepath) : file_path(filepath) {
    std::filesystem::path path(filepath);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    csv_file.open(filepath);
    if (csv_file.is_open()) {
        csv_file << "iteration_num,time_ms,total_edge_loss,volume_ratio,vertex_count,edge_count,face_count,"
                 << "split_count,collapse_count,flip_count,smooth_count\n";
    } else {
        std::cerr << "Error: Could not open log file at " << filepath << std::endl;
    }
}

Logger::~Logger() {
    if (csv_file.is_open()) {
        csv_file.close();
    }
}

void Logger::log_iteration(const IterationMetrics& metrics) {
    if (csv_file.is_open()) {
        csv_file << metrics.iteration_num << ","
                 << metrics.time_ms << ","
                 << metrics.total_edge_loss << ","
                 << metrics.volume_ratio << ","
                 << metrics.vertex_count << ","
                 << metrics.edge_count << ","
                 << metrics.face_count << ","
                 << metrics.split_count << ","
                 << metrics.collapse_count << ","
                 << metrics.flip_count << ","
                 << metrics.smooth_count << "\n";
    }
}

} // namespace ba
