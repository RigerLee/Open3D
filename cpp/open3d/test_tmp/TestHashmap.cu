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

#include <thrust/device_vector.h>

#include <random>

#include "open3d/core/hashmap/TemplateHashmap.h"

using namespace open3d;
using namespace open3d::core;

template <typename Key, typename Value, typename Hash, typename Eq>
void CompareFind(std::shared_ptr<DeviceHashmap<Hash, Eq>> &hashmap,
                 std::unordered_map<Key, Value> &hashmap_gt,
                 const std::vector<Key> &keys) {
    // Prepare GPU memory
    thrust::device_vector<Key> input_keys_device(keys);
    thrust::device_vector<iterator_t> output_iterators_device(keys.size());
    thrust::device_vector<bool> output_masks_device(keys.size());

    hashmap->Find(reinterpret_cast<void *>(
                          thrust::raw_pointer_cast(input_keys_device.data())),
                  reinterpret_cast<iterator_t *>(thrust::raw_pointer_cast(
                          output_iterators_device.data())),
                  reinterpret_cast<bool *>(
                          thrust::raw_pointer_cast(output_masks_device.data())),
                  keys.size());

    thrust::device_vector<Key> output_keys_device(keys.size());
    thrust::device_vector<Value> output_vals_device(keys.size());
    hashmap->UnpackIterators(
            reinterpret_cast<iterator_t *>(
                    thrust::raw_pointer_cast(output_iterators_device.data())),
            reinterpret_cast<bool *>(
                    thrust::raw_pointer_cast(output_masks_device.data())),
            reinterpret_cast<Key *>(
                    thrust::raw_pointer_cast(output_keys_device.data())),
            reinterpret_cast<Value *>(
                    thrust::raw_pointer_cast(output_vals_device.data())),
            keys.size());

    thrust::host_vector<Key> output_keys_host = output_keys_device;
    thrust::host_vector<Value> output_vals_host = output_vals_device;
    thrust::host_vector<bool> output_masks_host = output_masks_device;

    for (size_t i = 0; i < keys.size(); ++i) {
        auto iterator_gt = hashmap_gt.find(keys[i]);

        // Not found in gt => not found in ours
        if (iterator_gt == hashmap_gt.end()) {
            if (output_masks_host[i] != 0) {
                utility::LogError("Output mask mismatch for {}", i);
            }
        } else {  /// Found in gt => same key and value
            if (output_keys_host[i] != iterator_gt->first) {
                utility::LogError("Output keys mismatch for {}", i);
            }
            if (output_vals_host[i] != iterator_gt->second) {
                utility::LogError("Output vals mismatch for {}", i);
            }

            // iterator_t iterator = output_iterators_device[i];
            // Key key = *(thrust::device_ptr<Key>(
            //         reinterpret_cast<Key *>(iterator.first)));
            // Value val = *(thrust::device_ptr<Value>(
            //         reinterpret_cast<Value *>(iterator.second)));
            // assert(key == iterator_gt->first);
            // assert(val == iterator_gt->second);
        }
    }
}

template <typename Key, typename Value, typename Hash, typename Eq>
void CompareAllIterators(std::shared_ptr<DeviceHashmap<Hash, Eq>> &hashmap,
                         std::unordered_map<Key, Value> &hashmap_gt) {
    // Grab all iterators
    thrust::device_vector<iterator_t> all_iterators_device(hashmap->capacity_);
    size_t total_count = hashmap->GetIterators(reinterpret_cast<iterator_t *>(
            thrust::raw_pointer_cast(all_iterators_device.data())));
    utility::LogInfo("{} {}", total_count, hashmap_gt.size());
    if (total_count != hashmap_gt.size()) {
        utility::LogError("Total count mismatch for {} and {}", total_count,
                          hashmap_gt.size());
    }

    thrust::device_vector<Key> output_keys_device(total_count);
    thrust::device_vector<Value> output_vals_device(total_count);
    hashmap->UnpackIterators(
            reinterpret_cast<iterator_t *>(
                    thrust::raw_pointer_cast(all_iterators_device.data())),
            nullptr,
            reinterpret_cast<Key *>(
                    thrust::raw_pointer_cast(output_keys_device.data())),
            reinterpret_cast<Value *>(
                    thrust::raw_pointer_cast(output_vals_device.data())),
            total_count);

    thrust::host_vector<Key> output_keys_host = output_keys_device;
    thrust::host_vector<Value> output_vals_host = output_vals_device;

    // 2. Verbose check: every iterator should be observable in gt
    for (size_t i = 0; i < total_count; ++i) {
        auto key = output_keys_host[i];
        auto val = output_vals_host[i];

        auto iterator_gt = hashmap_gt.find(key);

        if (iterator_gt == hashmap_gt.end()) {
            utility::LogError("Iterator_gt not found!");
        }
        if (iterator_gt->first != key) {
            utility::LogError("Iterator_gt key not found!");
        }
        if (iterator_gt->second != val) {
            utility::LogError("Iterator_gt val not found!");
        }
    }
}

template <typename Key, typename Value, typename Hash, typename Eq>
void CompareInsert(std::shared_ptr<DeviceHashmap<Hash, Eq>> &hashmap,
                   std::unordered_map<Key, Value> &hashmap_gt,
                   const std::vector<Key> &keys,
                   const std::vector<Value> &vals) {
    // Prepare groundtruth
    for (int i = 0; i < keys.size(); ++i) {
        hashmap_gt.insert(std::make_pair(keys[i], vals[i]));
    }

    // Prepare GPU memory
    thrust::device_vector<Key> input_keys_device = keys;
    thrust::device_vector<Value> input_vals_device = vals;
    thrust::device_vector<bool> output_masks_device(keys.size());

    // Parallel insert
    hashmap->Insert(reinterpret_cast<void *>(
                            thrust::raw_pointer_cast(input_keys_device.data())),
                    reinterpret_cast<void *>(
                            thrust::raw_pointer_cast(input_vals_device.data())),
                    nullptr,
                    reinterpret_cast<bool *>(thrust::raw_pointer_cast(
                            output_masks_device.data())),
                    keys.size());

    size_t insert_count = thrust::reduce(output_masks_device.begin(),
                                         output_masks_device.end(), (size_t)0,
                                         thrust::plus<size_t>());
    utility::LogInfo("Successful insert_count = {}", insert_count);

    CompareAllIterators(hashmap, hashmap_gt);
}

