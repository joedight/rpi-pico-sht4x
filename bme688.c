#include "/usr/local/include/util/string.h"

#include "main.c"
#include "config.h"

enum {
	BME_I2C_ADDR = 0x76,
};

enum bme_error {
	ERROR_BME_WRITE = 1,
	ERROR_BME_READ_SEND,
	ERROR_BME_READ_RECV,
	ERROR_I2C_NOT_FOUND,
};

enum bme_reg {
	BME_REG_PAR_T1_LSB	= 0xE9,
	BME_REG_PAR_T1_MSB	= 0xEA,
	BME_REG_PAR_T2_LSB	= 0x8A,
	BME_REG_PAR_T2_MSB	= 0x8B,
	BME_REG_PAR_T3		= 0x8C,

	BME_REG_PAR_P1_LSB	= 0x8E,
	BME_REG_PAR_P1_MSB	= 0x8F,
	BME_REG_PAR_P2_LSB	= 0x90,
	BME_REG_PAR_P2_MSB	= 0x91,
	BME_REG_PAR_P3		= 0x92,
	BME_REG_PAR_P4_LSB	= 0x94,
	BME_REG_PAR_P4_MSB	= 0x95,
	BME_REG_PAR_P5_LSB	= 0x96,
	BME_REG_PAR_P5_MSB	= 0x97,
	BME_REG_PAR_P6		= 0x99,
	BME_REG_PAR_P7		= 0x98,
	BME_REG_PAR_P8_LSB	= 0x9C,
	BME_REG_PAR_P8_MSB	= 0x9D,
	BME_REG_PAR_P9_LSB	= 0x9E,
	BME_REG_PAR_P9_MSB	= 0x9F,
	BME_REG_PAR_P10		= 0xA0,

	BME_REG_PAR_H1_LSB	= 0xE2, /* <3:0> */
	BME_REG_PAR_H1_MSB	= 0xE3,
	BME_REG_PAR_H2_LSB	= 0xE2, /* <7:4> */
	BME_REG_PAR_H2_MSB	= 0xE1,

	BME_REG_PAR_H3		= 0xE4,
	BME_REG_PAR_H4		= 0xE5,
	BME_REG_PAR_H5		= 0xE6,
	BME_REG_PAR_H6		= 0xE7,
	BME_REG_PAR_H7		= 0xE8,

	BME_REG_TEMP_ADC_0_MSB	= 0x22, /* temp_adc[19:12] */
	BME_REG_TEMP_ADC_0_LSB	= 0x23, /* temp_adc[11:4] */
	BME_REG_TEMP_ADC_0_XLSB	= 0x24, /* temp_adc[3:0] */

	BME_REG_PRESS_ADC_0_MSB	= 0x1F, /* press_adc[19:12] */
	BME_REG_PRESS_ADC_0_LSB	= 0x20, /* press_adc[11:4] */
	BME_REG_PRESS_ADC_0_XLSB= 0x21, /* press_adc[3:0] */

	BME_REG_HUM_ADC_0_MSB	= 0x25, /* hum_adc[15:8] */
	BME_REG_HUM_ADC_0_LSB	= 0x26, /* hum_adc[7:0] */

	/* spi_3w_int_en<6,6>, osrs_h<2:0> */
	BME_REG_CTRL_HUM	= 0x72,
	/* osrs_t<7:5>, osrs_p<4:2>, mode<1:0>  */
	BME_REG_CTRL_MEAS	= 0x74,
	/* new_data<7> */
	BME_REG_MEAS_STATUS	= 0x1D,
};

enum bme_ctrl {
	BME_OVERSAMPLE_0x = 0x0,
	BME_OVERSAMPLE_1x = 0x1,
	BME_OVERSAMPLE_2x = 0x2,
	BME_OVERSAMPLE_4x = 0x3,
	BME_OVERSAMPLE_8x = 0x4,
	BME_OVERSAMPLE_16x = 0x5,

	BME_MODE_SLEEP		= 0x0,
	BME_MODE_FORCED		= 0x1,
	BME_MODE_PARALLEL	= 0x2,
};

