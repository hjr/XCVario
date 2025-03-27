/***********************************************************
 ***   THIS DOCUMENT CONTAINS PROPRIETARY INFORMATION.   ***
 ***    IT IS THE EXCLUSIVE CONFIDENTIAL PROPERTY OF     ***
 ***     Rohs Engineering Design AND ITS AFFILIATES.     ***
 ***                                                     ***
 ***       Copyright (C) Rohs Engineering Design         ***
 ***********************************************************/

#pragma once

#include "protocol/NMEA.h"
#include "protocol/ClockIntf.h"

class CANMasterRegMsg  final : public NmeaPlugin, public Clock_I
{
public:
    // The master registry takes it's own identity, not knowing what kind of device
    // will request. Registration happens on one single port and a unique query id.
    explicit CANMasterRegMsg(NmeaPrtcl &nr);
    virtual ~CANMasterRegMsg() = default;

    const ParserEntry* getPT() const override { return _pt; }

public:
    // Clock tick callback
    bool tick() override;

    // Some transmitter
    void sendLossOfResgitrations();

private:
    // Received messages
    static dl_action_t registration_query(NmeaPrtcl *nmea);
    static const ParserEntry _pt[];
};
