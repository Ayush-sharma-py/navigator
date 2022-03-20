#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <rclcpp/rclcpp.hpp>

#include <iostream>
#include <fstream>
#include <iomanip>

#include <ctime>
#include <cstdlib> 

using namespace std;

#include "path_publisher/PathPublisherNode.hpp"

using geometry_msgs::msg::Point;
using geometry_msgs::msg::Vector3;
using nav_msgs::msg::Odometry;
using std_msgs::msg::ColorRGBA;
using visualization_msgs::msg::Marker;
using visualization_msgs::msg::MarkerArray;
using voltron_msgs::msg::CostedPath;
using voltron_msgs::msg::CostedPaths;
using namespace std::chrono_literals;

PathPublisherNode::PathPublisherNode() : Node("path_publisher_node") {

	this->declare_parameter<std::string>("xodr_path", "/home/main/navigator/data/maps/town10/Town10HD_Opt.xodr");
	this->declare_parameter<double>("path_resolution", 2.0);
	paths_pub = this->create_publisher<CostedPaths>("paths", 1);
	odom_sub = this->create_subscription<Odometry>("/odometry/filtered", 1, [this](Odometry::SharedPtr msg) {
		cached_odom = msg;
	});
	viz_pub = this->create_publisher<MarkerArray>("path_pub_viz", 1);

	// int roads[] = {20,875,21,630,3,0,10,17,7,90,6,735,5,516,4,8,1,765,2,566,3,0};
	onramp_ids = std::set<std::string>{
		"20","875","21","630", "3"
	};
	loop_ids = std::set<std::string>{
		"3","0","10","17","7","90","6","735",
		"5","516","4","8","1","675","2","630"
	};
	all_ids = std::set<std::string>{
		"20","875","21","630","3","0","10","17","7","90","6","735",
		"5","516","4","8","1","675","2","630"
	};

	auto route_1_road_ids = std::vector<std::string>{
		"20","875","21","630","3",
		"0","10","17","7","90",
		"6","735","5","516","4",
		"8","1"//,"675","2","566"
	};
	auto route_1_lane_ids = std::vector<int> {
		-1, -1, -1, -1, -2,
		-2, -2, -2, 2, 2,
		2, 2, 2, 2, 2,
		-2, -2, //-2, -2, -2
	};
	auto route_2_road_ids = std::vector<std::string>{
		"3",
		"0","10","17","7","90",
		"6","735","5","516","4",
		"8","1","675","2","566"
	};
	auto route_2_lane_ids = std::vector<int> {
		-2,
		-2, -2, -2, 2, 2,
		2, 2, 2, 2, 2,
		-2, -2, -2, -2, -2
	};

	path_pub_timer = this->create_wall_timer(0.5s, std::bind(&PathPublisherNode::generatePaths, this));

	// Read map from file, using our path param
	std::string xodr_path = this->get_parameter("xodr_path").as_string();
	path_resolution = this->get_parameter("path_resolution").as_double();
	RCLCPP_INFO(this->get_logger(), "Reading from " + xodr_path);
	map = new odr::OpenDriveMap(xodr_path, true, true, false, true);

	

	this->route1 = generate_path(route_1_road_ids, route_1_lane_ids, map);
	this->route2 = generate_path(route_2_road_ids, route_2_lane_ids, map);
	this->path = this->route1;
}

