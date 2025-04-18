
#include "sensor.h"

#include "Cipher.h"
#include "BME280_ESP32_SPI.h"
#include <driver/adc.h>
#include <driver/gpio.h>
#include "mcp3221.h"
#include "mcp4018.h"
#include "ESP32NVS.h"
#include "MP5004DP.h"
#include "MS4525DO.h"
#include "ABPMRR.h"
#include "MCPH21.h"
#include "BMPVario.h"
#include "BTSender.h"
#include "BLESender.h"
#include "Protocols.h"
#include "DS18B20.h"
#include "Setup.h"
#include "ESPAudio.h"
#include "SetupMenu.h"
#include "ESPRotary.h"
#include "AnalogInput.h"
#include "Atmosphere.h"
#include "IpsDisplay.h"
#include "S2F.h"
#include "Version.h"
#include "Polars.h"
#include "Flarm.h"
#include "Blackboard.h"
#include "SetupMenuValFloat.h"

#include "quaternion.h"
#include "wmm/geomag.h"
#include <SPI.h>
#include <AdaptUGC.h>
#include <OTA.h>
#include "SetupNG.h"
#include "Switch.h"
#include "AverageVario.h"

#include "MPU.hpp"        // main file, provides the class itself
#include "mpu/math.hpp"   // math helper for dealing with MPU data
#include "mpu/types.hpp"  // MPU data types and definitions
#include "I2Cbus.hpp"
#include "KalmanMPU6050.h"
#include "WifiApp.h"
#include "WifiClient.h"
#include "Serial.h"
#include "LeakTest.h"
#include "Units.h"
#include "Flap.h"
#include "SPL06-007.h"
#include "StraightWind.h"
#include "CircleWind.h"
#include <coredump_to_server.h>
#include "canbus.h"
#include "Router.h"

#include "sdkconfig.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_task_wdt.h>
#include <esp_log.h>
#include <esp32/rom/miniz.h>
#include <esp32-hal-adc.h> // needed for adc pin reset
#include <soc/sens_reg.h> // needed for adc pin reset
#include <esp_sleep.h>
#include <esp_wifi.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <nvs_flash.h>
#include <string>
#include <cstdio>
#include <cstring>
#include "DataMonitor.h"
#include "AdaptUGC.h"
#include "CenterAid.h"

// #include "sound.h"

/*
BMP:
    SCK - This is the SPI Clock pin, its an input to the chip
    SDO - this is the Serial Data Out / Master In Slave Out pin (MISO), for data sent from the BMP183 to your processor
    SDI - this is the Serial Data In / Master Out Slave In pin (MOSI), for data sent from your processor to the BME280
    CS - this is the Chip Select pin, drop it low to start an SPI transaction. Its an input to the chip
 */

#define CS_bme280BA GPIO_NUM_26   // before CS pin 33
#define CS_bme280TE GPIO_NUM_33   // before CS pin 26

#define SPL06_007_BARO 0x77
#define SPL06_007_TE   0x76

MCP3221 *MCP=0;
DS18B20  ds18b20( GPIO_NUM_23 );  // GPIO_NUM_23 standard, alternative  GPIO_NUM_17

AirspeedSensor *asSensor=0;
StraightWind theWind;

xSemaphoreHandle xMutex=NULL;
xSemaphoreHandle spiMutex=NULL;

S2F Speed2Fly;
Protocols OV( &Speed2Fly );

AnalogInput Battery( (22.0+1.2)/1200, ADC_ATTEN_DB_0, ADC_CHANNEL_7, ADC_UNIT_1 );

TaskHandle_t apid = NULL;
TaskHandle_t bpid = NULL;
TaskHandle_t tpid = NULL;
TaskHandle_t dpid = NULL;

e_wireless_type wireless;

PressureSensor *baroSensor = 0;
PressureSensor *teSensor = 0;

AdaptUGC *MYUCG = 0;  // ( SPI_DC, CS_Display, RESET_Display );
IpsDisplay *display = 0;
CenterAid  *centeraid = 0;

OTA *ota = 0;

ESPRotary Rotary;
SetupMenu  *Menu = 0;
DataMonitor DM;

// Gyro and acceleration sensor
I2C_t& i2c = i2c1;  // i2c0 or i2c1
I2C_t& i2c_0 = i2c0;  // i2c0 or i2c1
MPU_t MPU;         // create an object

// Magnetic sensor / compass
Compass *compass = 0;
BTSender btsender;
BLESender blesender;

float baroP=0; // barometric pressure
static float teP=0;   // TE pressure
static float temperature=15.0;
static float xcvTemp=15.0;
static long unsigned int _millis = 0;
long unsigned int _gps_millis = 0;


static float battery=0.0;
float dynamicP; // Pitot

float slipAngle = 0.0;

// global color variables for adaptable display variant
uint8_t g_col_background=255; // black
uint8_t g_col_highlight=0;
uint8_t g_col_header_r=101+g_col_background/5;
uint8_t g_col_header_g=108+g_col_background/5;
uint8_t g_col_header_b=g_col_highlight;
uint8_t g_col_header_light_r=161-g_col_background/4;
uint8_t g_col_header_light_g=168-g_col_background/3;
uint8_t g_col_header_light_b=g_col_highlight;
uint16_t gear_warning_holdoff = 0;
uint8_t gyro_flash_savings=0;

t_global_flags gflags = { true, false, false, false, false, false, false, false, false, false, false, false, false, false, false };

int  ccp=60;
float tas = 0;
float cas = 0;
float aTE = 0;
float alt_external;
float altSTD;
float netto = 0;
float as2f = 0;
float s2f_delta = 0;
float polar_sink = 0;

float      stall_alarm_off_kmh=0;
uint16_t   stall_alarm_off_holddown=0;

int count=0;
unsigned long int flarm_alarm_holdtime=0;
int the_can_mode = CAN_MODE_MASTER;
int active_screen = 0;  // 0 = Vario

float mpu_target_temp=45.0;

AdaptUGC *egl = 0;


float getTAS() { return tas; };

bool do_factory_reset() {
	return( SetupCommon::factoryReset() );
}

