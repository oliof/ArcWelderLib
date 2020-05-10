////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Arc Welder: Anti-Stutter Library
//
// Compresses many G0/G1 commands into G2/G3(arc) commands where possible, ensuring the tool paths stay within the specified resolution.
// This reduces file size and the number of gcodes per second.
//
// Uses the 'Gcode Processor Library' for gcode parsing, position processing, logging, and other various functionality.
//
// Copyright(C) 2020 - Brad Hochgesang
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This program is free software : you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU Affero General Public License for more details.
//
//
// You can contact the author at the following email address: 
// FormerLurker@pm.me
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "segmented_shape.h"
#include <cmath>
#include <iostream>
#pragma region Operators for Vector and Point
point operator +(point lhs, const vector rhs) {
	point p(
		lhs.x + rhs.x,
		lhs.y + rhs.y,
		lhs.z + rhs.z,
		lhs.e_relative + rhs.e_relative
	);
	return p;
}

point operator -(point lhs, const vector rhs) {
	return point(
		lhs.x - rhs.x,
		lhs.y - rhs.y,
		lhs.z - rhs.z,
		lhs.e_relative - rhs.e_relative
	);
}

vector operator -(point& lhs, point& rhs) {
	return vector(
		lhs.x - rhs.x,
		lhs.y - rhs.y,
		lhs.z - rhs.z
	);
}

vector operator *(vector lhs, const double& rhs) {
	return vector(
		lhs.x*rhs,
		lhs.y*rhs,
		lhs.z*rhs
	);
}
#pragma endregion Operators for Vector and Point

#pragma region Point Functions
point point::get_midpoint(point p1, point p2)
{
	double x = (p1.x + p2.x) / 2.0;
	double y = (p1.y + p2.y) / 2.0;
	double z = (p1.z + p2.z) / 2.0;

	return point(x, y, z, 0);
}
#pragma endregion Point Functions

#pragma region Segment Functions
bool segment::get_closest_perpendicular_point(point c, point &d)
{
	return segment::get_closest_perpendicular_point(p1, p2, c, d);
}

bool segment::get_closest_perpendicular_point(point p1, point p2, point c, point& d)
{
	// [(Cx - Ax)(Bx - Ax) + (Cy - Ay)(By - Ay)] / [(Bx - Ax) ^ 2 + (By - Ay) ^ 2]
	double num = (c.x - p1.x)*(p2.x - p1.x) + (c.y - p1.y)*(p2.y - p1.y);
	double denom = (pow((p2.x - p1.x), 2) + pow((p2.y - p1.y), 2));
	double t = num / denom;

	// We're considering this a failure if t == 0 or t==1 within our tolerance.  In that case we hit the endpoint, which is OK.
	if (utilities::less_than_or_equal(t, 0, CIRCLE_FLOATING_POINT_TOLERANCE) || utilities::greater_than_or_equal(t, 1, CIRCLE_FLOATING_POINT_TOLERANCE))
		return false;

	d.x = p1.x + t * (p2.x - p1.x);
	d.y = p1.y + t * (p2.y - p1.y);

	return true;
}

#pragma endregion

#pragma region Vector Functions
double vector::get_magnitude()
{
	return sqrt(x * x + y * y + z * z);
}

double vector::cross_product_magnitude(vector v1, vector v2)
{
	return (v1.x * v2.y - v1.y * v2.x);
}
#pragma endregion Vector Functions

#pragma region Distance Calculation Source
// Distance Calculation code taken from the following source:
// Copyright for distance calculations:
// Copyright 2001 softSurfer, 2012 Dan Sunday
// This code may be freely used, distributed and modified for any purpose
// providing that this copyright notice is included with it.
// SoftSurfer makes no warranty for this code, and cannot be held
// liable for any real or imagined damage resulting from its use.
// Users of this code must verify correctness for their application.
// dot product (3D) which allows vector operations in arguments
#define dot(u,v)   ((u).x * (v).x + (u).y * (v).y + (u).z * (v).z)
#define norm(v)     sqrt(dot(v,v))     // norm = length of  vector
#define d(u,v)      norm(u-v)          // distance = norm of difference

double distance_from_segment(segment s, point p)
{
	vector v = s.p2 - s.p1;
	vector w = p - s.p1;

	double c1 = dot(w, v);
	if (c1 <= 0)
		return d(p, s.p1);

	double c2 = dot(v, v);
	if (c2 <= c1)
		return d(p, s.p2);

	double b = c1 / c2;
	point pb = s.p1 + (v * b);
	return d(p, pb);
}

