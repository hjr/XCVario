/*  WiFi softAP Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#include "WifiApp.h"
#include "Setup.h"
#include "Router.h"
#include "sensor.h"
#include "Flarm.h"
#include "Switch.h"
#include "DataMonitor.h"
#include "logdef.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lwip/sockets.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_mac.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <lwip/err.h>
#include <lwip/sys.h>
#include "WifiClient.h"
#include <cerrno>
#include "comm/DataLink.h"
#include "esp_assert.h"

extern bool netif_initialized;

#define WIFI_BUFFER_SIZE 513

bool WifiApp::full[4];

WifiApp::WifiApp():  InterfaceCtrl(false),
		_intfid(InterfaceId((int)WIFI))
{
	_id_memo = "WiFi";
	_socks.idle = 0;
	_socks.pid = 0;
	_socks.clients={};
	_socks.dl=&_dlink;
	for(int i=0; i<4; i++)
		full[i] = false;
}

WifiApp::~WifiApp(){
	vTaskDelete(_socks.pid);
	_socks.txq->clear();
}

int  WifiApp::queueFull(){
	// ESP_LOGI(FNAME, "WQF: 0:%d 1:%d 2:%d 3:%d", full[0], full[0],full[0],full[0] );
	if( full[0] || full[1] || full[2] || full[3] )
		return 1;
	else
		return 0;
}

// it's mostly static IP a cfg dependig of port so give here port minus 8880
void WifiApp::ConfigureIntf(int cfg){
	if (cfg < 0 || cfg > 3) {
		ESP_LOGE(FNAME, "Invalid cfg: %d, should be between 0 and 3", cfg);
		return;
	}
	wifi_init_softap();
	_socks.port=8880+cfg;
	sprintf( taskname, "wifitsk-%d", _socks.port );
	int prio=0;
	switch( cfg ){
	case 0:
		_socks.txq = &wl_vario_tx_q;
		prio=7;
		break;
	case 1:
		_socks.txq = &wl_flarm_tx_q;
		prio=8;
		break;
	case 2:
		_socks.txq = &wl_aux_tx_q;
		prio=9;
		break;
	case 3:
		_socks.txq = &can_tx_q;
		prio=6;
		break;
	default:
		break;
	}
	xTaskCreatePinnedToCore(&socket_server, taskname, 4096, &_socks, prio, &_socks.pid, 0);
}

int WifiApp::Send(const char *msg, int &len, int port){
	if (port < 8880 || port > 8884) {
	    ESP_LOGE(FNAME, "Invalid port: %d, should be between 8880 and 8884", port);
	    return 100;
	}
	SString s( msg, len );
	if( _socks.txq->isFull() ){
		full[port-8880]=true;
		return 10;
	}
	else{
		full[port-8880]=false;
		_socks.txq->add(s);
	}
	return 0;
}

int WifiApp::create_socket( int port ){
	struct sockaddr_in serverAddress;
	// Create a socket that we will listen upon.
	int mysock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (mysock < 0) {
		ESP_LOGE(FNAME, "socket: %d %s", mysock, strerror(errno));
		return -1;
	}
	// Bind our server socket to a port.
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddress.sin_port = htons(port);
	int rc  = bind(mysock, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
	if (rc < 0) {
		ESP_LOGE(FNAME, "bind: %d %s", rc, strerror(errno));
		return -1;
	}
	ESP_LOGV(FNAME, "bind port: %d", port  );
	// Flag the socket as listening for new connections.
	rc = listen(mysock, 5);
	if (rc < 0) {
		ESP_LOGE(FNAME, "listen: %d %s", rc, strerror(errno));
		return -1;
	}
	return mysock;
}

// Multi client TCP server with dynamic updated list of clients connected
void WifiApp::on_client_connect( int port, int msg ){
	if( port == 8884 ){ // have a client to XCVario protocol connected
		// ESP_LOGV(FNAME, "on_client_connect: Send MC, Ballast, Bugs, etc");
		SetupCommon::syncEntry(msg);
	}
}

void WifiApp::socket_server(void *setup) {
	sock_server_t *config = (sock_server_t *)setup;
	struct sockaddr_in clientAddress[10];  // we support max 10 clients try to connect same time
	socklen_t clientAddressLength = sizeof(struct sockaddr_in);
	std::list<client_record_t>  &clients = config->clients;
	int num_send = 0;
	int sock = create_socket( config->port );
	if( sock < 0 ) {
		ESP_LOGE(FNAME, "Socket creation for %d port FAILED: Abort task", config->port );
		vTaskDelete(NULL);
	}
	fcntl(sock, F_SETFL, O_NONBLOCK); // socket should not block, so we can server several clients
	while (1)
	{
		int new_client = accept(sock, (struct sockaddr *)&clientAddress[clients.size()], &clientAddressLength);
		if( new_client >= 0 && clients.size() < 10 ){
			client_record_t new_client_rec;
			new_client_rec.client = new_client;
			new_client_rec.retries = 0;
			clients.push_back( new_client_rec );
			num_send = 0;
			ESP_LOGI(FNAME, "New sock client: %d, number of clients: %d", new_client, clients.size()  );
		}
		// ESP_LOGI(FNAME, "Number of clients %d, port %d, idle %d", clients.size(), config->port, config->idle );
		if( clients.size() ) {
			int size=400;
			char buffer[WIFI_BUFFER_SIZE+1];
			int	len = Router::pullBlock( *(config->txq), buffer, size );
			if( len ){
				// ESP_LOGI(FNAME, "port %d to sent %d: bytes, %s", config->port, len, buffer );
				config->idle = 0;
				DM.monitorString( ItfTarget(WIFI,config->port), DIR_TX, buffer, len );
			}
			else
			{
				config->idle++;
				if( config->idle > 10 ){  // if there is no data, send a \n all 2 seconds as keep alive
					buffer[0] = '\n';
					len=1;
					config->idle = 0;
					// ESP_LOGI(FNAME, "KEEP-ALIVE port %d", config->port );
				}
			}
			std::list<client_record_t>::iterator it;
			for(it = clients.begin(); it != clients.end(); ++it)
			{
				client_record_t &client_rec = *it;
				char r[SSTRLEN+1];
				// ESP_LOGI(FNAME, "read from wifi client %d", client_rec.client );
				ssize_t sizeRead = recv(client_rec.client, r, SSTRLEN-1, MSG_DONTWAIT);
				if (sizeRead > 0) {
					for( auto dlit = config->dl->begin(); dlit != config->dl->end(); dlit++ ) {
						dlit->second->process(r, sizeRead);
					}
					DM.monitorString( ItfTarget(WIFI,config->port), DIR_RX, r, sizeRead );
					// ESP_LOG_BUFFER_HEXDUMP(FNAME,tcprx.c_str(),sizeRead, ESP_LOG_INFO);
				}
				if( config->port == 8884 ){
					if( num_send < SetupCommon::numEntries() ){
						on_client_connect( config->port, num_send );
						num_send ++;
					}
				}
				// ESP_LOGI(FNAME, "loop tcp client %d, port %d", client_rec.client, config->port );
				if ( len ){
					// ESP_LOGI(FNAME, "TX wifi client %d, bytes %d, port %d, trials:%d", client_rec.client, len, config->port, client_rec.retries );
					// ESP_LOG_BUFFER_HEXDUMP(FNAME,buffer,len, ESP_LOG_INFO);
					client_rec.retries++;
					if( client_rec.client >= 0 ){
						int num = send(client_rec.client, buffer, len, MSG_DONTWAIT);
						// ESP_LOGI(FNAME, "client %d, num send %d", client_rec.client, num );
						if( num >= 0 ){
							client_rec.retries = 0;
							// ESP_LOGI(FNAME, "tcp send to client %d (port: %d), bytes %d success", client_rec.client, config->port, num );
						}
					}
				}
				// if( client_rec.retries != 0 )
				//	ESP_LOGI(FNAME, "tcp retry %d (port: %d), %d", client_rec.client, config->port, client_rec.retries );
				if( client_rec.retries > 100 ){
					ESP_LOGW(FNAME, "tcp client %d (port %d) permanent send err: %s, remove!", client_rec.client,  config->port, strerror(errno) );
					close(client_rec.client );
					it = clients.erase( it );
					if( config->port == 8884 )
						num_send = 0;
				}
			}
		}
		Router::routeWLAN();   // to be removed at times
		Router::routeCAN();    // dito

		if( uxTaskGetStackHighWaterMark( config->pid ) < 128 )
			ESP_LOGW(FNAME,"Warning wifi task stack low: %d bytes, port %d", uxTaskGetStackHighWaterMark( config->pid ), config->port );
		vTaskDelay(200/portTICK_PERIOD_MS);
	}
	vTaskDelete(NULL);
}

void WifiApp::wifi_event_handler(void* arg, esp_event_base_t event_base,	int32_t event_id, void* event_data)
{
	if (event_id == WIFI_EVENT_AP_STACONNECTED) {
		wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
		ESP_LOGI(FNAME,"station %x:%x:%x:%x:%x:%x joined, AID=%d", MAC2STR(event->mac), event->aid);
	} else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
		wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
		ESP_LOGI(FNAME,"station %x:%x:%x:%x:%x:%x left, AID=%d", MAC2STR(event->mac), event->aid);
	}
}

void WifiApp::wifi_init_softap()
{
	if ( ! netif_initialized ) {
		netif_initialized = true;
		ESP_ERROR_CHECK(esp_netif_init()); // can only be called once
		ESP_ERROR_CHECK(esp_event_loop_create_default());
		ESP_LOGV(FNAME,"now esp_netif_create_default_wifi_ap");
		esp_netif_create_default_wifi_ap();
		ESP_LOGV(FNAME,"now esp_wifi_init");
		wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
		ESP_ERROR_CHECK(esp_wifi_init(&cfg));

		ESP_LOGV(FNAME,"now esp_event_handler_instance_register");
		ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

		ESP_LOGV(FNAME,"now esp_wifi_set_config ssid:%s", SetupCommon::getID() );
		wifi_config_t wc;
		strcpy( (char *)wc.ap.ssid, SetupCommon::getID() );
		wc.ap.ssid_len = strlen( (char *)wc.ap.ssid );
		strcpy( (char *)wc.ap.password, PASSPHARSE );
		wc.ap.channel = 1;
		wc.ap.max_connection = 4;
		wc.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
		wc.ap.ssid_hidden = 0;
		wc.ap.beacon_interval = 50;

		ESP_LOGV(FNAME,"now esp_wifi_set_mode");
		ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
		ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wc));

		ESP_LOGV(FNAME,"now esp_wifi_set_storage");
		ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
		// For further testing, may improve wifi range
		// ESP_ERROR_CHECK(esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCOL_LR ));

		sleep(1);
		ESP_LOGI(FNAME,"now start WSP wifi access point");
		ESP_ERROR_CHECK(esp_wifi_start());
		ESP_ERROR_CHECK(esp_wifi_set_max_tx_power( int(wifi_max_power.get()*80.0/100.0) ));
		ESP_LOGV(FNAME, "wifi_init_softap finished SUCCESS. SSID:%s password:%s channel:%d", (char *)wc.ap.ssid, (char *)wc.ap.password, wc.ap.channel );
	}
}