void drawDisplay(void *pvParameters){
	while (1) {
		if( Flarm::bincom ) {
			if( gflags.flarmDownload == false ) {
				gflags.flarmDownload = true;
				display->clear();
				Flarm::drawDownloadInfo();
			}
			// Flarm IGC download is running, display will be blocked, give Flarm
			// download all cpu power.
			vTaskDelay(20/portTICK_PERIOD_MS);
			continue;
		}
		else if( gflags.flarmDownload == true ) {
			gflags.flarmDownload = false;
			display->clear();
		}
		// TickType_t dLastWakeTime = xTaskGetTickCount();
		if( gflags.inSetup != true ) {
			float t=OAT.get();
			if( gflags.validTemperature == false )
				t = DEVICE_DISCONNECTED_C;
			float airspeed = 0;
			if( airspeed_mode.get() == MODE_IAS )
				airspeed = ias.get();
			else if( airspeed_mode.get() == MODE_TAS )
				airspeed = tas;
			else if( airspeed_mode.get() == MODE_CAS )
				airspeed = cas;
			else
				airspeed = ias.get();

			// Stall Warning Screen
			if( stall_warning.get() && gload_mode.get() != GLOAD_ALWAYS_ON ){  // In aerobatics stall warning is contra productive, we concentrate on G-Load Display if permanent enabled
				if( gflags.stall_warning_armed ){
					float acceleration=IMU::getGliderAccelZ();
					if( acceleration < 0.3 )
						acceleration = 0.3;  // limit acceleration effect to minimum 30% of 1g
					float acc_stall= stall_speed.get() * sqrt( acceleration + ( ballast.get()/100));  // accelerated and ballast(ed) stall speed
					if( ias.get() < acc_stall && ias.get() > acc_stall*0.7 ){
						if( !gflags.stall_warning_active ){
							Audio::alarm( true, max_volume.get() );
							display->drawWarning( "! STALL !", true );
							gflags.stall_warning_active = true;
						}
					}
					else{
						if( gflags.stall_warning_active ){
							Audio::alarm( false );
							display->clear();
							gflags.stall_warning_active = false;
						}
					}
					if( ias.get() < stall_alarm_off_kmh ){
						stall_alarm_off_holddown++;
						if( stall_alarm_off_holddown > 1200 ){  // ~30 seconds holddown
							gflags.stall_warning_armed = false;
							stall_alarm_off_holddown=0;
						}
					}
					else{
						stall_alarm_off_holddown=0;
					}
				}
				else{
					if( ias.get() > stall_speed.get() ){
						gflags.stall_warning_armed = true;
						stall_alarm_off_holddown=0;
					}
				}
			}
			if( gear_warning.get() ){
				if( !gear_warning_holdoff ){
					int gw = 0;
					if( gear_warning.get() == GW_EXTERNAL ){
						gw = gflags.gear_warn_external;
					}else{
						gw = digitalRead( SetupMenu::getGearWarningIO() );
						if( gear_warning.get() == GW_FLAP_SENSOR_INV || gear_warning.get() == GW_S2_RS232_RX_INV ){
							gw = !gw;
						}
					}
					if( gw ){
						if( ESPRotary::readSwitch() ){   // Acknowledge Warning -> Warning OFF
							gear_warning_holdoff = 25000;  // ~500 sec
							Audio::alarm( false );
							display->clear();
							gflags.gear_warning_active = false;
							SetupMenu::catchFocus( false );
						}
						else if( !gflags.gear_warning_active && !gflags.stall_warning_active ){
							Audio::alarm( true, max_volume.get() );
							display->drawWarning( "! GEAR !", false );
							gflags.gear_warning_active = true;
							SetupMenu::catchFocus( true );
						}
					}
					else{
						if( gflags.gear_warning_active ){
							SetupMenu::catchFocus( false );
							Audio::alarm( false );
							display->clear();
							gflags.gear_warning_active = false;
						}
					}
				}
				else{
					gear_warning_holdoff--;
				}
			}

			// Flarm Warning Screen
			if( flarm_warning.get() && !gflags.stall_warning_active && Flarm::alarmLevel() >= flarm_warning.get() ){ // 0 -> Disable
				// ESP_LOGI(FNAME,"Flarm::alarmLevel: %d, flarm_warning.get() %d", Flarm::alarmLevel(), flarm_warning.get() );
				if( !gflags.flarmWarning ) {
					gflags.flarmWarning = true;
					delay(100);
					display->clear();
				}
				flarm_alarm_holdtime = millis()+flarm_alarm_time.get()*1000;
			}
			else{
				if( gflags.flarmWarning && (millis() > flarm_alarm_holdtime) ){
					gflags.flarmWarning = false;
					display->clear();
					Audio::alarm( false );
				}
			}
			if( gflags.flarmWarning )
				Flarm::drawFlarmWarning();
			// G-Load Display
			// ESP_LOGI(FNAME,"Active Screen = %d", active_screen );
			if( ((IMU::getGliderAccelZ() > gload_pos_thresh.get() || IMU::getGliderAccelZ() < gload_neg_thresh.get()) && gload_mode.get() == GLOAD_DYNAMIC ) ||
					( gload_mode.get() == GLOAD_ALWAYS_ON ) || (active_screen == SCREEN_GMETER)  )
			{
				if( !gflags.gLoadDisplay ){
					gflags.gLoadDisplay = true;
				}
			}
			else{
				if( gflags.gLoadDisplay ) {
					gflags.gLoadDisplay = false;
				}
			}
			if( gflags.gLoadDisplay ) {
				display->drawLoadDisplay( IMU::getGliderAccelZ() );
			}
			if( active_screen == SCREEN_HORIZON ) {
				float roll =  IMU::getRollRad();
				float pitch = IMU::getPitchRad();
				display->drawHorizon( pitch, roll, 0 );
				gflags.horizon = true;
			}
			else{
				gflags.horizon = false;
			}
			// G-Load Alarm when limits reached
			if( gload_mode.get() != GLOAD_OFF  ){
				if( IMU::getGliderAccelZ() > gload_pos_limit.get() || IMU::getGliderAccelZ() < gload_neg_limit.get()  ){
					if( !gflags.gload_alarm ) {
						Audio::alarm( true, gload_alarm_volume.get() );
						gflags.gload_alarm = true;
					}
				}else
				{
					if( gflags.gload_alarm ) {
						Audio::alarm( false );
						gflags.gload_alarm = false;
					}
				}
			}
			// Vario Screen
			if( !(gflags.stall_warning_active || gflags.gear_warning_active || gflags.flarmWarning || gflags.gLoadDisplay || gflags.horizon )  ) {
				// ESP_LOGI(FNAME,"TE=%2.3f", te_vario.get() );
				display->drawDisplay( airspeed, te_vario.get(), aTE, polar_sink, altitude.get(), t, battery, s2f_delta, as2f, average_climb.get(), Switch::getCruiseState(), gflags.standard_setting, flap_pos.get() );
			}
			if( screen_centeraid.get() ){
				if( centeraid ){
					centeraid->tick();
				}
			}
		}
		vTaskDelay(20/portTICK_PERIOD_MS);
		if( uxTaskGetStackHighWaterMark( dpid ) < 512  )
			ESP_LOGW(FNAME,"Warning drawDisplay stack low: %d bytes", uxTaskGetStackHighWaterMark( dpid ) );
	}
}

// depending on mode calculate value for Audio and set values accordingly
void doAudio(){
	polar_sink = Speed2Fly.sink( ias.get() );
	float netto = te_vario.get() - polar_sink;
	as2f = Speed2Fly.speed( netto, !Switch::getCruiseState() );
	s2f_delta = s2f_delta + ((as2f - ias.get()) - s2f_delta)* (1/(s2f_delay.get()*10)); // low pass damping moved to the correct place
	// ESP_LOGI( FNAME, "te: %f, polar_sink: %f, netto %f, s2f: %f  delta: %f", aTES2F, polar_sink, netto, as2f, s2f_delta );
	if( vario_mode.get() == VARIO_NETTO || (Switch::getCruiseState() &&  (vario_mode.get() == CRUISE_NETTO)) ){
		if( netto_mode.get() == NETTO_RELATIVE )
			Audio::setValues( te_vario.get() - polar_sink + Speed2Fly.circlingSink( ias.get() ), s2f_delta );
		else if( netto_mode.get() == NETTO_NORMAL )
			Audio::setValues( te_vario.get() - polar_sink, s2f_delta );
	}
	else {
		Audio::setValues( te_vario.get(), s2f_delta );
	}
}

void audioTask(void *pvParameters){
	while (1)
	{
		TickType_t xLastWakeTime = xTaskGetTickCount();
		if( Flarm::bincom ) {
			// Flarm IGC download is running, audio will be blocked, give Flarm
			// download all cpu power.
			vTaskDelayUntil(&xLastWakeTime, 100/portTICK_PERIOD_MS);
			continue;
		}
		doAudio();
		Router::routeXCV();
		if( uxTaskGetStackHighWaterMark( apid )  < 512 )
			ESP_LOGW(FNAME,"Warning audio task stack low: %d", uxTaskGetStackHighWaterMark( apid ) );
		vTaskDelayUntil(&xLastWakeTime, 100/portTICK_PERIOD_MS);
	}
}


static void grabMPU()
{
	// Automatically trac the gyro bias
	static int32_t cur_gyro_bias[3];
	const int MAXDRIFT         = 2;    // °/s maximum drift that is automatically compensated on ground
	const int NUM_GYRO_SAMPLES = 3000; // 10 per second -> 5 minutes, so T has been settled after power on
	static uint16_t num_gyro_samples = 0;

	// Read the IMU registers and check the output
	if( IMU::MPU6050Read() == ESP_OK )
	{
		// Do the gyro auto bias
		vector_ijk gyroDPS = IMU::getGliderGyro();
		// ESP_LOGI(FNAME,"Gyro:\t%4f\t%4f\t%4f", gyroDPS.a, gyroDPS.b, gyroDPS.c);
		// vector_ijk accl = IMU::getGliderAccel();
		// if (compass != nullptr) {
		// 	ESP_LOGI(FNAME,"Accl:\t%4f\t%4f\t%4f\tL%.2f Gyro:\t%4f\t%4f\t%4f Mag:\t%4f\t%4f\t%4f", accl.a, accl.b, accl.c, accl.get_norm(), 
		// 		gyroDPS.a, gyroDPS.b, gyroDPS.c,
		// 		compass->rawX(), compass->rawY(), compass->rawZ());
		// }

		float GS=0; // Autoleveling Gyro feature only with GPS and GS close to zero to avoid triggering at push back taxi with zero AS
		bool gpsOK = Flarm::getGPSknots( GS );
		// ESP_LOGI(FNAME,"GS=%.3f %d", GS, gpsOK );
		if( gpsOK && GS < 2 && ias.get() < 5 ){  // GPS status, groundspeed and airspeed regarded for still stand
			// check low rotation on all 3 axes = on ground
			if( abs( gyroDPS.a ) < MAXDRIFT && abs( gyroDPS.b ) < MAXDRIFT && abs( gyroDPS.c ) < MAXDRIFT ) {
				num_gyro_samples++;
				cur_gyro_bias[0] = IMU::getRawGyroX();
				cur_gyro_bias[1] = IMU::getRawGyroY();
				cur_gyro_bias[2] = IMU::getRawGyroZ();
				if( num_gyro_samples > NUM_GYRO_SAMPLES ) { // every 5 minute (3000 samples) recalculate offset
					mpud::raw_axes_t gb;
					mpud::raw_axes_t gbo = MPU.getGyroOffset();
					for(int i=0; i<3; i++){
						gb[i]  = gbo[i] -(( (cur_gyro_bias)[i]/(NUM_GYRO_SAMPLES*4)) ); // translate to 1000 DPS
						cur_gyro_bias[i] = 0;
					}
					ESP_LOGI(FNAME,"New gyro offset X/Y/Z: OLD:%d/%d/%d NEW:%d/%d/%d", gbo.x, gbo.y, gbo.z, gb.x, gb.y, gb.z );
					if( (abs( gbo.x-gb.x ) > 0) || (abs( gbo.y-gb.y ) > 0) || (abs( gbo.z-gb.z ) > 0)  ){  // any delta is directly set in RAM
						ESP_LOGI(FNAME,"Set new gyro offset X/Y/Z: OLD:%d/%d/%d NEW:%d/%d/%d", gbo.x, gbo.y, gbo.z, gb.x, gb.y, gb.z );
						MPU.setGyroOffset( gb );
					}
					// if we have temperature control, we check if control is locked, otherwise we have no idea but anyway takeover better offset
					if( (HAS_MPU_TEMP_CONTROL && (MPU.getSiliconTempStatus() == MPU_T_LOCKED)) || !HAS_MPU_TEMP_CONTROL ){
						if( (abs( gbo.x-gb.x ) > 1 || abs( gbo.y-gb.y ) > 1 || abs( gbo.z-gb.z ) > 1) && gyro_flash_savings<5 ){ // Set only changes > 1 in Flash and only 5 times per boot
							gyro_bias.set( gb );
							ESP_LOGI(FNAME,"Store the new offset also in Flash, store number: %d", gyro_flash_savings );
							gyro_flash_savings++;
						}
					}
					num_gyro_samples = 0;
				}
			}
		}
		IMU::Process();
	}

}

