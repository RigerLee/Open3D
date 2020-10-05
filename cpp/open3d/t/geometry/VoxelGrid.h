// ----------------------------------------------------------------------------
// -                        Open3D: www.open3d.org                            -
// ----------------------------------------------------------------------------
// The MIT License (MIT)
//
// Copyright (c) 2018 www.open3d.org
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
// ----------------------------------------------------------------------------

#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "open3d/core/SparseTensorList.h"
#include "open3d/core/Tensor.h"
#include "open3d/core/TensorList.h"
#include "open3d/core/hashmap/Hashmap.h"
#include "open3d/t/geometry/Geometry.h"
#include "open3d/t/geometry/Image.h"
#include "open3d/t/geometry/PointCloud.h"

namespace open3d {
namespace t {
namespace geometry {
using namespace open3d::core;

class VoxelGrid : public Geometry {
public:
    /// \brief Default Constructor.
    VoxelGrid(float voxel_size = 0.01,
              float sdf_trunc = 0.03,
              int64_t resolution = 16,
              int64_t capacity = 1000,
              const Device &device = Device("CPU:0"));

    ~VoxelGrid() override{};

    VoxelGrid &Clear() override { return *this; };

    bool IsEmpty() const override { return false; };

    void Integrate(const t::geometry::Image &depth,
                   const Tensor &intrinsic,
                   const Tensor &extrinsic);

    std::pair<SparseTensorList, Tensor> ExtractNearestNeighbors();
    PointCloud ExtractSurfacePoints();
    PointCloud MarchingCubes();

protected:
    float voxel_size_;
    float sdf_trunc_;

    int64_t resolution_;
    int64_t capacity_;
    Device device_;

    std::shared_ptr<Hashmap> hashmap_;
};
}  // namespace geometry
}  // namespace t
}  // namespace open3d