#pragma endregion Distance Calculation Source


#pragma region Circle Functions
bool circle::is_point_on_circle(point p, double resolution_mm)
{
	// get the difference between the point and the circle's center.
	double difference = std::abs(utilities::get_cartesian_distance(p.x, p.y, center.x, center.y) - radius);
	return utilities::less_than(difference, resolution_mm, CIRCLE_FLOATING_POINT_TOLERANCE);
}

bool circle::try_create_circle(point p1, point p2, point p3, circle& new_circle)
{
	double x1 = p1.x;
	double y1 = p1.y;
	double x2 = p2.x;
	double y2 = p2.y;
	double x3 = p3.x;
	double y3 = p3.y;

	double a = x1 * (y2 - y3) - y1 * (x2 - x3) + x2 * y3 - x3 * y2;

	if (utilities::is_zero(a, CIRCLE_FLOATING_POINT_TOLERANCE))
	{
		return false;
	}


	double b = (x1 * x1 + y1 * y1) * (y3 - y2)
		+ (x2 * x2 + y2 * y2) * (y1 - y3)
		+ (x3 * x3 + y3 * y3) * (y2 - y1);

	double c = (x1 * x1 + y1 * y1) * (x2 - x3)
		+ (x2 * x2 + y2 * y2) * (x3 - x1)
		+ (x3 * x3 + y3 * y3) * (x1 - x2);

	double x = -b / (2.0 * a);
	double y = -c / (2.0 * a);

	new_circle.center.x = x;
	new_circle.center.y = y;
	new_circle.center.z = p1.z;
	new_circle.radius = utilities::get_cartesian_distance(x, y, x1, y1);
	return true;
}
double circle::get_radians(point p1, point p2)
{
	double distance_sq = pow(utilities::get_cartesian_distance(p1.x, p1.y, p2.x, p2.y), 2.0);
	double two_r_sq = 2.0 * radius * radius;
	return acos((two_r_sq - distance_sq) / two_r_sq);
}

point circle::get_closest_point(point p)
{
	vector v = p - center;
	double mag = v.get_magnitude();
	double px = center.x + v.x / mag * radius;
	double py = center.y + v.y / mag * radius;
	double pz = center.z + v.z / mag * radius;
	return point(px, py, pz, 0);
}
#pragma endregion Circle Functions

#pragma region Arc Functions
bool arc::try_create_arc(circle c, point start_point, point mid_point, point end_point, double approximate_length, double resolution, arc& target_arc)
{
	point p1 = c.get_closest_point(start_point);
	point p2 = c.get_closest_point(mid_point);
	point p3 = c.get_closest_point(end_point);
	// Get the radians between p1 and p2 (short angle)
	double p1_p2_rad = c.get_radians(p1, p2);
	double p2_p3_rad = c.get_radians(p2, p3);
	double p3_p1_rad = c.get_radians(p3, p1);

	bool found_angle = false;
	double angle_radians = 0;
	double angle_1, angle_2;
	if (utilities::is_equal(p1_p2_rad + p2_p3_rad + p3_p1_rad, 2 * PI_DOUBLE, CIRCLE_FLOATING_POINT_TOLERANCE))
	{
		found_angle = true;
		angle_1 = p1_p2_rad;
		angle_2 = p2_p3_rad;
	}
	else if (utilities::is_equal(p1_p2_rad + p2_p3_rad + (2 * PI_DOUBLE - p3_p1_rad), 2 * PI_DOUBLE, CIRCLE_FLOATING_POINT_TOLERANCE))
	{
		found_angle = true;
		angle_1 = p2_p3_rad;
		angle_2 = p1_p2_rad;
	}
	else 
	{
		double p1_p2_rad_lg = (2 * PI_DOUBLE - p1_p2_rad);
		if (utilities::is_equal(p1_p2_rad_lg + p2_p3_rad + p3_p1_rad, 2 * PI_DOUBLE, CIRCLE_FLOATING_POINT_TOLERANCE))
		{
			found_angle = true;
			angle_1 = p1_p2_rad_lg;
			angle_2 = p2_p3_rad;
		}
		else
		{
			double p2_p3_rad_lg = (2 * PI_DOUBLE - p2_p3_rad);
			if (utilities::is_equal(p1_p2_rad + p2_p3_rad_lg + p3_p1_rad, 2 * PI_DOUBLE, CIRCLE_FLOATING_POINT_TOLERANCE))
			{
				found_angle = true;
				angle_1 = p1_p2_rad;
				angle_2 = p2_p3_rad_lg;
			}
		}
	}
	if (!found_angle)
		return false; // No angle could be found, exit.
	angle_radians = angle_1 + angle_2;
	double length = angle_radians * c.radius;
	if (!utilities::is_equal(length, approximate_length, resolution))
		return false;

	// Very small angles can't be relied upon to calculate the sign of the arc (clockwise vs anticlockwise)
	if (angle_radians < MIN_ALLOWED_ARC_THETA)
	{
		return false;
	}
	
	// Calculate the sign of the angle.  This should be accurate now that we have filtered out small angles and exited due to lengh mismatches
	vector v1 = p1 - p2;
	vector v2 = p3 - p2;
	// Try to make a reasonable guess about the angle's direction.  This works well unless the the angle is very small
	double magnitude1 = vector::cross_product_magnitude(v1, v2);
	// We can't use our utility compare (utility::greater_that) here, else we will lose
	// very important resolution information
	bool is_clockwise = false;
	
	if (magnitude1 > 0.0)
	{
		is_clockwise = true;
	}
	// If the calculated length isn't within the resolution, exit
	if (is_clockwise)
		angle_radians *= -1.0f;
	
	target_arc.center.x = c.center.x;
	target_arc.center.y = c.center.y;
	target_arc.center.z = c.center.z;
	target_arc.radius = c.radius;
	target_arc.start_point = start_point;
	target_arc.end_point = end_point;
	target_arc.length = length;
	target_arc.angle_radians = angle_radians;
	return true;
	
}

