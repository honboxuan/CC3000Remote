/*
 * CC3000.h
 *
 * Created: 14/08/2014 18:34:41
 *  Author: Hon Bo Xuan
 */ 


#ifndef CC3000_H_
#define CC3000_H_

extern "C" {
	#include "CC3000HostDriver/wlan.h"
	#include "CC3000HostDriver/evnt_handler.h"
	#include "CC3000HostDriver/nvmem.h"
	#include "CC3000HostDriver/socket.h"
	#include "CC3000HostDriver/netapp.h"
	#include "CC3000HostDriver/spi.h"
	#include "CC3000HostDriver/board.h"
	#include "CC3000HostDriver/security.h"
}

#include "USART.h"

#define SCAN_STORE_COUNT 8 //Memory limit

class CC3000 {
private:
	USART usart;
	
	uint8_t result_[SCAN_STORE_COUNT][50];
	uint8_t results_returned_;
		
	int16_t socket_handle_;
		
	sockaddr_in from_;
	socklen_t sockLen_;
	uint8_t udpbuf_[16];
public:		
	CC3000();
	bool initUDPServer(uint16_t port);
	void runActiveScan(void);
	void displayActiveScan(void);
	void connect(void);
	void disconnect(void);
	void displayIP(void);
	uint8_t pollUDPReceive(void);
	void connectActiveScan(void);
};

#endif /* CC3000_H_ */