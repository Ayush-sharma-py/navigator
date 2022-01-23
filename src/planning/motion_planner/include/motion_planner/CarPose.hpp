/*
 * Package:   MotionPlanner
 * Filename:  MotionPlanner.cpp
 * Author:    Jim Moore
 * Email:     jim3moore@gmail.com
 * Copyright: 2021, Nova UTD
 * License:   MIT License
 */

#pragma once

//temporary class for input from behavior planner
namespace navigator
{
	namespace MotionPlanner
	{
		class CarPose
		{
		public:
			double x;
			double y;
			double heading;
            double xv;
            double yv;
			CarPose(double x, double y, double heading, double xv, double yv) : x(x), y(y), heading(heading), xv(xv), yv(yv) {}
			CarPose() : x(0), y(0), heading(0), xv(0), yv(0) {}
		};
	}
}