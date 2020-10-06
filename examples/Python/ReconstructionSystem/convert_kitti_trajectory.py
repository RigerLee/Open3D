import pathlib
import argparse
import numpy as np
import math
import itertools

def files_in_path(directory):
    """TODO: Docstring for files_in_path.

    :function: TODO
    :returns: TODO

    """
    directory_path = pathlib.Path(directory)
    assert(directory_path.is_dir())
    file_list = []
    for x in sorted(directory_path.iterdir()):
        if x.is_file():
            file_list.append(x)
        elif x.is_dir():
            file_list.extend(files_in_path(x))
    return file_list

def load_file_list(file_list):
    """Loads the files into a list of numpy arrays

    :function: TODO
    :returns: TODO

    """
    trajectory_list = []
    for filename in file_list:
        trajectory_list.append(np.genfromtxt(filename, delimiter="\t", autostrip=True, usecols=(0, 1, 2, 3), max_rows=4))

    return trajectory_list

def load_trajectory_file(poses_path):
    poses_path = pathlib.Path(poses_path)
    if not poses_path.exists():
        assert False, f"Camera trajectory file does not exist at: {str(poses_path)}"

    with open(poses_path, 'r') as trajectory_file:
        lines = trajectory_file.readlines()
        pose_lines = []
        for i in range(0, len(lines)):
            if not (i == 0 or i % 5 == 0):
                pose_lines.append(lines[i])

        trajectory = np.loadtxt(pose_lines)
        trajectory = np.vsplit(trajectory, trajectory.shape[0]/trajectory.shape[1])

    return trajectory



if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Calculate absolute trajectory error for dataset")

    parser.add_argument("gt_poses_path", help="Path to ground truth poses of the dataset")
    parser.add_subparsers
    parser.add_argument("poses_path", help="Path to the poses generated from open3d reconstruction system")

    args = parser.parse_args();

    gt_file_list = files_in_path(args.gt_poses_path)
    gt_trajectory_list = load_file_list(gt_file_list)

    trajectory_list = load_trajectory_file(args.poses_path)

    gt_poses_list, poses_list = [], []
    for idx, (gt_pose, pose) in enumerate(zip(gt_trajectory_list, trajectory_list)):
        gt_pose = gt_pose[0:3, 0:4]
        pose = pose[0:3, 0:4]
        gt_poses_list.append(gt_pose.flatten())
        poses_list.append(pose.flatten())

    gt_poses_list = np.vstack(gt_poses_list)
    poses_list = np.vstack(poses_list)
    print(gt_poses_list.shape)
    print(poses_list.shape)
    save_path = pathlib.Path(args.poses_path).parent
    np.savetxt(save_path / "gt_poses_kitti.txt", gt_poses_list)
    np.savetxt(save_path / "poses_kitti.txt", poses_list)

