/*
 * SetupMenu.h
 *
 *  Created on: Feb 4, 2018
 *      Author: iltis
 */

#pragma once

#include "MenuEntry.h"
#include "SetupMenuValCommon.h"
#include "SetupNG.h"

class SetupMenuSelect : public MenuEntry
{
	using ITEM_t = std::pair<const char *, int>;

public:
	SetupMenuSelect() = delete;
	explicit SetupMenuSelect( const char* title, e_restart_mode_t restart=RST_NONE, int (*action)(SetupMenuSelect *p) = nullptr, 
		bool save=true, SetupNG<int> *anvs=0, bool ext_handler=false, bool end_menu=false );
	virtual ~SetupMenuSelect() = default;
	void enter() override;
	void display(int mode=0) override;
	void rot(int count);
	void press() override;
	void longPress() override;
	const char *value() const override;
	void setTerminateMenu() { bits._end_menu = true; }
	void lock() { bits._locked = true; }
	void unlock() { bits._locked = false; }
	bool isLocked() const { return bits._locked; }

	bool existsEntry( std::string ent );
    void addEntry(const char* ent, int val);
    void addEntry(const char* ent);
	void addEntryList( const char ent[][4], int size );
	void delEntry( const char * ent );
	void delAllEntries();
	void mkEnable(const char *what=nullptr, int val=0);
	void mkConfirm();
	void updateEntry( const char *ent, int num );
	int getSelect() const { return _select; }
	int getValue() const;
	void setSelect( int sel );
	int numEntries() const { return _values.size(); };

private:
	mutable int  _select = 0;
	int  _select_save = 0;
	int  _char_index = 0;   // position of character to be altered
	bitfield_select bits = {};
	std::vector<ITEM_t> _values;
	int (*_action)( SetupMenuSelect *p );
	SetupNG<int> *_nvs;
	bool _show_inline;
};

