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

#pragma once
#include "segmented_shape.h"
#include <iomanip>
#include <sstream>

#define GCODE_CHAR_BUFFER_SIZE 100
class segmented_arc :
	public segmented_shape
{
public:
	segmented_arc();
	segmented_arc(int max_segments, double resolution_mm);
	virtual ~segmented_arc();
	virtual bool try_add_point(point p, double e_relative);
	std::string get_shape_gcode_absolute(double e, double f);
	std::string get_shape_gcode_relative(double f);
	
	virtual bool is_shape();
	point pop_front(double e_relative);
	point pop_back(double e_relative);
	bool try_get_arc(arc & target_arc);
	// static gcode buffer

private:
	char gcode_buffer_[GCODE_CHAR_BUFFER_SIZE];
	bool try_add_point_internal_(point p, double pd);
	bool does_circle_fit_points_(circle c, point p, double additional_distance);
	bool try_get_arc_(circle& c, point endpoint, double additional_distance, arc & target_arc);
	std::string get_shape_gcode_(bool has_e, double e, double f);
	int min_segments_;
	circle arc_circle_;
	int test_count_ = 0;
	std::ostringstream s_stream_;
};

