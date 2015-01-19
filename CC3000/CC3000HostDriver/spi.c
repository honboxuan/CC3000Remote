/*
 * spi.c
 *
 * Created: 17/02/2014 15:47:38
 *  Author: Hon Bo Xuan
 */ 

#include "hci.h"
#include "spi.h"
//#include "evnt_handler.h"
#include "board.h"

#define READ	3
#define WRITE	1

#define HI(value)	(((value) & 0xFF00) >> 8)
#define LO(value)	((value) & 0x00FF)

#define ASSERT_CS		SPI_PORT &= ~(1 << CS_PIN)
#define DEASSERT_CS		SPI_PORT |= (1 << CS_PIN)

#define HEADERS_SIZE_EVNT	(SPI_HEADER_SIZE + 5)

#define SPI_HEADER_SIZE		(5)

#define eSPI_STATE_POWERUP 				 (0)
#define eSPI_STATE_INITIALIZED  		 (1)
#define eSPI_STATE_IDLE					 (2)
#define eSPI_STATE_WRITE_IRQ	   		 (3)
#define eSPI_STATE_WRITE_FIRST_PORTION   (4)
#define eSPI_STATE_WRITE_EOT			 (5)
#define eSPI_STATE_READ_IRQ				 (6)
#define eSPI_STATE_READ_FIRST_PORTION	 (7)
#define eSPI_STATE_READ_EOT				 (8)

typedef struct {
	gcSpiHandleRx  SPIRxHandler;
	unsigned short usTxPacketLength;
	unsigned short usRxPacketLength;
	unsigned long  ulSpiState;
	unsigned char *pTxPacket;
	unsigned char *pRxPacket;
} tSpiInformation;

tSpiInformation sSpiInformation;

// buffer for 5 bytes of SPI HEADER
unsigned char tSpiReadHeader[] = {READ, 0, 0, 0, 0};
	
// The magic number that resides at the end of the TX/RX buffer (1 byte after
// the allocated size) for the purpose of detection of the overrun. The location
// of the memory where the magic number resides shall never be written. In case 
// it is written - the overrun occurred and either receive function or send
// function will stuck forever.
#define CC3000_BUFFER_MAGIC_NUMBER (0xDE)

char spi_buffer[CC3000_RX_BUFFER_SIZE];
unsigned char wlan_tx_buffer[CC3000_TX_BUFFER_SIZE];

//*****************************************************************************
//
//!  SpiOpen
//!
//!  @param  none
//!
//!  @return none
//!
//!  @brief  Open Spi interface 
//
//*****************************************************************************
void SpiOpen(gcSpiHandleRx pfRxHandler) {
	sSpiInformation.ulSpiState = eSPI_STATE_POWERUP;
	
	//memset(spi_buffer, 0, sizeof(spi_buffer));
	//memset(wlan_tx_buffer, 0, sizeof(spi_buffer));
	
	sSpiInformation.SPIRxHandler = pfRxHandler;
	sSpiInformation.usTxPacketLength = 0;
	sSpiInformation.pTxPacket = NULL;
	sSpiInformation.pRxPacket = (unsigned char *)spi_buffer;
	sSpiInformation.usRxPacketLength = 0;
	
	spi_buffer[CC3000_RX_BUFFER_SIZE - 1] = CC3000_BUFFER_MAGIC_NUMBER;
	wlan_tx_buffer[CC3000_TX_BUFFER_SIZE - 1] = CC3000_BUFFER_MAGIC_NUMBER;
	
	// Enable interrupt on WLAN IRQ pin 
	tSLInformation.WlanInterruptEnable();
}

//*****************************************************************************
//
//!  SpiClose
//!
//!  @param  none
//!
//!  @return none
//!
//!  @brief  Close Spi interface
//
//*****************************************************************************
void SpiClose(void) {
	if (sSpiInformation.pRxPacket) {
		sSpiInformation.pRxPacket = 0;
	}
	
	//	Disable Interrupt
	tSLInformation.WlanInterruptDisable();
}

//*****************************************************************************
//
//!  init_spi
//!
//!  @param  none
//!
//!  @return none
//!
//!  @brief  initializes an SPI interface
//
//*****************************************************************************