voltron_msgs::msg::CostedPaths PathPublisherNode::generate_path(std::vector<std::string> &road_ids, std::vector<int> &lane_ids, odr::OpenDriveMap *map)
{
	CostedPaths costed_paths;
	costed_paths.header.frame_id = "map";
	costed_paths.header.stamp = get_clock()->now();

	std::vector<odr::Vec3D> route;
	double step = 0.25;
	for (size_t i = 0; i < road_ids.size(); i++) {
		std::string id = road_ids[i];
		int lane_id = lane_ids[i];
		double road_progress = 0;
		auto road = map->roads[id];
		//there is only one lanesection per road on this map
		std::shared_ptr<odr::LaneSection> lanesection = *(road->get_lanesections().begin());
		odr::LaneSet laneset = lanesection->get_lanes();
		//RCLCPP_INFO(this->get_logger(), "There are %d lanes for road %s", laneset.size(), id.c_str());
		std::shared_ptr<odr::Lane> lane = nullptr;
		for (auto l : laneset) {
			if (l->id == lane_id) {
				lane = l;
				break;
			}
		}
		if (lane == nullptr) {
			RCLCPP_WARN(this->get_logger(), "NO LANE FOR ROAD %s (i=%d)", id.c_str(), i);
			continue;
		}
		odr::Line3D centerline;
		centerline = lane->get_centerline_as_xy(lanesection->s0, lanesection->get_end(), 0.25, lane_id>0);
		for (odr::Vec3D point : centerline) {
			route.push_back(point);
		}
	}
	RCLCPP_INFO(this->get_logger(), "generated path");

	CostedPath costed_path;
	for (odr::Vec3D pt3d : route) {
		// RCLCPP_INFO(get_logger(), "%f, %f", pt3d[0], pt3d[1]);
		Point path_pt;
		path_pt.x = pt3d[0];
		path_pt.y = pt3d[1];
		path_pt.z = pt3d[2];
		
		costed_path.points.push_back(path_pt);
	}
	costed_paths.paths.push_back(costed_path);
	return costed_paths;
}

//shamelessly stolen from egan
void PathPublisherNode::publish_paths_viz(CostedPath path)
{
	MarkerArray marker_array;
	Marker marker;

	// Set header and identifiers
	marker.header.frame_id = "map";
	// marker.header.stamp = this->now();
	marker.ns = "path_pub_viz";
	marker.id = 1;

	// Add data contents
	marker.type = Marker::LINE_STRIP;
	marker.action = Marker::ADD;
	marker.points = path.points;

	// Set visual display. Not sure if this is needed
	marker.scale.x = 1;
	marker.color.a = 1.0;
	marker.color.r = static_cast<float>(1.0 / (1 + exp(-path.safety_cost / 5.0)));
	marker.color.g = static_cast<float>(1.0 / (1 + exp(path.routing_cost / 2.0)));
	marker.color.g *= marker.color.g; // make better paths more visible
	marker.color.b = 0;

	// Add path to array
	marker_array.markers.push_back(marker);
	RCLCPP_INFO(this->get_logger(), "path viz");

	viz_pub->publish(marker_array);
}

/**
 * PSEUDOCODE
 * 1. Find current lane + road ID
 * 2. If within road on onramp (moving onto loop, so road ID is not within "loop sequence"):
 * 	a. Find "s" of car on current road
 * 	b. Sample lane centerline at 1 meter intervals within current road, from "s" to end. Append points to path
 * 	c. For each remaining road in onramp seq, sample lane centerline and append points to path
 * 
 */

