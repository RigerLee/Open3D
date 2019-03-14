//
// Created by wei on 2/4/19.
//

#include <vector>
#include <string>
#include <sstream>
#include <cmath>
#include <iomanip>

#include <Open3D/Open3D.h>
#include <Open3D/Registration/GlobalOptimization.h>

#include <Cuda/Odometry/RGBDOdometryCuda.h>
#include <Cuda/Integration/ScalableTSDFVolumeCuda.h>
#include <Cuda/Integration/ScalableMeshVolumeCuda.h>

#include <opencv2/opencv.hpp>

#include "examples/Cuda/DatasetConfig.h"
#include "ORBPoseEstimation.h"

using namespace open3d;

namespace MakeFragment {
void MakePoseGraphForFragment(int fragment_id, DatasetConfig &config) {

    cuda::RGBDOdometryCuda<3> odometry;
    odometry.SetIntrinsics(config.intrinsic_);
    odometry.SetParameters(odometry::OdometryOption({20, 10, 5},
                                                    config.max_depth_diff_,
                                                    config.min_depth_,
                                                    config.max_depth_), 0.968f);

    cuda::RGBDImageCuda rgbd_source((float) config.max_depth_,
                                    (float) config.depth_factor_);
    cuda::RGBDImageCuda rgbd_target((float) config.max_depth_,
                                    (float) config.depth_factor_);

    const int begin = fragment_id * config.n_frames_per_fragment_;
    const int end = std::min((fragment_id + 1) * config.n_frames_per_fragment_,
                             (int) config.color_files_.size());

    // world_to_source
    Eigen::Matrix4d trans_odometry = Eigen::Matrix4d::Identity();
    registration::PoseGraph pose_graph;
    pose_graph.nodes_.emplace_back(
        registration::PoseGraphNode(trans_odometry));

    /** Add odometry and keyframe info **/
    std::vector<ORBPoseEstimation::KeyframeInfo> keyframe_infos;
    cv::Ptr<cv::ORB> orb = cv::ORB::create(100);

    for (int s = begin; s < end; ++s) {
        geometry::Image depth, color;

        io::ReadImage(config.depth_files_[s], depth);
        io::ReadImage(config.color_files_[s], color);
        rgbd_source.Upload(depth, color);

        /** Insert a keyframe **/
        if (config.with_opencv_ && s % config.n_keyframes_per_n_frame_ == 0) {
            cv::Mat im;
            rgbd_source.intensity_.DownloadMat().convertTo(im, CV_8U, 255.0);
            std::vector<cv::KeyPoint> kp;
            cv::Mat desc;
            orb->detectAndCompute(im, cv::noArray(), kp, desc);

            ORBPoseEstimation::KeyframeInfo keyframe_info;
            keyframe_info.idx = s;
            keyframe_info.descriptor = desc;
            keyframe_info.keypoints = kp;
            keyframe_info.color = im;
            keyframe_info.depth = rgbd_source.depth_.DownloadMat();
            keyframe_infos.emplace_back(keyframe_info);
        }

        int t = s + 1;
        if (t >= end) break;

        io::ReadImage(config.depth_files_[t], depth);
        io::ReadImage(config.color_files_[t], color);
        rgbd_target.Upload(depth, color);

        utility::PrintInfo("RGBD Odometry between (%d %d)\n", s, t);
        odometry.transform_source_to_target_ = Eigen::Matrix4d::Identity();
        odometry.Initialize(rgbd_source, rgbd_target);
        odometry.ComputeMultiScale();

        Eigen::Matrix4d trans = odometry.transform_source_to_target_;
        Eigen::Matrix6d information = odometry.ComputeInformationMatrix();

        // source_to_target * world_to_source = world_to_target
        trans_odometry = trans * trans_odometry;

        // target_to_world
        Eigen::Matrix4d trans_odometry_inv = trans_odometry.inverse();

        pose_graph.nodes_.emplace_back(
            registration::PoseGraphNode(trans_odometry_inv));
        pose_graph.edges_.emplace_back(
            registration::PoseGraphEdge(
                s - begin, t - begin, trans, information, false));
    }

    /** Add Loop closures **/
    if (config.with_opencv_) {
        for (int i = 0; i < keyframe_infos.size() - 1; ++i) {
            for (int j = i + 1; j < keyframe_infos.size(); ++j) {
                int s = keyframe_infos[i].idx;
                int t = keyframe_infos[j].idx;
                utility::PrintInfo("RGBD Loop closure between (%d %d)\n", s, t);

                bool is_success;
                Eigen::Matrix4d trans_source_to_target;

                std::tie(is_success, trans_source_to_target) =
                    ORBPoseEstimation::PoseEstimation(keyframe_infos[i],
                                                      keyframe_infos[j],
                                                      config.intrinsic_);

                if (is_success) {
                    odometry.transform_source_to_target_ =
                        trans_source_to_target;

                    geometry::Image depth, color;

                    io::ReadImage(config.depth_files_[s], depth);
                    io::ReadImage(config.color_files_[s], color);
                    rgbd_source.Upload(depth, color);

                    io::ReadImage(config.depth_files_[t], depth);
                    io::ReadImage(config.color_files_[t], color);
                    rgbd_target.Upload(depth, color);

                    odometry.Initialize(rgbd_source, rgbd_target);
                    auto result = odometry.ComputeMultiScale();

                    if (std::get<0>(result)) {
                        Eigen::Matrix4d
                            trans = odometry.transform_source_to_target_;
                        Eigen::Matrix6d
                            information = odometry.ComputeInformationMatrix();

                        pose_graph.edges_.emplace_back(
                            registration::PoseGraphEdge(
                                s - begin,
                                t - begin,
                                trans,
                                information,
                                true));
                    }
                }
            }
        }
    }
    io::WritePoseGraph(config.GetPoseGraphFileForFragment(fragment_id, false),
                       pose_graph);
}

void OptimizePoseGraphForFragment(int fragment_id,
                                  DatasetConfig &config) {

    registration::PoseGraph pose_graph;
    io::ReadPoseGraph(config.GetPoseGraphFileForFragment(fragment_id, false),
                      pose_graph);

    registration::GlobalOptimizationConvergenceCriteria criteria;
    registration::GlobalOptimizationOption option(
        config.max_depth_diff_,
        0.25,
        config.preference_loop_closure_odometry_,
        0);
    registration::GlobalOptimizationLevenbergMarquardt optimization_method;
    GlobalOptimization(pose_graph, optimization_method,
                       criteria, option);

    auto pose_graph_prunned = CreatePoseGraphWithoutInvalidEdges(
        pose_graph, option);

    io::WritePoseGraph(config.GetPoseGraphFileForFragment(fragment_id, true),
                       *pose_graph_prunned);
}

void IntegrateForFragment(int fragment_id, DatasetConfig &config) {

    registration::PoseGraph pose_graph;
    io::ReadPoseGraph(config.GetPoseGraphFileForFragment(fragment_id, true),
                      pose_graph);

    float voxel_length = config.tsdf_cubic_size_ / 512.0;

    cuda::PinholeCameraIntrinsicCuda intrinsic(config.intrinsic_);
    cuda::TransformCuda trans = cuda::TransformCuda::Identity();
    cuda::ScalableTSDFVolumeCuda<8> tsdf_volume(
        20000,
        400000,
        voxel_length,
        (float) config.tsdf_truncation_,
        trans);

    cuda::RGBDImageCuda rgbd((float) config.max_depth_,
                             (float) config.depth_factor_);

    const int begin = fragment_id * config.n_frames_per_fragment_;
    const int
        end = std::min((fragment_id + 1) * config.n_frames_per_fragment_,
                       (int) config.color_files_.size());

    for (int i = begin; i < end; ++i) {
        utility::PrintDebug("Integrating frame %d ...\n", i);

        geometry::Image depth, color;
        io::ReadImage(config.depth_files_[i], depth);
        io::ReadImage(config.color_files_[i], color);
        rgbd.Upload(depth, color);

        /* Use ground truth trajectory */
        Eigen::Matrix4d pose = pose_graph.nodes_[i - begin].pose_;
        trans.FromEigen(pose);

        tsdf_volume.Integrate(rgbd, intrinsic, trans);
    }

    tsdf_volume.GetAllSubvolumes();
    cuda::ScalableMeshVolumeCuda<8> mesher(
        tsdf_volume.active_subvolume_entry_array().size(),
        cuda::VertexWithNormalAndColor, 10000000, 20000000);
    mesher.MarchingCubes(tsdf_volume);
    auto mesh = mesher.mesh().Download();

    geometry::PointCloud pcl;
    pcl.points_ = mesh->vertices_;
    pcl.normals_ = mesh->vertex_normals_;
    pcl.colors_ = mesh->vertex_colors_;

    /** Write original fragments **/
    io::WritePointCloudToPLY(config.GetPlyFileForFragment(fragment_id), pcl);

    /** Write downsampled thumbnail fragments **/
    auto pcl_downsampled = VoxelDownSample(pcl, config.voxel_size_);
    io::WritePointCloudToPLY(config.GetThumbnailPlyFileForFragment(fragment_id),
                             *pcl_downsampled);
}

static int Run(DatasetConfig &config) {
    utility::Timer timer;
    timer.Start();

    utility::filesystem::MakeDirectoryHierarchy(
        config.path_dataset_ + "/fragments_cuda/thumbnails");

    const int num_fragments =
        DIV_CEILING(config.color_files_.size(),
                    config.n_frames_per_fragment_);

    for (int i = 0; i < num_fragments; ++i) {
        utility::PrintInfo("Processing fragment %d / %d\n", i, num_fragments -
            1);
        MakePoseGraphForFragment(i, config);
        OptimizePoseGraphForFragment(i, config);
        IntegrateForFragment(i, config);
    }
    timer.Stop();
    utility::PrintInfo("MakeFragment takes %.3f s\n", timer.GetDuration() / 1000.0f);
}
};