int init_spi(void) {	
	VBEN_DDR |= (1 << VBEN_PIN); 
	VBEN_PORT &= ~(1 << VBEN_PIN);
	
	SPI_DDR |= (1 << CS_PIN)|(1 << MOSI_PIN)|(1 << SCK_PIN);
	SPI_DDR &= ~(1 << MISO_PIN);
	
	IRQ_DDR &= ~(1 << IRQ_INP);
	IRQ_PORT |= (1 << IRQ_INP); //Weak pull-up (preferably replace with resistor)
	
	SPCR = (1 << SPE)|(1 << MSTR)|(1 << CPHA); //Enable SPI, master, clock phase
	//SPCR = (1 << SPE)|(1 << MSTR);
	SPSR = (1 << SPI2X); //10MHz (F_OSC/4 and double speed)
	
	DEASSERT_CS;
	
	return(ESUCCESS);
}

//*****************************************************************************
//
//! SpiFirstWrite
//!
//!  @param  ucBuf     buffer to write
//!  @param  usLength  buffer's length
//!
//!  @return none
//!
//!  @brief  enter point for first write flow
//
//*****************************************************************************
long SpiFirstWrite(unsigned char *ucBuf, unsigned short usLength) {
	//Workaround for first transaction
	ASSERT_CS;
	
	_delay_us(50);
	
	// SPI writes first 4 bytes of data
	SpiWriteDataSynchronous(ucBuf, 4);
	
	_delay_us(50);
	
	SpiWriteDataSynchronous(ucBuf + 4, usLength - 4);
	
	// From this point on - operate normally
	sSpiInformation.ulSpiState = eSPI_STATE_IDLE;
	
	DEASSERT_CS;
	
	return(0);
}


//*****************************************************************************
//
//!  SpiWrite
//!
//!  @param  pUserBuffer  buffer to write
//!  @param  usLength     buffer's length
//!
//!  @return none
//!
//!  @brief  Spi write operation
//
//*****************************************************************************
long SpiWrite(unsigned char *pUserBuffer, unsigned short usLength) {
	unsigned char ucPad = 0;
	
	// Figure out the total length of the packet in order to figure out if there 
	// is padding or not
	if(!(usLength & 0x0001))
	{
		ucPad++;
	}
	
	pUserBuffer[0] = WRITE;
	pUserBuffer[1] = HI(usLength + ucPad);
	pUserBuffer[2] = LO(usLength + ucPad);
	pUserBuffer[3] = 0;
	pUserBuffer[4] = 0;
	
	usLength += (SPI_HEADER_SIZE + ucPad);
	
	// The magic number that resides at the end of the TX/RX buffer (1 byte after 
	// the allocated size) for the purpose of detection of the overrun. If the 
	// magic number is overwritten - buffer overrun occurred - and we will stuck 
	// here forever!
	if (wlan_tx_buffer[CC3000_TX_BUFFER_SIZE - 1] != CC3000_BUFFER_MAGIC_NUMBER) {
		while (1);
	}
	
	if (sSpiInformation.ulSpiState == eSPI_STATE_POWERUP) {
		while (sSpiInformation.ulSpiState != eSPI_STATE_INITIALIZED);
	}
	
	if (sSpiInformation.ulSpiState == eSPI_STATE_INITIALIZED) {
		// This is time for first TX/RX transactions over SPI: the IRQ is down - 
		// so need to send read buffer size command
		SpiFirstWrite(pUserBuffer, usLength);
	} else {
		// We need to prevent here race that can occur in case 2 back to back 
		// packets are sent to the  device, so the state will move to IDLE and once 
		//again to not IDLE due to IRQ
		tSLInformation.WlanInterruptDisable();
		
		while (sSpiInformation.ulSpiState != eSPI_STATE_IDLE);
		
		sSpiInformation.ulSpiState = eSPI_STATE_WRITE_IRQ;
		sSpiInformation.pTxPacket = pUserBuffer;
		sSpiInformation.usTxPacketLength = usLength;
		
		// Assert the CS line and wait till SSI IRQ line is active and then
		// initialize write operation
		ASSERT_CS;
		
		// Re-enable IRQ - if it was not disabled - this is not a problem...
		tSLInformation.WlanInterruptEnable();

		// check for a missing interrupt between the CS assertion and enabling back the interrupts
		if (tSLInformation.ReadWlanInterruptPin() == 0) {
			SpiWriteDataSynchronous(sSpiInformation.pTxPacket, sSpiInformation.usTxPacketLength);
			
			sSpiInformation.ulSpiState = eSPI_STATE_IDLE;
			
			DEASSERT_CS;
		}
	}
	
	// Due to the fact that we are currently implementing a blocking situation
	// here we will wait till end of transaction
	while (eSPI_STATE_IDLE != sSpiInformation.ulSpiState);
	
	return(0);
}

