/*
 * IM-Me display functions
 *
 * Copyright 2010 Dave
 * http://daveshacks.blogspot.com/2010/01/im-me-lcd-interface-hacked.html
 *
 * Copyright 2010 Michael Ossmann
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#include <cc1110.h>
#include "ioCCxx10_bitdef.h"
#include "display.h"
#include "bits.h"
#include "types.h"
#include "5x7.h"

void sleepMillis(int ms) {
	int j;
	while (--ms > 0) { 
		for (j=0; j<1200;j++); // about 1 millisecond
	};
}

void xtalClock() { // Set system clock source to 26 Mhz
    SLEEP &= ~SLEEP_OSC_PD; // Turn both high speed oscillators on
    while( !(SLEEP & SLEEP_XOSC_S) ); // Wait until xtal oscillator is stable
    CLKCON = (CLKCON & ~(CLKCON_CLKSPD | CLKCON_OSC)) | CLKSPD_DIV_1; // Select xtal osc, 26 MHz
    while (CLKCON & CLKCON_OSC); // Wait for change to take effect
    SLEEP |= SLEEP_OSC_PD; // Turn off the other high speed oscillator (the RC osc)
}

void setIOPorts() {
	//No need to set PERCFG or P2DIR as default values on reset are fine
	P0SEL |= (BIT5 | BIT3 ); // set SCK and MOSI as peripheral outputs
	P0DIR |= BIT4 | BIT2; // set SSN and A0 as outputs
	P1DIR |= BIT1; // set LCDRst as output
	P2DIR = BIT3 | BIT4; // set LEDs  as outputs
	//LED_GREEN = LOW; // Turn the Green LED on (LEDs driven by reverse logic: 0 is ON)
}

// Set a clock rate of approx. 2.5 Mbps for 26 MHz Xtal clock
#define SPI_BAUD_M  170
#define SPI_BAUD_E  16

void configureSPI() {
	U0CSR = 0;  //Set SPI Master operation
	U0BAUD =  SPI_BAUD_M; // set Mantissa
	U0GCR = U0GCR_ORDER | SPI_BAUD_E; // set clock on 1st edge, -ve clock polarity, MSB first, and exponent
}
void tx(unsigned char ch) {
	U0DBUF = ch;
	while(!(U0CSR & U0CSR_TX_BYTE)); // wait for byte to be transmitted
	U0CSR &= ~U0CSR_TX_BYTE;         // Clear transmit byte status
}

void txData(unsigned char ch) {
	A0 = HIGH;
	tx(ch);
}

void txCtl(unsigned char ch){
	A0 = LOW;
	tx(ch);
}

void LCDReset(void) {
	LCDRst = LOW; // hold down the RESET line to reset the display
	sleepMillis(1);
	LCDRst = HIGH;
	SSN = LOW;
	/* initialization sequence from sniffing factory firmware */
	txCtl(RESET);
	txCtl(SET_REG_RESISTOR);
	txCtl(VOLUME_MODE_SET);
	txCtl(0x60); /* contrast */
	txCtl(DC_DC_CLOCK_SET);
	txCtl(0x00); /* fOSC (no division) */
	txCtl(POWER_SUPPLY_ON);
	txCtl(ADC_REVERSE);
	txCtl(DISPLAY_ON);
	txCtl(ALL_POINTS_NORMAL);
	SSN = HIGH;
}

/* initiate sleep mode */
void LCDPowerSave() {
	txCtl(STATIC_INDIC_OFF);
	txCtl(DISPLAY_OFF);
	txCtl(ALL_POINTS_ON); // Display all Points on cmd = Power Save when following LCD off
}

void setCursor(unsigned char row, unsigned char col) {
	txCtl(SET_ROW | (row & 0x0f));
	txCtl(SET_COL_LO | (col & 0x0f));
	txCtl(SET_COL_HI | ( (col>>4) & 0x0f));
}

void setDisplayStart(unsigned char start) {
	txCtl(0x40 | (start & 0x3f)); // set Display start address
}

void setNormalReverse(unsigned char normal) {  // 0 = Normal, 1 = Reverse
	txCtl(DISPLAY_NORMAL | (normal & 0x01) );
}

/* clear all LCD pixels */
void clear() {
	u8 row;
	u8 col;

	SSN = LOW;
	setDisplayStart(0);

	/* normal display mode (not inverted) */
	setNormalReverse(0);

	for (row = 0; row <= 9; row++) {
		setCursor(row, 0);
	for (col = 0; col < WIDTH; col++)
		txData(0x00);
	}

	SSN = HIGH;
}

/* sdcc provides printf if we provide this */
void putchar(char c) {
	u8 i;

	c &= 0x7f;

	if (c >= FONT_OFFSET) {
		for (i = 0; i < FONT_WIDTH; i++)
			txData(font[c - FONT_OFFSET][i]);
		txData(0x00);
	}
}
