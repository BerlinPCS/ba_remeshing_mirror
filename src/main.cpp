#include "args/args.hxx"
#include "ui/app_viewer.h"

int main(int argc, char **argv) {
	// Configure the argument parser
	args::ArgumentParser parser("Computer Graphics 2 Exercises");
	try {
		parser.ParseCLI(argc, argv);
	} catch (const args::Help &) {
		std::cout << parser;
		return 0;
	} catch (const args::ParseError &e) {
		std::cerr << e.what() << std::endl;

		std::cerr << parser;
		return 1;
	}

	ba::ui::AppViewer viewer = ba::ui::AppViewer();
	viewer.init();
	viewer.run();

	return 0;
}