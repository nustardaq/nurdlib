/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2019, 2021, 2023-2024
 * Bastian Löher
 * Hans Toshihide Törnqvist
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301  USA
 */

#include <module/sis_3316/sis_3316_i2c.h>
#include <sys/types.h>
#include <string.h>
#include <module/map/map.h>
#include <module/sis_3316/offsets.h>
#include <nurdlib/log.h>
#include <nurdlib/serialio.h>
#include <util/assert.h>
#include <util/time.h>

#define I2C_ACK        8
#define I2C_START      9
#define I2C_REP_START 10
#define I2C_STOP      11
#define I2C_WRITE     12
#define I2C_READ      13
#define I2C_BUSY      31

#define SPI_SI5325_ADDR  0x0000
#define SPI_SI5325_WRITE 0x4000
#define SPI_SI5325_READ  0x8000
#define SPI_BUSY_MASK    0x80000000

#define SPI_REG_BYPASS		0
#define SPI_REG_BW_SELECT	2
#define SPI_REG_POWER_DOWN	11
#define SPI_REG_N1		25
#define SPI_REG_NC1_19_16	31
#define SPI_REG_NC1_15_8	32
#define SPI_REG_NC1_7_0		33
#define SPI_REG_NC2_19_16	34
#define SPI_REG_NC2_15_8	35
#define SPI_REG_NC2_7_0		36
#define SPI_REG_N2_19_16	40
#define SPI_REG_N2_15_8		41
#define SPI_REG_N2_7_0		42
#define SPI_REG_N31_18_16	43
#define SPI_REG_N31_15_8	44
#define SPI_REG_N31_7_0		45
#define SPI_REG_ICAL		136

#define SPI_DO_ICAL 0x40
#define SPI_BYPASS_OFF 0
#define SPI_BYPASS_ON 2
#define SPI_POWER_DOWN_CLOCK_INPUT_2 2

#define SPI_IS_BUSY(data) ((data & SPI_BUSY_MASK) == SPI_BUSY_MASK)
#define SPI_ICAL_IS_ACTIVE(data) ((data & SPI_DO_ICAL) == SPI_DO_ICAL)
#define SPI_POLL_COUNTER_MAX 100
#define SPI_CALIB_READY_POLL_COUNTER_MAX 1000

#define OSC_ADR 0x55

/* I2C interface to si570 sample clock oscillator */
int sis_3316_i2c_wait_busy(struct Sis3316Module* m, int osc);
void sis_3316_i2c_start(struct Sis3316Module* m, int osc);
void sis_3316_i2c_stop(struct Sis3316Module* m, int osc);
void sis_3316_i2c_write_byte(struct Sis3316Module* m,
    int osc, unsigned char data);
void sis_3316_si570_freeze_dco(struct Sis3316Module* m, int osc);
void sis_3316_si570_divider(struct Sis3316Module* m,
    int osc, unsigned char preset[6]);
void sis_3316_si570_unfreeze_dco(struct Sis3316Module* m, int osc);
void sis_3316_si570_new_freq(struct Sis3316Module* m, int osc);
void sis_3316_si570_dcm_reset(struct Sis3316Module* m, int osc);
void sis_3316_si570_read_divider(struct Sis3316Module *, int, unsigned char *,
    size_t);
void sis_3316_i2c_read_byte(struct Sis3316Module *, int, unsigned char *,
    char);

uint8_t sis_3316_spi_action(struct Sis3316Module *, uint16_t);
void sis_3316_si5325_clk_multiplier_write(struct Sis3316Module *, uint8_t,
    uint8_t);
uint8_t sis_3316_si5325_clk_multiplier_read(struct Sis3316Module *, uint8_t);
void sis_3316_si5325_clk_multiplier_do_internal_calibration(struct
    Sis3316Module *m);

#define SIS3316_SET_EXT_MUL_CFG(c, bw, hs, n1, n2) \
	c.bandwidth_select = bw; \
	c.n1_high_speed_divider = hs; \
	c.n1_output_divider_clock_1 = n1; \
	c.n2_feedback_divider = n2;

