import torch
import torch.nn as nn
from torch.utils import dlpack
import open3d as o3d
from tqdm import tqdm

if __name__ == '__main__':
    # Construct hashmap
    capacity = 10

    dtype_key=o3d.core.Dtype(o3d.core.DtypeCode.Object, 4 * 3, 'Int32x3')
    # dtype_value=o3d.core.Dtype(o3d.core.DtypeCode.Object, 4 * 128 * 3, 'Float32x384')
    hashmap = o3d.core.Hashmap(init_capacity=10,
                               dtype_key=dtype_key,
                               dtype_value=o3d.core.Dtype.Float32,
                               device=o3d.core.Device('cuda:0'))

    # Get keys
    key_blob = hashmap.get_key_blob_as_tensor((10, 3), o3d.core.Dtype.Int32)
    value_blob = hashmap.get_value_blob_as_tensor((10,), o3d.core.Dtype.Float32)

    # Wrap them with a torch parameter
    key_tensor = dlpack.from_dlpack(key_blob.to_dlpack())
    value_tensor = nn.Parameter(dlpack.from_dlpack(value_blob.to_dlpack()))
    optimizer = torch.optim.SGD([value_tensor], lr=0.01)

    # Insert in hashmap and check synchronization
    coords = o3d.core.Tensor(
        [[100, 200, 300], [200, 300, 400], [100, 500, 200], [200, 300, 400]],
        dtype=o3d.core.Dtype.Int32).cuda()
    values = o3d.core.Tensor(
        [[1.0], [2.0], [3.0], [2.0]],
        dtype=o3d.core.Dtype.Float32).cuda()
    iterators, masks, blob_indices = hashmap.insert(coords, values)

    index_tensor = dlpack.from_dlpack(blob_indices[masks].to(
        o3d.core.Dtype.Int32).to_dlpack()).long()

    # Apply some naive differentiable computation here
    print('key_tensor:', key_tensor[index_tensor])
    print('value_tensor:', value_tensor[index_tensor])

    # for i in tqdm(range(1000)):
    #     optimizer.zero_grad()
    #     loss = (value_tensor[index_tensor].abs()).sum()
    #     loss.backward()
    #     optimizer.step()

    # print('after:', value_tensor)
    # print(value_blob)
