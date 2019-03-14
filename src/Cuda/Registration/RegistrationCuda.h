//
// Created by wei on 1/10/19.
//

#pragma once

#include <Open3D/Registration/Registration.h>
#include <Open3D/Geometry/PointCloud.h>
#include <Open3D/Geometry/KDTreeFlann.h>

#include <src/Cuda/Container/ArrayCuda.h>
#include <src/Cuda/Geometry/PointCloudCuda.h>
#include <src/Cuda/Registration/ColoredICPCuda.h>
#include <src/Cuda/Registration/TransformEstimationCuda.h>

namespace open3d {

namespace cuda {
class ICPConvergenceCriteriaCuda {
public:
    ICPConvergenceCriteriaCuda(float relative_fitness = 1e-6f,
                               float relative_rmse = 1e-6f,
                               int max_iteration = 30) :
        relative_fitness_(relative_fitness), relative_rmse_(relative_rmse),
        max_iteration_(max_iteration) {}
    ~ICPConvergenceCriteriaCuda() {}

public:
    float relative_fitness_;
    float relative_rmse_;
    int max_iteration_;
};

/** Basically it doesn't involve cuda functions. It
 *  - collects correspondences on CPU and uploads them to CorrespondenceSetCuda
 *  - initializes and calls TransformEstimationCuda with cuda functions.
 **/
class RegistrationCuda {
public:
    Eigen::Matrix4d transform_source_to_target_;
    std::shared_ptr<TransformEstimationCuda> estimator_;

public:
    explicit RegistrationCuda(const
                              registration::TransformationEstimationType &type);
    ~RegistrationCuda() {};

public:
    /* Preparation */
    void Initialize(
        geometry::PointCloud &source,
        geometry::PointCloud &target,
        float max_correspondence_distance,
        const Eigen::Matrix4d &init = Eigen::Matrix4d::Identity());

    /* ICP computations */
    RegistrationResultCuda DoSingleIteration(int iter);
    Eigen::Matrix6d ComputeInformationMatrix();

    /* Wrapper of multiple iterations */
    RegistrationResultCuda ComputeICP(int iter = 60);
};
}
}


