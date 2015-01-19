/*
 * CC3000.cpp
 *
 * Created: 14/08/2014 18:34:31
 *  Author: Hon Bo Xuan
 */ 

#include "CC3000.h"

CC3000::CC3000() : usart(10) //Config USART BAUD at 115200bps, UBRR = [F_CPU/(16*BAUD)]-1
{	
	sei(); //Enable global interrupts (needed for IRQ pin)
	
	init_spi();
	
	wlan_init(CC3000_UsynchCallback, sendWLFWPatch, sendDriverPatch, sendBootLoaderPatch, ReadWlanInterruptPin, WlanInterruptEnable, WlanInterruptDisable, WriteWlanPin);
	wlan_start(0);
	wlan_set_event_mask(HCI_EVNT_WLAN_KEEPALIVE|HCI_EVNT_WLAN_UNSOL_INIT|HCI_EVNT_WLAN_ASYNC_PING_REPORT);
	
	usart.transmit("CC3000 setup done\n");
}

void CC3000::runActiveScan(void) {
	//Active scan
	uint8_t attempts = 0;
	uint32_t results_remaining = 0;
	
	results_returned_ = 0;
	
	uint32_t intervals[16];
	wlan_ioctl_set_scan_params(1, 100, 100, 5, 0x7ff, -80, 0, 205, intervals);
	
	while ((results_remaining != 0 || results_returned_ == 0) && attempts < 250) {
		attempts++;
		wlan_ioctl_get_scan_results(0, result_[results_returned_]);
		//Number of results left
		results_remaining = *(uint32_t*)result_[results_returned_]; //First 4 bytes
		if (results_remaining != 0) {
			results_returned_++;
		}
		if (results_returned_ == SCAN_STORE_COUNT) {
			break;
		}
	}
	
	displayActiveScan();
}

void CC3000::displayActiveScan(void) {
	//Display
	usart.transmit("%u found\n", results_returned_);
	for (uint8_t i = 0; i < results_returned_; i++) {
		usart.transmit("[%u]\t", i);
		//RSSI
		usart.transmit("RSSI: %u\t", (result_[i][8] & 0xFE) >> 1); //Upper 7 bits
		//Security
		usart.transmit("Sec: ");
		switch (result_[i][9] & 0x03) { //Lower 2 bits
			case WLAN_SEC_UNSEC:
				usart.transmit("Open");
				break;
			case WLAN_SEC_WEP:
				usart.transmit("WEP");
				break;
			case WLAN_SEC_WPA:
				usart.transmit("WPA");
				break;
			case WLAN_SEC_WPA2:
				usart.transmit("WPA2");
				break;
		}
		usart.transmit("\t");
		//SSID
		
		uint8_t ssid_length = (result_[i][9] & 0xFC) >> 2; //Upper 6 bits
		for (uint8_t j = 0; j < ssid_length; j++) {
		//for (uint8_t j = 0; j < 32; j++) {
			/*if (result[i][j+12] == 0) {
				break;
			}*/
			usart.transmit("%c", result_[i][j+12]);
		}
		usart.transmit("\tBSSID: ");
		//BSSID
		for (uint8_t j = 0; j < 5; j++) {
			usart.transmit("%X:", result_[i][j+44]);
		}
		usart.transmit("%X\n", result_[i][49]);
	}
}

bool CC3000::initUDPServer(uint16_t port) {
	sockaddr_in  socketAddr;
	//Create socket
	socket_handle_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (socket_handle_ == -1) {
		//return -1; //Error
		return false;
	}
	uint32_t timeout = 0;
	if (setsockopt(socket_handle_, SOL_SOCKET, SOCKOPT_RECV_TIMEOUT, &timeout, sizeof(timeout)) == -1) {
		//return -1; //Error
		return false;
	}
	//Bind
	socketAddr.sin_family = AF_INET;
	socketAddr.sin_addr.s_addr = 0;
	socketAddr.sin_port = htons(port);
	if (bind(socket_handle_, (sockaddr*)&socketAddr, sizeof(sockaddr_in)) == -1) {
		//return -1; //Error
		return false;
	}
	
	FLAGS |= (1 << UDP);
	usart.transmit("UDP server done\n");
	
	return true;
	
	//return sd_; //Returns socket handle if no errors, -1 if error
}

