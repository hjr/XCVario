/***********************************************************
 ***   THIS DOCUMENT CONTAINS PROPRIETARY INFORMATION.   ***
 ***    IT IS THE EXCLUSIVE CONFIDENTIAL PROPERTY OF     ***
 ***     Rohs Engineering Design AND ITS AFFILIATES.     ***
 ***                                                     ***
 ***       Copyright (C) Rohs Engineering Design         ***
 ***********************************************************/

#pragma once

#include "ProtocolItf.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <map>


// A max. 4 char long key to map different NMEA message id's
union Key {
    char str[4];
    uint32_t value;

    inline bool notAsciiUpper(char c) {
        return (c < 'A' || c > 'Z');
    }
    
    constexpr Key(const char* s) : value(0) {
        for (int i=0; i<4; i++) {
            if ( notAsciiUpper(*s) ) break;
            str[i] = *s++;
        }
    }
    constexpr Key(const uint32_t v) : value(v) {}

    std::string toString() const {
        return std::string(str, 4).c_str();  // Null termination
    }

    // comparator for the std::map
    bool operator<(const Key& other) const {
        return value < other.value;
    }
};

// message table
class NmeaPrtcl;
class NmeaPlugin;
using NmeaMessageParser = std::function<datalink_action_t(NmeaPrtcl*)>;
typedef const std::map<Key, NmeaMessageParser> ParserMap; // intrinsicly const to reside in flash memory
typedef std::map<Key, ParserMap*> SenderMap;

// generic mnea message parser
class NmeaPrtcl final : public ProtocolItf
{
public:
    explicit NmeaPrtcl(DeviceId did, int mp, ProtocolState &sm, DataLink &dl) : ProtocolItf(did, mp, sm, dl) {};
    virtual ~NmeaPrtcl();

public:
    void addPlugin(NmeaPlugin *pm);
    datalink_action_t nextByte(const char c) override;

private:
    SenderMap _sender;
    std::vector<NmeaPlugin*> _plugs;
    ParserMap* _parser;
    inline void nmeaIncrCRC(int &crc, const char c) {crc ^= c;}
};

// nmea message extension
class NmeaPlugin
{
public:
    explicit NmeaPlugin(NmeaPrtcl &nr) : _nmeaRef(nr), _sm(*nr.getSM()) {}
    virtual ~NmeaPlugin() = default;

    // API
    virtual const ParserMap* getPM() const = 0;
    virtual const char* getSenderId() const = 0;

protected:
    // helping the parse and send routines
    const NmeaPrtcl &_nmeaRef;
    ProtocolState &_sm;
};