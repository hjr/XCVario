/*
 * Polars.h
 *
 *  Created on: Mar 1, 2019
 *      Author: iltis
 */

#pragma once

struct compressed_polar;

struct t_polar {
	t_polar(const compressed_polar*);
	int      index;
	const char *type;
	float    wingload;		// kg/mxm
	float    speed1;		// km/h
	float    sink1;			// m/s
	float    speed2;		// km/h
	float    sink2;			// m/s
	float    speed3;		// km/h
	float    sink3;			// m/s
	float    max_ballast;	// in liters or kg
	float    wingarea;		// mxm
};


namespace Polars {
	const t_polar getPolar( int x );
	int numPolars();
	void begin();
};

