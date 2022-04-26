#!/usr/bin/python

'''
Nova at UT Dallas, 2022

The Navigator Simulation Bridge for CARLA

The goal is to mimick Hail Bopp as much as possible.

Targetted sensors:
- GNSS (GPS)
✓ IMU ()
- Front and rear Lidar
✓ Front  RGB camera
✓ Front depth camera
- CARLA ground truths for
    - Detected objects
    ✓ Car's odometry (position, orientation, speed)
    ✓ CARLA virtual bird's-eye camera (/carla/birds_eye_rgb)

Todos:
- Specific todos are dispersed in this script. General ones are here.
- Ensure all sensors publish in ROS coordinate system, NOT Unreal Engine's.

'''

import cv2
import time
from scipy import rand
from sensor_msgs.msg import PointCloud2, Image, Imu
from geometry_msgs.msg import Point, Quaternion, Vector3, PoseWithCovariance, PoseWithCovarianceStamped
from voltron_msgs.msg import Obstacle3DArray, Obstacle3D, BoundingBox3D, BoundingBox2D, Obstacle2D, Obstacle2DArray
from visualization_msgs.msg import Marker, MarkerArray
from nav_msgs.msg import Odometry  # For GPS, ground truth
from std_msgs.msg import Bool, Header, Float32, ColorRGBA
import math
from tf2_ros.transform_broadcaster import TransformBroadcaster
from tf2_ros.transform_listener import TransformListener
import tf2_py
from tf2_ros.buffer import Buffer
import tf2_msgs
from tf2_ros import TransformException, TransformStamped
import pymap3d
from cv_bridge import CvBridge
import numpy as np
import ros2_numpy as rnp
from rclpy.node import Node
import rclpy
from scipy.spatial.transform import Rotation as R

# FAST_GICP
import pygicp
import open3d


# Image format conversion

'''
CHECKLIST
=========

Publish to ROS:
- [x] RGB image from left camera (15 Hz)
- [x] Depth map (15 Hz)
- [-] Point cloud -- Skipped, too slow for now
- [x] Array of detected objects
- [x] Pose data (with sensor fusion of optical odom) (max Hz)

'''


class ScanMatchingNode(Node):

    def __init__(self):
        super().__init__('scan_matcher')
        self.get_logger().info("Hello, world!")

        # Read our map
        self.get_logger().info("Reading map...")
        map_file = open3d.io.read_point_cloud(
            '/home/main/navigator-2/data/maps/grand_loop/grand_loop.pcd')
        map_cloud = np.asarray(map_file.points)

        self.pcd_map = pygicp.downsample(map_cloud, 0.25)
        self.get_logger().info(
            f"Map loaded with shape {self.pcd_map.shape}")
        print(self.pcd_map)

        self.map_pub = self.create_publisher(PointCloud2, '/map/pcd', 1)
        self.aligned_lidar_pub = self.create_publisher(
            PointCloud2, '/atlas/aligned_lidar', 1)

        # Publish the map periodically

        self.map_pub_timer = self.create_timer(10, self.publish_map)

        self.lidar_sub = self.create_subscription(
            PointCloud2, '/lidar_front/velodyne_points', self.lidar_cb, 10)

        self.gnss_sub = self.create_subscription(
            Odometry, '/sensors/gnss/odom', self.gnss_cb, 10)

        self.tf_broadcaster = TransformBroadcaster(self)

        self.initial_guess = None

    def publish_map(self):
        self.publish_cloud_from_array(self.pcd_map, 'map', self.map_pub)

    def gnss_cb(self, msg: Odometry):
        self.initial_guess = msg

    def lidar_cb(self, msg: PointCloud2):
        if self.initial_guess is None:
            self.get_logger().warning(
                "Initial guess from GNSS not yet received, skipping alignment.")
            return

        self.get_logger().info("Trying alignment")

        # Convert PointCloud2 to np array
        lidar_arr_dtype = rnp.numpify(msg)
        lidar_list = [lidar_arr_dtype['x'],
                      lidar_arr_dtype['y'], lidar_arr_dtype['z']]
        lidar_arr = np.array(lidar_list).T.reshape(-1,
                                                   3)
        lidar_arr = lidar_arr[~np.isnan(lidar_arr).any(axis=1)]
        lidar_arr = pygicp.downsample(lidar_arr, 0.1)

        # Transform lidar from base_link->map
        map_pos = self.initial_guess.pose.pose.position
        lidar_arr[:, 0] += map_pos.x
        lidar_arr[:, 1] += map_pos.y
        lidar_arr[:, 2] += map_pos.z

        gicp = pygicp.FastGICP()
        gicp.set_input_source(lidar_arr)
        gicp.set_input_target(self.pcd_map)
        matrix = gicp.align()

        self.publish_cloud_from_array(lidar_arr, 'map', self.aligned_lidar_pub)

        # Publish our transform result
        tf = TransformStamped()
        tf.header.stamp = self.get_clock().now().to_msg()
        tf.header.frame_id = 'map'
        tf.child_frame_id = 'odom'
        tf.transform.translation = Vector3(
            x=self.initial_guess.pose.pose.position.x,
            y=self.initial_guess.pose.pose.position.y,
            z=self.initial_guess.pose.pose.position.z
        )
        tf.transform.rotation = self.initial_guess.pose.pose.orientation
        self.tf_broadcaster.sendTransform(tf)

    def publish_cloud_from_array(self, arr, frame_id: str, publisher):
        data = np.zeros(arr.shape[0], dtype=[
            ('x', np.float32),
            ('y', np.float32),
            ('z', np.float32)
        ])
        print(arr.shape)
        print(data.shape)
        print(arr[0])
        data['x'] = arr[:, 0]
        data['y'] = arr[:, 1]
        data['z'] = arr[:, 2]
        msg: PointCloud2 = rnp.msgify(PointCloud2, data)
        msg.header.stamp = self.get_clock().now().to_msg()
        msg.header.frame_id = frame_id

        publisher.publish(msg)


def main(args=None):
    rclpy.init(args=args)

    scan_matcher = ScanMatchingNode()
    rclpy.spin(scan_matcher)

    # Destroy the node explicitly
    # (optional - otherwise it will be done automatically
    # when the garbage collector destroys the node object)
    scan_matcher.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()
