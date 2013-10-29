/*
 * StreamSPI.cpp - Hardware SPI stream
 * Copyright (c) 2013 DogHunter
 * Author Federico Vaga <federicov@linino.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "StreamSPI.h"
#define DEBUG 1

/* Preinstantiate objects */
StreamSPI StreamSPI0(SPI);

StreamSPI::StreamSPI(SPIClass spidev)
{
	spi = spidev;
}

int StreamSPI::begin()
{
	#if DEBUG
	Serial.begin(250000);
	#endif
	return StreamSPI::begin(SPI_DEFAULT_BUFFER_SIZE, SPI_MODE0);
}

int StreamSPI::begin(unsigned int buf_size, unsigned int spi_mode)
{
	int err;

	buffer_size = buf_size;

	/* Allocates RX and TX buffers */
	rx_buffer = (uint8_t *) malloc(buf_size);
	if (!rx_buffer)
		return -1;

	tx_buffer = (uint8_t *) malloc(buf_size);
	if (!rx_buffer) {
		free(rx_buffer);
		return -1;
	}

	/* Clean up memory */
	memset(rx_buffer, 0, buf_size);
	memset(tx_buffer, 0, buf_size);

	/* Reset buffer current position */
	rx_head = rx_buffer;
	tx_head = tx_buffer;
	rx_tail = rx_buffer;
	tx_tail = tx_buffer;

	/* * * Configure SPI as Slave * * */
	pinMode(MISO, OUTPUT);	/* We have to send on master in, *slave out* */
	pinMode(SS, INPUT);
	pinMode(SCK, INPUT);
	pinMode(MOSI, INPUT);

	/* turn on SPI in slave mode */
	SPCR |= _BV(SPE);
	SPCR &= ~_BV(MSTR);

	/* Initialize data before start interrupt */
	SPDR = 0;
	rx_ignore = SPI_DEFAULT_IGNORE_RX;
	tx_ignore = SPI_DEFAULT_IGNORE_TX;

	spi.setDataMode(spi_mode);
	spi.attachInterrupt();
	PORTB &= ~(0x1);	/* Enable pull-up on SS signal */

	/* * * Configure Interrupt for main processor */
	ACSR  &= (~0x04); /* Disable Analog Comparator interrupt
	                     to prevent interrupt during disable */
	ACSR  &= (~0x80); /* Disable Analog Comparator on PE6 */
	EIMSK &= (~0x40); /* Disable INT6 on PE6 */

	PORTE &= (~0x40); /* Set PE6 to 0 */
	DDRE  |= (0x40);  /* Set PE6 as Output */

	tx_flag = 0;

	return 0;
}

void StreamSPI::end()
{
	spi.detachInterrupt();
	free(tx_buffer);
	free(rx_buffer);
}

void StreamSPI::raiseInterrupt()
{
	tx_flag |= SPI_TX_FLAG_REQ_TRANS;
	PORTE |= 0x40;
	PORTE &= ~0x40;
}

void StreamSPI::waitRequestByteTransfer()
{
	/* Raise an interrupt to request transmission */
	raiseInterrupt();

	/* Wait until transmission occur */
	while (tx_flag & SPI_TX_FLAG_REQ_TRANS)
		;
}


int StreamSPI::storeRX(uint8_t val)
{
	int i, ignore = 0;

	if (val == rx_ignore || rx_buf_ignore[0] == rx_ignore) {
		rx_buf_ignore[rx_ignore_index] = val;
		rx_ignore_index++;
		if (rx_ignore_index < SPI_IGNORE_CODE_LENGHT_RX)
			return 1;

		/* Code acquisition is over, verify it */
		for (i = 0; i < SPI_IGNORE_CODE_LENGHT_RX; ++i) {
			if (rx_buf_ignore[i] == rx_ignore)
				continue;

			/* The code tell us to ignore the byte */
			memset(rx_buf_ignore, 0, SPI_IGNORE_CODE_LENGHT_RX);
			return 1;
		}

		/*
		 * At this point, if the code is telling us that the byte is
		 * valid, we can proceed as usual because val contains the
		 * valid value
		 */
	}

	/*
	 * FIXME here we can loose bytes because buffer is full and we cannot
	 * do anything to consume it. It is duty of the program to consume it
	 */
	if (rx_head == rx_tail - 1 || (rx_tail == rx_buffer && rx_head == rx_buffer + buffer_size - 1))
		return 0;	/* Buffer is full */

	#if DEBUG
	Serial.print("RX store  Head pre: ");
	Serial.print((unsigned long)rx_head, HEX);
	Serial.print(" | RX store  Tail pre: ");
	Serial.print((unsigned long)rx_tail, HEX);
	Serial.println(" ===");
	#endif

	*rx_head = val;
	rx_head = rx_head < rx_buffer + buffer_size - 1 ? rx_head + 1 : rx_buffer;

	#if DEBUG
	Serial.print("RX store  Head post: ");
	Serial.print((unsigned long)rx_head, HEX);
	Serial.print(" | RX store  Tail post: ");
	Serial.print((unsigned long)rx_tail, HEX);
	Serial.println(" ===");
	#endif
	return 1;
}

