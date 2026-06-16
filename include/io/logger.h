#pragma once

#include <fstream>
#include <string>

#include "core/types.h"

namespace ba::io {

class Logger {
private:
	std::ofstream csv_file;
	std::string file_path;

public:
	/**
	 * \brief Constructor. Opens the file and automatically writes the CSV header row.
	 * \param filepath The path where the CSV will be saved (e.g., "data/logs/standard_run.csv")
	 */
	explicit Logger(const std::string &filepath);

	/**
	 * \brief Destructor. Ensures the file stream is safely closed when the Logger goes out of scope.
	 */
	~Logger();

	/**
	 * \brief Writes a single row of metrics to the CSV file.
	 * \param state The metrics and queue state gathered from the current iteration.
	 */
	void log_iteration(const ProgressState &state);
};

} // namespace ba::io
