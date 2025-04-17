/////////////////////////////////////////////////////////////////
/*
  Library for reading rotary encoder values using Observer Pattern, and GPIO Interrups
*/

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include <driver/pulse_cnt.h>
#include <esp_timer.h>
#include <driver/gpio.h>


// use this to build an event infrastructure
constexpr const int SHORT_PRESS     = 1;
constexpr const int LONG_PRESS      = 2;
constexpr const int BUTTON_RELEASED = 3;
constexpr const int ROTARY_EVTMASK  = 0xf0;

union KnobEvent {
	struct {
		int ButtonEvent : 4; // 1,2,3
		int RotaryEvent : 4; // -3,-2,-1, 1,2,3
	};
	int raw;
	KnobEvent() = default;
	constexpr KnobEvent(const int v) : raw(v) {}
};


class RotaryObserver
{
public:
	RotaryObserver() {};
	virtual ~RotaryObserver() {};
	virtual void rot(int count) = 0;
	virtual void press() = 0;
	virtual void longPress() = 0;
	virtual void release() = 0;
	virtual void escape() = 0;
	void setRotDynamic(float d) { _rot_dynamic = d; }
	float getRotDynamic() const { return _rot_dynamic; }

	void attach();
	void detach();

private:
	float _rot_dynamic = 2.f; // optional rotary accelerator.
};


class ESPRotary
{
	friend void button_isr_handler(void* arg);
	friend void longpress_timeout(void *arg);

public:
	static constexpr const int DEFAULT_LONG_PRESS_THRESHOLD = 400;

	ESPRotary(gpio_num_t aclk, gpio_num_t adt, gpio_num_t asw);
	~ESPRotary();
	void begin();
	void stop();
	QueueHandle_t getQueue() const { return buttonQueue; }
	esp_err_t updateRotDir();
	void updateIncrement(int inc);
	void setLongPressTimeout(int lptime_ms) { lp_duration = (uint64_t)1000 * lptime_ms; }

	// observer feed
	void sendRot(int diff) const;
	void sendPress() const;
	void sendRelease() const;
	void sendLongPress() const;
	void sendEscape() const;
	// gpio's
	bool readSwitch() const; // safe to call in rotary callback
	bool readBootupStatus() const { return gpio_get_level(sw) == 0; }
	gpio_num_t getSw() const { return sw; };
	gpio_num_t getClk() const { return clk; };
	gpio_num_t getDt() const { return dt; };

private:
	QueueHandle_t buttonQueue = nullptr;
	gpio_num_t clk, dt, sw; // actually used pins
	pcnt_unit_handle_t pcnt_unit = nullptr;
	pcnt_channel_handle_t pcnt_chan = nullptr;
	esp_timer_handle_t lp_timer = nullptr;
	uint64_t lp_duration = DEFAULT_LONG_PRESS_THRESHOLD * 1000; // default 400msec
	bool state = false;
	bool hold = false; // timer timeout set the hold state
	int increment = 1;
};

extern ESPRotary *Rotary;