static void toyFeed()
{
	xSemaphoreTake(xMutex,portMAX_DELAY );
	// reduce also messages from 10 per second to 5 per second to reduce load in XCSoar
	// maybe just 1 or 2 per second
	static char lb[150];

	if( ahrs_rpyl_dataset.get() ){
		OV.sendNMEA( P_AHRS_RPYL, lb, baroP, dynamicP, te_vario.get(), OAT.get(), ias.get(), tas, MC.get(), bugs.get(), ballast.get(), Switch::getCruiseState(), altitude.get(), gflags.validTemperature,
				IMU::getGliderAccelX(), IMU::getGliderAccelY(), IMU::getGliderAccelZ(), IMU::getGliderGyroX(), IMU::getGliderGyroY(), IMU::getGliderGyroZ() );
		OV.sendNMEA( P_AHRS_APENV1, lb, baroP, dynamicP, te_vario.get(), OAT.get(), ias.get(), tas, MC.get(), bugs.get(), ballast.get(), Switch::getCruiseState(), altitude.get(), gflags.validTemperature,
				IMU::getGliderAccelX(), IMU::getGliderAccelY(), IMU::getGliderAccelZ(), IMU::getGliderGyroX(), IMU::getGliderGyroY(), IMU::getGliderGyroZ() );
	}
	if( nmea_protocol.get() == BORGELT ) {
		OV.sendNMEA( P_BORGELT, lb, baroP, dynamicP, te_vario.get(), OAT.get(), ias.get(), tas, MC.get(), bugs.get(), ballast.get(), Switch::getCruiseState(), altSTD, gflags.validTemperature  );
		OV.sendNMEA( P_GENERIC, lb, baroP, dynamicP, te_vario.get(), OAT.get(), ias.get(), tas, MC.get(), bugs.get(), ballast.get(), Switch::getCruiseState(), altSTD, gflags.validTemperature  );
	}
	else if( nmea_protocol.get() == OPENVARIO ){
		OV.sendNMEA( P_OPENVARIO, lb, baroP, dynamicP, te_vario.get(), OAT.get(), ias.get(), tas, MC.get(), bugs.get(), ballast.get(), Switch::getCruiseState(), altitude.get(), gflags.validTemperature  );
	}
	else if( nmea_protocol.get() == CAMBRIDGE ){
		OV.sendNMEA( P_CAMBRIDGE, lb, baroP, dynamicP, te_vario.get(), OAT.get(), ias.get(), tas, MC.get(), bugs.get(), ballast.get(), Switch::getCruiseState(), altitude.get(), gflags.validTemperature  );
	}
	else if( nmea_protocol.get() == XCVARIO ) {
		OV.sendNMEA( P_XCVARIO, lb, baroP, dynamicP, te_vario.get(), OAT.get(), ias.get(), tas, MC.get(), bugs.get(), ballast.get(), Switch::getCruiseState(), altitude.get(), gflags.validTemperature,
				IMU::getGliderAccelX(), IMU::getGliderAccelY(), IMU::getGliderAccelZ(), IMU::getGliderGyroX(), IMU::getGliderGyroY(), IMU::getGliderGyroZ() );
	}
	else if( nmea_protocol.get() == NMEA_OFF ) {
		;
	}
	else
		ESP_LOGE(FNAME,"Protocol %d not supported error", nmea_protocol.get() );
	xSemaphoreGive(xMutex);
}


void clientLoop(void *pvParameters)
{
	int ccount = 0;
	gflags.validTemperature = true;
	while (true)
	{
		TickType_t xLastWakeTime = xTaskGetTickCount();
		ccount++;
		aTE += (te_vario.get() - aTE)* (1/(10*vario_av_delay.get()));
		if( gflags.haveMPU ) {
			grabMPU();
		}
		if( !(ccount%5) )
		{
			double tmpalt = altitude.get(); // get pressure from altitude
			if( (fl_auto_transition.get() == 1) && ((int)( Units::meters2FL( altitude.get() )) + (int)(gflags.standard_setting) > transition_alt.get() ) ) {
				ESP_LOGI(FNAME,"Above transition altitude");
				baroP = baroSensor->calcPressure(1013.25, tmpalt); // above transition altitude
			}
			else {
				baroP = baroSensor->calcPressure( QNH.get(), tmpalt);
			}
			dynamicP = Atmosphere::kmh2pascal(ias.get());
			tas = Atmosphere::TAS2( ias.get(), altitude.get(), OAT.get() );
			if( airspeed_mode.get() == MODE_CAS )
				cas = Atmosphere::CAS( dynamicP );
			if( gflags.haveMPU && HAS_MPU_TEMP_CONTROL ){
				MPU.temp_control( ccount, xcvTemp );
			}
			if( IMU::getGliderAccelZ() > gload_pos_max.get() ){
				gload_pos_max.set( IMU::getGliderAccelZ() );
			}else if( IMU::getGliderAccelZ() < gload_neg_max.get() ){
				gload_neg_max.set( IMU::getGliderAccelZ() );
			}
			toyFeed();
			Router::routeXCV();
			if( true && !(ccount%5) ) { // todo need a mag_hdm.valid() flag
				if( compass_nmea_hdm.get() ) {
					xSemaphoreTake( xMutex, portMAX_DELAY );
					OV.sendNmeaHDM( mag_hdm.get() );
					xSemaphoreGive( xMutex );
				}

				if( compass_nmea_hdt.get() ) {
					xSemaphoreTake( xMutex, portMAX_DELAY );
					OV.sendNmeaHDT( mag_hdt.get() );
					xSemaphoreGive( xMutex );
				}
			}
			esp_task_wdt_reset();
			if( uxTaskGetStackHighWaterMark( bpid ) < 512 )
				ESP_LOGW(FNAME,"Warning client task stack low: %d bytes", uxTaskGetStackHighWaterMark( bpid ) );
		}
		vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));
	}
}

