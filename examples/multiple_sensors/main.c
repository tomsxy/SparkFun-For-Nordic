/*
 * 版权所有 (c) 2026 XyShen
 *
 * SPDX-License-Identifier: MIT
 *
 * 本文件属于 Nordic 示例和适配贡献范围。许可证范围见
 * LICENSE-NORDIC.md；SparkFun 派生代码与固件继续适用其原有许可证。
 */

/*
 * 基于 SparkFun ICM-20948 Arduino Library 1.3.2 的
 * Example9_DMP_MultipleSensors，改写为 Zephyr 纯 C 示例。
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "icm20948_app.h"

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

static const struct i2c_dt_spec icm20948_i2c =
	I2C_DT_SPEC_GET(DT_NODELABEL(icm20948));

#define CONFIGURE_OR_RETURN(name, expression)                              \
	do {                                                                 \
		status = (expression);                                         \
		if (status != ICM_20948_Stat_Ok) {                            \
			LOG_ERR("%s失败: status=%d", name, status);             \
			return 0;                                                \
		}                                                            \
	} while (0)

static void reset_fifo_after_error(ICM_20948_Device_t *device)
{
	ICM_20948_Status_e status = ICM_20948_reset_FIFO(device);

	if (status != ICM_20948_Stat_Ok) {
		LOG_ERR("FIFO 恢复复位失败: status=%d", status);
	}
}

int main(void)
{
	k_sleep(K_MSEC(1000));

	icm20948_t icm = {0};
	icm_20948_DMP_data_t data;
	ICM_20948_Status_e status;
	uint32_t valid_frames = 0U;

	status = icm20948_init(&icm, &icm20948_i2c);
	if (status != ICM_20948_Stat_Ok) {
		LOG_ERR("ICM-20948 基础初始化失败: status=%d", status);
		return 0;
	}

	status = icm20948_initialize_dmp(&icm.device);
	if (status != ICM_20948_Stat_Ok) {
		LOG_ERR("ICM-20948 DMP 初始化失败: status=%d", status);
		return 0;
	}

	CONFIGURE_OR_RETURN("启用 Quat6",
		inv_icm20948_enable_dmp_sensor(&icm.device,
			INV_ICM20948_SENSOR_GAME_ROTATION_VECTOR, true));
	CONFIGURE_OR_RETURN("启用 Raw Gyro",
		inv_icm20948_enable_dmp_sensor(&icm.device,
			INV_ICM20948_SENSOR_RAW_GYROSCOPE, true));
	CONFIGURE_OR_RETURN("启用 Raw Accel",
		inv_icm20948_enable_dmp_sensor(&icm.device,
			INV_ICM20948_SENSOR_RAW_ACCELEROMETER, true));
	CONFIGURE_OR_RETURN("启用未校准 Compass",
		inv_icm20948_enable_dmp_sensor(&icm.device,
			INV_ICM20948_SENSOR_MAGNETIC_FIELD_UNCALIBRATED,
			true));

	/* DMP 基准频率约 55 Hz：Quat6=5 Hz，其余数据=1 Hz。 */
	CONFIGURE_OR_RETURN("设置 Quat6 ODR",
		inv_icm20948_set_dmp_sensor_period(&icm.device,
			DMP_ODR_Reg_Quat6, 10));
	CONFIGURE_OR_RETURN("设置 Accel ODR",
		inv_icm20948_set_dmp_sensor_period(&icm.device,
			DMP_ODR_Reg_Accel, 54));
	CONFIGURE_OR_RETURN("设置 Gyro ODR",
		inv_icm20948_set_dmp_sensor_period(&icm.device,
			DMP_ODR_Reg_Gyro, 54));
	CONFIGURE_OR_RETURN("设置校准 Gyro ODR",
		inv_icm20948_set_dmp_sensor_period(&icm.device,
			DMP_ODR_Reg_Gyro_Calibr, 54));
	CONFIGURE_OR_RETURN("设置 Compass ODR",
		inv_icm20948_set_dmp_sensor_period(&icm.device,
			DMP_ODR_Reg_Cpass, 54));
	CONFIGURE_OR_RETURN("设置校准 Compass ODR",
		inv_icm20948_set_dmp_sensor_period(&icm.device,
			DMP_ODR_Reg_Cpass_Calibr, 54));

	CONFIGURE_OR_RETURN("启用 FIFO",
		ICM_20948_enable_FIFO(&icm.device, true));
	CONFIGURE_OR_RETURN("启用 DMP",
		ICM_20948_enable_DMP(&icm.device, true));
	CONFIGURE_OR_RETURN("复位 DMP", ICM_20948_reset_DMP(&icm.device));
	CONFIGURE_OR_RETURN("复位 FIFO", ICM_20948_reset_FIFO(&icm.device));

	LOG_INF("DMP 多传感器已启动：Quat6=5 Hz，Accel/Gyro/Compass=1 Hz");

	while (true) {
		status = inv_icm20948_read_dmp_data(&icm.device, &data);
		if ((status == ICM_20948_Stat_Ok) ||
		    (status == ICM_20948_Stat_FIFOMoreDataAvail)) {
			uint32_t frame = valid_frames + 1U;
			bool recognized = false;

			if ((data.header & DMP_header_bitmap_Quat6) != 0U) {
				recognized = true;
				LOG_INF("Multi frame=%u header=0x%04x Quat6 Q1=%" PRId32
					" Q2=%" PRId32 " Q3=%" PRId32,
					frame, data.header, data.Quat6.Data.Q1,
					data.Quat6.Data.Q2, data.Quat6.Data.Q3);
			}
			if ((data.header & DMP_header_bitmap_Accel) != 0U) {
				recognized = true;
				LOG_INF("Multi frame=%u header=0x%04x Accel X=%d Y=%d Z=%d",
					frame, data.header,
					(int)data.Raw_Accel.Data.X,
					(int)data.Raw_Accel.Data.Y,
					(int)data.Raw_Accel.Data.Z);
			}
			if ((data.header & DMP_header_bitmap_Gyro) != 0U) {
				recognized = true;
				LOG_INF("Multi frame=%u header=0x%04x Gyro X=%d Y=%d Z=%d",
					frame, data.header,
					(int)data.Raw_Gyro.Data.X,
					(int)data.Raw_Gyro.Data.Y,
					(int)data.Raw_Gyro.Data.Z);
			}
			if ((data.header & DMP_header_bitmap_Compass) != 0U) {
				recognized = true;
				LOG_INF("Multi frame=%u header=0x%04x Compass X=%d Y=%d Z=%d",
					frame, data.header,
					(int)data.Compass.Data.X,
					(int)data.Compass.Data.Y,
					(int)data.Compass.Data.Z);
			}

			if (recognized) {
				valid_frames = frame;
			} else {
				LOG_WRN("FIFO 帧没有已启用的数据: header=0x%04x header2=0x%04x",
					data.header, data.header2);
				reset_fifo_after_error(&icm.device);
			}

			if (status == ICM_20948_Stat_FIFOMoreDataAvail) {
				continue;
			}
			k_msleep(10);
			continue;
		}

		if (status == ICM_20948_Stat_FIFONoDataAvail) {
			k_msleep(10);
			continue;
		}

		LOG_ERR("读取 DMP FIFO 失败: status=%d", status);
		reset_fifo_after_error(&icm.device);
		k_msleep(10);
	}

	return 0;
}

#undef CONFIGURE_OR_RETURN