struct Sis3316ExternalClockMultiplierConfig
sis_3316_make_ext_clk_mul_config(enum Sis3316ExternalClockInputFrequency
    a_in_freq, enum Sis3316ExternalClockOutputFrequency a_out_freq)
{
	struct Sis3316ExternalClockMultiplierConfig c;

	LOGF(debug)(LOGL, "make_ext_clk_mul_config:a_in_freq = %d",
	    a_in_freq);
	LOGF(debug)(LOGL, "make_ext_clk_mul_config:a_out_freq = %d",
	    a_out_freq);

	switch (a_out_freq)
	{
	case OUT_250_MHZ:
		switch (a_in_freq)
		{
		case IN_10_MHZ:
			SIS3316_SET_EXT_MUL_CFG(c, 0, 5, 4, 500);
			break;
		case IN_12_MHZ:
			SIS3316_SET_EXT_MUL_CFG(c, 0, 11, 2, 440);
			break;
		case IN_20_MHZ:
			SIS3316_SET_EXT_MUL_CFG(c, 1, 5, 4, 250);
			break;
		case IN_50_MHZ:
			SIS3316_SET_EXT_MUL_CFG(c, 2, 11, 1, 110);
			break;
		default:
			log_die(LOGL, "Unsupported input frequency %d.",
			    a_in_freq);
		}
		break;
	case OUT_125_MHZ:
		switch (a_in_freq)
		{
		case IN_10_MHZ:
			SIS3316_SET_EXT_MUL_CFG(c, 0, 4, 10, 500);
			break;
		case IN_12_MHZ:
			SIS3316_SET_EXT_MUL_CFG(c, 0, 7, 6, 420);
			break;
		case IN_20_MHZ:
			SIS3316_SET_EXT_MUL_CFG(c, 1, 5, 8, 250);
			break;
		case IN_50_MHZ:
			SIS3316_SET_EXT_MUL_CFG(c, 2, 5, 8, 100);
			break;
		default:
			log_die(LOGL, "Unsupported input frequency %d.",
			    a_in_freq);
		}
		break;
	default:
		log_die(LOGL, "Unsupported output frequency %d.",
		    a_out_freq);
	}

	c.n1_output_divider_clock_2 = c.n1_output_divider_clock_1;
	c.n3_input_divider = 1;

	LOGF(debug)(LOGL, "ext_clk_mul_cfg.n1_1 = %u",
	    c.n1_output_divider_clock_1);
	LOGF(debug)(LOGL, "ext_clk_mul_cfg.n1_2 = %u",
	    c.n1_output_divider_clock_2);
	LOGF(debug)(LOGL, "ext_clk_mul_cfg.n2 = %u", c.n2_feedback_divider);
	LOGF(debug)(LOGL, "ext_clk_mul_cfg.n3 = %u", c.n3_input_divider);
	LOGF(debug)(LOGL, "ext_clk_mul_cfg.bw_select = %u",
	    c.bandwidth_select);
	LOGF(debug)(LOGL, "ext_clk_mul_cfg.n1_hs = %u",
	    c.n1_high_speed_divider);

	return c;
}

void
sis_3316_si5325_clk_multiplier_bypass(struct Sis3316Module *m)
{
	sis_3316_si5325_clk_multiplier_write(m, SPI_REG_BYPASS,
	    SPI_BYPASS_ON);
	sis_3316_si5325_clk_multiplier_write(m, SPI_REG_POWER_DOWN,
	    SPI_POWER_DOWN_CLOCK_INPUT_2);
}

