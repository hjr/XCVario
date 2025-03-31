/***********************************************************
 ***   THIS DOCUMENT CONTAINS PROPRIETARY INFORMATION.   ***
 ***    IT IS THE EXCLUSIVE CONFIDENTIAL PROPERTY OF     ***
 ***     Rohs Engineering Design AND ITS AFFILIATES.     ***
 ***                                                     ***
 ***       Copyright (C) Rohs Engineering Design         ***
 ***********************************************************/

#include "SetupAction.h"
#include "sensor.h"

#include "logdef.h"

SetupAction::SetupAction(const char *title, int (*action)(SetupAction *), int code, bool end_menu) :
    _action(action),
    _code(code),
    _end_menu(end_menu)
{
    ESP_LOGI(FNAME, "SetupAction( %s ) ", title);
    _title = title;
    attach(this);
}

SetupAction::~SetupAction()
{
    detach(this);
}

void SetupAction::display(int mode)
{
    if ((selected != this) || !gflags.inSetup) {
        return;
    }
    ESP_LOGI(FNAME, "display() pressed=%d instance=%p mode=%d", pressed, this, mode);
    int line = _parent->getMenuPos()+1;
    int indent = ucg->getStrWidth(_title) + 12;
    ucg->setPrintPos(indent, (line + 1) * 25);
    ucg->printf( " %3d  ", mode );
}

void SetupAction::press()
{
    if( selected != this ) {
        return;
    }
    ESP_LOGI(FNAME, "press() pressed=%d instance=%p", pressed, this);
    (*_action)(this);

    if (!pressed)
    {
        // trap until long press
        pressed = true;
    }
}

void SetupAction::longPress()
{
    pressed = false;
    selected = _parent;
    if ( _end_menu ) {
        _parent->menuSetTop();
    }
}