void CC3000::connect(void) {
	char fixed_ssid[] = "";
	uint8_t wpa2[] = "";
	//wlan_ioctl_set_connection_policy(0, 1, 1);
	wlan_ioctl_set_connection_policy(0, 0, 0); //Connect once
	
	usart.transmit("Wait: Connect\n");
	wlan_connect(WLAN_SEC_WPA2, fixed_ssid, 8, NULL, wpa2, 8);
	
	volatile uint32_t timeout_counter = 0;
	usart.transmit("Wait: Connect flag\n");
	while (!(FLAGS & (1 << CONNECTED)) && timeout_counter++ < 600000000UL);
	if (FLAGS & (1 << CONNECTED)) {
		timeout_counter = 0;
		usart.transmit("Wait: DHCP flag\n");
		while (!(FLAGS & (1 << DHCP)) && timeout_counter++ < 600000000UL);
		if (FLAGS & (1 << DHCP)) {
			displayIP();
		}
	}
}

void CC3000::disconnect(void) {
	closesocket(socket_handle_);
	wlan_disconnect();
	volatile uint32_t timeout_counter = 0;
	usart.transmit("Wait: Disconnect flag\n");
	while ((FLAGS & (1 << CONNECTED)) && timeout_counter++ < 600000000UL);
	timeout_counter = 0;
	usart.transmit("Wait: Un-DHCP flag\n");
	while ((FLAGS & (1 << DHCP)) && timeout_counter++ < 600000000UL);
	FLAGS &= ~(1 << UDP);
}

void CC3000::displayIP(void) {
	tNetappIpconfigRetArgs ipconfig;
	netapp_ipconfig(&ipconfig);
	usart.transmit("%u.%u.%u.%u\n", ipconfig.aucIP[3], ipconfig.aucIP[2], ipconfig.aucIP[1], ipconfig.aucIP[0]);
}

uint8_t CC3000::pollUDPReceive(void) {
	if (/*FLAGS & (1 << CONNECTED) && */FLAGS & (1 << UDP)) {
		/*sockaddr_in from;
		socklen_t sockLen;
		uint8_t udpbuf[16];*/
			
		int16_t recvcount = recvfrom(socket_handle_, udpbuf_, 16, 0, (sockaddr*)&from_, &sockLen_);
		if (recvcount) {
			/*
			PORTC ^= (1 << PORTC0);
				
			//usart.transmit("%d\n", recvcount);
			for (uint8_t i = 0; i < recvcount-1; i++) {
				usart.transmit("%c", udpbuf_[i]);
			}
			usart.transmit("%c\n", udpbuf_[recvcount-1]);
			*/
			return udpbuf_[0];
		}
	}
	return 0;
}

void CC3000::connectActiveScan(void) {
	uint8_t selection = 0;
	uint8_t ssid_length = 0;
	char ssid[31];
	uint8_t sec_key_length = 0;
	uint8_t sec_key[31];
	uint8_t usart_in;
	
	displayActiveScan();
	//uint8_t selection = 0;
	while (1) {
		if (UCSR0A & (1 << RXC0)) {
			usart_in = UDR0;
			if (usart_in >= '0' && usart_in <= '9') {
				selection = selection * 10 + usart_in - '0';
			} else if (usart_in == ';') {
				//Input done
				break;
			} else {
				usart.transmit("Invalid\n");
				selection = 0;
			}
		}
	}
	usart.transmit("Selected [%u], SSID: \t", selection);
	//SSID
	//uint8_t ssid_length = (result[selection][9] & 0xFC) >> 2; //Upper 6 bits
	ssid_length = (result_[selection][9] & 0xFC) >> 2; //Upper 6 bits
	//char ssid[31];
	for (uint8_t j = 0; j < ssid_length; j++) {
		ssid[j] = char(result_[selection][j+12]);
		usart.transmit("%c", ssid[j]);
	}
	usart.transmit("\n");
	
	//uint8_t sec_key_length = 0;
	sec_key_length = 0;
	//uint8_t sec_key[31];
	while (1) {
		if (UCSR0A & (1 << RXC0)) {
			usart_in = UDR0;
			if (usart_in != ';') {
				sec_key[sec_key_length] = usart_in;
				sec_key_length++;
				} else {
				break;
			}
		}
	}
	
	//Connect
	disconnect();
	
	wlan_connect(result_[selection][9] & 0x03, ssid, ssid_length, NULL, sec_key, sec_key_length);
	while (!(FLAGS & (1 << CONNECTED)));
	while (!(FLAGS & (1 << DHCP)));
	
	//UDP
	initUDPServer(9750);
}

