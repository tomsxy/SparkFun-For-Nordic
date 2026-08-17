/*
 * Quat6 采集基于现有 Nordic 移植；Euler 计算来自 SparkFun
 * ICM-20948 Arduino Library 1.3.2 Example7_DMP_Quat6_EulerAngles。
 */

#include <inttypes.h>
#include <math.h>
#include <stdint.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "icm20948_app.h"

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

static const struct i2c_dt_spec icm20948_i2c =
	I2C_DT_SPEC_GET(DT_NODELABEL(icm20948));

#define Q30_SCALE 1073741824.0f
#define RADIANS_TO_MILLIDEGREES 57295.779513f

static void reset_fifo_after_error(ICM_20948_Device_t *device)
{
	ICM_20948_Status_e status = ICM_20948_reset_FIFO(device);

	if (status != ICM_20948_Stat_Ok) {
		LOG_ERR("FIFO 恢复复位失败: status=%d", status);
	}
}

static void log_quat6_euler(uint32_t frame, uint16_t header,
			    const icm_20948_DMP_data_t *data)
{
	float q1 = (float)data->Quat6.Data.Q1 / Q30_SCALE;
	float q2 = (float)data->Quat6.Data.Q2 / Q30_SCALE;
	float q3 = (float)data->Quat6.Data.Q3 / Q30_SCALE;
	float q0_squared = 1.0f - ((q1 * q1) + (q2 * q2) + (q3 * q3));
	float q0 = sqrtf(q0_squared > 0.0f ? q0_squared : 0.0f);

	/* SparkFun Example7 将芯片坐标转换为航空坐标后计算 Z-Y-X 欧拉角。 */
	float qw = q0;
	float qx = q2;
	float qy = q1;
	float qz = -q3;
	float pitch_term = 2.0f * ((qw * qy) - (qx * qz));

	if (pitch_term > 1.0f) {
		pitch_term = 1.0f;
	} else if (pitch_term < -1.0f) {
		pitch_term = -1.0f;
	}

	int32_t roll_mdeg = (int32_t)(atan2f(
		2.0f * ((qw * qx) + (qy * qz)),
		1.0f - (2.0f * ((qx * qx) + (qy * qy)))) *
		RADIANS_TO_MILLIDEGREES);
	int32_t pitch_mdeg = (int32_t)(asinf(pitch_term) *
		RADIANS_TO_MILLIDEGREES);
	int32_t yaw_mdeg = (int32_t)(atan2f(
		2.0f * ((qw * qz) + (qx * qy)),
		1.0f - (2.0f * ((qy * qy) + (qz * qz)))) *
		RADIANS_TO_MILLIDEGREES);

	LOG_INF("Quat6 frame=%u header=0x%04x Q1=%" PRId32
		" Q2=%" PRId32 " Q3=%" PRId32
		" roll_mdeg=%" PRId32 " pitch_mdeg=%" PRId32
		" yaw_mdeg=%" PRId32,
		frame, header, data->Quat6.Data.Q1, data->Quat6.Data.Q2,
		data->Quat6.Data.Q3, roll_mdeg, pitch_mdeg, yaw_mdeg);
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
		INV_ICM20948_SENSOR_GAME_ROTATION_VECTOR, true);
	if (status != ICM_20948_Stat_Ok) {
		LOG_ERR("启用 Quat6 失败: status=%d", status);
		return 0;
	}
	status = inv_icm20948_set_dmp_sensor_period(&icm.device,
		DMP_ODR_Reg_Quat6, 0);
	if (status != ICM_20948_Stat_Ok) {
		LOG_ERR("设置 Quat6 ODR 失败: status=%d", status);
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

	LOG_INF("DMP Quat6/Euler 已启动，开始轮询 FIFO");

	while (true) {
		status = inv_icm20948_read_dmp_data(&icm.device, &data);
		if ((status == ICM_20948_Stat_Ok) ||
		    (status == ICM_20948_Stat_FIFOMoreDataAvail)) {
			if ((data.header & DMP_header_bitmap_Quat6) != 0U) {
				++valid_frames;
				log_quat6_euler(valid_frames, data.header, &data);
			} else {
				LOG_WRN("FIFO 帧缺少 Quat6: header=0x%04x",
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
