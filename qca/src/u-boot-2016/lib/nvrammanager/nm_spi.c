#include <common.h>
#include <spi_flash.h>
#include <mmc.h>

static int debug = 0;

static struct spi_flash *flash = NULL;

#ifndef CONFIG_SF_DEFAULT_SPEED
# define CONFIG_SF_DEFAULT_SPEED	1000000
#endif
#ifndef CONFIG_SF_DEFAULT_MODE
# define CONFIG_SF_DEFAULT_MODE		SPI_MODE_3
#endif
#ifndef CONFIG_SF_DEFAULT_CS
# define CONFIG_SF_DEFAULT_CS		0
#endif
#ifndef CONFIG_SF_DEFAULT_BUS
# define CONFIG_SF_DEFAULT_BUS		0
#endif

int
nm_flashOpPortInit(void)
{
    unsigned int bus = CONFIG_SF_DEFAULT_BUS;
    unsigned int cs = CONFIG_SF_DEFAULT_CS;
    unsigned int speed = CONFIG_SF_DEFAULT_SPEED;
    unsigned int mode = CONFIG_SF_DEFAULT_MODE;

    flash = spi_flash_probe(bus, cs, speed, mode);
    if (!flash)
    {
        printf("Failed to initialize SPI flash at %u:%u\n", bus, cs);
        return -1;
    }

    return 0;
}

int
nm_flashOpPortFree(void)
{
    if (flash)
    {
        spi_flash_free(flash);
        flash = NULL;
    }

    return 0;
}

int
nm_flashOpPortErase(unsigned int offset, unsigned int len)
{
    if (debug)
    {
        printf("sf erase 0x%x +0x%x\n", offset, len);
    }

    if (spi_flash_erase(flash, offset, len))
    {
        printf("SPI flash erase failed\n");
        return -1;
    }
    return 0;
}

int
nm_flashOpPortRead(unsigned int offset, unsigned char *buf, unsigned int len)
{
    if (debug)
    {
        printf("sf read 0x%p 0x%x 0x%x\n", buf, offset, len);
    }

    if (spi_flash_read(flash, offset, len, buf))
    {
        printf("SPI flash read failed\n");
        return -1;
    }
    return 0;
}

int
nm_flashOpPortWrite(unsigned int offset, unsigned char *buf, unsigned int len)
{
    if (debug)
    {
        printf("sf write 0x%p 0x%x 0x%x\n", buf, offset, len);
    }

    if (spi_flash_write(flash, offset, len, buf))
    {
        printf("SPI flash write failed\n");
        return -1;
    }
    return 0;
}

/* add by yangxv for EMMC, 2016.11 */
#define EMMC_BASE 0x04000000	/* 64M */
#define EMMC_BLOCK 512
#define FILE_BASE 0x80000000

u64 nvram_emmc_getsize(void)
{
	struct mmc *mmc = NULL;
	int curr_device = -1;
	u64 size = 0;

	if (get_mmc_num() > 0)
	{
		curr_device = 0;
	}
	else 
	{
		printf("No MMC device available\n");
		return -1;
	}

	mmc = find_mmc_device(curr_device);

	if (!mmc) 
	{
		printf("no mmc device at slot %x\n", curr_device);
		return -1;
	}

	
	mmc_init(mmc);
	printf("mmc size is %llu.\n", mmc->capacity);
	
	size = mmc->capacity + EMMC_BASE;

	return size;
}

int nvram_emmc_write(unsigned int *address, const void *data, int len)
{
	unsigned int blk, cnt, n;

	char cmd_buff[128] = {0};

	if ((unsigned int)address < EMMC_BASE)
	{
		printf("Invalid address - %x\n", address);
		return -1;
	}
	
	blk = (unsigned int)address - EMMC_BASE;
	blk = blk/EMMC_BLOCK;

	cnt = len/EMMC_BLOCK;
	if (len%EMMC_BLOCK != 0)
	{
		cnt++;
	}

	memmove(FILE_BASE, data, len);

	sprintf(cmd_buff, "mmc erase 0x%x 0x%x.\n", blk, cnt);
	if (debug)
	{
		printf("erase cmd is %s.\n", cmd_buff);
	}

	if (run_command(cmd_buff, 0) != CMD_RET_SUCCESS) 
	{
		printf("cmd %s error.\n", cmd_buff);
		return -1;
	}
	mdelay(1000);

	sprintf(cmd_buff, "mmc write 0x%x 0x%x 0x%x.\n", FILE_BASE, blk, cnt);
	if (debug)
	{
		printf("write cmd is %s.\n", cmd_buff);
	}
	
	if (run_command(cmd_buff, 0) != CMD_RET_SUCCESS) 
	{
		printf("cmd %s error.\n", cmd_buff);
		return -1;
	}
	mdelay(1000);
	
	return 0;
}

/* end add */