void
sis_3316_si5325_clk_multiplier_set(struct Sis3316Module *m, struct
    Sis3316ExternalClockMultiplierConfig a_config)
{
	LOGF(debug)(LOGL, "Begin setting clock multiplier.");
	ASSERT(uint8_t, "u", a_config.bandwidth_select, <=, 15);
	ASSERT(uint8_t, "u", a_config.n1_high_speed_divider, >=, 4);
	ASSERT(uint8_t, "u", a_config.n1_high_speed_divider, <=, 11);
	ASSERT(uint32_t, "u", a_config.n1_output_divider_clock_1, !=, 0);
	ASSERT(uint32_t, "u", a_config.n1_output_divider_clock_1, <, 1 << 20);
	ASSERT(uint32_t, "u", a_config.n1_output_divider_clock_2, !=, 0);
	ASSERT(uint32_t, "u", a_config.n1_output_divider_clock_2, <, 1 << 20);
	ASSERT(uint32_t, "u", a_config.n2_feedback_divider, >=, 32);
	ASSERT(uint32_t, "u", a_config.n2_feedback_divider, <=, 512);
	ASSERT(uint32_t, "u", a_config.n3_input_divider, !=, 0);
	ASSERT(uint32_t, "u", a_config.n3_input_divider, <, 1 << 19);

#ifndef NDEBUG
	if (a_config.n1_output_divider_clock_1 != 1 &&
	    (a_config.n1_output_divider_clock_1 & 0x1) == 1) {
		log_die(LOGL, "n1_output_divider_clock_1 must be odd or 1");
	}
	if (a_config.n1_output_divider_clock_2 != 1 &&
	    (a_config.n1_output_divider_clock_2 & 0x1) == 1) {
		log_die(LOGL, "n1_output_divider_clock_2 must be odd or 1");
	}
#endif

	a_config.n3_input_divider--;
	a_config.n1_high_speed_divider -= 4;
	a_config.n1_output_divider_clock_1--;
	a_config.n1_output_divider_clock_2--;

	sis_3316_si5325_clk_multiplier_write(m, SPI_REG_BYPASS,
	    SPI_BYPASS_OFF);
	sis_3316_si5325_clk_multiplier_write(m, SPI_REG_POWER_DOWN, 0x0
	    /*SPI_POWER_DOWN_CLOCK_INPUT_2*/);

	sis_3316_si5325_clk_multiplier_write(m, SPI_REG_N31_18_16,
	    (a_config.n3_input_divider >> 16) & 0x7);
	sis_3316_si5325_clk_multiplier_write(m, SPI_REG_N31_15_8,
	    (a_config.n3_input_divider >> 8) & 0xff);
	sis_3316_si5325_clk_multiplier_write(m, SPI_REG_N31_7_0,
	    a_config.n3_input_divider & 0xff);

	sis_3316_si5325_clk_multiplier_write(m, SPI_REG_N2_19_16,
	    (a_config.n2_feedback_divider >> 16) & 0xf);
	sis_3316_si5325_clk_multiplier_write(m, SPI_REG_N2_15_8,
	    (a_config.n2_feedback_divider >> 8) & 0xff);
	sis_3316_si5325_clk_multiplier_write(m, SPI_REG_N2_7_0,
	    a_config.n2_feedback_divider & 0xff);

	sis_3316_si5325_clk_multiplier_write(m, SPI_REG_N1,
	    (a_config.n1_high_speed_divider << 5) & 0xff);

	sis_3316_si5325_clk_multiplier_write(m, SPI_REG_NC1_19_16,
	    (a_config.n1_output_divider_clock_1 >> 16) & 0xf);
	sis_3316_si5325_clk_multiplier_write(m, SPI_REG_NC1_15_8,
	    (a_config.n1_output_divider_clock_1 >> 8) & 0xff);
	sis_3316_si5325_clk_multiplier_write(m, SPI_REG_NC1_7_0,
	    a_config.n1_output_divider_clock_1 & 0xff);
	sis_3316_si5325_clk_multiplier_write(m, SPI_REG_NC2_19_16,
	    (a_config.n1_output_divider_clock_2 >> 16) & 0xf);
	sis_3316_si5325_clk_multiplier_write(m, SPI_REG_NC2_15_8,
	    (a_config.n1_output_divider_clock_2 >> 8) & 0xff);
	sis_3316_si5325_clk_multiplier_write(m, SPI_REG_NC2_7_0,
	    a_config.n1_output_divider_clock_2 & 0xff);

	sis_3316_si5325_clk_multiplier_write(m, SPI_REG_BW_SELECT,
	    a_config.bandwidth_select << 5 /* should be 4 ??? */);

	sis_3316_si5325_clk_multiplier_do_internal_calibration(m);
	LOGF(debug)(LOGL, "Finished setting clock multiplier.");
}

uint8_t
sis_3316_spi_action(struct Sis3316Module *m, uint16_t data)
{
	int poll_counter;
	uint32_t result;

	LOGF(debug)(LOGL, "spi_action:data = %04x", data);

	MAP_WRITE(m->sicy_map, spi_ext_nim_clock_multiplier, data);
	SERIALIZE_IO;
	time_sleep(10e-3);

	poll_counter = 0;
	do {
		SERIALIZE_IO;
		result = MAP_READ(m->sicy_map, spi_ext_nim_clock_multiplier);
		++poll_counter;
		LOGF(debug)(LOGL, "  Polled for %d times, result = %08x",
		    poll_counter, result);
	} while (SPI_IS_BUSY(result) && poll_counter < SPI_POLL_COUNTER_MAX);

	if (poll_counter == SPI_POLL_COUNTER_MAX) {
		log_die(LOGL, "sis3316 spi: Timeout while busy.");
	}
	LOGF(debug)(LOGL, "spi_action:done, result = %08x", result);
	return result & 0xff;
}

