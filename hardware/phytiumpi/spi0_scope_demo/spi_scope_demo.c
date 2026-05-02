/*
 * SPI0 (spidev) burst demo for oscilloscope — Phytium Pi
 *
 * Sends repeating byte patterns on SPI0 at a moderate SCK rate, with a
 * gap between bursts so triggers on a scope are easy.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <getopt.h>
#include <sys/ioctl.h>
#include <linux/ioctl.h>
#include <linux/spi/spidev.h>

#define DEFAULT_DEV	"/dev/spidev0.0"
#define DEFAULT_HZ	100000u
#define DEFAULT_GAP_MS	40u
#define PATTERN_LEN	8

static void usage(const char *argv0)
{
	fprintf(stderr,
		"用法: %s [选项]\n"
		"  -d <路径>   spidev 设备 (默认 %s)\n"
		"  -s <Hz>     SCK 速率上限 (默认 %u，约 100 kHz，便于示波器)\n"
		"  -g <毫秒>   每次突发后的间隙 (默认 %u ms)\n"
		"  -c <次数>   发送突发次数；0=一直跑 (默认 0)\n"
		"  -m <0-3>    SPI 模式 CPOL/CPHA (默认 0)\n"
		"  -h          帮助\n"
		"\n"
		"字节样式固定为 %u 字节: A5 5A FF 00 33 CC AA 55 (周期性重复)。\n"
		"示波器可接 SCK、MOSI、SPI0_CSN0(SPI0_CSN0) 与 GND。\n",
		argv0, DEFAULT_DEV, DEFAULT_HZ, DEFAULT_GAP_MS, PATTERN_LEN);
}

static int set_spi_mode_if_needed(int fd, unsigned int mode_idx)
{
	uint8_t mode8;

	if (mode_idx > 3)
		return -1;

	/*
	 * SPI_IOC_WR_MODE(32) makes spidev merge SPI_CS_HIGH when the
	 * controller uses GPIO CS. Phytium's spi-phytium advertises
	 * mode_bits without SPI_CS_HIGH, so that merge breaks spi_setup
	 * with EINVAL. Skip mode ioctls when mode 0 matches probe default.
	 */
	switch (mode_idx) {
	case 0:
		return 0;
	case 1:
		mode8 = SPI_MODE_1;
		break;
	case 2:
		mode8 = SPI_MODE_2;
		break;
	default:
		mode8 = SPI_MODE_3;
		break;
	}
	if (ioctl(fd, SPI_IOC_WR_MODE, &mode8) < 0)
		return -1;
	return 0;
}

static int set_spi_cfg(int fd, uint32_t speed, unsigned int mode_idx)
{
	uint8_t bits = 8;

	if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0)
		return -1;
	if (set_spi_mode_if_needed(fd, mode_idx) < 0)
		return -1;
	if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0)
		return -1;
	return 0;
}

static int xfer_burst(int fd, const uint8_t *tx, uint8_t *rx, size_t len,
		      uint32_t speed_hz)
{
	struct spi_ioc_transfer tr;

	memset(&tr, 0, sizeof(tr));
	tr.tx_buf = (unsigned long)tx;
	tr.rx_buf = (unsigned long)rx;
	tr.len = len;
	tr.speed_hz = speed_hz;
	tr.bits_per_word = 8;
	tr.delay_usecs = 0;
	tr.cs_change = 0;

	return ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
}

int main(int argc, char **argv)
{
	const char *dev = DEFAULT_DEV;
	uint32_t speed = DEFAULT_HZ;
	unsigned int gap_ms = DEFAULT_GAP_MS;
	unsigned long repeat = 0;
	unsigned int mode = SPI_MODE_0;
	int c, ret, fd;
	static const uint8_t pattern[PATTERN_LEN] = {
		0xA5, 0x5A, 0xFF, 0x00, 0x33, 0xCC, 0xAA, 0x55,
	};
	uint8_t rx[PATTERN_LEN];

	while ((c = getopt(argc, argv, "hd:s:g:c:m:")) != -1) {
		switch (c) {
		case 'd':
			dev = optarg;
			break;
		case 's':
			speed = (uint32_t)strtoul(optarg, NULL, 0);
			break;
		case 'g':
			gap_ms = (unsigned int)strtoul(optarg, NULL, 0);
			break;
		case 'c':
			repeat = strtoul(optarg, NULL, 0);
			break;
		case 'm':
			mode = (unsigned int)strtoul(optarg, NULL, 0);
			if (mode > 3) {
				fprintf(stderr, "模式须为 0..3\n");
				return 1;
			}
			break;
		case 'h':
		default:
			usage(argv[0]);
			return c == 'h' ? 0 : 1;
		}
	}

	fd = open(dev, O_RDWR);
	if (fd < 0) {
		perror(dev);
		fprintf(stderr, "需要读写 %s（通常请 sudo 运行）\n", dev);
		return 1;
	}

	if (set_spi_cfg(fd, speed, mode) < 0) {
		perror("ioctl SPI setup");
		close(fd);
		return 1;
	}

	fprintf(stderr,
		"%s  mode=%u  speed=%u Hz  gap=%u ms  count=%s\n"
		"pattern (%u bytes): ",
		dev, mode, speed, gap_ms,
		repeat ? "有限" : "无限",
		PATTERN_LEN);
	for (c = 0; c < PATTERN_LEN; c++)
		fprintf(stderr, "%02X ", pattern[c]);
	fprintf(stderr, "\n");

	for (unsigned long n = 0; repeat == 0 || n < repeat; n++) {
		ret = xfer_burst(fd, pattern, rx, PATTERN_LEN, speed);
		if (ret < 0) {
			perror("SPI_IOC_MESSAGE");
			close(fd);
			return 1;
		}
		if (gap_ms)
			usleep((useconds_t)gap_ms * 1000u);
	}

	close(fd);
	return 0;
}
