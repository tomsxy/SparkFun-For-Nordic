#include "icm20948_app.h"

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/logging/log.h>

#include "AK09916_REGISTERS.h"

LOG_MODULE_REGISTER(icm20948_dmp, CONFIG_LOG_DEFAULT_LEVEL);

#define DMP_STEP(name, expression)                                             \
	do {                                                                     \
		status = (expression);                                             \
		if (status != ICM_20948_Stat_Ok) {                                \
			LOG_ERR("DMP 初始化失败: step=%s status=%d", name, status); \
			return status;                                               \
		}                                                                \
	} while (false)

ICM_20948_Status_e icm20948_initialize_dmp(ICM_20948_Device_t *device)
{
	ICM_20948_Status_e status = ICM_20948_Stat_Ok;
	ICM_20948_INT_enable_t interrupt_enable;
	ICM_20948_fss_t full_scale = {0};
	ICM_20948_smplrt_t sample_rate = {0};
	uint8_t value;

	if (device == NULL) {
		return ICM_20948_Stat_ParamErr;
	}
	if (!device->_dmp_firmware_available) {
		return ICM_20948_Stat_DMPNotSupported;
	}

	DMP_STEP("配置 AK09916 SLV0",
		ICM_20948_i2c_controller_configure_peripheral(device, 0,
			MAG_AK09916_I2C_ADDR, AK09916_REG_RSV2, 10,
			true, true, false, true, true, 0));
	DMP_STEP("配置 AK09916 SLV1",
		ICM_20948_i2c_controller_configure_peripheral(device, 1,
			MAG_AK09916_I2C_ADDR, AK09916_REG_CNTL2, 1,
			false, true, false, false, false,
			AK09916_mode_single));

	DMP_STEP("选择 Bank 3", ICM_20948_set_bank(device, 3));
	value = 0x04;
	DMP_STEP("设置 I2C Master ODR",
		ICM_20948_execute_w(device, AGB3_REG_I2C_MST_ODR_CONFIG,
			&value, 1));
	DMP_STEP("设置自动时钟",
		ICM_20948_set_clock_source(device, ICM_20948_Clock_Auto));

	DMP_STEP("选择 Bank 0", ICM_20948_set_bank(device, 0));
	value = 0x40;
	DMP_STEP("启用 Accel 和 Gyro",
		ICM_20948_execute_w(device, AGB0_REG_PWR_MGMT_2, &value, 1));
	DMP_STEP("设置 I2C Master cycled 模式",
		ICM_20948_set_sample_mode(device, ICM_20948_Internal_Mst,
			ICM_20948_Sample_Mode_Cycled));
	DMP_STEP("禁用 FIFO", ICM_20948_enable_FIFO(device, false));
	DMP_STEP("禁用 DMP", ICM_20948_enable_DMP(device, false));

	full_scale.a = gpm4;
	full_scale.g = dps2000;
	DMP_STEP("设置 Accel/Gyro FSR",
		ICM_20948_set_full_scale(device,
			ICM_20948_Internal_Acc | ICM_20948_Internal_Gyr,
			full_scale));
	DMP_STEP("启用 Gyro DLPF",
		ICM_20948_enable_dlpf(device, ICM_20948_Internal_Gyr, true));

	DMP_STEP("清空 FIFO_EN_1 前选择 Bank 0",
		ICM_20948_set_bank(device, 0));
	value = 0U;
	DMP_STEP("清空 FIFO_EN_1",
		ICM_20948_execute_w(device, AGB0_REG_FIFO_EN_1, &value, 1));
	DMP_STEP("清空 FIFO_EN_2",
		ICM_20948_execute_w(device, AGB0_REG_FIFO_EN_2, &value, 1));

	DMP_STEP("读取中断配置",
		ICM_20948_int_enable(device, NULL, &interrupt_enable));
	interrupt_enable.RAW_DATA_0_RDY_EN = 0U;
	DMP_STEP("关闭 Raw Data Ready 中断",
		ICM_20948_int_enable(device, &interrupt_enable,
			&interrupt_enable));
	DMP_STEP("复位 FIFO", ICM_20948_reset_FIFO(device));

	sample_rate.g = 19U;
	sample_rate.a = 19U;
	DMP_STEP("设置 Accel/Gyro 采样分频",
		ICM_20948_set_sample_rate(device,
			ICM_20948_Internal_Acc | ICM_20948_Internal_Gyr,
			sample_rate));
	DMP_STEP("设置 DMP 起始地址",
		ICM_20948_set_dmp_start_address(device, DMP_START_ADDRESS));
	DMP_STEP("写入并校验 DMP 固件", ICM_20948_firmware_load(device));
	DMP_STEP("重新设置 DMP 起始地址",
		ICM_20948_set_dmp_start_address(device, DMP_START_ADDRESS));

	DMP_STEP("HW fix 前选择 Bank 0", ICM_20948_set_bank(device, 0));
	value = 0x48;
	DMP_STEP("设置 HW fix",
		ICM_20948_execute_w(device, AGB0_REG_HW_FIX_DISABLE, &value, 1));
	value = 0xE4;
	DMP_STEP("设置 FIFO priority",
		ICM_20948_execute_w(device, AGB0_REG_SINGLE_FIFO_PRIORITY_SEL,
			&value, 1));

	const uint8_t accel_scale[4] = {0x04, 0x00, 0x00, 0x00};
	const uint8_t accel_scale_2[4] = {0x00, 0x04, 0x00, 0x00};
	DMP_STEP("写入 ACC_SCALE",
		inv_icm20948_write_mems(device, ACC_SCALE,
			sizeof(accel_scale), accel_scale));
	DMP_STEP("写入 ACC_SCALE2",
		inv_icm20948_write_mems(device, ACC_SCALE2,
			sizeof(accel_scale_2), accel_scale_2));

	const uint8_t mount_zero[4] = {0x00, 0x00, 0x00, 0x00};
	const uint8_t mount_plus[4] = {0x09, 0x99, 0x99, 0x99};
	const uint8_t mount_minus[4] = {0xF6, 0x66, 0x66, 0x67};
	DMP_STEP("写入 CPASS_MTX_00", inv_icm20948_write_mems(device,
		CPASS_MTX_00, 4, mount_plus));
	DMP_STEP("写入 CPASS_MTX_01", inv_icm20948_write_mems(device,
		CPASS_MTX_01, 4, mount_zero));
	DMP_STEP("写入 CPASS_MTX_02", inv_icm20948_write_mems(device,
		CPASS_MTX_02, 4, mount_zero));
	DMP_STEP("写入 CPASS_MTX_10", inv_icm20948_write_mems(device,
		CPASS_MTX_10, 4, mount_zero));
	DMP_STEP("写入 CPASS_MTX_11", inv_icm20948_write_mems(device,
		CPASS_MTX_11, 4, mount_minus));
	DMP_STEP("写入 CPASS_MTX_12", inv_icm20948_write_mems(device,
		CPASS_MTX_12, 4, mount_zero));
	DMP_STEP("写入 CPASS_MTX_20", inv_icm20948_write_mems(device,
		CPASS_MTX_20, 4, mount_zero));
	DMP_STEP("写入 CPASS_MTX_21", inv_icm20948_write_mems(device,
		CPASS_MTX_21, 4, mount_zero));
	DMP_STEP("写入 CPASS_MTX_22", inv_icm20948_write_mems(device,
		CPASS_MTX_22, 4, mount_minus));

	const uint8_t b2s_zero[4] = {0x00, 0x00, 0x00, 0x00};
	const uint8_t b2s_plus[4] = {0x40, 0x00, 0x00, 0x00};
	DMP_STEP("写入 B2S_MTX_00", inv_icm20948_write_mems(device,
		B2S_MTX_00, 4, b2s_plus));
	DMP_STEP("写入 B2S_MTX_01", inv_icm20948_write_mems(device,
		B2S_MTX_01, 4, b2s_zero));
	DMP_STEP("写入 B2S_MTX_02", inv_icm20948_write_mems(device,
		B2S_MTX_02, 4, b2s_zero));
	DMP_STEP("写入 B2S_MTX_10", inv_icm20948_write_mems(device,
		B2S_MTX_10, 4, b2s_zero));
	DMP_STEP("写入 B2S_MTX_11", inv_icm20948_write_mems(device,
		B2S_MTX_11, 4, b2s_plus));
	DMP_STEP("写入 B2S_MTX_12", inv_icm20948_write_mems(device,
		B2S_MTX_12, 4, b2s_zero));
	DMP_STEP("写入 B2S_MTX_20", inv_icm20948_write_mems(device,
		B2S_MTX_20, 4, b2s_zero));
	DMP_STEP("写入 B2S_MTX_21", inv_icm20948_write_mems(device,
		B2S_MTX_21, 4, b2s_zero));
	DMP_STEP("写入 B2S_MTX_22", inv_icm20948_write_mems(device,
		B2S_MTX_22, 4, b2s_plus));

	DMP_STEP("设置 Gyro scaling factor",
		inv_icm20948_set_gyro_sf(device, 19, 3));
	const uint8_t gyro_full_scale[4] = {0x10, 0x00, 0x00, 0x00};
	DMP_STEP("写入 GYRO_FULLSCALE", inv_icm20948_write_mems(device,
		GYRO_FULLSCALE, 4, gyro_full_scale));

	const uint8_t accel_only_gain[4] = {0x03, 0xA4, 0x92, 0x49};
	const uint8_t accel_alpha_var[4] = {0x34, 0x92, 0x49, 0x25};
	const uint8_t accel_a_var[4] = {0x0B, 0x6D, 0xB6, 0xDB};
	const uint8_t accel_cal_rate[2] = {0x00, 0x00};
	const uint8_t compass_rate[2] = {0x00, 0x45};
	DMP_STEP("写入 ACCEL_ONLY_GAIN", inv_icm20948_write_mems(device,
		ACCEL_ONLY_GAIN, 4, accel_only_gain));
	DMP_STEP("写入 ACCEL_ALPHA_VAR", inv_icm20948_write_mems(device,
		ACCEL_ALPHA_VAR, 4, accel_alpha_var));
	DMP_STEP("写入 ACCEL_A_VAR", inv_icm20948_write_mems(device,
		ACCEL_A_VAR, 4, accel_a_var));
	DMP_STEP("写入 ACCEL_CAL_RATE", inv_icm20948_write_mems(device,
		ACCEL_CAL_RATE, 2, accel_cal_rate));
	DMP_STEP("写入 CPASS_TIME_BUFFER", inv_icm20948_write_mems(device,
		CPASS_TIME_BUFFER, 2, compass_rate));

	LOG_INF("DMP 固件和参数初始化完成");
	return ICM_20948_Stat_Ok;
}

#undef DMP_STEP