void bme_reg_write(unsigned char reg, unsigned char data)
{
	unsigned char buf[] = { reg, data };
	int ret = i2c_write_blocking(our_i2c, BME_I2C_ADDR, buf, 2, false);
	if (ret == PICO_ERROR_GENERIC) {
		fatal_error(ERROR_I2C_NOT_FOUND);
	} else if (ret != 2) {
		fatal_error(ERROR_BME_WRITE);
	}
}

unsigned char bme_reg_read(unsigned char reg)
{
	unsigned char data = 0;
	if (i2c_write_blocking(our_i2c, BME_I2C_ADDR, &reg, 1, false) != 1) {
		fatal_error(ERROR_BME_READ_SEND);
	}
	if (i2c_read_blocking(our_i2c, BME_I2C_ADDR, &data, 1, false) != 1) {
		fatal_error(ERROR_BME_READ_RECV);
	}
	return data;
}

void bme_reg_reads(unsigned char reg, size_t num, unsigned char *buffer)
{
	if (i2c_write_blocking(our_i2c, BME_I2C_ADDR, &reg, 1, false) != 1) {
		fatal_error(ERROR_BME_READ_SEND);
	}
	if (i2c_read_blocking(our_i2c, BME_I2C_ADDR, buffer, num, false) != num) {
		fatal_error(ERROR_BME_READ_RECV);
	}
}

void measure_init(void)
{
	/* osrs_h */
	bme_reg_write(BME_REG_CTRL_HUM, BME_OVERSAMPLE_16x);
}

struct measurement *measure_take(void)
{
	static struct measurement ms[] = {
		{ .name = "temp", .type = "gauge", .value = NULL },
		{ .name = "pressure", .type = "gauge", .value = NULL },
		{ .name = "humid", .type = "gauge", .value = NULL },
		{ 0 },
	};

	/* osrs_t, osrs_p, mode  */
	bme_reg_write(BME_REG_CTRL_MEAS, BME_MODE_FORCED | (BME_OVERSAMPLE_16x << 2) | (BME_OVERSAMPLE_16x << 5));

	do {
		sleep_ms(1000);
	} while (~bme_reg_read(BME_REG_MEAS_STATUS) & (1 << 7));

	const uint16_t par_t1 = bme_reg_read(BME_REG_PAR_T1_LSB) | (bme_reg_read(BME_REG_PAR_T1_MSB) << 8),
		 par_t2 = bme_reg_read(BME_REG_PAR_T2_LSB) | (bme_reg_read(BME_REG_PAR_T2_MSB) << 8),
		 par_t3 = bme_reg_read(BME_REG_PAR_T3);

	const uint32_t temp_adc =
		(bme_reg_read(BME_REG_TEMP_ADC_0_MSB) << 12) |
		(bme_reg_read(BME_REG_TEMP_ADC_0_LSB) << 4) |
		(bme_reg_read(BME_REG_TEMP_ADC_0_XLSB) >> 4);


	double temp_comp, t_fine;
	{
		double var1, var2;
		var1 = (((double)temp_adc / 16384.0) - ((double)par_t1 / 1024.0)) * (double)par_t2;
		var2 = ((((double)temp_adc / 131072.0) - ((double)par_t1 / 8192.0)) *
			(((double)temp_adc / 131072.0) - ((double)par_t1 / 8192.0))) *
			((double)par_t3 * 16.0);
		t_fine = var1 + var2;
		temp_comp = t_fine / 5120.0;
	}


