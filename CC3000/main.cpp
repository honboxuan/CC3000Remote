/*
 * main.cpp
 *
 * Created: 08/06/2014 16:26:12
 *  Author: Hon Bo Xuan
 */ 

#ifndef F_CPU
#define F_CPU 20000000UL
#endif

#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/wdt.h>
#include <stdint.h>

#include <util/delay.h>

#include "USART.h"
#include "CC3000.h"

USART usart(10);

volatile uint8_t FLAGS = 0;

//States: 0 is uninitialised, 1-255 levels from off to full
uint8_t light_state = 0;
uint8_t EEMEM light_state_eeprom = 0;

ISR(WDT_vect) {
	usart.transmit("Updating EEPROM state: %u\n", light_state);
	eeprom_update_byte(&light_state_eeprom, light_state);
}

void updateLight(const uint8_t target_state) {
	if (light_state != target_state) {
		if (target_state == 1) {
			TCCR0B = 0;
			TCCR0A = 0;
			PORTD &= ~(1 << PORTD6);
		} else if (target_state == 255) {
			TCCR0B = 0;
			TCCR0A = 0;
			PORTD |= (1 << PORTD6);
		} else {
			PORTD &= ~(1 << PORTD6);
			TCCR0A = (1 << COM0A1)|(1 << WGM01)|(1 << WGM00);
			OCR0A = target_state - 1;
			TCCR0B = (1 << CS00);
		}
		light_state = target_state;
	}
}

void enableWatchdog() {
	wdt_enable(WDTO_8S);
	WDTCSR |= (1 << WDIE);
}

int main(void) {
	uint8_t mcu_status = MCUSR;
	MCUSR = 0;
	wdt_disable();
	
	//MCUCR |= (1 << PUD); //Disable internal pull-ups
	
	//USART usart(10); //Config USART BAUD at 115200bps, UBRR = [F_CPU/(16*BAUD)]-1
	
	//Connection Indicator LED
	DDRD |= (1 << PORTD5);
	PORTD |= (1 << PORTD5);
	
	//Gate
	DDRD |= (1 << PORTD6);
	PORTD &= ~(1 << PORTD6);
	
	if (mcu_status & (1 << WDRF)) {
		//Watchdog system reset occurred
		uint8_t previous_state = eeprom_read_byte(&light_state_eeprom);
		usart.transmit("Restoring light to previous state: %u\n", previous_state);
		updateLight(previous_state);
	} else {
		usart.transmit("State in EEPROM: %u\n", eeprom_read_byte(&light_state_eeprom));
	}
		
	sei(); //Enable global interrupts (needed for IRQ pin)
	
	enableWatchdog();
	
	CC3000 cc3000_device;
	
	//Print MAC address
	uint8_t mac[6];
	nvmem_get_mac_address(mac);
	usart.transmit("MAC: ");
	for (uint8_t i = 0; i < 5; i++) {
		usart.transmit("%X:", mac[i]);
	}
	usart.transmit("%X\n", mac[5]);
		
	cc3000_device.runActiveScan();
		
	uint8_t cmd = 0;
	while(1) {
		wdt_reset();
		if (!(FLAGS & (1 << CONNECTED)) || !(FLAGS & (1 << DHCP))) {
			PORTD |= (1 << PORTD5);
			FLAGS &= ~(1 << UDP); //If not connected, UDP server cannot be ready
			usart.transmit("Connecting...\n");
			wdt_disable();
			cc3000_device.connect();
			enableWatchdog();
		} else if (!(FLAGS & (1 << UDP))) {
			PORTD |= (1 << PORTD5);
			usart.transmit("Initialising UDP server\n");
			wdt_disable();
			if (!cc3000_device.initUDPServer(9750)) {
				usart.transmit("initUDPServer failed.\n");
			}
			enableWatchdog();
		} else {
			PORTD &= ~(1 << PORTD5);
			cmd = cc3000_device.pollUDPReceive();
			if (cmd) {
				usart.transmit("%u\n", cmd);
				updateLight(cmd);
				cmd = 0;
			}
			
			if ((UCSR0A & (1 << RXC0))) {
				uint8_t usart_in = UDR0;
				
				if (usart_in == 's') {
					usart.transmit("Status:\n");
					if (FLAGS & (1 << CONNECTED)) {
						usart.transmit("Connected\n");
					} else {
						usart.transmit("Disconnected\n");
					}
					if (FLAGS & (1 << DHCP)) {
						usart.transmit("DHCP\n");
					} else {
						usart.transmit("No DHCP\n");
					}
					if (FLAGS & (1 << SHUTDOWN_OK)) {
						usart.transmit("Shutdown OK\n");
					} else {
						usart.transmit("Shutdown Not OK\n");
					}
					if (FLAGS & (1 << UDP)) {
						usart.transmit("UDP Server Done\n");
					} else {
						usart.transmit("UDP Server Not Ready\n");
					}
				}
			}
			/*
			if ((UCSR0A & (1 << RXC0))) {
				uint8_t usart_in = UDR0;
				
				switch (usart_in) {
					case 'I':
					cc3000_device.displayIP();
					break;
					case 'S':
					cc3000_device.runActiveScan();
					usart.transmit("End of scan\n");
					break;
					case 'C':
					cc3000_device.connectActiveScan();
					break;
					default:
					usart.transmit("I:\t IP\n");
					usart.transmit("S:\t Scan\n");
					usart.transmit("C:\t Connect\n");
					break;
				}
			}
			*/
		}
	}
	
	return 0;
}