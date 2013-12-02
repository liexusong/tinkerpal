/*-----------------------------------------------------------------------*/
/* MMC/SDC (in SPI mode) control module  (C)ChaN, 2007                   */
/*-----------------------------------------------------------------------*/
/* Only rcvr_spi(), xmit_spi(), disk_timerproc() and some macros         */
/* are platform dependent.                                               */
/*-----------------------------------------------------------------------*/

/*
 * This file was modified from a sample available from the FatFs
 * web site. It was modified to work with a Stellaris EK-LM3S6965
 * evaluation board.
 *
 * Note that the SSI port is shared with the osram display. The code
 * in this file does not attempt to share the SSI port with the osram,
 * it assumes the osram is not being used.
 *
 * File was modified to suit TinkerPal
 */

#include "util/debug.h"
#include "util/tp_types.h"
#include "main/console.h"
#include "platform/platform.h"
#include "drivers/gpio/gpio.h"
#include "drivers/spi/spi.h"
#include "drivers/block/block.h"
#include "drivers/mmc/mmc.h"

/* Definitions for MMC/SDC command */
#define CMD0 (0x40+0) /* GO_IDLE_STATE */
#define CMD1 (0x40+1) /* SEND_OP_COND */
#define CMD8 (0x40+8) /* SEND_IF_COND */
#define CMD9 (0x40+9) /* SEND_CSD */
#define CMD10 (0x40+10) /* SEND_CID */
#define CMD12 (0x40+12) /* STOP_TRANSMISSION */
#define CMD16 (0x40+16) /* SET_BLOCKLEN */
#define CMD17 (0x40+17) /* READ_SINGLE_BLOCK */
#define CMD18 (0x40+18) /* READ_MULTIPLE_BLOCK */
#define CMD23 (0x40+23) /* SET_BLOCK_COUNT */
#define CMD24 (0x40+24) /* WRITE_BLOCK */
#define CMD25 (0x40+25) /* WRITE_MULTIPLE_BLOCK */
#define CMD41 (0x40+41) /* SEND_OP_COND (ACMD) */
#define CMD55 (0x40+55) /* APP_CMD */
#define CMD58 (0x40+58) /* READ_OCR */

struct mmc_t {
    int spi_port;
    int mosi;
    int cs;
    volatile int disc_status;
};

#define CARD_MMC(c) ((c) & (1 << 0))
#define CARD_SDC(c) ((c) & (1 << 1))
#define CARD_BLOCK_ADDRESSING(c) ((c) & (1 << 2))
static u8 card_type;
static mmc_t g_mmc = { 
    .disc_status = BLOCK_DISK_STATUS_NO_INIT|BLOCK_DISK_STATUS_NO_DISK 
};

/* Platform dependent functions */

static inline int TICKS(void)
{
    return platform.get_ticks_from_boot();
}

static inline void cs_low(void)
{
    /* asserts the CS pin to the card */
    gpio_digital_write(g_mmc.cs, 0);
}

static inline void cs_high(void)
{
    /* de-asserts the CS pin to the card */
    gpio_digital_write(g_mmc.cs, 1);
}

static void xmit_spi(u8 dat)
{
    spi_send(g_mmc.spi_port, dat);
}

static void xmit_spi_multi(u8 *data, int cnt)
{
    while (cnt)
    {
	xmit_spi(*data++);
	xmit_spi(*data++);
	cnt -= 2;
    }
}

static u8 rcvr_spi(void)
{
    return (u8)spi_receive(g_mmc.spi_port);
}

static u8 wait_ready(void)
{
    u8 res;
    int expiry;

    /* Wait for ready in timeout of 500ms */
    expiry = TICKS() + 500;
    rcvr_spi();
    while (((res = rcvr_spi()) != 0xFF) && TICKS() < expiry);

    return res;
}

/* Send 80 or so clock transitions with CS and DI held high. This is
 * required after card power up to get it into SPI mode
 */
static void send_initial_clock_train(void)
{
    unsigned int i;

    cs_high();

    /* Switch the SSI TX line to a GPIO and drive it high too. */
    gpio_set_pin_mode(g_mmc.mosi, GPIO_PM_OUTPUT);
    gpio_digital_write(g_mmc.mosi, 1);

    /* Send 10 bytes over the SSI. This causes the clock to wiggle the */
    /* required number of times
     */
    for (i = 0; i < 10; i++)
	spi_send(g_mmc.spi_port, 0xFF); /* Write DUMMY data */

    /* Revert to hardware control of the SSI TX line. */
    spi_reconf(g_mmc.spi_port);
}