template <typename Key, typename Value, typename Hash, typename Eq>
void CompareActivate(std::shared_ptr<DeviceHashmap<Hash, Eq>> &hashmap,
                     std::unordered_map<Key, Value> &hashmap_gt,
                     const std::vector<Key> &keys,
                     const std::vector<Value> &vals) {
    // Prepare groundtruth
    for (int i = 0; i < keys.size(); ++i) {
        hashmap_gt.insert(std::make_pair(keys[i], vals[i]));
    }

    // Prepare GPU memory
    thrust::device_vector<Key> input_keys_device = keys;
    thrust::device_vector<Value> input_vals_device = vals;
    thrust::device_vector<iterator_t> output_iterators_device(keys.size());
    thrust::device_vector<bool> output_masks_device(keys.size());

    // Parallel activate
    hashmap->Activate(
            reinterpret_cast<void *>(
                    thrust::raw_pointer_cast(input_keys_device.data())),
            reinterpret_cast<iterator_t *>(
                    thrust::raw_pointer_cast(output_iterators_device.data())),
            reinterpret_cast<bool *>(
                    thrust::raw_pointer_cast(output_masks_device.data())),
            keys.size());

    // Naive manual in-place modification
    for (size_t i = 0; i < keys.size(); ++i) {
        iterator_t iterator = output_iterators_device[i];
        if (iterator.second != nullptr) {
            *(thrust::device_ptr<Value>(
                    reinterpret_cast<Value *>(iterator.second))) = vals[i];
        }
    }

    size_t activate_count = thrust::reduce(output_masks_device.begin(),
                                           output_masks_device.end(), (size_t)0,
                                           thrust::plus<size_t>());
    utility::LogInfo("Successful activate_count = {}", activate_count);
    CompareAllIterators(hashmap, hashmap_gt);
}

template <typename Key, typename Value, typename Hash, typename Eq>
void CompareErase(std::shared_ptr<DeviceHashmap<Hash, Eq>> &hashmap,
                  std::unordered_map<Key, Value> &hashmap_gt,
                  const std::vector<Key> &keys) {
    // Prepare groundtruth
    for (int i = 0; i < keys.size(); ++i) {
        hashmap_gt.erase(keys[i]);
    }

    // Prepare GPU memory
    thrust::device_vector<Key> input_keys_device = keys;
    thrust::device_vector<bool> output_masks_device(keys.size());

    hashmap->Erase(reinterpret_cast<void *>(
                           thrust::raw_pointer_cast(input_keys_device.data())),
                   reinterpret_cast<bool *>(thrust::raw_pointer_cast(
                           output_masks_device.data())),
                   keys.size());

    size_t erase_count = thrust::reduce(output_masks_device.begin(),
                                        output_masks_device.end(), (size_t)0,
                                        thrust::plus<size_t>());
    utility::LogInfo("Successful erase_count = {}", erase_count);

    CompareAllIterators(hashmap, hashmap_gt);
}

template <typename Key, typename Value, typename Hash, typename Eq>
void CompareRehash(std::shared_ptr<DeviceHashmap<Hash, Eq>> &hashmap,
                   std::unordered_map<Key, Value> &hashmap_gt,
                   const std::vector<Key> &keys) {
    hashmap->Rehash(hashmap->bucket_count_ * 2);
    hashmap_gt.rehash(hashmap_gt.bucket_count() * 2);

    CompareAllIterators(hashmap, hashmap_gt);
}

int main() {
    std::random_device rnd_device;
    std::mt19937 mersenne_engine{rnd_device()};

    for (size_t capacity = 1000; capacity <= 100000; capacity *= 10) {
        utility::LogInfo("Test with capacity = {}", capacity);
        using Key = int64_t;
        using Value = int64_t;

        // Generate data, allow around the half the duplicates
        std::uniform_int_distribution<Key> dist{-(Key)capacity / 4,
                                                (Key)capacity / 4};
        std::vector<Key> keys(capacity);
        std::vector<Value> vals(capacity);
        std::generate(std::begin(keys), std::end(keys),
                      [&]() { return dist(mersenne_engine); });
        for (size_t i = 0; i < keys.size(); ++i) {
            vals[i] = keys[i] * 100;
        }

        auto hashmap = CreateTemplateDeviceHashmap<DefaultHash, DefaultKeyEq>(
                capacity / 2, capacity, sizeof(Key), sizeof(Value),
                Device("CUDA:0"));
        auto hashmap_gt = std::unordered_map<Key, Value>();

        CompareInsert(hashmap, hashmap_gt, keys, vals);
        utility::LogInfo("TestInsert passed");

        // CompareActivate(hashmap, hashmap_gt, keys, vals);
        // utility::LogInfo("TestActivate passed");

        CompareFind(hashmap, hashmap_gt, keys);
        utility::LogInfo("TestFind passed");

        CompareRehash(hashmap, hashmap_gt, keys);
        utility::LogInfo("TestRehash passed");

        CompareErase(hashmap, hashmap_gt, keys);
        utility::LogInfo("TestErase passed");
    }
}
