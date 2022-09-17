#include "/usr/local/include/util/string.h"

#include "main.c"
#include "config.h"

enum sht3_cmd {
	SHT3_CMD_MEASURE_CS_HP	= 0x2C06,
};

static const unsigned char SHT_I2C_ADDR = 0x44;

enum sht_error {
	ERROR_SHT3_READ = 1,
	ERROR_SHT3_WRITE,
	ERROR_SHT3_CHECKSUM,
};

uint8_t crc8(uint8_t *data)
{
	uint8_t crc = 0xFF;
	for (int i = 0; i < 2; i++) {
		crc ^= data[i];
		for (int j = 0; j < 8; j++) {
			if (crc & 0x80)
				crc = (crc << 1) ^ 0x31;
			else
				crc = crc << 1;
		}
	}
	return crc;
}

void sht_cmd_blocking(uint16_t cmd, uint16_t *buf)
{
	uint8_t cmd_b[] = { (cmd >> 8) & 0xFF, cmd & 0xFF };
	if (i2c_write_blocking(our_i2c, SHT_I2C_ADDR, &cmd_b, 2, false) != 2) {
		fatal_error(ERROR_SHT3_WRITE);
	}

	sleep_ms(1000);

	uint8_t data[6] = { 0 };
	if (i2c_read_blocking(our_i2c, SHT_I2C_ADDR, data, sizeof(data), false) != sizeof(data)) {
		fatal_error(ERROR_SHT3_READ);
	}

	if (crc8(data) != data[2])
		fatal_error(ERROR_SHT3_CHECKSUM);
	if (crc8(data + 3) != data[5])
		fatal_error(ERROR_SHT3_CHECKSUM);

	if (buf) {
		buf[0] = (data[0] << 8) | data[1];
		buf[1] = (data[3] << 8) | data[4];
	}
}

void measure_init(void)
{
	/* Test vector from spec. */
	uint8_t chk[2] = {0xBE, 0xEF};
	if (crc8(chk) != 0x92) {
		fatal_error(ERROR_CHECKSUM_TEST);
	}

	/* Check we can read data */
	sht_cmd_blocking(SHT3_CMD_MEASURE_CS_HP, NULL);
}

struct measurement *measure_take(void)
{
	static struct measurement ms[3] = {
		{ .name = "temp", .type = "gauge", .value = NULL },
		{ .name = "humid", .type = "gauge", .value = NULL },
		{ 0 }
	};

	uint16_t buf[2] = {0, 0};
	sht_cmd_blocking(SHT3_CMD_MEASURE_CS_HP, buf);

	double temp = (175.0 * ((double)buf[0] / 65535.0)) - 45.0;
	double humid = 100.0 * ((double)buf[1] / 65535.0);

	ms[0].value = rsprintf(ms[0].value, "%f", temp);
	ms[1].value = rsprintf(ms[1].value, "%f", humid);

	return ms;
}

