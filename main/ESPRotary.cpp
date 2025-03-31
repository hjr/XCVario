
#include "ESPRotary.h"

#include "SetupNG.h"
#include "sensor.h"
#include <logdef.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <cstdio>
#include <cstring>
#include <stack>
#include <algorithm>
#include <driver/gpio.h>

#define ROTARY_SINGLE_INC 0
#define ROTARY_DOUBLE_INC 1

// the global access to the rotary knob
ESPRotary *Rotary = nullptr;
#define BUTTON_PIN GPIO_NUM_0  // Change to your actual button pin
#define SHORT_PRESS 1
#define LONG_PRESS  2
#define BUTTON_RELEASED 3
#define LONG_PRESS_THRESHOLD 400  // 400ms threshold
#define DEBOUNCE_TIME 25          // 25 ms debounce threshold


static QueueHandle_t buttonQueue;
static uint32_t lastPressTime = 0;
static TaskHandle_t pid = NULL;
static std::stack<RotaryObserver *> observers;

static void IRAM_ATTR button_isr_handler(void* arg) {
	uint32_t currentTime = esp_log_timestamp();
	// Ignore interrupts occurring within debounce time
	if (lastPressTime && ((currentTime - lastPressTime) < DEBOUNCE_TIME)) {
		return;
	}

	int buttonState = gpio_get_level(Rotary->getSw());

	if (buttonState == 0 ) { // Button pressed (active LOW)
		lastPressTime = currentTime;
	}
	else{ // Button released
		if( lastPressTime ){
			uint32_t pressDuration = currentTime - lastPressTime;
			uint8_t pressType = (pressDuration > LONG_PRESS_THRESHOLD) ? LONG_PRESS : SHORT_PRESS;
			xQueueSendFromISR(buttonQueue, &pressType, NULL);

			uint8_t releaseEvent = BUTTON_RELEASED;
			xQueueSendFromISR(buttonQueue, &releaseEvent, NULL);

			lastPressTime = 0;
		}
	}
}

// Observer task
void ObserverTask( void *args )
{
	ESPRotary &knob = *static_cast<ESPRotary*>(args);
	static int old_cnt = 0;
	int enc_count;
	while( true ) {
		// handle button events
		uint8_t event;
		if (xQueueReceive(buttonQueue, &event, 0) == pdTRUE) {
			if (event == SHORT_PRESS) {
				ESP_LOGI(FNAME,"Button short press detected");
				knob.sendPress();
			} else if (event == LONG_PRESS) {
				ESP_LOGI(FNAME,"Button long press detected");
				knob.sendLongPress();
			}else if (event == BUTTON_RELEASED) {
				ESP_LOGI(FNAME, "Button released");
				knob.sendRelease();
			}
		}
		// handle increment/decrement
		if( pcnt_unit_get_count(knob.pcnt_unit, &enc_count) != ESP_OK ) {
			ESP_LOGE(FNAME,"Error get counter");
		}
		if( abs( enc_count - old_cnt) > rotary_inc.get() )
		{
			// pcnt_counter_clear(PCNT_UNIT_0);
			// ESP_LOGI(FNAME,"Rotary counter %d", enc_count);
			int diff = (enc_count) - old_cnt;
			diff = diff / ( rotary_inc.get()+1 );
			// ESP_LOGI(FNAME,"Rotary diff %d", diff );
			old_cnt = enc_count;
			if( diff < 0 ) {
				// ESP_LOGI(FNAME,"Rotary down %d times",abs(diff) );
				knob.sendDown( abs(diff) );
			}
			else {
				// ESP_LOGI(FNAME,"Rotary up %d times", abs(diff) );
				knob.sendUp( abs(diff) );
			}
		}
		if( uxTaskGetStackHighWaterMark( pid ) < 256 ) {
			ESP_LOGW(FNAME,"Warning rotary task stack low: %d bytes", uxTaskGetStackHighWaterMark( pid ) );
		}
		vTaskDelay(100 / portTICK_PERIOD_MS);
	}
}

// Observer registration
void RotaryObserver::attach() {
	// ESP_LOGI(FNAME,"Attach obs: %p", obs );
	observers.push(this);
}
void RotaryObserver::detach() {
	// ESP_LOGI(FNAME,"Detach obs: %p", obs );
	if ( observers.empty() ) {
		ESP_LOGW(FNAME,"Hoppla observer stack empty on detach");
		return;
	}
	observers.pop();
}

