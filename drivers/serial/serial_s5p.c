/*
 * (C) Copyright 2009 SAMSUNG Electronics
 * Minkyu Kang <mk7.kang@samsung.com>
 * Heungjun Kim <riverful.kim@samsung.com>
 *
 * based on drivers/serial/s3c64xx.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <common.h>
#include <linux/compiler.h>		  /* __weak */
#include <serial.h>			  /* struct serial_device */
#include <asm/io.h>			  /* writel(), readl(), ... */
#include <asm/arch/uart.h>		  /* struct s5p_uart */
#include <asm/arch/clk.h>		  /* get_uart_clk() */
#include <asm/arch/cpu.h>		  /* samsung_get_base_uart() */

/* We need constant addresses for UART registers for initialization */
#define S5PC100_UART0 (S5PC100_UART_BASE + 0x0000)
#define S5PC100_UART1 (S5PC100_UART_BASE + 0x0400)
#define S5PC100_UART2 (S5PC100_UART_BASE + 0x0800)
#define S5PC100_UART3 (S5PC100_UART_BASE + 0x0C00)

#define S5PC110_UART0 (S5PC110_UART_BASE + 0x0000)
#define S5PC110_UART1 (S5PC110_UART_BASE + 0x0400)
#define S5PC110_UART2 (S5PC110_UART_BASE + 0x0800)
#define S5PC110_UART3 (S5PC110_UART_BASE + 0x0C00)

/*
 * The coefficient, used to calculate the baudrate on S5P UARTs is
 * calculated as
 * C = UBRDIV * 16 + number_of_set_bits_in_UDIVSLOT
 * however, section 31.6.11 of the datasheet doesn't recomment using 1 for 1,
 * 3 for 2, ... (2^n - 1) for n, instead, they suggest using these constants:
 */
static const int udivslot[] = {
	0,
	0x0080,
	0x0808,
	0x0888,
	0x2222,
	0x4924,
	0x4a52,
	0x54aa,
	0x5555,
	0xd555,
	0xd5d5,
	0xddd5,
	0xdddd,
	0xdfdd,
	0xdfdf,
	0xffdf,
};

static void s5p_serial_setbrg(const struct serial_device *sdev)
{
	DECLARE_GLOBAL_DATA_PTR;
	struct s5p_uart *const uart = (struct s5p_uart *)(sdev->dev.priv);
	u32 uclk = get_uart_clk(0);	/* Same for all ports on S5PC1xx */
	u32 baudrate = gd->baudrate;
	u32 val;

	val = uclk / baudrate;

	writel(val / 16 - 1, &uart->ubrdiv);

	if (s5p_uart_divslot())
		writew(udivslot[val % 16], &uart->rest.slot);
	else
		writeb(val % 16, &uart->rest.value);
}

/*
 * Initialise the serial port with the given baudrate. The settings
 * are always 8 data bits, no parity, 1 stop bit, no start bits.
 */
static int s5p_serial_start(const struct stdio_dev *pdev)
{
	struct serial_device *sdev = to_serial_device(pdev);
	struct s5p_uart *const uart = (struct s5p_uart *)(sdev->dev.priv);

	/* reset and enable FIFOs, set triggers to the maximum */
	writel(7, &uart->ufcon);
	writel(0, &uart->umcon);
	/* 8N1 */
	writel(0x3, &uart->ulcon);
	/* No interrupts, no DMA, pure polling, PCLK as clock source */
	writel(0x245, &uart->ucon);

	s5p_serial_setbrg(sdev);

	return 0;
}

static void s5p_ll_putc(struct s5p_uart *const uart, const char c)
{
	/* If \n, do \r first */
	if (c == '\n')
		s5p_ll_putc(uart, '\r');

	/* wait for room in the tx FIFO */
	while (!(readl(&uart->utrstat) & 0x2)) {
		/* Check for break */
		if (readl(&uart->uerstat) & 0x8)
			return;
	}

	writeb(c, &uart->utxh);
}

