#include <fmt/format.h>

#include "open3d/Open3D.h"
#include "open3d/t/geometry/Image.h"
#include "open3d/t/geometry/PointCloud.h"
#include "open3d/t/geometry/VoxelGrid.h"
#include "open3d/utility/Console.h"

using namespace open3d;
using namespace open3d::core;

template <class T, int M, int N, int A>
Tensor FromEigen(const Eigen::Matrix<T, M, N, A>& matrix) {
    Dtype dtype = Dtype::FromType<T>();
    Eigen::Matrix<T, M, N, Eigen::RowMajor> matrix_row_major = matrix;
    return Tensor(matrix_row_major.data(), {matrix.rows(), matrix.cols()},
                  dtype);
}

int main(int argc, char** argv) {
    std::string root_path = argv[1];

    // utility::SetVerbosityLevel(utility::VerbosityLevel::Debug);

    Tensor intrinsic = Tensor(
            std::vector<float>({525.0, 0, 319.5, 0, 525.0, 239.5, 0, 0, 1}),
            {3, 3}, Dtype::Float32);

    auto trajectory = io::CreatePinholeCameraTrajectoryFromFile(
            fmt::format("{}/trajectory.log", root_path));

    std::vector<Device> devices{Device("CUDA:0"), Device("CPU:0")};

    for (auto device : devices) {
        t::geometry::VoxelGrid voxel_grid(3.0 / 512, 0.04, 16, 10, device);
        for (int i = 0; i < 3000; ++i) {
            /// Load image
            std::string image_path =
                    fmt::format("{}/depth/{:06d}.png", root_path, i + 1);
            std::shared_ptr<geometry::Image> im_legacy =
                    io::CreateImageFromFile(image_path);
            auto depth_legacy = im_legacy->ConvertDepthToFloatImage();
            t::geometry::Image depth =
                    t::geometry::Image::FromLegacyImage(*depth_legacy, device);
            Eigen::Matrix4f extrinsic_ =
                    trajectory->parameters_[i].extrinsic_.cast<float>();
            Tensor extrinsic = FromEigen(extrinsic_).Copy(device);

            utility::Timer timer;
            timer.Start();
            voxel_grid.Integrate(depth, intrinsic, extrinsic);
            timer.Stop();
            utility::LogInfo("Integration takes {}", timer.GetDuration());
        }

        // auto pcd = voxel_grid.ExtractSurfacePoints();
        // auto pcd_legacy = std::make_shared<geometry::PointCloud>(
        //         pcd.ToLegacyPointCloud());
        // open3d::visualization::DrawGeometries({pcd_legacy});

        auto mesh = voxel_grid.MarchingCubes();
        auto mesh_legacy = std::make_shared<geometry::TriangleMesh>(
                mesh.ToLegacyTriangleMesh());
        // open3d::io::WriteTriangleMesh("mesh.ply", *mesh_legacy);
        mesh_legacy->ComputeVertexNormals();
        open3d::visualization::DrawGeometries({mesh_legacy});
    }
}
