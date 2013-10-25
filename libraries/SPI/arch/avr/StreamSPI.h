/*
 * StreamSPI.h - Hardware SPI stream
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

#ifndef HardwareSPIstream_h
#define HardwareSPIstream_h

#include <inttypes.h>
#include "SPI_Class.h"
#include "Stream.h"

#define SPI_DEFAULT_BUFFER_SIZE	64
#define SPI_DEFAULT_IGNORE_TX 0x0
#define SPI_DEFAULT_IGNORE_RX 0x0

#define SPI_TX_FLAG_REQ_TRANS	(1 << 0)

enum buffer_type {
	TX_BUFFER,
	RX_BUFFER,
};

class StreamSPI : public Stream
{
	protected:
	SPIClass spi;
	uint8_t *rx_buffer;	/* Allocated on begin(), freed on end() */
	uint8_t *tx_buffer;	/* Allocated on begin(), freed on end() */
	unsigned int buffer_size;
	/*
	 * head: is where we put a value
	 * tail: is where we get a value
	 *
	 * head == tail     ----> the buffer is empty
	 * head == tail + 1 ----> the buffer is full
	 */
	volatile uint8_t *rx_head;
	volatile uint8_t *tx_head;
	volatile uint8_t *rx_tail;
	volatile uint8_t *tx_tail;

	volatile unsigned int tx_flag;
	virtual void raiseInterrupt();
	virtual void waitRequestByteTransfer();

	uint8_t tx_ignore;
	uint8_t rx_ignore;

	public:
	StreamSPI(SPIClass spidev);
	int begin();
	int begin(unsigned int buf_size, unsigned int spi_mode);
	void end();

	int storeTX(uint8_t val);
	uint8_t retrieveTX();
	int storeRX(uint8_t val);
	uint8_t retrieveRX();

	/* From Stream.h */
	virtual int available(void);
	virtual int peek(void);
	virtual int read(void);
	virtual void flush(void);
	/* From Print.h */
	virtual size_t write(uint8_t val);
};

extern StreamSPI StreamSPI0;

#endif // HardwareSPIstream_h