/* When the target system does not support socket power control, there
 * is nothing to do in these functions and chk_power always returns 1.
 */
static void power_on(void)
{
    /* This doesn't really turn the power on, but initializes the
     * SSI port and pins needed to talk to the card.
     */

    if (spi_init(g_mmc.spi_port))
	return;

    gpio_set_pin_mode(g_mmc.cs, GPIO_PM_OUTPUT);

    /* Deassert the chip select */
    gpio_digital_write(g_mmc.cs, 1);

    /* Set DI and CS high and apply more than 74 pulses to SCLK for the card */
    /* to be able to accept a native command. */
    send_initial_clock_train();
}

/* Common MMC API */

/* Note: Byte count must be even number */
static int rcvr_datablock(u8 *buff, u32 byte_count)
{
    u8 token;
    int expiry;

    /* Wait for data packet in timeouts of 100ms */
    expiry = TICKS() + 100;
    while (((token = rcvr_spi()) == 0xFF) && TICKS() < expiry);

    if (token != 0xFE)
    {    
	/* Invalid token */
	return -1;
    }

    do 
    {
	/* Receive the data block into buffer */
        *buff++ = rcvr_spi();
        *buff++ = rcvr_spi();
    } while (byte_count -= 2);
    rcvr_spi(); /* Discard CRC */
    rcvr_spi();
    return 0;
}

#ifndef MMC_READONLY
/* buff: 512 byte data block to be transmitted
 * token: Data/Stop token
 */
static int xmit_datablock(const u8 *buff, u8 token)
{
    u8 resp;

    if (wait_ready() != 0xFF) 
	return -1;

    xmit_spi(token);
    if (token == 0xFD) 
    {
	/* Stop token. Finished */
	return 0;
    }

    /* Data token */
    xmit_spi_multi(buff, 512);
    xmit_spi(0xFF); /* CRC (Dummy) */
    xmit_spi(0xFF);
    return ((rcvr_spi() & 0x1F) == 0x05) ? 0 : -1;
}
#endif

static u8 send_cmd(u8 cmd, u32 arg)
{
    u8 n, res;
    
    if (wait_ready() != 0xFF) 
	return 0xFF;

    /* Send command packet */
    xmit_spi(cmd); /* Command */
    xmit_spi((u8)(arg >> 24)); /* Argument[31..24] */
    xmit_spi((u8)(arg >> 16)); /* Argument[23..16] */
    xmit_spi((u8)(arg >> 8)); /* Argument[15..8] */
    xmit_spi((u8)arg); /* Argument[7..0] */
    n = 0xff;
    if (cmd == CMD0) 
	n = 0x95; /* CRC for CMD0(0) */
    if (cmd == CMD8) 
	n = 0x87; /* CRC for CMD8(0x1AA) */
    xmit_spi(n);

    /* Receive command response */
    if (cmd == CMD12)
	rcvr_spi(); /* Skip a stuff byte when stop reading */

    /* Wait for a valid response in timeout of 10 attempts */
    n = 10;
    do
    {
        res = rcvr_spi();
    } while ((res & 0x80) && --n);

    return res;
}

/*-----------------------------------------------------------------------*
 * Send the special command used to terminate a multi-sector read.
 *
 * This is the only command which can be sent while the SDCard is sending
 * data. The SDCard spec indicates that the data transfer will stop 2 bytes
 * after the 6 byte CMD12 command is sent and that the card will then send
 * 0xFF for between 2 and 6 more bytes before the R1 response byte.  This
 * response will be followed by another 0xFF byte.  In testing, however, it
 * seems that some cards don't send the 2 to 6 0xFF bytes between the end of
 * data transmission and the response code.  This function, therefore, merely
 * reads 10 bytes and, if the last one read is 0xFF, returns the value of the
 * latest non-0xFF byte as the response code.
 *
 *-----------------------------------------------------------------------*/
static u8 send_cmd12(void)
{
    u8 n, res, val;

    /* For CMD12, we don't wait for the card to be idle before we send
     * the new command.
     */

    /* Send command packet - the argument for CMD12 is ignored. */
    xmit_spi(CMD12);
    xmit_spi(0);
    xmit_spi(0);
    xmit_spi(0);
    xmit_spi(0);
    xmit_spi(0);

    /* Read up to 10 bytes from the card, remembering the value read if it's
       not 0xFF */
    for(n = 0; n < 10; n++)
    {
        val = rcvr_spi();
        if (val != 0xFF)
            res = val;
    }

    return res;
}

