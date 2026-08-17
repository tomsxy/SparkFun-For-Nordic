/*
 * 基于 SparkFun ICM-20948 Arduino Library 1.3.2 的
 * Example8_DMP_RawAccel，改写为 Zephyr 纯 C 示例。
 */

#include <stdint.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "icm20948_app.h"

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

static const struct i2c_dt_spec icm20948_i2c =
	I2C_DT_SPEC_GET(DT_NODELABEL(icm20948));

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

	status = inv_icm20948_enable_dmp_sensor(&icm.device,
		INV_ICM20948_SENSOR_ACCELEROMETER, true);
	if (status != ICM_20948_Stat_Ok) {
		LOG_ERR("启用 DMP Accel 失败: status=%d", status);
		return 0;
	}
	status = inv_icm20948_set_dmp_sensor_period(&icm.device,
		DMP_ODR_Reg_Accel, 0);
	if (status != ICM_20948_Stat_Ok) {
		LOG_ERR("设置 DMP Accel ODR 失败: status=%d", status);
		return 0;
	}
	status = ICM_20948_enable_FIFO(&icm.device, true);
	if (status != ICM_20948_Stat_Ok) {
		LOG_ERR("启用 FIFO 失败: status=%d", status);
		return 0;
	}
	status = ICM_20948_enable_DMP(&icm.device, true);
	if (status != ICM_20948_Stat_Ok) {
		LOG_ERR("启用 DMP 失败: status=%d", status);
		return 0;
	}
	status = ICM_20948_reset_DMP(&icm.device);
	if (status != ICM_20948_Stat_Ok) {
		LOG_ERR("复位 DMP 失败: status=%d", status);
		return 0;
	}
	status = ICM_20948_reset_FIFO(&icm.device);
	if (status != ICM_20948_Stat_Ok) {
		LOG_ERR("复位 FIFO 失败: status=%d", status);
		return 0;
	}

	LOG_INF("DMP Raw Accel 已启动，开始轮询 FIFO");

	while (true) {
		status = inv_icm20948_read_dmp_data(&icm.device, &data);
		if ((status == ICM_20948_Stat_Ok) ||
		    (status == ICM_20948_Stat_FIFOMoreDataAvail)) {
			if ((data.header & DMP_header_bitmap_Accel) != 0U) {
				++valid_frames;
				LOG_INF("RawAccel frame=%u header=0x%04x X=%d Y=%d Z=%d",
					valid_frames, data.header,
					(int)data.Raw_Accel.Data.X,
					(int)data.Raw_Accel.Data.Y,
					(int)data.Raw_Accel.Data.Z);
			} else {
				LOG_WRN("FIFO 帧缺少 Accel: header=0x%04x",
					data.header);
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