void PathPublisherNode::generatePaths() {
	// Wait until odometry data is available
	if (cached_odom == nullptr) {
		RCLCPP_WARN(get_logger(), "Odometry not yet received, skipping...");
		return;
	}
	Point current_pos = cached_odom->pose.pose.position;
	MarkerArray marker_array;
	//this visual doesn't work but that doesn't matter right now
	Marker car;
	car.header.frame_id = "map";
	car.ns = "path_pub_viz";
	car.id = 0; 
	car.type = Marker::CUBE;
	car.action = Marker::ADD;

	car.pose.position = current_pos;
	car.pose.orientation = cached_odom->pose.pose.orientation;
   
    car.scale.x = 4.0;
    car.scale.y = 2.0;
    car.scale.z = 2.0;
   
    car.color.r = 0.0f;
    car.color.g = 1.0f;
    car.color.b = 0.0f;
    car.color.a = 1.0;
	
	marker_array.markers.push_back(car);
	viz_pub->publish(marker_array);

	paths_pub->publish(this->path);
	publish_paths_viz(this->path.paths[0]);
	RCLCPP_INFO(this->get_logger(), "publish path");
	CostedPaths costed_paths;
	CostedPath costed_path;
	costed_paths.header.frame_id = "map";
	costed_paths.header.stamp = get_clock()->now();


	auto currentLane = map->get_lane_from_xy_with_route(current_pos.x, current_pos.y, all_ids);
	if (currentLane == nullptr) {
		RCLCPP_WARN(get_logger(), "Lane could not be located.");
		return;
	}
	// auto currentLane = map->get_lane_from_xy(current_pos.x, current_pos.y);
	auto currentRoad = currentLane->road.lock();
	auto refline = currentRoad->ref_line;
	double s = refline->match(current_pos.x, current_pos.y);

	
	RCLCPP_INFO(get_logger(), "Road %s, Current lane: %i", currentRoad->id.c_str(), currentLane->id);
	if (currentRoad->id == "10") {
		RCLCPP_INFO(get_logger(), "SWITCHED PATH");
		this->path = this->route2;
	}
	/*odr::Line3D centerline;
	// RCLCPP_INFO(get_logger(), "Getting centerline.");
	if (currentLane->id < 0) {
		centerline = currentLane->get_centerline_as_xy(s+1.0, refline->length, 0.25);
	} else {
		centerline = currentLane->get_centerline_as_xy(currentLane->lane_section.lock()->s0, s-1.0, 0.25);
		// centerline = currentLane->get_centerline_as_xy(refline->length, s-1.0, 1.0);
	}
	// RCLCPP_INFO(get_logger(), "Got centerline.");
	for (odr::Vec3D pt3d : centerline) {
		// RCLCPP_INFO(get_logger(), "%f, %f", pt3d[0], pt3d[1]);
		Point path_pt;
		path_pt.x = pt3d[0];
		path_pt.y = pt3d[1];
		path_pt.z = pt3d[2];
		
		costed_path.points.push_back(path_pt);
	}


	// if (costed_path.points.size() < 10) { // If path is short, add successor lane's points
	// 	double end_dx = (costed_path.points.at(costed_path.points.size()-1).x - costed_path.points.at(costed_path.points.size()-2).x);
	// 	double end_dy = (costed_path.points.at(costed_path.points.size()-1).y - costed_path.points.at(costed_path.points.size()-2).y);
	// 	double next_x = costed_path.points.at(costed_path.points.size()-1).x + end_dx;
	// 	double next_y = costed_path.points.at(costed_path.points.size()-1).y + end_dy;
	// 	currentLane = currentLane = map->get_lane_from_xy_with_route(next_x, next_y, all_ids);
	// 	if (currentLane == nullptr)
	// 		return;
	// 	if (currentLane->id < 0) {
	// 		RCLCPP_INFO(get_logger(), "Getting neg. centerline.");
	// 		centerline = currentLane->get_centerline_as_xy(s+1.0, refline->length, 1.0);
	// 	} else {
	// 		RCLCPP_INFO(get_logger(), "Getting pos. centerline.");
	// 		centerline = currentLane->get_centerline_as_xy(s+1.0, refline->length, 1.0);
	// 		// centerline = currentLane->get_centerline_as_xy(refline->length, s-1.0, 1.0);
	// 	}
	// 	for (odr::Vec3D pt3d : centerline) {
	// 		// RCLCPP_INFO(get_logger(), "%f, %f", pt3d[0], pt3d[1]);
	// 		Point path_pt;
	// 		path_pt.x = pt3d[0];
	// 		path_pt.y = pt3d[1];
	// 		path_pt.z = pt3d[2];
			
	// 		costed_path.points.push_back(path_pt);
	// 	}
	// }

	costed_paths.paths.push_back(costed_path);
	

	paths_pub->publish(costed_paths);*/
}