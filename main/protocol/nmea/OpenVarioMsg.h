/***********************************************************
 ***   THIS DOCUMENT CONTAINS PROPRIETARY INFORMATION.   ***
 ***    IT IS THE EXCLUSIVE CONFIDENTIAL PROPERTY OF     ***
 ***     Rohs Engineering Design AND ITS AFFILIATES.     ***
 ***                                                     ***
 ***       Copyright (C) Rohs Engineering Design         ***
 ***********************************************************/

#pragma once

#include "protocol/NMEA.h"

class OpenVarioMsg final : public NmeaPlugin
{
public:
    OpenVarioMsg(NmeaPrtcl &nr) : NmeaPlugin(nr) {};
    virtual ~OpenVarioMsg() = default;
    ConstParserMap* getPM() const { return &_pm; }
    const char* getSenderId() const { return "PXC"; };

    // Declare send routines in NmeaPrtcl class !

private:
    // Received messages
    static ConstParserMap _pm;
};