void
sis_3316_si5325_clk_multiplier_write(struct Sis3316Module *m, uint8_t a_addr,
    uint8_t a_data)
{
	uint32_t result;
	LOGF(debug)(LOGL, "si5325 write: addr=0x%02x, data=0x%02x", a_addr,
	    a_data);

	result = sis_3316_spi_action(m, a_addr);
	/*if (result != 0) {
		log_die(LOGL, "Error writing address (write), got %02x, "
		    "expected %02x.", result, 0);
	}*/

	result = sis_3316_spi_action(m, SPI_SI5325_WRITE + a_data);
	/*if (result != 0) {
		log_die(LOGL, "sis3316 spi: Error writing data.");
	}*/
	(void)result;
}

uint8_t
sis_3316_si5325_clk_multiplier_read(struct Sis3316Module *m, uint8_t a_addr)
{
	uint8_t result;

	LOGF(debug)(LOGL, "si5325 read: addr=0x%02x", a_addr);

	result = sis_3316_spi_action(m, a_addr);
	/*if (result != 0) {
		log_die(LOGL, "Error writing address (read), got %02x, "
		    "expected %02x.", result, 0);
	}*/

	result = sis_3316_spi_action(m, SPI_SI5325_READ);
	LOGF(debug)(LOGL, "si5325 read: result=0x%02x", result);
	return result;
}


void
sis_3316_si5325_clk_multiplier_do_internal_calibration(struct Sis3316Module
    *m)
{
	int poll_counter;
	uint8_t result;

	LOGF(debug)(LOGL, "Start ICAL.");
	sis_3316_si5325_clk_multiplier_write(m, SPI_REG_ICAL, SPI_DO_ICAL);

	poll_counter = 0;
	do {
		result = sis_3316_si5325_clk_multiplier_read(m, SPI_REG_ICAL);
		++poll_counter;
	} while (SPI_ICAL_IS_ACTIVE(result)
	    && poll_counter < SPI_CALIB_READY_POLL_COUNTER_MAX);
	LOGF(debug)(LOGL, "Done ICAL.");
}

/* **************************************************** */

int
sis_3316_i2c_wait_busy(struct Sis3316Module *m, int osc)
{
	unsigned int tmp = 0;
	int i = 0;
	int timeout = 100000;

	(void) osc;

	do {
		SERIALIZE_IO;
		tmp = MAP_READ(m->sicy_map, i2c_adc_clock);
		++i;
	} while ((tmp & (1UL << I2C_BUSY)) && (i < timeout));

	if (i == timeout) {
		log_error(LOGL, "Timeout while waiting for i2c.");
		return 1;
	}

	return 0;
}

void
sis_3316_i2c_start(struct Sis3316Module* m, int osc)
{
	int ret = 1;
	MAP_WRITE(m->sicy_map, i2c_adc_clock, 1 << I2C_START);
	do {
		ret = sis_3316_i2c_wait_busy(m, osc);
	} while (ret != 0);
}

void
sis_3316_i2c_write_byte(struct Sis3316Module* m, int osc, unsigned char data)
{
	int ret = 1;
	MAP_WRITE(m->sicy_map, i2c_adc_clock, 1 << I2C_WRITE ^ data);
	do {
		ret = sis_3316_i2c_wait_busy(m,osc);
	} while (ret != 0);
}

void
sis_3316_i2c_read_byte(struct Sis3316Module* m, int osc, unsigned char *data,
    char ack)
{
	int ret = 1;

	MAP_WRITE(m->sicy_map, i2c_adc_clock,
	    1 << I2C_READ | (ack ? 1 << I2C_ACK : 0));
	do {
		ret = sis_3316_i2c_wait_busy(m,osc);
	} while (ret != 0);
	*data = MAP_READ(m->sicy_map, i2c_adc_clock) & 0xff;
}

void
sis_3316_i2c_stop(struct Sis3316Module* m, int osc)
{
	int ret = 1;

	MAP_WRITE(m->sicy_map, i2c_adc_clock, 1 << I2C_STOP);

	do {
		ret = sis_3316_i2c_wait_busy(m,osc);
	} while (ret != 0);
}

