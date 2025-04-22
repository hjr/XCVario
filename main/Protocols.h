/*
 * Protocols.h
 *
 *  Created on: Dec 20, 2017
 *      Author: iltis
 */
#ifndef PROTOCOLS_H_
#define PROTOCOLS_H_

#include "S2F.h"
#include "SetupNG.h"

#include <string>

// Supported Protocols
typedef enum protocol_t  { P_OPENVARIO, P_BORGELT, P_CAMBRIDGE, P_EYE_PEYA, P_EYE_PEYI, P_AHRS_RPYL, P_AHRS_APENV1, P_GENERIC, P_XCVARIO, P_XCVARIO_DEVEL } proto_t;


class Protocols {
public:
	Protocols( S2F * as2f );
	virtual ~Protocols( );
	void sendNMEA( proto_t proto, char* str, float baro, float dp, float te, float temp, float ias, float tas,
			float mc, int bugs, float ballast, bool cruise, float alt,
			bool validTemp=false, float ax=0, float ay=0, float az=0, float gx=0, float gy=0, float gz=0 );

	static void parseNMEA( const char *str );
	static int calcNMEACheckSum(const char *nmea);
	static int getNMEACheckSum(const char *nmea);

private:
	static S2F *   _s2f;
	static float   _mc_prev;
	static float   _qnh_prev;
	static bool     _can_send_error;
};

#endif /* PROTOCOLS_H_ */
