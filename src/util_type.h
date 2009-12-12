/*
 *  Portions Copyright 2006,2009 David Carne and 2007,2008 Spark Fun Electronics
 *
 *
 *  This file is part of gerberDRC.
 *
 *  gerberDRC is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  gerberDRC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "math.h"
#ifndef _UTIL_TYPE_H_
#define _UTIL_TYPE_H_
#include <stdio.h>


class Point {
	public:
		Point(double x, double y);
		Point();
		double x, y;
		
};

class Vector2D {
	public:
		Vector2D(double x, double y);
		Vector2D(Point start, Point end);
		
		Point getStart(void);
		Point getEnd(void);
		double getDeltaX(void);
		double getDeltaY(void);

	private:
		Point m_start, m_end;

};

double cross(Vector2D & a, Vector2D & b);

class Rect {

	public:
		Rect();
		Rect(double x1, double y1, double x2, double y2);
		
		Point getStartPoint() const;
		Point getCWP1() const;
		Point getEndPoint() const;
		Point getCWP2() const;
		
		double getWidth() const;
		
		double getHeight() const;
		void printRect()  const;
		bool pointInRectOpen(double x, double y) const;
		bool pointInRectClosed(Point a) const;
		
		bool pointInRectClosed(double x, double y) const;
		
		void mergeBounds(const Rect &r);
		
		void mergePoint(const Point &r, double radius = 0);
		
		void feather(double s);
		void feather(double x, double y);
		bool intersectsWith(const Rect & r);
	private:
		Point a,b;
		bool set;
		
};

#endif