	const uint16_t
		par_p1 = bme_reg_read(BME_REG_PAR_P1_LSB) | (bme_reg_read(BME_REG_PAR_P1_MSB) << 8),
		par_p2 = bme_reg_read(BME_REG_PAR_P2_LSB) | (bme_reg_read(BME_REG_PAR_P2_MSB) << 8),
		par_p3 = bme_reg_read(BME_REG_PAR_P3),
		par_p4 = bme_reg_read(BME_REG_PAR_P4_LSB) | (bme_reg_read(BME_REG_PAR_P4_MSB) << 8),
		par_p5 = bme_reg_read(BME_REG_PAR_P5_LSB) | (bme_reg_read(BME_REG_PAR_P5_MSB) << 8),
		par_p6 = bme_reg_read(BME_REG_PAR_P6),
		par_p7 = bme_reg_read(BME_REG_PAR_P7),
		par_p8 = bme_reg_read(BME_REG_PAR_P8_LSB) | (bme_reg_read(BME_REG_PAR_P8_MSB) << 8),
		par_p9 = bme_reg_read(BME_REG_PAR_P9_LSB) | (bme_reg_read(BME_REG_PAR_P9_MSB) << 8),
		par_p10 = bme_reg_read(BME_REG_PAR_P10);

	const uint32_t press_adc =
		(bme_reg_read(BME_REG_PRESS_ADC_0_MSB) << 12) |
		(bme_reg_read(BME_REG_PRESS_ADC_0_LSB) << 4) |
		(bme_reg_read(BME_REG_PRESS_ADC_0_XLSB) >> 4);

	double press_comp;
	{
		double var1, var2, var3;

		var1 = ((double)t_fine / 2.0) - 64000.0;
		var2 = var1 * var1 * ((double)par_p6 / 131072.0);
		var2 = var2 + (var1 * (double)par_p5 * 2.0);
		var2 = (var2 / 4.0) + ((double)par_p4 * 65536.0);
		var1 = ((((double)par_p3 * var1 * var1) / 16384.0) +
			((double)par_p2 * var1)) / 524288.0;
		var1 = (1.0 + (var1 / 32768.0)) * (double)par_p1;
		press_comp = 1048576.0 - (double)press_adc;
		if (var1 != 0.0) {
			press_comp = ((press_comp - (var2 / 4096.0)) * 6250.0) / var1;
			var1 = ((double)par_p9 * press_comp * press_comp) / 2147483648.0;
			var2 = press_comp * ((double)par_p8 / 32768.0);
			var3 = (press_comp / 256.0) * (press_comp / 256.0) *
				(press_comp / 256.0) * (par_p10 / 131072.0);
			press_comp = press_comp + (var1 + var2 + var3 +
				((double)par_p7 * 128.0)) / 16.0;
		} else {
			press_comp = 0.0;
		}
	}

	const uint16_t
		par_h1 = (bme_reg_read(BME_REG_PAR_H1_LSB) & 0x0F) | (bme_reg_read(BME_REG_PAR_H1_MSB) << 4),
		par_h2 = (bme_reg_read(BME_REG_PAR_H2_LSB) >> 4) | (bme_reg_read(BME_REG_PAR_H2_MSB) << 4),
		par_h3 = bme_reg_read(BME_REG_PAR_H3),
		par_h4 = bme_reg_read(BME_REG_PAR_H4),
		par_h5 = bme_reg_read(BME_REG_PAR_H5),
		par_h6 = bme_reg_read(BME_REG_PAR_H6),
		par_h7 = bme_reg_read(BME_REG_PAR_H7);

	const uint32_t hum_adc =
		bme_reg_read(BME_REG_HUM_ADC_0_LSB) | (bme_reg_read(BME_REG_HUM_ADC_0_MSB) << 8);

	double hum_comp;
	{
		const double
			var1 = hum_adc - (((double)par_h1 * 16.0) + (((double)par_h3 / 2.0) * temp_comp)),
			var2 = var1 * (((double)par_h2 / 262144.0) * (1.0 + (((double)par_h4 / 16384.0) *
				temp_comp) + (((double)par_h5 / 1048576.0) * temp_comp * temp_comp))),
			var3 = (double)par_h6 / 16384.0,
			var4 = (double)par_h7 / 2097152.0;
		hum_comp = var2 + ((var3 + (var4 * temp_comp)) * var2 * var2);
	}

	ms[0].value = rsprintf(ms[0].value, "%f", temp_comp);
	ms[1].value = rsprintf(ms[1].value, "%f", press_comp);
	ms[2].value = rsprintf(ms[2].value, "%f", hum_comp);

	return ms;
}

