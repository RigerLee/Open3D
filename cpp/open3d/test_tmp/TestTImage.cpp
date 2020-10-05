#include "open3d/Open3D.h"
#include "open3d/t/geometry/Image.h"
#include "open3d/t/geometry/PointCloud.h"

using namespace open3d;
using namespace open3d::core;

int main(int argc, char** argv) {
    std::shared_ptr<geometry::Image> im_legacy =
            io::CreateImageFromFile(argv[1]);
    auto depth_legacy = im_legacy->ConvertDepthToFloatImage();
    visualization::DrawGeometries({depth_legacy});

    utility::LogInfo("From legacy image");
    t::geometry::Image im = t::geometry::Image::FromLegacyImage(
            *depth_legacy, Device("CUDA:0"));
    auto im_legacy_ret = std::make_shared<geometry::Image>(im.ToLegacyImage());
    visualization::DrawGeometries({im_legacy_ret});

    SizeVector dims = {0, 1, 2};
    utility::LogInfo("{}", im.AsTensor().ToString());
    Tensor intrinsic = Tensor(
            std::vector<float>({525.0, 0, 319.5, 0, 525.0, 239.5, 0, 0, 1}),
            {3, 3}, Dtype::Float32);
    Tensor vertex_map = im.Unproject(intrinsic);
    utility::LogInfo("{}", vertex_map.ToString());
    Tensor pcd_map = vertex_map.View({3, 480 * 640});

    t::geometry::PointCloud pcd(core::TensorList::FromTensor(pcd_map.T()));

    // core::Tensor transform =
    //         core::Tensor(std::vector<float>({1, 0, 0, 1, 0, -1, 0, 2, 0, 0,
    //         -1,
    //                                          3, 0, 0, 0, 1}),
    //                      {4, 4}, Dtype::Float32, Device("CUDA:0"));
    // pcd.Transform(transform);

    auto pcd_legacy =
            std::make_shared<geometry::PointCloud>(pcd.ToLegacyPointCloud());
    visualization::DrawGeometries({pcd_legacy});
}
