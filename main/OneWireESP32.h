#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <driver/rmt_types.h>

#include <esp_attr.h>

#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
#define MAX_BLOCKS	64
#else
#define MAX_BLOCKS	48
#endif

#define DEVICE_DISCONNECTED_C -127

IRAM_ATTR bool owrxdone(rmt_channel_handle_t ch, const rmt_rx_done_event_data_t *edata, void *udata);

class OneWire32 {
	private:
		gpio_num_t owpin = GPIO_NUM_NC;
		rmt_channel_handle_t owtx = 0;
		rmt_channel_handle_t owrx = 0;
		rmt_encoder_handle_t owcenc = 0;
		rmt_encoder_handle_t owbenc = 0;
		rmt_symbol_word_t owbuf[MAX_BLOCKS];
		QueueHandle_t owqueue = nullptr;
		uint8_t drv = 0;
	public:
		OneWire32(uint8_t pin);
		~OneWire32();
		bool reset();
		void request();
		uint8_t getTemp(uint64_t &addr, float &temp);
		uint8_t search(uint64_t *addresses, uint8_t total);
		bool read(uint8_t &data, uint8_t len = 8);
		bool write(const uint8_t data, uint8_t len = 8);
};
