//
// Created by wei on 11/9/18.
//

#include <src/Cuda/Container/HashTableCudaHost.hpp>

#include "UniformTSDFVolumeCudaHost.hpp"
#include "UniformMeshVolumeCudaHost.hpp"
#include "ScalableTSDFVolumeCudaHost.hpp"
#include "ScalableMeshVolumeCudaHost.hpp"


namespace open3d {

namespace cuda {
template
class UniformTSDFVolumeCuda<8>;
template
class UniformTSDFVolumeCuda<16>;
template
class UniformTSDFVolumeCuda<256>;
template
class UniformTSDFVolumeCuda<512>;

template
class UniformMeshVolumeCuda<8>;
template
class UniformMeshVolumeCuda<16>;
template
class UniformMeshVolumeCuda<256>;
template
class UniformMeshVolumeCuda<512>;

template
class ScalableMeshVolumeCuda<8>;
template
class ScalableTSDFVolumeCuda<8>;

} // cuda
} // open3d