//*****************************************************************************
//
//!  SpiWriteDataSynchronous
//!
//!  @param  data  buffer to write
//!  @param  size  buffer's size
//!
//!  @return none
//!
//!  @brief  Spi write operation
//
//*****************************************************************************
void SpiWriteDataSynchronous(unsigned char *data, unsigned short size) {
	for (uint16_t i = 0; i < size; i++) {
		SPDR = data[i];
		volatile uint32_t timeout_counter = 0;
		while(!(SPSR & (1 << SPIF)));// && timeout_counter++ < 600000000UL); //Blocking wait
	}
}

//*****************************************************************************
//
//! SpiReadDataSynchronous
//!
//!  @param  data  buffer to read
//!  @param  size  buffer's size
//!
//!  @return none
//!
//!  @brief  Spi read operation
//
//*****************************************************************************
void SpiReadDataSynchronous(unsigned char *data, unsigned short size) {
	unsigned char *data_to_send = tSpiReadHeader;
	
	for (uint16_t i = 0; i < size; i++) {
		SPDR = data_to_send[0];
		volatile uint32_t timeout_counter = 0;
		while(!(SPSR & (1 << SPIF)));// && timeout_counter++ < 600000000UL); //Blocking wait
		data[i] = SPDR;
	}
}

//*****************************************************************************
//
//!  SpiReadHeader
//!
//!  \param  buffer
//!
//!  \return none
//!
//!  \brief   This function enter point for read flow: first we read minimal 5 
//!	          SPI header bytes and 5 Event Data bytes
//
//*****************************************************************************
void SpiReadHeader(void) {
	SpiReadDataSynchronous(sSpiInformation.pRxPacket, 10);
}

//*****************************************************************************
//
//!  SpiReadDataCont
//!
//!  @param  None
//!
//!  @return None
//!
//!  @brief  This function processes received SPI Header and in accordance with 
//!	         it - continues reading the packet
//
//*****************************************************************************
long SpiReadDataCont(void) {
	long data_to_recv;
	unsigned char *evnt_buff, type;
	
	//determine what type of packet we have
	evnt_buff =  sSpiInformation.pRxPacket;
	data_to_recv = 0;
	STREAM_TO_UINT8((char *)(evnt_buff + SPI_HEADER_SIZE), HCI_PACKET_TYPE_OFFSET, type);
	
	switch(type) {
	case HCI_TYPE_DATA: {
			// We need to read the rest of data..
			STREAM_TO_UINT16((char *)(evnt_buff + SPI_HEADER_SIZE), HCI_DATA_LENGTH_OFFSET, data_to_recv);
			if (!((HEADERS_SIZE_EVNT + data_to_recv) & 1)) {
				data_to_recv++;
			}			
			if (data_to_recv) {
				SpiReadDataSynchronous(evnt_buff + 10, data_to_recv);
			}
			break;
		}
	case HCI_TYPE_EVNT: {
			// Calculate the rest length of the data
			STREAM_TO_UINT8((char *)(evnt_buff + SPI_HEADER_SIZE), HCI_EVENT_LENGTH_OFFSET, data_to_recv);
			data_to_recv -= 1;
			
			// Add padding byte if needed
			if ((HEADERS_SIZE_EVNT + data_to_recv) & 1) {
				
				data_to_recv++;
			}
			
			if (data_to_recv) {
				SpiReadDataSynchronous(evnt_buff + 10, data_to_recv);
			}
			
			sSpiInformation.ulSpiState = eSPI_STATE_READ_EOT;
			break;
		}
	}
	
	return (0);
}

