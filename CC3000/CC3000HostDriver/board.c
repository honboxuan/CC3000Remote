/*
 * board.c
 *
 * Created: 08/06/2014 18:39:52
 *  Author: Hon Bo Xuan
 */ 

#include "wlan.h"
#include "evnt_handler.h"    // callback function declaration
#include "nvmem.h"
#include "socket.h"
#include "netapp.h"
#include "board.h"

#include "../debug.h"

//*****************************************************************************
//
//! ReadWlanInterruptPin
//!
//! @param  none
//!
//! @return none
//!
//! @brief  return wlan interrupt pin
//
//*****************************************************************************
long ReadWlanInterruptPin(void) {
	return (IRQ_PIN & (1 << IRQ_INP));
}

//*****************************************************************************
//
//! WlanInterruptEnable
//!
//! @param  none
//!
//! @return none
//!
//! @brief  Enable wlan IrQ pin
//
//*****************************************************************************
void WlanInterruptEnable(void) {
	IRQ_MSK |= (1 << IRQ_CHN);
}

//*****************************************************************************
//
//! WlanInterruptDisable
//!
//! @param  none
//!
//! @return none
//!
//! @brief  Disable wlan IrQ pin
//
//*****************************************************************************
void WlanInterruptDisable(void) {
	IRQ_MSK &= ~(1 << IRQ_CHN);
}

//*****************************************************************************
//
//! WriteWlanPin
//!
//! @param  val value to write to wlan pin
//!
//! @return none
//!
//! @brief  write value to wlan pin
//
//*****************************************************************************

void WriteWlanPin(uint8_t trueFalse) {	
	if(trueFalse) {
		VBEN_PORT |= (1 << VBEN_PIN);
	} else {
		VBEN_PORT &= ~(1 << VBEN_PIN);
	}
}

//*****************************************************************************
//
//! sendDriverPatch
//!
//! @param  pointer to the length
//!
//! @return none
//!
//! @brief  The function returns a pointer to the driver patch: 
//!         since there is no patch in the host - it returns 0
//
//*****************************************************************************
char *sendDriverPatch(unsigned long *Length) {
	*Length = 0;
	return NULL;
}


//*****************************************************************************
//
//! sendBootLoaderPatch
//!
//! @param  pointer to the length
//!
//! @return none
//!
//! @brief  The function returns a pointer to the boot loader patch: 
//!         since there is no patch in the host - it returns 0
//
//*****************************************************************************
char *sendBootLoaderPatch(unsigned long *Length) {
	*Length = 0;
	return NULL;
}

//*****************************************************************************
//
//! sendWLFWPatch
//!
//! @param  pointer to the length
//!
//! @return none
//!
//! @brief  The function returns a pointer to the FW patch:
//!         since there is no patch in the host - it returns 0
//
//*****************************************************************************
char *sendWLFWPatch(unsigned long *Length) {
	*Length = 0;
	return NULL;
}

//*****************************************************************************
//
//! CC3000_UsynchCallback
//!
//! @param  lEventType Event type
//! @param  data
//! @param  length
//!
//! @return none
//!
//! @brief  The function handles asynchronous events that come from CC3000  
//!		      device and operates a LED4 to have an on-board indication
//
//*****************************************************************************

void CC3000_UsynchCallback(long lEventType, char * data, unsigned char length) {
	//debug_Transmit("->2\n");
	
	if (lEventType == HCI_EVNT_WLAN_ASYNC_SIMPLE_CONFIG_DONE) {
		//ulSmartConfigFinished = 1;
		//ucStopSmartConfig     = 1;  
	}
	if (lEventType == HCI_EVNT_WLAN_UNSOL_CONNECT) {
		//ulCC3000Connected = 1;
		
		// Turn on the LED2
		//turnLedOn(LED2);
		FLAGS |= (1 << CONNECTED);
		debug_Transmit("Connected\n");
	}
	if (lEventType == HCI_EVNT_WLAN_UNSOL_DISCONNECT) {		
		//ulCC3000Connected = 0;
		//ulCC3000DHCP      = 0;
		//ulCC3000DHCP_configured = 0;
		//printOnce = 1;
		FLAGS &= ~(1 << CONNECTED);
		FLAGS &= ~(1 << DHCP);
		debug_Transmit("Disconnected\n");
		
		// Turn off the LED3
		//turnLedOff(LED2);
		// Turn off LED3
		//turnLedOff(LED3);          

	}
	if (lEventType == HCI_EVNT_WLAN_UNSOL_DHCP) {
		// Notes: 
		// 1) IP config parameters are received swapped
		// 2) IP config parameters are valid only if status is OK, i.e. ulCC3000DHCP becomes 1
		
		// only if status is OK, the flag is set to 1 and the addresses are valid
		if ( *(data + NETAPP_IPCONFIG_MAC_OFFSET) == 0) {
			//sprintf( (char*)pucCC3000_Rx_Buffer,"IP:%d.%d.%d.%d\f\r", data[3],data[2], data[1], data[0] );

			//ulCC3000DHCP = 1;
			
			FLAGS |= (1 << DHCP);
			debug_Transmit("DHCP\n");

			//turnLedOn(LED3);
		} else {
			//ulCC3000DHCP = 0;
			
			
			FLAGS &= ~(1 << DHCP);
			debug_Transmit("Not DHCP\n");
			
			//turnLedOff(LED3);
		}
		
	}
	if (lEventType == HCI_EVENT_CC3000_CAN_SHUT_DOWN) {
		//OkToDoShutDown = 1;
		
		FLAGS |= (1 << SHUTDOWN_OK);
		debug_Transmit("Shut down ok\n");
	}
	
	//debug_Transmit("2->\n");
}