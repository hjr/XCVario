#pragma once

#include "comm/InterfaceCtrl.h"
#include "comm/DataLink.h"

#include <esp_wifi.h>
#include <esp_event.h>
#include <lwip/sockets.h>

#include <cstring>
#include <list>

class WifiAP;

#define NUM_TCP_PORTS 4

typedef struct client_record {
	int client;
	int retries;
	struct sockaddr_in clientAddress;
}client_record_t;

typedef struct xcv_sock_server {
	xcv_sock_server(int p, WifiAP* mw) : port(p), mywifi(mw) {};
	// This should store the handle to send data to the port
	const int port;
	int socket;
	int idle = 0;
	std::list<client_record_t>  clients;
	WifiAP *mywifi;
}sock_server_t;

class WifiAP final : public InterfaceCtrl
{
	friend class WIFI_EVENT_HANDLER;
	friend void socket_server(void *setup);

public:
	WifiAP();
	~WifiAP();

public:
    // Ctrl
    InterfaceId getId() const override { return WIFI_AP; }
    const char* getStringId() const override { return "WiFi"; }
    void ConfigureIntf(int port) override;                              // 8880, 8881, 8882, 8883
	virtual int Send(const char *msg, int &len, int port=0) override;

	// returns 1 if queue is full, changes color of WiFi symbol, connection is stuck
	static int queueFull();

	static sock_server_t *_socks[NUM_TCP_PORTS];
	static TaskHandle_t pid;

private:
	static bool full[NUM_TCP_PORTS];
	static bool task_created;

	// internal functionality
	void wifi_init_softap();
};

extern WifiAP *Wifi;