//*****************************************************************************
//
//! SpiPauseSpi
//!
//!  @param  none
//!
//!  @return none
//!
//!  @brief  Spi pause operation
//
//*****************************************************************************

void SpiPauseSpi(void) {
	IRQ_MSK &= ~(1 << IRQ_CHN);
}


//*****************************************************************************
//
//! SpiResumeSpi
//!
//!  @param  none
//!
//!  @return none
//!
//!  @brief  Spi resume operation
//
//*****************************************************************************

void SpiResumeSpi(void) {
	IRQ_MSK |= (1 << IRQ_CHN);
}

//*****************************************************************************
//
//! SpiTriggerRxProcessing
//!
//!  @param  none
//!
//!  @return none
//!
//!  @brief  Spi RX processing 
//
//*****************************************************************************
void SpiTriggerRxProcessing(void) {
	// Trigger Rx processing
	SpiPauseSpi();
	DEASSERT_CS;
	
	// The magic number that resides at the end of the TX/RX buffer (1 byte after 
	// the allocated size) for the purpose of detection of the overrun. If the 
	// magic number is overwritten - buffer overrun occurred - and we will stuck 
	// here forever!
	if (sSpiInformation.pRxPacket[CC3000_RX_BUFFER_SIZE - 1] != CC3000_BUFFER_MAGIC_NUMBER) {
		while (1);
	}
	
	sSpiInformation.ulSpiState = eSPI_STATE_IDLE;
	sSpiInformation.SPIRxHandler(sSpiInformation.pRxPacket + SPI_HEADER_SIZE);
}

//*****************************************************************************
//
//! SSIContReadOperation
//!
//!  @param  none
//!
//!  @return none
//!
//!  @brief  SPI read operation
//
//*****************************************************************************
void SSIContReadOperation(void) {
	// The header was read - continue with  the payload read
	if (!SpiReadDataCont()) {
		// All the data was read - finalize handling by switching to the task
		//	and calling from task Event Handler
		SpiTriggerRxProcessing();
	}
}

//*****************************************************************************
//
//!  SPI_IRQ
//!
//!  @param  none
//!
//!  @return none
//!
//!  @brief  When the external SSI WLAN device is read to
//!          interact with Host CPU it generates an interrupt signal.
//!          After that Host CPU has registered this interrupt request
//!          it set the corresponding /CS in active state.
//
//*****************************************************************************
void SPI_IRQ(void) {
	if (sSpiInformation.ulSpiState == eSPI_STATE_POWERUP) {
		//This means IRQ line was low call a callback of HCI Layer to inform
		//on event
		sSpiInformation.ulSpiState = eSPI_STATE_INITIALIZED;
	} else if (sSpiInformation.ulSpiState == eSPI_STATE_IDLE) {
		sSpiInformation.ulSpiState = eSPI_STATE_READ_IRQ;
			
		/* IRQ line goes down - we are start reception */
		ASSERT_CS;
			
		// Wait for TX/RX Compete which will come as DMA interrupt
		SpiReadHeader();
			
		sSpiInformation.ulSpiState = eSPI_STATE_READ_EOT;
			
		SSIContReadOperation();
	} else if (sSpiInformation.ulSpiState == eSPI_STATE_WRITE_IRQ) {
		SpiWriteDataSynchronous(sSpiInformation.pTxPacket,
		sSpiInformation.usTxPacketLength);
			
		sSpiInformation.ulSpiState = eSPI_STATE_IDLE;
			
		DEASSERT_CS;
	}
}


//*****************************************************************************
//
//!  ISR(INT0_vect)
//!
//!  @param  none
//!
//!  @return none
//!
//!  @brief  IRQ interrupt handler.
//
//*****************************************************************************
ISR(INT0_vect) {
	SPI_IRQ();
}

//*****************************************************************************
//
//!  poll_irq
//!
//!  @brief               Checks if the interrupt pin is low
//!                       in case the hardware missed a falling edge
//
//*****************************************************************************
/*
void poll_irq(void) {
	if (!(IRQ_PIN & (1 << IRQ_INP))) {
		cli();
		SPI_IRQ();
		sei();
	}
}
*/