void readSensors(void *pvParameters){
	int client_sync_dataIdx = 0;
	float tasraw = 0;
	while (1)
	{
		count++;   // 10x per second
		TickType_t xLastWakeTime = xTaskGetTickCount();
		float T=OAT.get();
		if( !gflags.validTemperature ) {
			T= 15 - ( (altitude.get()/100) * 0.65 );
			// ESP_LOGW(FNAME,"T invalid, using 15 deg");
		}
		// ESP_LOGI(FNAME,"Start");
		if( gflags.haveMPU  )  // 3th Generation HW, MPU6050 avail and feature enabled
		{
			grabMPU();  // IMU
		}
		// ESP_LOGI(FNAME,"IMU");
		if( asSensor ){  // AS differential Sensor
			bool ok=false;
			float p = asSensor->readPascal(60, ok);
			if( ok )
				dynamicP = p;
		}
		_millis=millis();
		struct timeval tv;
		gettimeofday(&tv, NULL);
		// ESP_LOGI(FNAME,"AS");
		xSemaphoreTake(xMutex,portMAX_DELAY );   // Static Pressure
		bool bok=false;
		float bp = baroSensor->readPressure(bok);
		// ESP_LOGI(FNAME,"Baro");
		bool tok=false;
		float tp = teSensor->readPressure(tok);  // TE Pressure
		xSemaphoreGive(xMutex);
		// ESP_LOGI(FNAME,"TE, Delta: %d", (int)(millis() - m));
		if( logging.get() == LOG_SENSOR_RAW ){
			char log[SSTRLEN];
			sprintf( log, "$SENS;");
			int pos = strlen(log);
			long int delta = _millis - _gps_millis;
			if( delta < 0 )
				delta += 1000;
			sprintf( log+pos, "%d.%03d,%ld,%.3f,%.3f,%.3f,%.2f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f", (int)(tv.tv_sec%(60*60*24)), (int)(tv.tv_usec / 1000), delta, bp, tp, dynamicP, T, IMU::getGliderAccelX(), IMU::getGliderAccelY(), IMU::getGliderAccelZ(),
					IMU::getGliderNogateGyroX(), IMU::getGliderNogateGyroY(), IMU::getGliderNogateGyroZ() );
			if( compass ){
				pos=strlen(log);
				sprintf( log+pos,",%.4f,%.4f,%.4f", compass->rawX(), compass->rawY(), compass->rawZ());
			}
			pos=strlen(log);
			sprintf( log+pos, "\n");
			Router::sendXCV( log );
			ESP_LOGI(FNAME,"%s", log );
		}
		if( tok )
			teP = tp;
		if( bok )
			baroP = bp;
		float te = bmpVario.readTE( tasraw, teP );   // TE value caclulation
		if( (int( te_vario.get()*20 +0.5 ) != int( te*20 +0.5)) || !(count%10) ){  // a bit more fine granular updates than 0.1 m/s as of sound
					te_vario.set( te );  // max 10x per second
		}
		if( !(count%10) ){  // every second read temperature of baro sensor
					bool ok=false;
					float xt = baroSensor->readTemperature(ok);
					if( ok ){
						xcvTemp = xt;
					}
		}
		float iasraw = Atmosphere::pascal2kmh( dynamicP );
		if( baroP != 0 )
			tasraw =  Atmosphere::TAS( iasraw , baroP, T);  // True airspeed in km/h

		// ESP_LOGI("FNAME","P: %f  IAS:%f", dynamicP, iasraw );

		if( airspeed_mode.get() == MODE_CAS ){
			float casraw=Atmosphere::CAS( dynamicP );
			cas += (casraw-cas)*0.25;       // low pass filter
			// ESP_LOGI(FNAME,"IAS=%f, TAS=%f CAS=%f baro=%f", iasraw, tasraw, cas, baroP );
		}
		static float new_ias = 0;
		new_ias = ias.get() + (iasraw - ias.get())*0.25;
		if( (int( ias.get()+0.5 ) != int( new_ias+0.5 ) ) || !(count%20) ){
			ias.set( new_ias );  // low pass filter
		}
		if( airspeed_max.get() < ias.get() ){
			airspeed_max.set( ias.get() );
		}
		// ESP_LOGI("FNAME","P: %f  IAS:%f IASF: %d", dynamicP, iasraw, ias );
		if( !compass || !(compass->externalData()) ){
			tas += (tasraw-tas)*0.25;       // low pass filter
		}
		// ESP_LOGI(FNAME,"IAS=%f, T=%f, TAS=%f baroP=%f", ias, T, tas, baroP );

		// Slip angle estimation
		float as = tas/3.6;                  // tas in m/s
		const float K = 4000 * 180/M_PI;      // airplane constant and Ay correction factor
		if( tas > 25.0 ){
			slipAngle += ((IMU::getGliderAccelY()*K / (as*as)) - slipAngle)*0.09;   // with atan(x) = x for small x
			// ESP_LOGI(FNAME,"AS: %f m/s, CURSL: %f°, SLIP: %f", as, IMU::getGliderAccelY()*K / (as*as), slipAngle );
		}

		// ESP_LOGI(FNAME,"count %d ccp %d", count, ccp );
		if( !(count % ccp) ) {
			AverageVario::recalcAvgClimb();
		}
		if (FLAP) { FLAP->progress(); }

		// ESP_LOGI(FNAME,"Baro Pressure: %4.3f", baroP );
		float altSTD = 0;
		if( Flarm::validExtAlt() && alt_select.get() == AS_EXTERNAL )
			altSTD = alt_external;
		else
			altSTD = baroSensor->calcAltitudeSTD( baroP );
		float new_alt = 0;
		if( alt_select.get() == AS_TE_SENSOR ) // TE
			new_alt = bmpVario.readAVGalt();
		else if( alt_select.get() == AS_BARO_SENSOR  || alt_select.get() == AS_EXTERNAL ){ // Baro or external
			if(  alt_unit.get() == ALT_UNIT_FL ) { // FL, always standard
				new_alt = altSTD;
				gflags.standard_setting = true;
				// ESP_LOGI(FNAME,"au: %d", alt_unit.get() );
			}else if( (fl_auto_transition.get() == 1) && ((int)Units::meters2FL( altSTD ) + (int)(gflags.standard_setting) > transition_alt.get() ) ) { // above transition altitude
				new_alt = altSTD;
				gflags.standard_setting = true;
				ESP_LOGI(FNAME,"auto:%d alts:%f ss:%d ta:%f", fl_auto_transition.get(), altSTD, gflags.standard_setting, transition_alt.get() );
			}
			else {
				if( Flarm::validExtAlt() && alt_select.get() == AS_EXTERNAL )
					new_alt = altSTD + ( QNH.get()- 1013.25)*8.2296;  // correct altitude according to ISA model = 27ft / hPa
				else
					new_alt = baroSensor->calcAltitude( QNH.get(), baroP );
				gflags.standard_setting = false;
				// ESP_LOGI(FNAME,"QNH %f baro: %f alt: %f SS:%d", QNH.get(), baroP, alt, gflags.standard_setting  );
			}
		}
		if( (int( new_alt +0.5 ) != int( altitude.get() +0.5 )) || !(count%20) ){
			// ESP_LOGI(FNAME,"New Altitude: %.1f", new_alt );
			altitude.set( new_alt );
		}

		aTE = bmpVario.readAVGTE();
		doAudio();

		if( !Flarm::bincom && ((count % 2) == 0 ) ){
			toyFeed();
			vTaskDelay(2/portTICK_PERIOD_MS);
		}
		Router::routeXCV();
		// ESP_LOGI(FNAME,"Compass, have sensor=%d  hdm=%d ena=%d", compass->haveSensor(),  compass_nmea_hdt.get(),  compass_enable.get() );
		if( compass ){
			if( !Flarm::bincom && ! compass->calibrationIsRunning() ) {
				// Trigger heading reading and low pass filtering. That job must be
				// done periodically.
				bool ok;
				float heading = compass->getGyroHeading( &ok );
				if(ok){
					if( (int)heading != (int)mag_hdm.get() && !(count%10) ){
						mag_hdm.set( heading );
					}
					if( !(count%5) && compass_nmea_hdm.get() == true ) {
						xSemaphoreTake( xMutex, portMAX_DELAY );
						OV.sendNmeaHDM( heading );
						xSemaphoreGive( xMutex );
					}
				}
				else{
					if( mag_hdm.get() != -1 )
						mag_hdm.set( -1 );
				}
				float theading = compass->filteredTrueHeading( &ok );
				if(ok){
					if( (int)theading != (int)mag_hdt.get() && !(count%10) ){
						mag_hdt.set( theading );
					}
					if( !(count%5) && ( compass_nmea_hdt.get() == true )  ) {
						xSemaphoreTake( xMutex, portMAX_DELAY );
						OV.sendNmeaHDT( theading );
						xSemaphoreGive( xMutex );
					}
				}
				else{
					if( mag_hdt.get() != -1 )
						mag_hdt.set( -1 );
				}
			}
		}
		if( IMU::getGliderAccelZ() > gload_pos_max.get() ){
			gload_pos_max.set( IMU::getGliderAccelZ() );
		}else if( IMU::getGliderAccelZ() < gload_neg_max.get() ){
			gload_neg_max.set( IMU::getGliderAccelZ() );
		}

		// Check on new clients connecting
		if ( CAN && CAN->GotNewClient() ) { // todo use also for Wifi client?!
			while( client_sync_dataIdx < SetupCommon::numEntries() ) {
				if ( SetupCommon::syncEntry(client_sync_dataIdx++) ) {
					break; // Hit entry to actually sync and send data
				}
			}
			if ( client_sync_dataIdx >= SetupCommon::numEntries() ) {
				// Synch complete
				client_sync_dataIdx = 0;
				CAN->ResetNewClient();
			}
		}
		if( gflags.haveMPU && HAS_MPU_TEMP_CONTROL ){
			// ESP_LOGI(FNAME,"MPU temp control; T=%.2f", MPU.getTemperature() );
			MPU.temp_control( count, xcvTemp );
		}
		esp_task_wdt_reset();
		if( uxTaskGetStackHighWaterMark( bpid ) < 512 )
			ESP_LOGW(FNAME,"Warning sensor task stack low: %d bytes", uxTaskGetStackHighWaterMark( bpid ) );
		vTaskDelayUntil(&xLastWakeTime, 100/portTICK_PERIOD_MS);
	}
}

static int ttick = 0;
static float temp_prev = -3000;

