//
// Created by wei on 2/4/19.
//

#include <vector>
#include <string>
#include <Core/Core.h>
#include <IO/IO.h>

#include <Cuda/Registration/RegistrationCuda.h>
#include <Cuda/Registration/ColoredICPCuda.h>
#include <Cuda/Registration/FastGlobalRegistrationCuda.h>

#include <Core/Registration/PoseGraph.h>
#include <Core/Registration/GlobalOptimization.h>

#include "DatasetConfig.h"

using namespace open3d;

namespace RegisterFragments {
std::vector<Match> MatchFragments(DatasetConfig &config) {
    std::vector<Match> matches;

    for (int s = 0; s < config.fragment_files_.size() - 1; ++s) {
        auto source = CreatePointCloudFromFile(config.fragment_files_[s]);

        PoseGraph pose_graph_s;
        ReadPoseGraph(config.GetPoseGraphFileForFragment(s, true),
                      pose_graph_s);

        auto rbegin = pose_graph_s.nodes_.rbegin();
        Eigen::Matrix4d init_source_to_target = rbegin->pose_.inverse();

        for (int t = s + 1; t < config.fragment_files_.size(); ++t) {
            auto target = CreatePointCloudFromFile(config.fragment_files_[t]);

            Match match;
            match.s = s;
            match.t = t;

            if (t == s + 1) {
                cuda::RegistrationCuda registration(
                    TransformationEstimationType::ColoredICP);
                registration.Initialize(*source, *target,
                                        (float) config.voxel_size_ * 1.4f,
                                        init_source_to_target);
                registration.ComputeICP();
                match.trans_source_to_target =
                    registration.transform_source_to_target_;
                match.information = registration.ComputeInformationMatrix();
                match.success = true;
                PrintDebug("Pair (%d %d) odometry computed.\n",
                           match.s, match.t);
            }
            else {
                cuda::FastGlobalRegistrationCuda fgr;
                fgr.Initialize(*source, *target);

                auto result = fgr.ComputeRegistration();
                match.trans_source_to_target = result.transformation_;

                /**!!! THIS SHOULD BE REFACTORED !!!**/
                cuda::RegistrationCuda registration(
                    TransformationEstimationType::PointToPoint);
                auto source_copy = *source;
                source_copy.Transform(result.transformation_);
                registration.Initialize(source_copy, *target,
                                        config.voxel_size_ * 1.4f);
                registration.transform_source_to_target_ =
                    result.transformation_;
                match.information = registration.ComputeInformationMatrix();

                match.success = match.trans_source_to_target.trace() != 4.0
                    && match.information(5, 5) /
                        std::min(source->points_.size(),
                                 target->points_.size()) >= 0.3;
                if (match.success) {
                    PrintDebug("Pair (%d %d) registration computed.\n",
                               match.s, match.t);
                } else {
                    PrintDebug("Skip (%d %d) registration.\n",
                               match.s, match.t);
                }
            }
            matches.push_back(match);
        }
    }

    return matches;
}

void MakePoseGraphForScene(
    const std::vector<Match> &matches, DatasetConfig &config) {
    PoseGraph pose_graph;

    /* world_to_frag_0 */
    Eigen::Matrix4d trans_odometry = Eigen::Matrix4d::Identity();
    pose_graph.nodes_.emplace_back(PoseGraphNode(trans_odometry));

    for (auto &match : matches) {
        if (!match.success) continue;
        if (match.t == match.s + 1) {
            /* world_to_frag_i */
            trans_odometry = match.trans_source_to_target * trans_odometry;
            auto trans_odometry_inv = trans_odometry.inverse();

            pose_graph.nodes_.emplace_back(PoseGraphNode(trans_odometry_inv));
            pose_graph.edges_.emplace_back(PoseGraphEdge(
                match.s, match.t,
                match.trans_source_to_target, match.information,
                false));
        } else {
            pose_graph.edges_.emplace_back(PoseGraphEdge(
                match.s, match.t,
                match.trans_source_to_target, match.information,
                true));
        }
    }

    WritePoseGraph(config.GetPoseGraphFileForScene(false), pose_graph);
}

void OptimizePoseGraphForScene(DatasetConfig &config) {

    PoseGraph pose_graph;
    ReadPoseGraph(config.GetPoseGraphFileForScene(false), pose_graph);

    GlobalOptimizationConvergenceCriteria criteria;
    GlobalOptimizationOption option(
        config.voxel_size_ * 1.4, 0.25,
        config.preference_loop_closure_registration_, 0);
    GlobalOptimizationLevenbergMarquardt optimization_method;
    GlobalOptimization(pose_graph, optimization_method,
                       criteria, option);

    auto pose_graph_prunned = CreatePoseGraphWithoutInvalidEdges(
        pose_graph, option);

    WritePoseGraph(config.GetPoseGraphFileForScene(true),
                   *pose_graph_prunned);
}

int Run(DatasetConfig &config) {
    SetVerbosityLevel(VerbosityLevel::VerboseDebug);

    Timer timer;
    timer.Start();
    filesystem::MakeDirectory(config.path_dataset_ + "/scene_cuda");

    bool is_success = config.GetFragmentFiles();
    if (! is_success) {
        PrintError("Unable to get fragment files\n");
        return -1;
    }

    auto matches = MatchFragments(config);
    MakePoseGraphForScene(matches, config);
    OptimizePoseGraphForScene(config);
    timer.Stop();
    PrintInfo("RegisterFragments takes %.3f s\n",
              timer.GetDuration() / 1000.0f);
    return 0;
}
};