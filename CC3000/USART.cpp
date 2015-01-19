/*
 * USART.cpp
 *
 * Created: 07/12/2012 16:49:54
 *  Author: Hon Bo Xuan
 */ 

#include "USART.h"

USART::USART(const uint32_t ubrr) {
	//UBRR = [F_CPU/(16*BAUD)]-1, 10 for 115200bps
	UBRR0H = uint8_t(ubrr >> 8);
	UBRR0L = uint8_t(ubrr);
	UCSR0B = (1 << RXEN0) | (1 << TXEN0);
}

void USART::transmitChar(const uint8_t data) {
	//Blocking transmit
	while (!(UCSR0A & (1 << UDRE0)));
	UDR0 = data;
}

void USART::transmit(const char* format, ...) {
	//Blocking transmit
	char buffer[255];
	va_list args;
	va_start(args, format);
	uint8_t txcount = vsprintf(buffer, format, args);
	va_end(args);
	for (uint8_t i = 0; i < txcount; i++) {
		while (!(UCSR0A & (1 << UDRE0)));
		UDR0 = buffer[i];
	}
}

uint8_t USART::receive(void) {
	//BLocking receive
	while (!(UCSR0A & (1 << RXC0)));
	return UDR0;
}