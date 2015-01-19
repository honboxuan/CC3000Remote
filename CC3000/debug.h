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

void debug_Init (const uint32_t ubrr);
void debug_TransmitChar(const uint8_t data);
void debug_Transmit(const char* format, ...);
uint8_t debug_Receive(void);

#endif /* USART_H_ */