//void StartSmartConfig(void);
//volatile unsigned long ulSmartConfigFinished, ulCC3000Connected,ulCC3000DHCP,OkToDoShutDown, ulCC3000DHCP_configured;
//volatile unsigned char ucStopSmartConfig;
//volatile long ulSocket;
//Simple Config Prefix
//const char aucCC3000_prefix[] = {'T', 'T', 'T'};
//AES key "smartconfigAES16"
//const unsigned char smartconfigkey[] = {0x73,0x6d,0x61,0x72,0x74,0x63,0x6f,0x6e,0x66,0x69,0x67,0x41,0x45,0x53,0x31,0x36};

//*****************************************************************************
//
//! StartSmartConfig
//!
//!  @param  None
//!
//!  @return none
//!
//!  @brief  The function triggers a smart configuration process on CC3000.
//!			it exists upon completion of the process
//
//*****************************************************************************
/*
void StartSmartConfig(void) {
	ulSmartConfigFinished = 0;
	ulCC3000Connected = 0;
	ulCC3000DHCP = 0;
	OkToDoShutDown = 0;
	
	// Reset all the previous configuration
	wlan_ioctl_set_connection_policy(DISABLE, DISABLE, DISABLE);	
	wlan_ioctl_del_profile(255);
	
	//Wait until CC3000 is disconnected
	while (ulCC3000Connected == 1) {
		_delay_us(5);
		hci_unsolicited_event_handler();
	}
	
	// Start blinking LED6 during Smart Configuration process
	//turnLedOn(LED1);	
	wlan_smart_config_set_prefix((char*)aucCC3000_prefix);
	//turnLedOff(LED1);	 

	// Start the SmartConfig start process
	wlan_smart_config_start(1);
	
	//turnLedOn(LED1);   
	
	// Wait for Smartconfig process complete
	while (ulSmartConfigFinished == 0) {
		
		//__delay_cycles(6000000); //500ms
		//turnLedOff(LED1);
		
		//__delay_cycles(6000000);
		
		//turnLedOn(LED1);  
		
	}
	
	//turnLedOn(LED1);
	
	// create new entry for AES encryption key
	nvmem_create_entry(NVMEM_AES128_KEY_FILEID, 16);
	
	// write AES key to NVMEM
	aes_write_key((unsigned char *)(&smartconfigkey[0]));
	
	// Decrypt configuration information and add profile
	wlan_smart_config_process();
	
	// Configure to connect automatically to the AP retrieved in the 
	// Smart config process
	wlan_ioctl_set_connection_policy(DISABLE, DISABLE, ENABLE);
	
	// reset the CC3000
	wlan_stop();
	
	//__delay_cycles(6000000);
	
	//DispatcherUartSendPacket((unsigned char*)pucUARTCommandSmartConfigDoneString, sizeof(pucUARTCommandSmartConfigDoneString));
	
	wlan_start(0);
	
	// Mask out all non-required events
	wlan_set_event_mask(HCI_EVNT_WLAN_KEEPALIVE|HCI_EVNT_WLAN_UNSOL_INIT|HCI_EVNT_WLAN_ASYNC_PING_REPORT);
}*/

/*
struct CC3000_ActiveScanResult {
	uint8_t RSSI;
	uint8_t Security;
	uint8_t SSID_length;
	char* SSID;
};
*/