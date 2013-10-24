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

#include <errno.h>
#include "StreamSPI.h"


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

	/* Allocates RX and TX buffers */
	rx_buffer = malloc(buf_size);
	if (!rx_buffer)
		return -ENOMEM;

	tx_buffer = malloc(buf_size);
	if (!rx_buffer) {
		free(rx_buffer);
		return -ENOMEM;
	}

	/* Clean up memory */
	memset(rx_buffer, 0, buf_size);
	memset(tx_buffer, 0, buf_size);

	/* Reset buffer current position */
	rx_pos = 0;
	tx_pos = 0;

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

	return 0;
}

void StreamSPI::end()
{
	free(tx_buffer);
	free(rx_buffer);
}
