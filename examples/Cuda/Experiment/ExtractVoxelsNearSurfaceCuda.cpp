//
// Created by wei on 4/4/19.
//

#include <Cuda/Experiment/ScalableTSDFVolumeProcessorCuda.h>
#include <Cuda/Open3DCuda.h>
#include <Open3D/Open3D.h>

#include "../ReconstructionSystem/DatasetConfig.h"

using namespace open3d;
using namespace open3d::registration;
using namespace open3d::geometry;
using namespace open3d::io;
using namespace open3d::utility;

void ReadAndComputeGradient(int fragment_id, DatasetConfig &config) {
    PoseGraph pose_graph;
    ReadPoseGraph(config.GetPoseGraphFileForFragment(fragment_id, true),
                  pose_graph);

    float voxel_length = config.tsdf_cubic_size_ / 512.0;

    Timer timer;
    timer.Start();

    std::string filename = config.GetBinFileForFragment(fragment_id);
    auto tsdf_volume = io::ReadScalableTSDFVolumeFromBIN("target.bin");
    timer.Stop();
    utility::LogInfo("Read takes {} ms\n", timer.GetDuration());

    utility::SetVerbosityLevel(utility::VerbosityLevel::Debug);
    tsdf_volume.GetAllSubvolumes();
    cuda::ScalableMeshVolumeCuda mesher(
            cuda::VertexWithNormalAndColor, 8,
            tsdf_volume.active_subvolume_entry_array_.size());
    mesher.MarchingCubes(tsdf_volume);
    auto mesh = mesher.mesh().Download();
    visualization::DrawGeometries({mesh});

    cuda::ScalableTSDFVolumeProcessorCuda gradient_volume(
            8, tsdf_volume.active_subvolume_entry_array_.size());
    gradient_volume.ComputeGradient(tsdf_volume);

    cuda::PointCloudCuda pcl =
            gradient_volume.ExtractVoxelsNearSurface(tsdf_volume, 0.5f);
    auto pcl_cpu = pcl.Download();

    visualization::DrawGeometries({pcl_cpu});
}

int main(int argc, char **argv) {
    DatasetConfig config;
    std::string config_path =
            argc > 1 ? argv[1]
                     : kDefaultDatasetConfigDir + "/stanford/lounge.json";
    bool is_success = io::ReadIJsonConvertible(config_path, config);
    if (!is_success) return 1;
    config.GetFragmentFiles();

    for (int i = 0; i < config.fragment_files_.size(); ++i) {
        utility::LogInfo("{}\n", i);
        ReadAndComputeGradient(i, config);
    }
}