/************************ SIS3316 functions *****************/
/************************     SI570         *****************/
void
sis_3316_si570_freeze_dco(struct Sis3316Module* m, int osc)
{
	uint8_t offset = 0x89;
	char data = 0x10;

	sis_3316_i2c_start(m,osc);
	sis_3316_i2c_write_byte(m,osc,OSC_ADR<<1);
	sis_3316_i2c_write_byte(m,osc,offset);
	sis_3316_i2c_write_byte(m,osc,data);
	sis_3316_i2c_stop(m,osc);
}

void
sis_3316_si570_read_divider(struct Sis3316Module* m, int osc,
    unsigned char *data, size_t len)
{
	size_t i;

	sis_3316_i2c_start(m, osc);
	sis_3316_i2c_write_byte(m, osc, OSC_ADR<<1);
	sis_3316_i2c_write_byte(m, osc, 0x0D);

	sis_3316_i2c_start(m, osc);
	sis_3316_i2c_write_byte(m, osc, (OSC_ADR<<1) + 1);
	for (i = 0; i < len; ++i) {
		int ack;

		ack = (i == (len-1)) ? 0 : 1;
		sis_3316_i2c_read_byte(m, osc, &data[i], ack);
	}
	sis_3316_i2c_stop(m, osc);
}

void
sis_3316_si570_divider(struct Sis3316Module* m, int osc, unsigned char
    preset[6])
{
	char offset = 0x0D;
	int i = 0;

	sis_3316_i2c_start(m,osc);
	sis_3316_i2c_write_byte(m,osc,OSC_ADR<<1);
	sis_3316_i2c_write_byte(m,osc,offset);

	for (i = 0; i < 2; ++i) {
		sis_3316_i2c_write_byte(m,osc,preset[i]);
	}

	sis_3316_i2c_stop(m,osc);
}

void
sis_3316_si570_unfreeze_dco(struct Sis3316Module* m, int osc)
{
	uint8_t offset = 0x89;
	char data = 0x00;

	sis_3316_i2c_start(m,osc);
	sis_3316_i2c_write_byte(m,osc,OSC_ADR<<1);
	sis_3316_i2c_write_byte(m,osc,offset);
	sis_3316_i2c_write_byte(m,osc,data);
	sis_3316_i2c_stop(m,osc);
}

void
sis_3316_si570_new_freq(struct Sis3316Module* m, int osc)
{
	uint8_t offset = 0x87;
	char data = 0x40;

	sis_3316_i2c_start(m,osc);
	sis_3316_i2c_write_byte(m,osc,OSC_ADR<<1);
	sis_3316_i2c_write_byte(m,osc,offset);
	sis_3316_i2c_write_byte(m,osc,data);
	sis_3316_i2c_stop(m,osc);
}

void
sis_3316_si570_dcm_reset(struct Sis3316Module* m, int osc)
{
	(void) osc;
	MAP_WRITE(m->sicy_map, reset_adc_clock, 1);
}

/* New frequency is: 5 GHz / (a_hs * a_n1)
 * a_hs = {4, 5, 6, 7, 9, 11}
 * a_n1 = {2, 4,..., 126}
 * */
void
sis_3316_change_frequency(struct Sis3316Module* m, int osc, unsigned int a_hs,
	unsigned int a_n1)
{
	unsigned char data[6];
	unsigned int hs;
	unsigned int n1;

	memset(data, 0, sizeof(char) * 6);

	sis_3316_si570_read_divider(m, osc, data, 6);
	LOGF(verbose)(LOGL, "Divider setting = 0x%2x:%2x:%2x:%2x:%2x:%2x",
	    data[0],data[1],data[2],data[3],data[4],data[5]);

	hs = a_hs - 4;
	n1 = a_n1 - 1;

	data[0] = ((hs & 0x7) << 5) | ((n1 & 0x7f) >> 2);
	data[1] = ((n1 & 0x3) << 6) | (data[1] & 0x3f);

	sis_3316_set_frequency(m, osc, data);
	sis_3316_si570_read_divider(m, osc, data, 6);
	LOGF(verbose)(LOGL, "New setting = 0x%2x:%2x:%2x:%2x:%2x:%2x",
	    data[0],data[1],data[2],data[3],data[4],data[5]);
}

void
sis_3316_set_frequency(struct Sis3316Module* m, int osc,
    unsigned char preset[6])
{
	sis_3316_si570_freeze_dco(m,osc);
	sis_3316_si570_divider(m,osc,preset);
	sis_3316_si570_unfreeze_dco(m,osc);
	sis_3316_si570_new_freq(m,osc);

	time_sleep(20e-3);
	sis_3316_si570_dcm_reset(m,osc);
}
