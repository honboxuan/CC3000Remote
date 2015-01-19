/*
 * board.h
 *
 * Created: 08/06/2014 17:00:33
 *  Author: Hon Bo Xuan
 */ 


#ifndef BOARD_H_
#define BOARD_H_

#include <avr/io.h>
#include <stdint.h>

#define VBEN_DDR	DDRB
#define VBEN_PORT	PORTB
#define VBEN_PIN	PORTB0

#define IRQ_DDR		DDRD
#define IRQ_PORT	PORTD
#define IRQ_PIN		PIND
#define IRQ_INP		PIND2 //Pin input
#define IRQ_MSK		EIMSK
#define IRQ_CHN		INT0

#define NETAPP_IPCONFIG_MAC_OFFSET 20

#define CONNECTED		0
#define DHCP			1
#define SHUTDOWN_OK		2
#define UDP				3

extern volatile uint8_t FLAGS;

long ReadWlanInterruptPin(void);
void WlanInterruptEnable(void);
void WlanInterruptDisable(void);
void WriteWlanPin(uint8_t trueFalse);

extern char *sendDriverPatch(unsigned long *Length);
extern char *sendBootLoaderPatch(unsigned long *Length);
extern char *sendWLFWPatch(unsigned long *Length);

void CC3000_UsynchCallback(long lEventType, char * data, unsigned char length);

#endif /* BOARD_H_ */