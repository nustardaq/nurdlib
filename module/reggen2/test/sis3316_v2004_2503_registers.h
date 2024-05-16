/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015, 2019, 2023-2024
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

/* sis3302 v1411 reduced address range registers */

/* Name, Offset, size in bytes, access [, skip odd [, skip even [, skip third]]]*/

/* Define groups with a name, that will be used as a prefix
 * Define registers with an offset, the number of bits, and
 * an access mode. The special name 'none' will not produce
 * a prefix:
 *
 * prefix {
 *     reg1,	0x0004,	32,	rwk;
 * }
 *
 * If groups of registers appear several times, the
 * number of appearances can be given in parentheses:
 *
 * prefix (12) {
 *	reg2,	0x0008, 16,	rw;
 * }
 *
 * If the registers in the group are not consecutive, but
 * rather have some spacing, this can be given after the
 * number of appearances:
 *
 * prefix (4, 0x1000) {
 *	reg3,	0x0010,	32,	r;
 * }
 *
 * Two more spacing definitions can be given to differentiate
 * between the spacing of 'even' and 'odd' instances, and a
 * 'third' spacing. The offset is calculated according to
 * register_calc_offset(int, uint32_t) in register.c
 *
 */

none {
control_status,		0x00000000,	32,	rwk;
module_id_firmware,	0x00000004,	32,	r;
interrupt_config,	0x00000008,	32,	rw;
interrupt_control,	0x0000000C,	32,	rw;

access_arbitration_control,	0x00000010,	32,	rwk;
broadcast_setup,	0x00000014,	32,	rw;
internal_test_1,	0x00000018,	32,	rwval;
internal_test_2,	0x0000001C,	32,	rw;

temperature,		0x00000020,	32,	r;
onewire_control,	0x00000024,	32,	rw;
serial_number,		0x00000028,	32,	r;
reserved_1,			0x0000002C,	32,	rw;

adc_boot_controller,	0x00000030,	32,	rw;
spi_flash_control,		0x00000034,	32,	rw;
spi_flash_data,			0x00000038,	32,	rw;
reserved_2,				0x0000003C,	32,	rw;

i2c_adc_clock,		0x00000040,	32,	rw;
i2c_mgt1_clock,		0x00000044,	32,	rw;
i2c_mgt2_clock,		0x00000048,	32,	rw;
i2c_ddr3_clock,		0x0000004C,	32,	rw;

sample_clock_distribution_control,	0x00000050,	32,	rw;
spi_ext_nim_clock_multiplier,		0x00000054,	32,	rw;
fpbus_control,						0x00000058,	32,	rw;
nim_in_control,						0x0000005C,	32,	rw;

acquisition_control,	0x00000060,	32,	rw;
reserved_3,				0x00000064,	32,	rw;
reserved_4,				0x00000068,	32,	rw;
reserved_5,				0x0000006C,	32,	rw;

lemo_out_co_select,		0x00000070,	32,	rw;
lemo_out_to_select,		0x00000074,	32,	rw;
lemo_out_uo_select,		0x00000078,	32,	rw;
reserved_6,				0x0000007C,	32,	rw;
}

fpga_ctrl_status(4,0x4) {
	data_transfer_control,	0x00000080,	32,	rw;
	data_transfer_status,	0x00000090,	32,	r;
}

none {
data_link_status,	0x000000A0,	32,	rw;
spi_busy_status,	0x000000A4,	32,	r;
reserved_7,			0x000000A8,	32,	rw;
reserved_8,			0x000000AC,	32,	rw;

register_reset,			0x400,	32,	wk;
user_function,			0x404,	32,	wk;
arm_sampling_logic,		0x410,	32,	wk;
disarm_sampling_logic,	0x414,	32,	wk;
trigger,				0x418,	32,	wk;
timestamp_clear,		0x41C,	32,	wk;
disarm_and_arm_bank1,	0x420,	32,	wk;
disarm_and_arm_bank2,	0x424,	32,	wk;
reset_adc_fpga,			0x434,	32,	wk;
reset_adc_clock,		0x438,	32,	wk;
trigger_out_pulse,		0x43C,	32,	wk;
}

/* Per ADC group, 4 groups */
/* ADC FPGA Offset: 0x00001000 */
fpga_adc(4,0x1000) {
	tap_delay,				0x1000,	32,	rw;
	gain_termination,		0x1004,	32,	rw;
	offset_dac_control,		0x1008,	32,	rw;
	spi_control,			0x100C,	32,	rw;
	event_config,			0x1010,	32,	rw;
	header_id,				0x1014,	32,	rw;
	end_addr_threshold,		0x1018,	32,	rw;
	trigger_gate_length,	0x101C,	32,	rw;
	raw_data_buffer_config,	0x1020,	32,	rw;
	pileup_config,			0x1024,	32,	rw;
	pretrigger_delay,		0x1028,	32,	rw;
	data_format_config,		0x1030,	32,	rw;
	maw_test_config,		0x1034,	32,	rw;
	int_trigger_delay,		0x1038,	32,	rw;
	int_gate_length,		0x103C,	32,	rw;
	trigger_setup_sum,		0x1080,	32,	rw;
	trigger_threshold_sum,	0x1084,	32,	rw;
	trigger_thr_high_e_sum,	0x1088,	32,	rw;
	trigger_counter_mode,	0x108C,	32,	rw;
	accumulator_gate_1,		0x10A0,	32,	rw;
	accumulator_gate_2,		0x10A4,	32,	rw;
	accumulator_gate_3,		0x10A8,	32,	rw;
	accumulator_gate_4,		0x10AC,	32,	rw;
	accumulator_gate_5,		0x10B0,	32,	rw;
	accumulator_gate_6,		0x10B4,	32,	rw;
	accumulator_gate_7,		0x10B8,	32,	rw;
	accumulator_gate_8,		0x10BC,	32,	rw;
	fir_energy_setup_1,		0x10C0, 32,      rw;
	fir_energy_setup_2,		0x10C4, 32,      rw;
	fir_energy_setup_3,		0x10C8, 32,      rw;
	fir_energy_setup_4,		0x10CC, 32,      rw;
	energy_hist_1,		0x10D0, 32,      rw;
	energy_hist_2,		0x10D4, 32,      rw;
	energy_hist_3,		0x10D8, 32,      rw;
	energy_hist_4,		0x10DC, 32,      rw;
	adc_fpga_version,		0x1100,	32,	r;
	adc_fpga_status,		0x1104,	32,	r;
	adc_offset_readback,	0x1108,	32,	r;
	adc_spi_readback,		0x110C,	32,	r;
}

channel_addr(16,0x4,0x8,0x1000) {
	actual_sample_address,	0x1110,	32,	r;
	previous_bank_address,	0x1120,	32,	r;
}

/* Per ADC, 16 ADCs */
/* Channel offset: 0x10 */
channel_trigger(16,0x10,0x20,0x1000) {
	trigger_setup,		0x1040,	32,	rw;
	trigger_threshold,	0x1044,	32,	rw;
	trigger_thr_high_e,	0x1048,	32,	rw;
}

/* ADC MEMORY Offset: 0x00100000 */
adc_fifo(4,0x100000) {
	adc_memory_fifo,	0x100000,	32,	rw;
}

/*
group(channel_memory,16,0x10000000,nomap) {
	bank_1,	0x80000000,	32,	r,	0x200000,	0x10000000;
	bank_2,	0x80100000,	32,	r,	0x200000,	0x10000000;
}
 */