/* MMC CBs */
int mmc_spi_disk_init(void)
{
    u8 n, ty, ocr[4];
    int expiry;

    if (g_mmc.disc_status & BLOCK_DISK_STATUS_NO_DISK)
    { 
	/* No card in the socket */
	return g_mmc.disc_status;
    }

    /* Force socket power on */
    power_on();
    send_initial_clock_train();

    cs_low();
    ty = 0;
    if (send_cmd(CMD0, 0) == 1) 
    {
	/* Enter Idle state */
        expiry = TICKS() + 1000; /* Initialization timeout of 1000 msec */
        if (send_cmd(CMD8, 0x1AA) == 1) 
	{
	    /* SDC Ver2+ */
            for (n = 0; n < 4; n++) ocr[n] = rcvr_spi();
            if (ocr[2] == 0x01 && ocr[3] == 0xAA) 
	    {    
		/* The card can work at vdd range of 2.7-3.6V */
                do 
		{
                    if (send_cmd(CMD55, 0) <= 1 && 
			send_cmd(CMD41, 1UL << 30) == 0)
		    {    
			/* ACMD41 with HCS bit */
		    	break;
		    }
                } while (TICKS() < expiry);
                if (TICKS() < expiry && send_cmd(CMD58, 0) == 0) 
		{
		    /* Check CCS bit */
                    for (n = 0; n < 4; n++)
			ocr[n] = rcvr_spi();
                    ty = (ocr[0] & 0x40) ? 6 : 2;
                }
            }
        } 
	else 
	{
	    /* SDC Ver1 or MMC */
	    expiry = TICKS() + 1000; /* Initialization timeout of 1000 msec */
            ty = (send_cmd(CMD55, 0) <= 1 && send_cmd(CMD41, 0) <= 1) ? 2 : 1; /* SDC : MMC */
            do 
	    {
                if (ty == 2) 
		{
                    if (send_cmd(CMD55, 0) <= 1 && send_cmd(CMD41, 0) == 0) 
			break;    /* ACMD41 */
                } 
		else 
		{
                    if (send_cmd(CMD1, 0) == 0) 
			break;                                /* CMD1 */
                }
            } while (TICKS() < expiry);
	    /* Select R/W block length */
            if (TICKS() >= expiry || send_cmd(CMD16, 512) != 0)
                ty = 0;
        }
    }
    card_type = ty;
    cs_high();
    rcvr_spi(); /* Idle (Release DO) */

    if (ty) 
    {
	/* Initialization succeded */
        g_mmc.disc_status &= ~BLOCK_DISK_STATUS_NO_INIT;
	spi_set_max_speed(g_mmc.spi_port, platform.get_system_clock() / 2);
    } 
    else 
    { 
	/* Initialization failed */
    }

    return g_mmc.disc_status ? -1 : 0;
}

int mmc_spi_disk_status(void)
{
    return g_mmc.disc_status ? -1 : 0;
}

int mmc_spi_disk_read(u8 *buff, int sector, int count)
{
    if (!count) 
	return -1;

    if (g_mmc.disc_status & BLOCK_DISK_STATUS_NO_INIT) 
	return -1;

    if (!(CARD_BLOCK_ADDRESSING(card_type)))
    { 
	/* Convert to byte address if needed */
	sector *= 512;
    }

    cs_low();

    if (count == 1) 
    {    
	/* Single block read */
        if ((send_cmd(CMD17, sector) == 0) /* READ_SINGLE_BLOCK */ && 
	    !rcvr_datablock(buff, 512))
	{
            count = 0;
	}
    }
    else 
    {
	/* Multiple block read */
        if (send_cmd(CMD18, sector) == 0) 
	{ 
	    /* READ_MULTIPLE_BLOCK */
            do 
	    {
                if (rcvr_datablock(buff, 512)) 
		    break;
                buff += 512;
            } while (--count);
            send_cmd12(); /* STOP_TRANSMISSION */
        }
    }

    cs_high();
    rcvr_spi(); /* Idle (Release DO) */

    return count ? -1 : 0;
}