int StreamSPI::storeTX(uint8_t val)
{
	int n;

	/*
	 * FIXME here we can loose bytes because buffer is full and we cannot
	 * do anything to consume it. It is duty of the program to consume it
	 */
	if (tx_head == tx_tail - 1 || (tx_tail == tx_buffer && tx_head == tx_buffer + buffer_size - 1))
		return 0;	/* Buffer is full */

	#if DEBUG
	Serial.print("TX store  Head pre: ");
	Serial.print((unsigned long)tx_head, HEX);
	Serial.print(" | TX store  Tail pre: ");
	Serial.print((unsigned long)tx_tail, HEX);
	Serial.println(" ===");
	#endif

	*tx_head = val;
	tx_head = tx_head < tx_buffer + buffer_size - 1 ? tx_head + 1 : tx_buffer;

	#if DEBUG
	Serial.print("TX store  Head post: ");
	Serial.print((unsigned long)tx_head, HEX);
	Serial.print(" | TX store  Tail post: ");
	Serial.print((unsigned long)tx_tail, HEX);
	Serial.println(" ===");
	#endif

	/*
	 * When storing the invalid value in the transmission buffer, this
	 * function must repeat the value in order to build the code to
	 * validate the byte.
	 */
	if (val == tx_ignore && tx_ignore_index < SPI_IGNORE_CODE_LENGHT_TX) {
		tx_ignore_index++;
		write(tx_ignore);
	} else {
		tx_ignore_index = 0;
	}

	return 1;
}

uint8_t StreamSPI::retrieveTX()
{
	uint8_t val;

	/*
	 * When we retrieve a value from he transmission buffer it to
	 * send it. So here we can clear the transmission request flag
	 * because we are going to do it.
	 */
	tx_flag &= ~SPI_TX_FLAG_REQ_TRANS;

	if (tx_head == tx_tail || tx_last_ignore == tx_ignore) {
		/*
		 * There are no byte to send, so send invalid bytes.
		 * Invalid transmission byte is 2 byte length so we can just
		 * toggle the value on each transfer.
		 *
		 * If the buffer become not empty, but it was sent only half
		 * of the code, we MUST send the other part of it. (this is
		 * the reason of the || in the if)
		 */
		tx_last_ignore = (tx_last_ignore == tx_ignore ? 0x0 : tx_ignore);

		#if DEBUG
		Serial.print("TX ignore retrieve: ");
		Serial.print((unsigned long)tx_last_ignore, HEX);
		Serial.println(" ===");
		#endif

		return tx_last_ignore;
	}

	#if DEBUG
	Serial.print("TX retrieve  Head pre: ");
	Serial.print((unsigned long)tx_head, HEX);
	Serial.print(" | TX retrieve  Tail pre: ");
	Serial.print((unsigned long)tx_tail, HEX);
	Serial.println(" ===");
	#endif

	val = *tx_tail;
	tx_tail = tx_tail < tx_buffer + buffer_size - 1 ? tx_tail + 1 : tx_buffer;

	#if DEBUG
	Serial.print("TX retrieve  Head post: ");
	Serial.print((unsigned long)tx_head, HEX);
	Serial.print(" | TX retrieve  Tail post: ");
	Serial.print((unsigned long)tx_tail, HEX);
	Serial.println(" ===");
	#endif

	return 	val;
}

uint8_t StreamSPI::retrieveRX()
{
	uint8_t val;

	if (rx_head == rx_tail)
		return 0;	/* There are no byte to send */

	#if DEBUG
	Serial.print("RX retrieve  Head pre: ");
	Serial.print((unsigned long)rx_head, HEX);
	Serial.print(" | RX retrieve  Tail pre: ");
	Serial.print((unsigned long)rx_tail, HEX);
	Serial.println(" ===");
	#endif

	val = *rx_tail;
	rx_tail = rx_tail < rx_buffer + buffer_size - 1 ? rx_tail + 1 : rx_buffer;

	#if DEBUG
	Serial.print("RX retrieve  Head post: ");
	Serial.print((unsigned long)rx_head, HEX);
	Serial.print(" | RX retrieve  Tail post: ");
	Serial.print((unsigned long)rx_tail, HEX);
	Serial.println(" ===");
	#endif

	return 	val;
}

/*
 * Each time a byte transfer is complete, the AVR invokes this
 * interrupt which stores the incoming byte and write the next
 * byte to transfer to the master
 */
#ifdef SPI_STC_vect
ISR (SPI_STC_vect)
{
	int err;

	/* Do not handle interrupt on SPI collision */
	if (SPSR & 0x40) {
		return;
	}

	/* Retrieve the next byte to send and store the incoming byte */
	SPDR = StreamSPI0.retrieveTX();
	err = StreamSPI0.storeRX(SPDR);
}
#endif

/* * * Stream methods implementations * * */
int StreamSPI::available(void)
{
	return (rx_tail != rx_head);
}

int StreamSPI::peek(void)
{
	return *tx_tail;
}

int StreamSPI::read(void)
{
	/* FIXME HardwareSerial return -1 when empty, not 0 (EOF) */
	return retrieveRX();
}

void StreamSPI::flush(void)
{
	unsigned int tmp;

	/* Continue transmission until buffer is empty */
	while (tx_head != tx_tail)
		waitRequestByteTransfer();
}
/* * * Print methods implementations * * */
size_t StreamSPI::write(uint8_t val)
{
	int n;

	do {
		/* Try to store the value in the buffer */
		n = storeTX(val);
		if (n == 0)	/* TX buffer is full, transmit something */
			waitRequestByteTransfer();
	} while(n == 0);

	return 1;
}