/*
 * Output a single byte to the serial port.
 */
static void s5p_serial_putc(const struct stdio_dev *pdev, const char c)
{
	struct serial_device *sdev = to_serial_device(pdev);
	struct s5p_uart *const uart = (struct s5p_uart *)(sdev->dev.priv);

	s5p_ll_putc(uart, c);
}

/*
 * Output a string to the serial port.
 */
static void s5p_serial_puts(const struct stdio_dev *pdev, const char *s)
{
	struct serial_device *sdev = to_serial_device(pdev);
	struct s5p_uart *const uart = (struct s5p_uart *)(sdev->dev.priv);

	while (*s)
		s5p_ll_putc(uart, *s++);
}

/*
 * Read a single byte from the serial port. 
 */
static int s5p_serial_getc(const struct stdio_dev *pdev)
{
	struct serial_device *sdev = to_serial_device(pdev);
	struct s5p_uart *const uart = (struct s5p_uart *)(sdev->dev.priv);

	/* Wait for character to arrive */
	while (!(readl(&uart->utrstat) & 0x1)) {
		/* Check for break, Frame Err, Parity Err, Overrun Err */
		if (readl(&uart->uerstat) & 0xF)
			return 0;
	}

	return (int)(readb(&uart->urxh) & 0xff);
}

/*
 * Test whether a character is in the RX buffer
 */
static int s5p_serial_tstc(const struct stdio_dev *pdev)
{
	struct serial_device *sdev = to_serial_device(pdev);
	struct s5p_uart *const uart = (struct s5p_uart *)(sdev->dev.priv);

	return (int)(readl(&uart->utrstat) & 0x1);
}


#define INIT_S5P_SERIAL(_addr, _name, _hwname) {	\
	{       /* stdio_dev part */		\
		.name = _name,			\
		.hwname = _hwname,		\
		.flags = DEV_FLAGS_INPUT | DEV_FLAGS_OUTPUT, \
		.start = s5p_serial_start,	\
		.stop = NULL,			\
		.getc = s5p_serial_getc,	\
		.tstc =	s5p_serial_tstc,	\
		.putc = s5p_serial_putc,	\
		.puts = s5p_serial_puts,	\
		.priv = (void *)_addr,		\
	},					\
	.setbrg = s5p_serial_setbrg,		\
}

struct serial_device s5pc100_serial_device[] = {
	INIT_S5P_SERIAL(S5PC100_UART0, CONFIG_SYS_SERCON_NAME "0", "s5p_uart0"),
	INIT_S5P_SERIAL(S5PC100_UART1, CONFIG_SYS_SERCON_NAME "1", "s5p_uart1"),
	INIT_S5P_SERIAL(S5PC100_UART2, CONFIG_SYS_SERCON_NAME "2", "s5p_uart2"),
	INIT_S5P_SERIAL(S5PC100_UART3, CONFIG_SYS_SERCON_NAME "3", "s5p_uart3"),
};

struct serial_device s5pc110_serial_device[] = {
	INIT_S5P_SERIAL(S5PC110_UART0, CONFIG_SYS_SERCON_NAME "0", "s5p_uart0"),
	INIT_S5P_SERIAL(S5PC110_UART1, CONFIG_SYS_SERCON_NAME "1", "s5p_uart1"),
	INIT_S5P_SERIAL(S5PC110_UART2, CONFIG_SYS_SERCON_NAME "2", "s5p_uart2"),
	INIT_S5P_SERIAL(S5PC110_UART3, CONFIG_SYS_SERCON_NAME "3", "s5p_uart3"),
};

/* Get pointer to n-th serial device */
struct serial_device *get_serial_device(unsigned int n)
{
	if (n < 4) {
		if (cpu_is_s5pc100())
			return &s5pc100_serial_device[n];
		else if (cpu_is_s5pc110())
			return &s5pc110_serial_device[n];
	}

	return NULL;
}