#ifndef MMC_READONLY
int mmc_spi_disk_write(const u8 *buff, int sector, int count)
{
    if (!count || g_mmc.disc_status & BLOCK_DISK_STATUS_NO_INIT ||
        g_mmc.disc_status & BLOCK_DISK_STATUS_PROTECTED)
    {
	return -1;
    }

    if (!(CARD_BLOCK_ADDRESSING(card_type)))
    {
	/* Convert to byte address if needed */
	sector *= 512;
    }

    cs_low();

    if (count == 1) 
    {
    	/* Single block write */
        if ((send_cmd(CMD24, sector) == 0) /* WRITE_BLOCK */ && 
	    !xmit_datablock(buff, 0xFE))
	{
            count = 0;
	}
    }
    else 
    {
	/* Multiple block write */
        if (CARD_SDC(card_type))
	{
            send_cmd(CMD55, 0); 
	    send_cmd(CMD23, count); /* ACMD23 */
        }
        if (send_cmd(CMD25, sector) == 0) 
	{
	    /* WRITE_MULTIPLE_BLOCK */
            do 
	    {
                if (xmit_datablock(buff, 0xFC)) 
		    break;
                buff += 512;
            } while (--count);
            if (xmit_datablock(0, 0xFD))
	    {
		/* STOP_TRAN token */
                count = 1;
	    }
        }
    }

    cs_high();
    rcvr_spi(); /* Idle (Release DO) */

    return count ? -1 : 0;
}
#endif

int mmc_spi_disk_ioctl(int cmd, void *buff)
{
    int res = -1;
    u8 n, csd[16];
    u16 csize;

    if (g_mmc.disc_status & BLOCK_DISK_STATUS_NO_INIT) 
	return -1;

    cs_low();

    switch (cmd) 
    {
    case BLOCK_IOCTL_GET_SECTOR_COUNT:
    	/* Get number of sectors on the disk (u32) */
	if ((send_cmd(CMD9, 0) == 0) && !rcvr_datablock(csd, 16)) 
	{
	    if ((csd[0] >> 6) == 1) 
	    {
	    	/* SDC ver 2.00 */
		csize = csd[9] + ((u16)csd[8] << 8) + 1;
		*(u32*)buff = (u32)csize << 10;
	    } 
	    else 
	    {
		/* MMC or SDC ver 1.XX */
		n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
		csize = (csd[8] >> 6) + ((u16)csd[7] << 2) + ((u16)(csd[6] & 3) << 10) + 1;
		*(u32*)buff = (u32)csize << (n - 9);
	    }
	    res = 0;
	}
	break;
    case BLOCK_IOCTL_GET_SECTOR_SIZE:
    	/* Get sectors on the disk (u16) */
	*(u16*)buff = 512;
	res = 0;
	break;
    case BLOCK_IOCTL_SYNC:
    	/* Make sure that data has been written */
	if (wait_ready() == 0xFF)
	    res = 0;
	break;
#if 0
    case MMC_GET_CSD:
    	/* Receive CSD as a data block (16 bytes) */
	if (send_cmd(CMD9, 0) == 0 /* READ_CSD */ && !rcvr_datablock(ptr, 16))
	    res = 0;
	break;
    case MMC_GET_CID:
    	/* Receive CID as a data block (16 bytes) */
	if (send_cmd(CMD10, 0) == 0 /* READ_CID */ && !rcvr_datablock(ptr, 16))
	    res = 0;
	break;
    case MMC_GET_OCR:
    	/* Receive OCR as an R3 resp (4 bytes) */
	if (send_cmd(CMD58, 0) == 0) 
	{
	    /* READ_OCR */
	    for (n = 0; n < 4; n++)
		*ptr++ = rcvr_spi();
	    res = 0;
	}
#if 0
    case MMC_GET_TYPE:
    	/* Get card type flags (1 byte) */
	*ptr = card_type;
	res = 0;
	break;
#endif
#endif
    default:
	break;
    }

    cs_high();
    rcvr_spi();            /* Idle (Release DO) */
    return res;
}

void mmc_init(int spi_port, int mosi, int cs)
{
    g_mmc.mosi = mosi;
    g_mmc.cs = cs;
    g_mmc.spi_port = spi_port;
    g_mmc.disc_status = BLOCK_DISK_STATUS_NO_INIT;
}

#if 0
/* This is a real time clock service to be called from
* FatFs module. Any valid time must be returned even if
* the system does not support a real time clock
*/

u32 get_fattime (void)
{
    return    ((2007UL-1980) << 25)    // Year = 2007
            | (6UL << 21)            // Month = June
            | (5UL << 16)            // Day = 5
            | (11U << 11)            // Hour = 11
            | (38U << 5)            // Min = 38
            | (0U >> 1)                // Sec = 0
            ;
}
#endif