#pragma endregion

segmented_shape::segmented_shape() : points_(50)
{
	max_segments_ = 50;
	resolution_mm_ = 0.0250;
	e_relative_ = 0;
	is_shape_ = false;
	min_segments_ = 3;
	original_shape_length_ = 0;
	is_extruding_ = true;
}
segmented_shape::segmented_shape(int min_segments, int max_segments, double resolution_mm) : points_(max_segments)
{
	max_segments_ = max_segments;
	resolution_mm_ = resolution_mm / 2.0; // divide by 2 because it is + or - 1/2 of the desired resolution.
	e_relative_ = 0;
	is_shape_ = false;
	min_segments_ = min_segments;
	original_shape_length_ = 0;
	is_extruding_ = true;
}

segmented_shape::~segmented_shape()
{
	
}

bool segmented_shape::is_extruding()
{
	return is_extruding_;
}
segmented_shape& segmented_shape::operator=(const segmented_shape& obj)
{
	points_.clear();
	if (obj.max_segments_ != max_segments_)
	{
		max_segments_ = obj.max_segments_;
		
		points_.resize(max_segments_);
	}
	points_.copy(obj.points_);
		
	original_shape_length_ = obj.original_shape_length_;
	e_relative_ = obj.e_relative_;
	is_shape_ = obj.is_shape_;
	max_segments_ = obj.max_segments_;
	resolution_mm_ = obj.resolution_mm_;
	return *this;
}

int segmented_shape::get_num_segments()
{
	return points_.count();
}

double segmented_shape::get_shape_length()
{
	return original_shape_length_;
}

double segmented_shape::get_shape_e_relative()
{
	return e_relative_;
}

void segmented_shape::clear()
{
	points_.clear();
	is_shape_ = false;
	e_relative_ = 0;
	original_shape_length_ = 0;
}
bool segmented_shape::is_shape()
{
	// return the pre-calculated value.  This should be updated by the plugin;
	return is_shape_;
}
void segmented_shape::set_is_shape(bool value)
{
	is_shape_ = value;
}

int segmented_shape::get_min_segments()
{
	return min_segments_;
}
int segmented_shape::get_max_segments()
{
	return max_segments_;
}

double segmented_shape::get_resolution_mm()
{
	return resolution_mm_;
}
void segmented_shape::set_resolution_mm(double resolution_mm)
{
	resolution_mm_ = resolution_mm;
	
}
point segmented_shape::pop_front()
{
	return points_.pop_front();
}
point segmented_shape::pop_back()
{
	return points_.pop_back();
}

bool segmented_shape::try_add_point(point p, double e_relative)
{
	throw std::exception();
}

std::string segmented_shape::get_shape_gcode_absolute(double e_abs_start)
{
	throw std::exception();
}

std::string segmented_shape::get_shape_gcode_relative()
{
	throw std::exception();
}