void readTemp(void *pvParameters){
	float t=15.0;
	while (1) {
		TickType_t xLastWakeTime = xTaskGetTickCount();
		battery = Battery.get();
		// ESP_LOGI(FNAME,"Battery=%f V", battery );
		if( !SetupCommon::isClient() ) {  // client Vario will get Temperature info from main Vario
			if( !gflags.inSetup )
				t = ds18b20.getTemp();
			// ESP_LOGI(FNAME,"Temp %f", t );
			if( t ==  DEVICE_DISCONNECTED_C ) {
				if( gflags.validTemperature == true ) {
					ESP_LOGI(FNAME,"Temperatur Sensor disconnected");
					gflags.validTemperature = false;
				}
			}
			else
			{
				if( gflags.validTemperature == false ) {
					ESP_LOGI(FNAME,"Temperatur Sensor connected");
					gflags.validTemperature = true;
				}
				// ESP_LOGI(FNAME,"temperature=%2.1f", temperature );
				temperature +=  (t - temperature) * 0.3; // A bit low pass as strategy against toggling
				temperature = std::round(temperature*10)/10;
				if( temperature != temp_prev ){
					OAT.set( temperature );
					ESP_LOGI(FNAME,"NEW temperature=%2.1f, prev T=%2.1f", temperature, temp_prev );
					temp_prev = temperature;
				}
			}
			// ESP_LOGV(FNAME,"T=%f", temperature );
			Flarm::tick();
			if( compass )
				compass->tick();
		}else{
			if( (OAT.get() > -55.0) && (OAT.get() < 85.0) )
				gflags.validTemperature = true;
		}
		theWind.tick();
		CircleWind::tick();
		Flarm::progress();
		esp_task_wdt_reset();
		if( (ttick++ % 50) == 0) {
			ESP_LOGI(FNAME,"Free Heap: %d bytes", heap_caps_get_free_size(MALLOC_CAP_8BIT) );
			if( uxTaskGetStackHighWaterMark( tpid ) < 256 )
				ESP_LOGW(FNAME,"Warning temperature task stack low: %d bytes", uxTaskGetStackHighWaterMark( tpid ) );
			if( heap_caps_get_free_size(MALLOC_CAP_8BIT) < 20000 )
				ESP_LOGW(FNAME,"Warning heap_caps_get_free_size getting low: %d", heap_caps_get_free_size(MALLOC_CAP_8BIT));
		}
		if( (ttick%5) == 0 ){
			SetupCommon::commitDirty();
		}
		vTaskDelayUntil(&xLastWakeTime, 1000/portTICK_PERIOD_MS);
	}
}

static esp_err_t _coredump_to_server_begin_cb(void * priv)
{
	ets_printf("================= CORE DUMP START =================\r\n");
	return ESP_OK;
}

static esp_err_t _coredump_to_server_end_cb(void * priv)
{
	ets_printf("================= CORE DUMP END ===================\r\n");
	return ESP_OK;
}

static esp_err_t _coredump_to_server_write_cb(void * priv, char const * const str)
{
	ets_printf("%s\r\n", str);
	return ESP_OK;
}

void register_coredump() {
	coredump_to_server_config_t coredump_cfg = {
			.start = _coredump_to_server_begin_cb,
			.end = _coredump_to_server_end_cb,
			.write = _coredump_to_server_write_cb,
			.priv = NULL,
	};
	if( coredump_to_server(&coredump_cfg) != ESP_OK ){  // Dump to console and do not clear (will done after fetched from Webserver)
		ESP_LOGI( FNAME, "+++ All green, no coredump found in FLASH +++");
	}
}


