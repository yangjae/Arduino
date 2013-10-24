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

/* Preinstantiate objects */
StreamSPI StreamSPI0(SPI);

StreamSPI::StreamSPI(SPIClass spidev)
{
	spi = spidev;
}

int StreamSPI::begin()
{
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


int StreamSPI::store(enum buffer_type type, uint8_t val)
{
	volatile uint8_t *buf, *head, *tail;

	if (type == RX_BUFFER) {
		buf = rx_buffer;
		head = rx_head;
		tail = rx_tail;
	} else {
		buf = tx_buffer;
		head = tx_head;
		tail = tx_tail;
	}

	/*
	 * FIXME here we can loose bytes because buffer is full and we cannot
	 * do anything to consume it. It is duty of the program to consume it
	 */
	if (head == tail - 1 || (tail == buf && head == buf + buffer_size - 1))
		return 0;	/* Buffer is full */

	*head = val;
	head = head < buf + buffer_size - 1 ? head + 1 : buf;

	return 1;
}

uint8_t StreamSPI::retrieve(enum buffer_type type)
{
	volatile uint8_t *buf, *head, *tail;
	uint8_t val;

	if (type == RX_BUFFER) {
		buf = rx_buffer;
		head = rx_head;
		tail = rx_tail;
	} else {
		buf = tx_buffer;
		head = tx_head;
		tail = tx_tail;

		/*
		 * When we retrieve a value from he transmission buffer it to
		 * send it. So here we can clear the transmission request flag
		 * because we are going to do it.
		 */
		tx_flag &= ~SPI_TX_FLAG_REQ_TRANS;
	}

	if (head == tail)
		return 0;	/* There are no byte to send */

	val = *tail;
	tail = tail < buf + buffer_size - 1 ? tail + 1 : buf;

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
	SPDR = StreamSPI0.retrieve(TX_BUFFER);
	err = StreamSPI0.store(RX_BUFFER, SPDR);
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
	return retrieve(RX_BUFFER);
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
		n = store(TX_BUFFER, val);
		if (n == 0)	/* TX buffer is full, transmit something */
			waitRequestByteTransfer();
	} while(n == 0);

	return 1;
}
