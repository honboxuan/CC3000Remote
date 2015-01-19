/*
 * spi.h
 *
 * Created: 17/02/2014 15:47:07
 *  Author: Hon Bo Xuan
 */ 


#ifndef SPI_H_
#define SPI_H_

#ifndef F_CPU
#define F_CPU 20000000UL
#endif

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>

#include <util/delay.h>

#define SPI_DDR		DDRB
#define SPI_PORT	PORTB
#define CS_PIN		PORTB2
#define MOSI_PIN	PORTB3
#define MISO_PIN	PORTB4
#define SCK_PIN		PORTB5

#define VBEN_DDR	DDRB
#define VBEN_PORT	PORTB
#define VBEN_PIN	PORTB0

#define IRQ_DDR		DDRD
#define IRQ_PORT	PORTD
#define IRQ_PIN		PIND
#define IRQ_INP		PIND2 //Pin input
#define IRQ_MSK		EIMSK
#define IRQ_CHN		INT0

typedef void (*gcSpiHandleRx)(void *p);
typedef void (*gcSpiHandleTx)(void);

extern unsigned char wlan_tx_buffer[];


//*****************************************************************************
//
// Prototypes for the APIs.
//
//*****************************************************************************
extern void SpiOpen(gcSpiHandleRx pfRxHandler);
extern void SpiClose(void);
extern long SpiFirstWrite(unsigned char *ucBuf, unsigned short usLength);
extern long SpiWrite(unsigned char *pUserBuffer, unsigned short usLength);
extern void SpiResumeSpi(void);
extern void SpiConfigureHwMapping(	unsigned long ulPioPortAddress,
									unsigned long ulPort,
									unsigned long ulSpiCs,
									unsigned long ulPortInt,
									unsigned long uluDmaPort,
									unsigned long ulSsiPortAddress,
									unsigned long ulSsiTx,
									unsigned long ulSsiRx,
									unsigned long ulSsiClck);
//extern void SpiCleanGPIOISR(void);
extern int init_spi(void);

extern void ReadWriteFrame(unsigned char *pTxBuffer, unsigned char *pRxBuffer, unsigned short size);
//extern long TXBufferIsEmpty(void);
//extern long RXBufferIsEmpty(void);

void SpiWriteDataSynchronous(unsigned char *data, unsigned short size);
void SpiWriteAsync(const unsigned char *data, unsigned short size);
void SpiPauseSpi(void);
void SpiResumeSpi(void);
void SSIContReadOperation(void);

void SPI_IRQ(void);
//void poll_irq(void);


#endif /* SPI_H_ */