// Sensor board init method. Herein all functions that make the XCVario are launched and tested.
void system_startup(void *args){

	bool selftestPassed=true;
	int line = 1;
	ESP_LOGI( FNAME, "Now setup I2C bus IO 21/22");
	i2c.begin(GPIO_NUM_21, GPIO_NUM_22, 100000 );
	theWind.begin();

	MCP = new MCP3221();
	MCP->setBus( &i2c );
	gpio_set_drive_capability(GPIO_NUM_23, GPIO_DRIVE_CAP_1);

	esp_wifi_set_mode(WIFI_MODE_NULL);
	spiMutex = xSemaphoreCreateMutex();
	Menu = new SetupMenu();
	// esp_log_level_set("*", ESP_LOG_INFO);
	ESP_LOGI( FNAME, "Log level set globally to INFO %d; Max Prio: %d Wifi: %d",  ESP_LOG_INFO, configMAX_PRIORITIES, ESP_TASKD_EVENT_PRIO-5 );
	esp_chip_info_t chip_info;
	esp_chip_info(&chip_info);
	ESP_LOGI( FNAME,"This is ESP32 chip with %d CPU cores, WiFi%s%s, ",
			chip_info.cores,
			(chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
					(chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");
	ESP_LOGI( FNAME,"Silicon revision %d, ", chip_info.revision);
	ESP_LOGI( FNAME,"%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
			(chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
	ESP_LOGI(FNAME, "QNH.get() %.1f hPa", QNH.get() );
	register_coredump();
	Polars::begin();

	the_can_mode = can_mode.get(); // initialize variable for CAN mode
	if( hardwareRevision.get() == HW_UNKNOWN ){  // per default we assume there is XCV-20
		ESP_LOGI( FNAME, "Hardware Revision unknown, set revision 2 (XCV-20)");
		hardwareRevision.set(XCVARIO_20);
	}

	wireless = (e_wireless_type)(wireless_type.get()); // we cannot change this on the fly, so get that on boot
	AverageVario::begin();
	stall_alarm_off_kmh = stall_speed.get()/3;

	Battery.begin();  // for battery voltage
	xMutex=xSemaphoreCreateMutex();
	xSemaphoreTake(spiMutex,portMAX_DELAY );
	ccp = (int)(core_climb_period.get()*10);
	SPI.begin( SPI_SCLK, SPI_MISO, SPI_MOSI, CS_bme280BA );
	xSemaphoreGive(spiMutex);

	egl = new AdaptUGC();
	egl->begin();
	xSemaphoreTake(spiMutex,portMAX_DELAY );
	ESP_LOGI( FNAME, "setColor" );
	egl->setColor( 0, 255, 0 );
	ESP_LOGI( FNAME, "drawLine" );
	egl->drawLine( 20,20, 20,80 );
	ESP_LOGI( FNAME, "finish Draw" );
	xSemaphoreGive(spiMutex);

	MYUCG = egl; // new AdaptUGC( SPI_DC, CS_Display, RESET_Display );
	display = new IpsDisplay( MYUCG );
	Flarm::setDisplay( MYUCG );
	DM.begin( MYUCG );
	display->begin();
	display->bootDisplay();

	// int valid;
	String logged_tests;
	logged_tests += "\n\n\n";
	Version V;
	std::string ver( " Ver.: " );
	ver += V.version();
	char hw[24];
	sprintf( hw,", XCV-%d", hardwareRevision.get()+18);  // plus 18, e.g. 2 = XCVario-20
	std::string hwrev( hw );
	ver += hwrev;
	display->writeText(line++, ver.c_str() );
	sleep(1);
	bool doUpdate = software_update.get();
	if( Rotary.readSwitch() ){
		doUpdate = true;
		ESP_LOGI(FNAME,"Rotary pressed: Do Software Update");
	}
	if( doUpdate ) {
		if( hardwareRevision.get() == XCVARIO_20) { // only XCV-20 uses this GPIO for Rotary
			ESP_LOGI( FNAME,"Hardware Revision detected 2");
			Rotary.begin( GPIO_NUM_4, GPIO_NUM_2, GPIO_NUM_0);
		}
		else  {
			ESP_LOGI( FNAME,"Hardware Revision detected 3");
			Rotary.begin( GPIO_NUM_36, GPIO_NUM_39, GPIO_NUM_0);

		}
		ota = new OTA();
		ota->begin();
		ota->doSoftwareUpdate( display );
	}
	esp_err_t err=ESP_ERR_NOT_FOUND;
	MPU.setBus(i2c);  // set communication bus, for SPI -> pass 'hspi'
	MPU.setAddr(mpud::MPU_I2CADDRESS_AD0_LOW);  // set address or handle, for SPI -> pass 'mpu_spi_handle'
	err = MPU.reset();
	ESP_LOGI( FNAME,"MPU Probing returned %d MPU enable: %d ", err, attitude_indicator.get() );
	if( err == ESP_OK ){
		if( hardwareRevision.get() < XCVARIO_21 ){
			ESP_LOGI( FNAME,"MPU avail, increase hardware revision to 3 (XCV-21)");
			hardwareRevision.set(XCVARIO_21);  // there is MPU6050 gyro and acceleration sensor, at least we got an XCV-21
		}
		gflags.haveMPU = true;
		mpu_target_temp = mpu_temperature.get();
		ESP_LOGI( FNAME,"MPU initialize");
		MPU.initialize();  // this will initialize the chip and set default configurations
		MPU.setSampleRate(50);  // in (Hz)
		MPU.setAccelFullScale(mpud::ACCEL_FS_8G);
		MPU.setGyroFullScale( GYRO_FS );
		MPU.setDigitalLowPassFilter(mpud::DLPF_5HZ);  // smoother data
		mpud::raw_axes_t gb = gyro_bias.get();
		mpud::raw_axes_t ab = accl_bias.get();
		if( gb.isZero() && ab.isZero() ) {
			ESP_LOGI( FNAME,"MPU computeOffsets");
			MPU.computeOffsets( &ab, &gb );  // returns Offsets in 16G scale
			accl_bias.set( ab );
			gyro_bias.set( gb );
			ESP_LOGI( FNAME,"MPU new offsets accl:%d/%d/%d gyro:%d/%d/%d ZERO:%d", ab.x, ab.y, ab.z, gb.x,gb.y,gb.z, gb.isZero() );
		}
		MPU.setAccelOffset(ab);
		MPU.setGyroOffset(gb);
		mpud::raw_axes_t accelRaw;
		mpud::float_axes_t accelG;
		float samples = 0;
		delay(200);
		for( auto i=0; i<10; i++ ){
			esp_err_t err = MPU.acceleration(&accelRaw);  // fetch raw data from the registers
			if( err != ESP_OK ) {
				ESP_LOGE(FNAME, "AHRS acceleration I2C read error");
				continue;
			}
			samples++;
			accelG += mpud::accelGravity(accelRaw, mpud::ACCEL_FS_8G);  // raw data to gravity
			ESP_LOGI( FNAME,"MPU %.2f", accelG[0] );
			delay( 10 );
		}
		char ahrs[30];
		accelG /= samples;
		float accel = sqrt(accelG[0]*accelG[0]+accelG[1]*accelG[1]+accelG[2]*accelG[2]);
		sprintf( ahrs,"AHRS Sensor: OK (%.2f g)", accel );
		display->writeText( line++, ahrs );
		logged_tests += "MPU6050 AHRS test: PASSED\n";
		IMU::init();
		if ( IMU::MPU6050Read() == ESP_OK) {
			IMU::Process();
		}
		ESP_LOGI( FNAME,"MPU current offsets accl:%d/%d/%d gyro:%d/%d/%d ZERO:%d", ab.x, ab.y, ab.z, gb.x,gb.y,gb.z, gb.isZero() );
	}
	else{
		ESP_LOGI( FNAME,"MPU reset failed, check HW revision: %d",hardwareRevision.get() );
		if( hardwareRevision.get() >= XCVARIO_21 ) {
			ESP_LOGI( FNAME,"hardwareRevision detected = 3, XCVario-21+");
			display->writeText( line++, "AHRS Sensor: NOT FOUND");
			logged_tests += "MPU6050 AHRS test: NOT FOUND\n";
		}
	}
	char id[16] = { 0 };
	strcpy( id, custom_wireless_id.get().id );
	ESP_LOGI(FNAME,"Custom Wirelss-ID from Flash: %s len: %d", id, strlen(id) );
	if( strlen( id ) == 0 ){
		custom_wireless_id.set( SetupCommon::getDefaultID() ); // Default ID created from MAC address CRC
		ESP_LOGI(FNAME,"Empty ID: Initialize empty Wirelss-ID: %s", custom_wireless_id.get().id );
	}
	ESP_LOGI(FNAME,"Custom Wirelss-ID: %s", custom_wireless_id.get().id );

	String wireless_id;
	if( wireless == WL_BLUETOOTH ) {
		wireless_id="BT ID: ";
		btsender.begin();
	}
	else if( wireless == WL_BLUETOOTH_LE ){
		blesender.begin();
	}
	else
		wireless_id="WLAN SID: ";
	wireless_id += SetupCommon::getID();
	display->writeText(line++, wireless_id.c_str() );
	Cipher::begin();
	if( Cipher::checkKeyAHRS() ){
		ESP_LOGI( FNAME, "AHRS key valid=%d", gflags.ahrsKeyValid );
	}else{
		ESP_LOGI( FNAME, "AHRS key invalid=%d, disable AHRS Sensor", gflags.ahrsKeyValid );
		if( attitude_indicator.get() )
			attitude_indicator.set(0);
	}

	ESP_LOGI(FNAME,"Airspeed sensor init..  type configured: %d", airspeed_sensor_type.get() );
	int offset;
	bool found = false;
	if( hardwareRevision.get() >= XCVARIO_20 ){ // autodetect new type of sensors in any case
		ESP_LOGI(FNAME," HW revision 3, check configured airspeed sensor");
		bool valid_config=true;
		switch( airspeed_sensor_type.get() ){
		case PS_TE4525:
			asSensor = new MS4525DO();
			ESP_LOGI(FNAME,"MS4525DO configured");
			break;
		case PS_ABPMRR:
			asSensor = new ABPMRR();
			ESP_LOGI(FNAME,"ABPMRR configured");
			break;
		case PS_MP3V5004:
			asSensor = new MP5004DP();
			ESP_LOGI(FNAME,"PS_MP3V5004 configured");
			break;
		case PS_MCPH21:
			asSensor = new MCPH21();
			ESP_LOGI(FNAME,"PS_MCPH21 configured");
			break;
		default:
			valid_config = false;
			ESP_LOGI(FNAME,"No valid config found");
			break;
		}
		if( valid_config ){
			ESP_LOGI(FNAME,"There is valid config for airspeed sensor: check this one first...");
			asSensor->setBus( &i2c );
			if( asSensor->selfTest( offset ) ){
				ESP_LOGI(FNAME,"Selftest for configured sensor OKAY");
				found = true;
			}
			else{
				ESP_LOGI(FNAME,"AS sensor not found");
				delete asSensor;
			}
		}
		// Probe any kind of ever known sensors
		if( !found ){   // behaves same as above, so we can't detect this, needs to be setup in factory
			ESP_LOGI(FNAME,"Try MCPH21");
			asSensor = new MCPH21();
			asSensor->setBus( &i2c );
			ESP_LOGI(FNAME,"Try MCPH21");
			if( asSensor->selfTest( offset ) ){
				airspeed_sensor_type.set( PS_MCPH21 );
				found = true;
			}
			else{
				ESP_LOGI(FNAME,"MCPH21 sensor not found");
				delete asSensor;
			}
			delay( 100 );
		}
		if( !found ){
			ESP_LOGI(FNAME,"Try ABPMRR");
			asSensor = new ABPMRR();
			asSensor->setBus( &i2c );
			if( asSensor->selfTest( offset ) ){
				airspeed_sensor_type.set( PS_ABPMRR );
				found = true;
			}
			else{
				ESP_LOGI(FNAME,"ABPMRR sensor not found");
				delete asSensor;
			}
		}
		if( !found ){   // behaves same as above, so we can't detect this, needs to be setup in factory
			ESP_LOGI(FNAME,"Configured sensor not found");
			asSensor = new MS4525DO();
			asSensor->setBus( &i2c );
			ESP_LOGI(FNAME,"Try MS4525");
			if( asSensor->selfTest( offset ) ){
				airspeed_sensor_type.set( PS_ABPMRR );
				found = true;
			}
			else{
				ESP_LOGI(FNAME,"MS4525DO sensor not found");
				delete asSensor;
			}
		}
		if( !found ){
			ESP_LOGI(FNAME,"Try MP5004DP");
			asSensor = new MP5004DP();
			asSensor->setBus( &i2c );
			if( asSensor->selfTest( offset ) ){
				ESP_LOGI(FNAME,"MP5004DP selfTest OK");
				airspeed_sensor_type.set( PS_MP3V5004 );
				found = true;
			}
			else{
				ESP_LOGI(FNAME,"MP5004DP sensor not found");
				delete asSensor;
			}
		}
	}
	if( found ){
		ESP_LOGI(FNAME,"AS Speed sensors self test PASSED, offset=%d", offset);
		asSensor->doOffset();
		bool offset_plausible = asSensor->offsetPlausible( offset );
		bool ok=false;
		float p=asSensor->readPascal(60, ok);
		if( ok )
			dynamicP = p;
		ias.set( Atmosphere::pascal2kmh( dynamicP ) );
		ESP_LOGI(FNAME,"Aispeed sensor current speed=%f", ias.get() );
		if( !offset_plausible && ( ias.get() < 50 ) ){
			ESP_LOGE(FNAME,"Error: air speed presure sensor offset out of bounds, act value=%d", offset );
			display->writeText( line++, "AS Sensor: NEED ZERO" );
			logged_tests += "AS Sensor offset test: FAILED\n";
			selftestPassed = false;
		}
		else {
			ESP_LOGI(FNAME,"air speed offset test PASSED, readout value in bounds=%d", offset );
			char s[40];
			if( ias.get() > 50 ) {
				sprintf(s, "AS Sensor: %d km/h", (int)(ias.get()+0.5) );
				display->writeText( line++, s );
			}
			else
				display->writeText( line++, "AS Sensor: OK" );
			logged_tests += "AS Sensor offset test: PASSED\n";
		}
	}
	else{
		ESP_LOGE(FNAME,"Error with air speed pressure sensor, no working sensor found!");
		display->writeText( line++, "AS Sensor: NOT FOUND");
		logged_tests += "AS Sensor: NOT FOUND\n";
		selftestPassed = false;
		asSensor = 0;
	}
	ESP_LOGI(FNAME,"Now start T sensor test");
	// Temp Sensor test
	if( !SetupCommon::isClient()  ) {
		ESP_LOGI(FNAME,"Now start T sensor test");
		ds18b20.begin();
		if( !gflags.inSetup )
			temperature = ds18b20.getTemp();
		ESP_LOGI(FNAME,"End T sensor test");
		if( temperature == DEVICE_DISCONNECTED_C ) {
			ESP_LOGE(FNAME,"Error: Self test Temperatur Sensor failed; returned T=%2.2f", temperature );
			display->writeText( line++, "Temp Sensor: NOT FOUND");
			gflags.validTemperature = false;
			logged_tests += "External Temperature Sensor: NOT FOUND\n";
		}else
		{
			ESP_LOGI(FNAME,"Self test Temperatur Sensor PASSED; returned T=%2.2f", temperature );
			display->writeText( line++, "Temp Sensor: OK");
			gflags.validTemperature = true;
			logged_tests += "External Temperature Sensor:PASSED\n";

		}
	}
	ESP_LOGI(FNAME,"Absolute pressure sensors init, detect type of sensor type..");

	float ba_t, ba_p, te_t, te_p;
	SPL06_007 *splBA = new SPL06_007( SPL06_007_BARO );
	SPL06_007 *splTE = new SPL06_007( SPL06_007_TE );
	splBA->setBus( &i2c );
	splTE->setBus( &i2c );
	bool baok =  splBA->begin();
	bool teok =  splTE->begin();
	if( baok || teok ){
		ESP_LOGI(FNAME,"SPL06_007 type detected");
		i2c.begin(GPIO_NUM_21, GPIO_NUM_22, 100000 );  // higher speed, we have 10K pullups on that board
		baroSensor = splBA;
		teSensor = splTE;
	}
	else{
		delete splBA;
		ESP_LOGI(FNAME,"No SPL06_007 chip detected, now check BMP280");
		BME280_ESP32_SPI *bmpBA = new BME280_ESP32_SPI();
		BME280_ESP32_SPI *bmpTE= new BME280_ESP32_SPI();
		int spi_freq=rint( FREQ_BMP_SPI / 2 * ((100.0 + display_clock_adj.get())/100.0));
		bmpBA->setSPIBus(SPI_SCLK, SPI_MOSI, SPI_MISO, CS_bme280BA, spi_freq);
		bmpTE->setSPIBus(SPI_SCLK, SPI_MOSI, SPI_MISO, CS_bme280TE, spi_freq);
		bmpTE->begin();
		bmpBA->begin();
		baroSensor = bmpBA;
		teSensor = bmpTE;
		gpio_set_pull_mode(CS_bme280BA, GPIO_PULLUP_ONLY );
		gpio_set_pull_mode(CS_bme280TE, GPIO_PULLUP_ONLY );

	}
	bool tetest=true;
	bool batest=true;
	delay(200);

	if( !baroSensor->selfTest( ba_t, ba_p)  ) {
		ESP_LOGE(FNAME,"HW Error: Self test Barometric Pressure Sensor failed!");
		display->writeText( line++, "Baro Sensor: NOT FOUND");
		selftestPassed = false;
		batest=false;
		logged_tests += "Baro Sensor Test: NOT FOUND\n";
	}
	else {
		ESP_LOGI(FNAME,"Baro Sensor test OK, T=%f P=%f", ba_t, ba_p);
		display->writeText( line++, "Baro Sensor: OK");
		logged_tests += "Baro Sensor Test: PASSED\n";
	}
	if( !teSensor->selfTest(te_t, te_p) ) {
		ESP_LOGE(FNAME,"HW Error: Self test TE Pressure Sensor failed!");
		display->writeText( line++, "TE Sensor: NOT FOUND");
		selftestPassed = false;
		tetest=false;
		logged_tests += "TE Sensor Test: NOT FOUND\n";
	}
	else {
		ESP_LOGI(FNAME,"TE Sensor test OK,   T=%f P=%f", te_t, te_p);
		display->writeText( line++, "TE Sensor: OK");
		logged_tests += "TE Sensor Test: PASSED\n";
	}
	if( tetest && batest ) {
		ESP_LOGI(FNAME,"Both absolute pressure sensor TESTs SUCCEEDED, now test deltas");
		if( (abs(ba_t - te_t) >4.0)  && ( ias.get() < 50 ) ) {   // each sensor has deviations, and new PCB has more heat sources
			selftestPassed = false;
			ESP_LOGE(FNAME,"Severe T delta > 4 °C between Baro and TE sensor: °C %f", abs(ba_t - te_t) );
			display->writeText( line++, "TE/Baro Temp: Unequal");
			logged_tests += "TE/Baro Sensor T diff. <4°C: FAILED\n";
		}
		else{
			ESP_LOGI(FNAME,"Abs p sensors temp. delta test PASSED, delta: %f °C",  abs(ba_t - te_t));
			// display->writeText( line++, "TE/Baro Temp: OK");
			logged_tests += "TE/Baro Sensor T diff. <2°C: PASSED\n";
		}
		float delta = 2.5; // in factory we test at normal temperature, so temperature change is ignored.
		if( abs(factory_volt_adjust.get() - 0.00815) < 0.00001 )
			delta += 1.8; // plus 1.5 Pa per Kelvin, for 60K T range = 90 Pa or 0.9 hPa per Sensor, for both there is 2.5 plus 1.8 hPa to consider
		if( (abs(ba_p - te_p) >delta)  && ( ias.get() < 50 ) ) {
			selftestPassed = false;
			ESP_LOGI(FNAME,"Abs p sensors deviation delta > 2.5 hPa between Baro and TE sensor: %f", abs(ba_p - te_p) );
			display->writeText( line++, "TE/Baro P: Unequal");
			logged_tests += "TE/Baro Sensor P diff. <2hPa: FAILED\n";
		}
		else
			ESP_LOGI(FNAME,"Abs p sensor deta test PASSED, delta: %f hPa", abs(ba_p - te_p) );
		// display->writeText( line++, "TE/Baro P: OK");
		logged_tests += "TE/Baro Sensor P diff. <2hPa: PASSED\n";

	}
	else
		ESP_LOGI(FNAME,"Absolute pressure sensor TESTs failed");

	bmpVario.begin( teSensor, baroSensor, &Speed2Fly );
	bmpVario.setup();
	esp_task_wdt_reset();
	ESP_LOGI(FNAME,"Audio begin");
	Audio::begin( DAC_CHANNEL_1 );
	ESP_LOGI(FNAME,"Poti and Audio test");
	if( !Audio::selfTest() ) {
		ESP_LOGE(FNAME,"Error: Digital potentiomenter selftest failed");
		display->writeText( line++, "Digital Poti: Failure");
		selftestPassed = false;
		logged_tests += "Digital Audio Poti test: FAILED\n";
	}
	else{
		ESP_LOGI(FNAME,"Digital potentiometer test PASSED");
		logged_tests += "Digital Audio Poti test: PASSED\n";
		display->writeText( line++, "Digital Poti: OK");
	}

	// 2021 series 3, or 2022 model with new digital poti CAT5171 also features CAN bus
	String resultCAN;
	if( Audio::haveCAT5171() ) // todo && CAN configured
	{
		CAN = new CANbus();
		logged_tests += "CAN Interface: ";
		if( CAN->selfTest() ) { // series 2023 has fixed slope control, prio slope bit for AHRS temperature control
			resultCAN = "OK";
			ESP_LOGE(FNAME,"CAN Bus selftest (%sRS): OK", CAN->hasSlopeSupport() ? "" : "no ");
			logged_tests += "OK\n";
			if ( CAN->hasSlopeSupport() ) {
				gpio_set_level(GPIO_NUM_2, 0);  // correct slope bit for working state in XCV22
				if( hardwareRevision.get() < XCVARIO_22)
					hardwareRevision.set(XCVARIO_22);  // XCV-22, CAN but no AHRS temperature control
			} else {
				ESP_LOGI(FNAME,"CAN Bus selftest without RS control OK: set hardwareRevision (XCV-23)");
				if( hardwareRevision.get() < XCVARIO_23)
					hardwareRevision.set(XCVARIO_23);  // XCV-23, including AHRS temperature control
			}
		}
		else {
			resultCAN = "FAIL";
			logged_tests += "CAN Bus selftest: FAILED\n";
			ESP_LOGE(FNAME,"Error: CAN Interface failed");
		}
	}

	if( gflags.haveMPU ){
		if( MPU.whoAmI() == 0x12 ){
			if( hardwareRevision.get() < XCVARIO_25){
				hardwareRevision.set(XCVARIO_25);  // XCV-25: ICL20602
				ESP_LOGI(FNAME,"MPU ICM-20602 -> hardwareRevision (XCV-25)");
			}
		}
	}

	float bat = Battery.get(true);
	if( bat < 1 || bat > 28.0 ){
		ESP_LOGE(FNAME,"Error: Battery voltage metering out of bounds, act value=%f", bat );
		if( resultCAN.length() )
			display->writeText( line++, "Bat Meter/CAN: ");
		else
			display->writeText( line++, "Bat Meter/CAN: Fail/" + resultCAN );
		logged_tests += "Battery Voltage Sensor: FAILED\n";
		selftestPassed = false;
	}
	else{
		ESP_LOGI(FNAME,"Battery voltage metering test PASSED, act value=%f", bat );
		if( resultCAN.length() )
			display->writeText( line++, "Bat Meter/CAN: OK/"+ resultCAN );
		else
			display->writeText( line++, "Bat Meter: OK");
		logged_tests += "Battery Voltage Sensor: PASSED\n";
	}

	Serial::begin();
	// Factory test for serial interface plus cable
	String result("Serial ");
	if( Serial::selfTest( 1 ) )
		result += "S1 OK";
	else
		result += "S1 FAIL";
	if( (hardwareRevision.get() >= XCVARIO_21) && serial2_speed.get() ){
		if( Serial::selfTest( 2 ) )
			result += ",S2 OK";
		else
			result += ",S2 FAIL";
	}
	if( abs(factory_volt_adjust.get() - 0.00815) < 0.00001 ){
		display->writeText( line++, result.c_str() );
	}
	Serial::taskStart();

	if( wireless == WL_BLUETOOTH ) {
		if( btsender.selfTest() ){
			display->writeText( line++, "Bluetooth: OK");
			logged_tests += "Bluetooth test: PASSED\n";
		}
		else{
			display->writeText( line++, "Bluetooth: FAILED");
			logged_tests += "Bluetooth test: FAILED\n";
		}
	}else if ( wireless == WL_WLAN_MASTER || wireless == WL_WLAN_STANDALONE ){
		WifiApp::wifi_init_softap();
	}
	// 2021 series 3, or 2022 model with new digital poti CAT5171 also features CAN bus
	if(  can_speed.get() != CAN_SPEED_OFF && (resultCAN == "OK") && CAN )
	{
		ESP_LOGI(FNAME, "Now start CAN Bus Interface");
		CAN->begin();  // start CAN tasks and driver
	}

	if( compass_enable.get() == CS_CAN ){
		ESP_LOGI( FNAME, "Magnetic sensor type CAN");
		compass = new Compass( 0 );  // I2C addr 0 -> instantiate without I2C bus and local sensor
	}
	else if( compass_enable.get() == CS_I2C ){
		ESP_LOGI( FNAME, "Magnetic sensor type I2C");
		compass = new Compass( 0x0D, ODR_50Hz, RANGE_2GAUSS, OSR_512, &i2c_0 );
	}
	// magnetic sensor / compass selftest
	if( compass ) {
		compass->begin();
		ESP_LOGI( FNAME, "Magnetic sensor enabled: initialize");
		err = compass->selfTest();
		if( err == ESP_OK )		{
			// Activate working of magnetic sensor
			ESP_LOGI( FNAME, "Magnetic sensor selftest: OKAY");
			display->writeText( line++, "Compass: OK");
			logged_tests += "Compass test: OK\n";
		}
		else{
			ESP_LOGI( FNAME, "Magnetic sensor selftest: FAILED");
			display->writeText( line++, "Compass: FAILED");
			logged_tests += "Compass test: FAILED\n";
			selftestPassed = false;
		}
		compass->start();  // start task
	}

	Speed2Fly.begin();
	Version myVersion;
	ESP_LOGI(FNAME,"Program Version %s", myVersion.version() );
	ESP_LOGI(FNAME,"%s", logged_tests.c_str());
	if( !selftestPassed )
	{
		ESP_LOGI(FNAME,"\n\n\nSelftest failed, see above LOG for Problems\n\n\n");
		display->writeText( line++, "Selftest FAILED");
		if( !Rotary.readSwitch() )
			sleep(4);
	}
	else{
		ESP_LOGI(FNAME,"\n\n\n*****  Selftest PASSED  ********\n\n\n");
		display->writeText( line++, "Selftest PASSED");
		if( !Rotary.readSwitch() )
			sleep(2);
	}
	if( Rotary.readSwitch() )
	{
		LeakTest::start( baroSensor, teSensor, asSensor );
	}
	Menu->begin( display, baroSensor, &Battery );

	if ( wireless == WL_WLAN_CLIENT || the_can_mode == CAN_MODE_CLIENT ){
		ESP_LOGI(FNAME,"Client Mode");
	}
	else if( ias.get() < 50.0 ){
		ESP_LOGI(FNAME,"Master Mode: QNH Autosetup, IAS=%3f (<50 km/h)", ias.get() );
		// QNH autosetup
		float ae = elevation.get();
		float qnh_best = QNH.get();
		bool ok;
		baroP = baroSensor->readPressure(ok);
		if( ae > 0 ) {
			float step=10.0; // 80 m
			float min=1000.0;
			for( float qnh = 870; qnh< 1085; qnh+=step ) {
				float alt = 0;
				if( Flarm::validExtAlt() && alt_select.get() == AS_EXTERNAL )
					alt = alt_external + (qnh  - 1013.25) * 8.2296;  // correct altitude according to ISA model = 27ft / hPa
				else
					alt = baroSensor->readAltitude( qnh, ok);
				float diff = alt - ae;
				// ESP_LOGI(FNAME,"Alt diff=%4.2f  abs=%4.2f", diff, abs(diff) );
				if( abs( diff ) < 100 )
					step=1.0;  // 8m
				if( abs( diff ) < 10 )
					step=0.05;  // 0.4 m
				if( abs( diff ) < abs(min) ) {
					min = diff;
					qnh_best = qnh;
					// ESP_LOGI(FNAME,"New min=%4.2f", min);
				}
				if( diff > 1.0 ) // we are ready, values get already positive
					break;
				delay(50);
			}
			ESP_LOGI(FNAME,"Auto QNH=%4.2f\n", qnh_best);
			QNH.set( qnh_best );
		}
		display->clear();
		if( NEED_VOLTAGE_ADJUST ){
			ESP_LOGI(FNAME,"Do Factory Voltmeter adj");
			SetupMenuValFloat::showMenu( 0.0, SetupMenuValFloat::meter_adj_menu );
		}else{
			SetupMenuValFloat *qnh_menu = SetupMenu::createQNHMenu();
			SetupMenuValFloat::showMenu( qnh_best, qnh_menu );
		}
	}
	else
	{
		gflags.inSetup = false;
		display->clear();
	}

	if ( flap_enable.get() ) {
		Flap::init(MYUCG);
	}

	if( hardwareRevision.get() == XCVARIO_20 ){
		Rotary.begin( GPIO_NUM_4, GPIO_NUM_2, GPIO_NUM_0);  // XCV-20 uses GPIO_2 for Rotary
	}
	else {
		Rotary.begin( GPIO_NUM_36, GPIO_NUM_39, GPIO_NUM_0);
		gpio_pullup_en( GPIO_NUM_34 );
		if( gflags.haveMPU && HAS_MPU_TEMP_CONTROL && !gflags.mpu_pwm_initalized  ){ // series 2023 does not have slope support on CAN bus but MPU temperature control
			MPU.pwm_init();
			gflags.mpu_pwm_initalized = true;
		}
	}
	delay( 100 );
	if ( SetupCommon::isClient() ){
		if( wireless == WL_WLAN_CLIENT ){
			display->clear();

			int line=1;
			display->writeText( line++, "Wait for WiFi Master" );
			char mxcv[30] = "";
			if( master_xcvario.get() != 0 ){
				sprintf( mxcv+strlen(mxcv), "XCVario-%d", (int) master_xcvario.get() );
				display->writeText( line++, mxcv );
			}
			line++;
			std::string ssid = WifiClient::scan( master_xcvario.get() );
			if( ssid.length() ){
				display->writeText( line++, "Master XCVario found" );
				char id[30];
				sprintf( id, "Wifi ID: %s", ssid.c_str() );
				display->writeText( line++, id );
				display->writeText( line++, "Now start, sync" );
				WifiClient::start();
				delay( 5000 );
				gflags.inSetup = false;
				display->clear();
			}
			else{
				display->writeText( 3, "Abort Wifi Scan" );
			}
		}
		else if( the_can_mode == CAN_MODE_CLIENT ){
			display->clear();
			display->writeText( 1, "Wait for CAN Master" );
			while( 1 ) {
				if( CAN && CAN->connectedXCV() ){
					display->writeText( 3, "Master XCVario found" );
					display->writeText( 4, "Now start, sync" );
					delay( 5000 );
					gflags.inSetup = false;
					display->clear();
					break;
				}
				delay( 100 );
				if( Rotary.readSwitch() ){
					display->writeText( 3, "Abort CAN bus wait" );
					break;
				}
			}
		}
	}
	if( screen_centeraid.get() ){
		centeraid = new CenterAid( MYUCG );
	}
	if( SetupCommon::isClient() ){
		xTaskCreatePinnedToCore(&clientLoop, "clientLoop", 4096, NULL, 11, &bpid, 0);
		xTaskCreatePinnedToCore(&audioTask, "audioTask", 4096, NULL, 11, &apid, 0);
	}
	else {
		xTaskCreatePinnedToCore(&readSensors, "readSensors", 5120, NULL, 12, &bpid, 0);
	}
	xTaskCreatePinnedToCore(&readTemp, "readTemp", 3000, NULL, 5, &tpid, 0);       // increase stack by 500 byte
	xTaskCreatePinnedToCore(&drawDisplay, "drawDisplay", 6144, NULL, 4, &dpid, 0); // increase stack by 1K

	Audio::startAudio();
}

extern "C" void  app_main(void)
{
	// Init timer infrastructure
	Audio::shutdown();
	esp_timer_init();
	MPU.clearpwm();
	Router::begin();
	ESP_LOGI(FNAME,"app_main" );
	ESP_LOGI(FNAME,"Now init all Setup elements");
	bool setupPresent;
	SetupCommon::initSetup( setupPresent );
	Cipher::begin();
	if( !setupPresent ){
		if( Cipher::init() )
			attitude_indicator.set(1);
	}
	else
		ESP_LOGI(FNAME,"Setup already present");
	esp_log_level_set("*", ESP_LOG_INFO);

#ifdef Quaternionen_Test
		Quaternion::quaternionen_test();
#endif
#ifdef WMM_Test
		WMM_Model::geomag_test();
#endif
	system_startup( 0 );
	vTaskDelete( NULL );
}
