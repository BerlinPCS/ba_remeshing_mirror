#include <gtest/gtest.h>
#include <pmp/io/io.h>

#include "remesher/evaluation_strategy.h"
#include "remesher/loss.h"
#include "remesher/remeshers/remesher_prio_global.h"
#include <array>
#include <string>
#include <vector>

namespace ba {
namespace {

struct SplitMesh {
	Mesh mesh;
	Edge center;
};

SplitMesh make_split_mesh() {
	SplitMesh result;
	const Vertex v0 = result.mesh.add_vertex(Point(-1.5, 0.0, 0.0));
	const Vertex v1 = result.mesh.add_vertex(Point(1.5, 0.0, 0.0));
	const Vertex top = result.mesh.add_vertex(Point(0.0, 1.0, 0.0));
	const Vertex bottom = result.mesh.add_vertex(Point(0.0, -1.0, 0.0));
	result.mesh.add_triangle(v0, v1, top);
	result.mesh.add_triangle(v1, v0, bottom);
	result.center = result.mesh.find_edge(v0, v1);
	return result;
}

SplitMesh make_flip_mesh() {
	SplitMesh result;
	const Vertex v0 = result.mesh.add_vertex(Point(-2.0, 0.0, 0.0));
	const Vertex v1 = result.mesh.add_vertex(Point(2.0, 0.0, 0.0));
	const Vertex top = result.mesh.add_vertex(Point(0.0, 1.0, 0.0));
	const Vertex bottom = result.mesh.add_vertex(Point(0.0, -1.0, 0.0));
	result.mesh.add_triangle(v0, v1, top);
	result.mesh.add_triangle(v1, v0, bottom);
	result.center = result.mesh.find_edge(v0, v1);
	return result;
}

void expect_no_accepted_energy_operations(const Mesh &mesh, RemesherSettings &settings) {
	EvaluationStrategy evaluator(settings);
	for (auto e : mesh.edges()) {
		EXPECT_FALSE(evaluator.split(mesh, e).accepted);
		EXPECT_FALSE(evaluator.collapse(mesh, e).accepted);
		EXPECT_FALSE(evaluator.flip(mesh, e).accepted);
	}
	for (auto v : mesh.vertices()) EXPECT_FALSE(evaluator.smooth(mesh, v).accepted);
}

TEST(EvaluationStrategyTest, SplitModesChangePriorityButNotAcceptanceEnergy) {
	SplitMesh input = make_split_mesh();
	RemesherSettings settings;
	settings.target_length = 1.0f;
	settings.op_gain_threshold = 1e-6f;
	EvaluationStrategy evaluator(settings);

	settings.split = SplitMode::SUM;
	const OperationEvaluation sum = evaluator.split(input.mesh, input.center);
	settings.split = SplitMode::MAX;
	const OperationEvaluation max = evaluator.split(input.mesh, input.center);
	settings.split = SplitMode::AVG;
	const OperationEvaluation avg = evaluator.split(input.mesh, input.center);

	EXPECT_TRUE(sum.accepted);
	EXPECT_DOUBLE_EQ(sum.edge_loss_gain, max.edge_loss_gain);
	EXPECT_DOUBLE_EQ(sum.edge_loss_gain, avg.edge_loss_gain);
	EXPECT_NE(sum.priority, max.priority);
	EXPECT_NE(sum.priority, avg.priority);
	EXPECT_NE(max.priority, avg.priority);
}

TEST(EvaluationStrategyTest, EdgeLossFlipUsesComparableEnergyGain) {
	SplitMesh input = make_flip_mesh();
	RemesherSettings settings;
	settings.target_length = 2.0f;
	settings.op_gain_threshold = 1e-6f;
	settings.flip = FlipMode::EDGE_LOSS;
	EvaluationStrategy evaluator(settings);

	const OperationEvaluation evaluation = evaluator.flip(input.mesh, input.center);

	EXPECT_TRUE(evaluation.valid);
	EXPECT_TRUE(evaluation.accepted);
	EXPECT_GT(evaluation.edge_loss_gain, 0.0);
	EXPECT_DOUBLE_EQ(evaluation.priority, evaluation.edge_loss_gain);
}

TEST(GlobalRemesherTest, HighThresholdExecutesNoEnergyOperations) {
	SplitMesh input = make_split_mesh();
	RemesherSettings settings;
	settings.target_length = 1.0f;
	settings.op_gain_threshold = 100.0f;
	settings.flip = FlipMode::EDGE_LOSS;
	SyncState<ProgressState> progress;
	RemesherPrioGlobal remesher(input.mesh, settings, progress);

	remesher.remesh();
	const ProgressState state = progress.load();

	EXPECT_EQ(state.metrics.operations, 0);
	EXPECT_EQ(state.termination_reason, TerminationReason::EmptyQueues);
}

TEST(GlobalRemesherTest, EdgeLossModeDoesNotIncreaseTotalEnergy) {
	SplitMesh input = make_split_mesh();
	RemesherSettings settings;
	settings.target_length = 1.0f;
	settings.op_gain_threshold = 1e-4f;
	settings.flip = FlipMode::EDGE_LOSS;
	SyncState<ProgressState> progress;
	const double before = loss::calc_total_edge_loss(input.mesh, settings.target_length);
	RemesherPrioGlobal remesher(input.mesh, settings, progress);
	settings.progress_frequency = 1;
	std::vector<double> observed_losses = {before};
	remesher.set_progress_callback(
		[&](bool) { observed_losses.push_back(loss::calc_total_edge_loss(input.mesh, settings.target_length)); });

	remesher.remesh();
	const double after = loss::calc_total_edge_loss(input.mesh, settings.target_length);

	EXPECT_LE(after, before + 1e-8);
	EXPECT_EQ(progress.load().termination_reason, TerminationReason::EmptyQueues);
	for (size_t i = 1; i < observed_losses.size(); ++i) {
		EXPECT_LE(observed_losses[i], observed_losses[i - 1] + 1e-8);
	}
	EXPECT_GT(progress.load().queue_stats.stale, 0);
	expect_no_accepted_energy_operations(input.mesh, settings);
}

TEST(GlobalRemesherIntegrationTest, DISABLED_BunniesCompleteWithMaxAndAvgPriority) {
	const std::array<std::string, 2> fixtures = {"stanford-bunny-low.obj", "stanford-bunny.obj"};
	const std::array<SplitMode, 2> modes = {SplitMode::MAX, SplitMode::AVG};

	for (const auto &fixture : fixtures) {
		for (SplitMode mode : modes) {
			Mesh mesh;
			pmp::read(mesh, std::string(BA_SOURCE_DIR) + "/data/" + fixture);
			RemesherSettings settings;
			settings.target_length = static_cast<float>(avg_edge_length(mesh));
			settings.op_gain_threshold = 1e-2f;
			settings.flip = FlipMode::EDGE_LOSS;
			settings.split = mode;
			settings.progress_frequency = 0;
			settings.log_frequency = 0;
			const double before = loss::calc_total_edge_loss(mesh, settings.target_length);
			SyncState<ProgressState> progress;
			RemesherPrioGlobal remesher(mesh, settings, progress);

			remesher.remesh();

			EXPECT_EQ(progress.load().termination_reason, TerminationReason::EmptyQueues)
				<< fixture << " mode " << static_cast<int>(mode);
			EXPECT_LE(loss::calc_total_edge_loss(mesh, settings.target_length), before + 1e-8);
			expect_no_accepted_energy_operations(mesh, settings);
		}
	}
}

TEST(GlobalRemesherTest, HighThresholdCompletesForAllSplitModesAndFixtures) {
	const std::array<std::string, 4> fixtures = {"test_split.obj", "test_collapse.obj", "test_flip.obj",
												 "test_smooth.obj"};
	const std::array<SplitMode, 3> modes = {SplitMode::SUM, SplitMode::MAX, SplitMode::AVG};

	for (const auto &fixture : fixtures) {
		for (SplitMode mode : modes) {
			Mesh mesh;
			pmp::read(mesh, std::string(BA_SOURCE_DIR) + "/data/test/" + fixture);
			RemesherSettings settings;
			settings.target_length = static_cast<float>(avg_edge_length(mesh));
			settings.op_gain_threshold = 100.0f;
			settings.flip = FlipMode::EDGE_LOSS;
			settings.split = mode;
			SyncState<ProgressState> progress;
		    RemesherPrioGlobal remesher(mesh, settings, progress);

			remesher.remesh();

			EXPECT_EQ(progress.load().termination_reason, TerminationReason::EmptyQueues)
				<< fixture << " mode " << static_cast<int>(mode);
		}
	}
}

} // namespace
} // namespace ba