// The rotary knob
ESPRotary::ESPRotary(gpio_num_t aclk, gpio_num_t adt, gpio_num_t asw) :
	clk(aclk),
	dt(adt),
	sw(asw)
{
	// Init. the button early
	gpio_set_direction(sw, GPIO_MODE_INPUT);
	gpio_pullup_en(sw); // Rotary Encoder Button
}

void ESPRotary::begin()
{
	gpio_set_direction(dt, GPIO_MODE_INPUT);
	gpio_set_direction(clk, GPIO_MODE_INPUT);
	gpio_pullup_en(dt);
	gpio_pullup_en(clk);

	// Init pulse counter unit
	pcnt_unit_config_t unit_config = {
		.low_limit = -32000,
		.high_limit = 32000,
		.intr_priority = 0,
		.flags = {0}
	};
	ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_unit));

	pcnt_chan_config_t chan_config = {
		.edge_gpio_num = clk, // pulse
		.level_gpio_num = dt, // direction
		.flags = {
			.invert_edge_input = 0,
			.invert_level_input = 0,
			.virt_edge_io_level = 0,
			.virt_level_io_level = 0,
			.io_loop_back = 0,
		},
	};
	ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_config, &pcnt_chan));

	// Decide on rotary hardware direction sense
	ESP_ERROR_CHECK(updateRotDir());
	ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan, PCNT_CHANNEL_LEVEL_ACTION_INVERSE, PCNT_CHANNEL_LEVEL_ACTION_KEEP));

	pcnt_glitch_filter_config_t filter_config = {
		.max_glitch_ns = 2000 // Ignore pulses shorter than 2000 ns
	};
	ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit, &filter_config));

	pcnt_unit_enable(pcnt_unit);
	pcnt_unit_clear_count(pcnt_unit);
	pcnt_unit_start(pcnt_unit);

	buttonQueue = xQueueCreate(10, sizeof(uint8_t));
	xTaskCreate(&ObserverTask, "knob", 5096, this, 14, &pid);

	gpio_config_t io_conf = {
			.pin_bit_mask = (1ULL << sw),
			.mode = GPIO_MODE_INPUT,
			.pull_up_en = GPIO_PULLUP_ENABLE,
			.pull_down_en = GPIO_PULLDOWN_DISABLE,
			.intr_type = GPIO_INTR_ANYEDGE
	};
	gpio_config(&io_conf);
	gpio_install_isr_service(0);
	gpio_isr_handler_add(sw, button_isr_handler, NULL);
}

esp_err_t ESPRotary::updateRotDir()
{
	pcnt_channel_edge_action_t rot_action = PCNT_CHANNEL_EDGE_ACTION_INCREASE;
	if( hardwareRevision.get() <= XCVARIO_21 ) {
		rot_action = PCNT_CHANNEL_EDGE_ACTION_DECREASE;
	}
	if ( rotary_dir.get() == 1 ) {
		// flip the knob sens
		rot_action = (rot_action == PCNT_CHANNEL_EDGE_ACTION_INCREASE) ? PCNT_CHANNEL_EDGE_ACTION_DECREASE : PCNT_CHANNEL_EDGE_ACTION_INCREASE;
	}
	return pcnt_channel_set_edge_action(pcnt_chan, rot_action, PCNT_CHANNEL_EDGE_ACTION_HOLD);
}

void ESPRotary::sendUp( int diff ) const
{
	// ESP_LOGI(FNAME,"Rotary down action");
	if (!observers.empty()) {
		observers.top()->up( diff );
	}
}

void ESPRotary::sendDown( int diff ) const
{
	// ESP_LOGI(FNAME,"Rotary up action");
	if (!observers.empty()) {
		observers.top()->down( diff );
	}
}

void ESPRotary::sendPress() const
{
	// ESP_LOGI(FNAME,"Pressed action");
	if (!observers.empty()) {
		observers.top()->press();
	}
}

void ESPRotary::sendRelease() const
{
	// ESP_LOGI(FNAME,"Release action");
	if (!observers.empty()) {
		observers.top()->release();
	}
}

void ESPRotary::sendLongPress() const
{
	// ESP_LOGI(FNAME,"Long pressed action");
	if (!observers.empty()) {
		observers.top()->longPress();
	}
}

void ESPRotary::sendEscape() const
{
	// ESP_LOGI(FNAME,"Rotary up action");
	if (!observers.empty()) {
		observers.top()->escape();
	}
}

bool ESPRotary::readSwitch() {
	return gpio_get_level(sw) == 0; // Fixme, this need to be a vritual level after debouncing is done
}



