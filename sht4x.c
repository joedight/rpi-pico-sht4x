#include "/usr/local/include/util/string.h"

#include "main.c"
#include "config.h"

enum sht_cmd {
	SHT_CMD_MEASURE_HP		= 0xFD,
	SHT_CMD_MEASURE_MP		= 0xF6,
	SHT_CMD_MEASURE_LP		= 0xE0,
	SHT_CMD_READSERIAL		= 0x89,
	SHT_CMD_SOFTRESET		= 0x94,
	SHT_CMD_HEAT_200mW_1000ms	= 0x39,
	SHT_CMD_HEAT_200mW_100ms	= 0x32,
	SHT_CMD_HEAT_110mW_1000ms	= 0x2F,
	SHT_CMD_HEAT_110mW_100ms	= 0x24,
	SHT_CMD_HEAT_20mW_1000ms	= 0x1E,
	SHT_CMD_HEAT_20mW_100ms		= 0x15,
};

/* Delay for any command except heater.
 * Spec. says max is 8ms, so this is plenty. */
static const unsigned char SHT_DELAY_MEASURE = 25;

static const unsigned char SHT_I2C_ADDR = 0x44;

enum sht_error {
	ERROR_SHT_CHECKSERIAL_READ = ERROR_FINAL + 1,
	ERROR_SHT_CHECKSERIAL_WRITE,
	ERROR_SHT_CHECKSERIAL_CHECKSUM,
	ERROR_SHT_READ,
	ERROR_SHT_WRITE,
	ERROR_SHT_CHECKSUM,
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

void sht_cmd_blocking(uint8_t cmd, uint16_t *buf)
{
	if (i2c_write_blocking(our_i2c, SHT_I2C_ADDR, &cmd, 1, false) != 1) {
		fatal_error(ERROR_SHT_READ);
	}

	sleep_ms(SHT_DELAY_MEASURE);

	uint8_t data[6] = { 0 };
	if (i2c_read_blocking(our_i2c, SHT_I2C_ADDR, data, sizeof(data), false) != sizeof(data)) {
		fatal_error(ERROR_SHT_WRITE);
	}

	if (crc8(data) != data[2])
		fatal_error(ERROR_SHT_CHECKSUM);
	if (crc8(data + 3) != data[5])
		fatal_error(ERROR_SHT_CHECKSUM);

	buf[0] = (data[0] << 8) | data[1];
	buf[1] = (data[3] << 8) | data[4];
}

void measure_init(void)
{
	/* Test vector from spec. */
	uint8_t chk[2] = {0xBE, 0xEF};
	if (crc8(chk) != 0x92) {
		fatal_error(ERROR_CHECKSUM_TEST);
	}

	/* Test sensor by reading serial */
	uint8_t cmd = SHT_CMD_READSERIAL;
	if (i2c_write_blocking(our_i2c, SHT_I2C_ADDR, &cmd, 1, false) != 1) {
		fatal_error(ERROR_SHT_CHECKSERIAL_READ);
	}

	sleep_ms(SHT_DELAY_MEASURE);

	uint8_t serial[6] = { 0 };
	if (i2c_read_blocking(our_i2c, SHT_I2C_ADDR, serial, sizeof(serial), false) != sizeof(serial)) {
		fatal_error(ERROR_SHT_CHECKSERIAL_WRITE);
	}

	if (crc8(serial) != serial[2])
		fatal_error(ERROR_SHT_CHECKSERIAL_CHECKSUM);
	if (crc8(serial + 3) != serial[5])
		fatal_error(ERROR_SHT_CHECKSERIAL_CHECKSUM);
}

struct measurement *measure_take(void)
{
	static struct measurement ms[3] = {
		{ .name = "temp", .type = "gauge", .value = NULL },
		{ .name = "humid", .type = "gauge", .value = NULL },
		{ 0 }
	};

	uint16_t buf[2] = {0, 0};
	sht_cmd_blocking(SHT_CMD_MEASURE_HP, buf);

	double temp = (175.0 * ((double)buf[0] / 65535.0)) - 45.0;
	double humid = (125.0 * ((double)buf[1] / 65535.0)) - 6.0;

	ms[0].value = rsprintf(ms[0].value, "%f", temp);
	ms[1].value = rsprintf(ms[1].value, "%f", humid);

	return ms;
}

