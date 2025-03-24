/***********************************************************
 ***   THIS DOCUMENT CONTAINS PROPRIETARY INFORMATION.   ***
 ***    IT IS THE EXCLUSIVE CONFIDENTIAL PROPERTY OF     ***
 ***     Rohs Engineering Design AND ITS AFFILIATES.     ***
 ***                                                     ***
 ***       Copyright (C) Rohs Engineering Design         ***
 ***********************************************************/

#pragma once

#include "ProtocolItf.h"
#include "ClockIntf.h"

#include <string>

class KRT2Remote final : public ProtocolItf
{
public:
    explicit KRT2Remote(DeviceId did, int mp, ProtocolState &sm, DataLink &dl);
    virtual ~KRT2Remote();

    ProtocolType getProtocolId() override { return KRT2_REMOTE_P; }

public:
    datalink_action_t nextByte(const char c) override;



private:
    // Actions on commands
    int data_len;
    uint8_t cs;
    uint8_t mhz;
    uint8_t khz;
};
