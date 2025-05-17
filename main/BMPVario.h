#pragma once

#include "ESP32NVS.h"
#include "PressureSensor.h"

#include "setup/SetupNG.h"
#include "S2F.h"
#include <stdio.h>
/*
     Implementation of a stable Vario with flight optimized Iltis-Kalman filter

 */

// #define SPS 10                   // samples per second
constexpr const int FILTER_LEN = 34; // Max Filter length
constexpr const double ALPHA = 0.2; // Kalman Gain alpha
#define ERRORVAL 1.6             // damping Factor for values off the weeds
constexpr const float STANDARD = 1013.25; // ICAO standard pressure


class BMPVario {
public:
	BMPVario() {
		_positive = GPIO_NUM_0;
		_negative = GPIO_NUM_0;
		_qnh = STANDARD;
		index = 0;
		_alpha = ALPHA;
		predictAlt = 0;
		Altitude = 0;
		lastAltitude = 0;
		_errorval = ERRORVAL;
		_TEF = 0;
		_test = false;
		_sensorTE = 0;
		_avgTE = 0;
		averageAlt = 0;
		bmpTemp = 0;
		_damping = 1.0;
		_damping_factor = 1.0;
		_currentAlt = 0;
		_S2FTE = 0;
		_analog_adj = 0;
		myS2F = 0;
		_sensorBARO = 0;
		N = 0;
	}

	void begin( PressureSensor *te,  PressureSensor *baro, S2F* s2f );

	static void setHolddown( int hold ) { holddown = hold; }
	void setQNH( float qnh ) { _qnh = qnh; };
	void setAveragerTime(float t) { avgTE.setLength(t); };
	void setup();

	double readTE(float tas, float tePressure);   // get TE value im m/s
	double readAVGTE();   // get TE value im m/s
	float  readS2FTE();   // get TE value im m/s for S2F
	double readAVGalt() { return averageAlt; };    // get average Altitude
	double readCuralt() { return _currentAlt; };   // get current Altitude
	void setTE( double te ); // for testing purposes
	void configChange();

private:
	gpio_num_t _negative;
	gpio_num_t _positive;
	double _alpha;
	double _errorval;
	double _qnh;
	PressureSensor *_sensorTE;
	PressureSensor *_sensorBARO;
	double predictAlt;
	double Altitude;
	double lastAltitude;
	double averageAlt;
	Average<FILTER_LEN, double, double> TEavg;
	double _analog_adj;
	int    index;
	double _TEF;
	Average<60, float, float> avgTE;
	double _avgTE;
	double bmpTemp;
	bool _test;
	float _damping;
	double _damping_factor;
	float _S2FTE;
	double _currentAlt;
	static int   holddown;
	S2F * myS2F;
	int N;
};

extern BMPVario bmpVario;
