/*
 * USART.h
 *
 * Created: 07/12/2012 16:50:13
 *  Author: Hon Bo Xuan
 */ 


#ifndef USART_H_
#define USART_H_

#include <avr/io.h>
#include <stdint.h>
#include <stdio.h>

class USART {
public:
	USART(const uint32_t ubrr = 10);
	void transmitChar(const uint8_t data);
	void transmit(const char* format, ...);
	uint8_t receive(void);
};

#endif /* USART_H_ */