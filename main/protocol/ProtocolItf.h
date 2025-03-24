/***********************************************************
 ***   THIS DOCUMENT CONTAINS PROPRIETARY INFORMATION.   ***
 ***    IT IS THE EXCLUSIVE CONFIDENTIAL PROPERTY OF     ***
 ***     Rohs Engineering Design AND ITS AFFILIATES.     ***
 ***                                                     ***
 ***       Copyright (C) Rohs Engineering Design         ***
 ***********************************************************/

#pragma once

#include "comm/Devices.h"
#include <string>
#include <vector>

// Generic protocol state machine with special bit mask
typedef enum
{
    START_TOKEN = 0,
    HEADER,
    PAYLOAD,
    STOP_TOKEN,
    CHECK_CRC1,
    CHECK_CRC2,
    COMPLETE,
	GET_KRT2_FRAME,
	GET_KRT2_COMMAND,
	GET_KRT2_MHZ,
	GET_KRT2_KHZ,
	GET_KRT2_NAME,
	GET_KRT2_VOL_CS,
	GET_KRT2_VOX,
	GET_KRT2_VOL,
	GET_KRT2_SQL,
	GET_KRT2_PTT,
	GET_KRT2_ICV,
	GET_KRT2_SINGLE_DATABYTE,
	GET_KRT2_CHECKSUM
} gen_state_t;


constexpr int COMPLETE_BIT  = 0x10;
constexpr int FORWARD_BIT   = 0x20;
typedef enum
{
    NOACTION = 0,
    DO_ROUTING = COMPLETE_BIT | FORWARD_BIT,
    NOROUTING = COMPLETE_BIT,
    NXT_PROTO = COMPLETE_BIT | FORWARD_BIT | 1,
    GO_NMEA = COMPLETE_BIT | FORWARD_BIT | 2
} datalink_action_t;

class ProtocolState;
class DataLink;

// Protocol parser interface
class ProtocolItf
{
public:
    ProtocolItf(DeviceId id, int sp, ProtocolState &sm, DataLink& dl) : _did(id), _send_port(sp), _sm(sm), _dl(dl) {};
    virtual ~ProtocolItf() {}

    static constexpr int MAX_LEN = 128;

public:
    // API
    DeviceId getDeviceId() { return _did; } // The connected (!) device through protocol
    virtual ProtocolType getProtocolId() { return NO_ONE; }
    virtual datalink_action_t nextByte(const char c) { return NOACTION; } // return action for data link
    virtual datalink_action_t nextStreamChunk(const char *cptr, int count) { return NOACTION; } // for binary protocols
    inline Message* newMessage() const { return DEV::acqMessage(_did, _send_port); }
    void setDefaultAction(datalink_action_t da) { _default_action = da; }
    virtual bool isBinary() const { return false; }
    int getSendPort() const { return _send_port; }
    DataLink* getDL() const { return &_dl; }
    ProtocolState* getSM() const  { return &_sm; }

protected:
    // routing
    const DeviceId _did;
    const int _send_port;
    datalink_action_t _default_action = NOACTION; // no forwarding by default

    // state machine
    ProtocolState &_sm;

    // Reference to data link layer
    DataLink &_dl;

    // small crc character buffer
    char _crc_buf[3];
};

// Protocol parser state
class ProtocolState
{
public:
    ProtocolState() = default;
    void reset()
    {
        _frame.clear();
        _state = START_TOKEN;
        _frame_len = 0;
        _opt = 0;
        _esc = 0;
    }
    inline bool push(char c)
    {
        if ( _state )
        {
            if ( _frame.size() < ProtocolItf::MAX_LEN ) {
                _frame.push_back(c);
                return true;
            }
            else {
                return false;
            }
        }
        _frame.assign(1, c);
        _crc = 0;
        return true;
    }

    // frame buffer and state machine vars
    std::string _frame;
    int         _frame_len;
    gen_state_t _state = START_TOKEN;
    int         _crc = 0; // checksum (binary)
    int         _opt = 0; // some space to carry an optional datum
    int         _esc = 0; // another optional container
    int         _header_len; // for NMEA e.g. $PFL... this will be 4, pointing to the message id start
    std::vector<int> _word_start; // Indices of all the found "," chars
};
