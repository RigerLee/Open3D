//
// Created by wei on 2/5/19.
//

#include <Open3D/Open3D.h>
#include "examples/Cuda/DatasetConfig.h"

using namespace open3d;

std::shared_ptr<geometry::LineSet> VisualizePoseGraph(
    registration::PoseGraph &pose_graph) {
    std::shared_ptr<geometry::LineSet> pose_graph_vis =
        std::make_shared<geometry::LineSet>();

    int cnt = 0;

    const int kPointsPerFrustum = 5;
    const int kEdgesPerFrustum = 8;
    for (auto &node : pose_graph.nodes_) {
        auto pose = node.pose_;

        double norm = 0.1;
        pose_graph_vis->points_.emplace_back(
            (pose * Eigen::Vector4d(0, 0, 0, 1)).hnormalized());
        pose_graph_vis->points_.emplace_back(
            (pose * (norm * Eigen::Vector4d(1, 1, 2, 1/norm))).hnormalized());
        pose_graph_vis->points_.emplace_back(
            (pose * (norm * Eigen::Vector4d(1, -1, 2, 1/norm))).hnormalized());
        pose_graph_vis->points_.emplace_back(
            (pose * (norm * Eigen::Vector4d(-1, -1, 2, 1/norm))).hnormalized());
        pose_graph_vis->points_.emplace_back(
            (pose * (norm * Eigen::Vector4d(-1, 1, 2, 1/norm))).hnormalized());

        pose_graph_vis->lines_.emplace_back(
            Eigen::Vector2i(cnt + 0, cnt + 1));
        pose_graph_vis->lines_.emplace_back(
            Eigen::Vector2i(cnt + 0, cnt + 2));
        pose_graph_vis->lines_.emplace_back(
            Eigen::Vector2i(cnt + 0, cnt + 3));
        pose_graph_vis->lines_.emplace_back(
            Eigen::Vector2i(cnt + 0, cnt + 4));
        pose_graph_vis->lines_.emplace_back(
            Eigen::Vector2i(cnt + 1, cnt + 2));
        pose_graph_vis->lines_.emplace_back(
            Eigen::Vector2i(cnt + 2, cnt + 3));
        pose_graph_vis->lines_.emplace_back(
            Eigen::Vector2i(cnt + 3, cnt + 4));
        pose_graph_vis->lines_.emplace_back(
            Eigen::Vector2i(cnt + 4, cnt + 1));

        for (int k = 0; k < kEdgesPerFrustum; ++k) {
            pose_graph_vis->colors_.emplace_back(Eigen::Vector3d(1, 0, 0));
        }

        cnt += kPointsPerFrustum;
    }

    for (auto &edge : pose_graph.edges_) {
        int s = edge.source_node_id_;
        int t = edge.target_node_id_;

        if (edge.uncertain_) {
            pose_graph_vis->lines_.emplace_back(
                Eigen::Vector2i(s * kPointsPerFrustum, t * kPointsPerFrustum));
            pose_graph_vis->colors_.emplace_back(Eigen::Vector3d(0, 1, 0));
        } else {
            pose_graph_vis->lines_.emplace_back(
                Eigen::Vector2i(s * kPointsPerFrustum, t * kPointsPerFrustum));
            pose_graph_vis->colors_.emplace_back(Eigen::Vector3d(0, 0, 1));
        }
    }

    return pose_graph_vis;
}

int main(int argc, char **argv) {
    SetVerbosityLevel(utility::VerbosityLevel::VerboseDebug);

    DatasetConfig config;

    std::string config_path = argc > 1 ? argv[1] :
        kDefaultDatasetConfigDir + "/cmu/nsh.json";

    bool is_success = io::ReadIJsonConvertible(config_path, config);
    if (! is_success) return 1;

    registration::PoseGraph pose_graph_cuda;
    std::string path_cuda = config.GetPoseGraphFileForRefinedScene(true);
    io::ReadPoseGraph(path_cuda, pose_graph_cuda);

    auto pose_graph_vis_cuda = VisualizePoseGraph(pose_graph_cuda);
    auto mesh = io::CreateMeshFromFile(config.GetReconstructedSceneFile());

    visualization::DrawGeometries({pose_graph_vis_cuda, mesh});

    return 0;
}