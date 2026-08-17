#include "icm20948_app.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "AK09916_REGISTERS.h"

LOG_MODULE_REGISTER(icm20948_port, CONFIG_LOG_DEFAULT_LEVEL);

#define ICM20948_MAG_START_ATTEMPTS 10

static ICM_20948_Status_e icm20948_i2c_write(uint8_t regaddr,
					      uint8_t *data,
					      uint32_t len,
					      void *user)
{
	const struct i2c_dt_spec *i2c = user;
	uint8_t tx[INV_MAX_SERIAL_WRITE + 1];
	int err;

	if ((i2c == NULL) || ((data == NULL) && (len > 0U)) ||
	    (len > INV_MAX_SERIAL_WRITE)) {
		return ICM_20948_Stat_ParamErr;
	}

	tx[0] = regaddr;
	if (len > 0U) {
		memcpy(&tx[1], data, len);
	}

	err = i2c_write_dt(i2c, tx, len + 1U);
	if (err < 0) {
		LOG_ERR("I2C 写失败: reg=0x%02x len=%u err=%d",
			regaddr, (unsigned int)len, err);
		return ICM_20948_Stat_Err;
	}

	return ICM_20948_Stat_Ok;
}

static ICM_20948_Status_e icm20948_i2c_read(uint8_t regaddr,
					     uint8_t *data,
					     uint32_t len,
					     void *user)
{
	const struct i2c_dt_spec *i2c = user;
	int err;

	if ((i2c == NULL) || ((data == NULL) && (len > 0U))) {
		return ICM_20948_Stat_ParamErr;
	}

	err = i2c_write_read_dt(i2c, &regaddr, sizeof(regaddr), data, len);
	if (err < 0) {
		LOG_ERR("I2C 读失败: reg=0x%02x len=%u err=%d",
			regaddr, (unsigned int)len, err);
		return ICM_20948_Stat_Err;
	}

	return ICM_20948_Stat_Ok;
}

ICM_20948_Status_e icm20948_init(icm20948_t *ctx,
				  const struct i2c_dt_spec *i2c)
{
	ICM_20948_Status_e status;
	ICM_20948_Status_e last_mag_status = ICM_20948_Stat_WrongID;
	uint8_t who_am_i = 0U;
	uint8_t mag_who_am_i_1 = 0U;
	uint8_t mag_who_am_i_2 = 0U;
	uint8_t mag_reset = 1U;

	if ((ctx == NULL) || (i2c == NULL)) {
		return ICM_20948_Stat_ParamErr;
	}

	if (!i2c_is_ready_dt(i2c)) {
		LOG_ERR("I2C 控制器未就绪");
		return ICM_20948_Stat_Err;
	}

	memset(ctx, 0, sizeof(*ctx));
	status = ICM_20948_init_struct(&ctx->device);
	if (status != ICM_20948_Stat_Ok) {
		return status;
	}

	ctx->serif.write = icm20948_i2c_write;
	ctx->serif.read = icm20948_i2c_read;
	ctx->serif.user = (void *)i2c;
	status = ICM_20948_link_serif(&ctx->device, &ctx->serif);
	if (status != ICM_20948_Stat_Ok) {
		LOG_ERR("链接 I2C SERIF 失败: status=%d", status);
		return status;
	}

#if defined(ICM_20948_USE_DMP)
	ctx->device._dmp_firmware_available = true;
#else
	ctx->device._dmp_firmware_available = false;
#endif
	ctx->device._firmware_loaded = false;
	ctx->device._last_bank = UINT8_MAX;
	ctx->device._last_mems_bank = UINT8_MAX;

	status = ICM_20948_get_who_am_i(&ctx->device, &who_am_i);
	if (status != ICM_20948_Stat_Ok) {
		LOG_ERR("读取 ICM-20948 WHO_AM_I 失败: status=%d", status);
		return status;
	}
	if (who_am_i != ICM_20948_WHOAMI) {
		LOG_ERR("ICM-20948 WHO_AM_I 错误: got=0x%02x expected=0x%02x",
			who_am_i, ICM_20948_WHOAMI);
		return ICM_20948_Stat_WrongID;
	}
	LOG_INF("ICM-20948 WHO_AM_I=0x%02x", who_am_i);

	status = ICM_20948_sw_reset(&ctx->device);
	if (status != ICM_20948_Stat_Ok) {
		LOG_ERR("ICM-20948 软件复位失败: status=%d", status);
		return status;
	}

	/* 复位会令芯片回到 Bank 0，必须同时使驱动内部的 Bank 缓存失效。 */
	ctx->device._last_bank = UINT8_MAX;
	ctx->device._last_mems_bank = UINT8_MAX;
	ctx->device._firmware_loaded = false;
	k_msleep(50);

	status = ICM_20948_sleep(&ctx->device, false);
	if (status != ICM_20948_Stat_Ok) {
		LOG_ERR("退出睡眠模式失败: status=%d", status);
		return status;
	}
	status = ICM_20948_low_power(&ctx->device, false);
	if (status != ICM_20948_Stat_Ok) {
		LOG_ERR("退出低功耗模式失败: status=%d", status);
		return status;
	}

	status = ICM_20948_i2c_master_enable(&ctx->device, true);
	if (status != ICM_20948_Stat_Ok) {
		LOG_ERR("启用内部 I2C Master 失败: status=%d", status);
		return status;
	}
	status = ICM_20948_i2c_master_single_w(&ctx->device,
		MAG_AK09916_I2C_ADDR, AK09916_REG_CNTL3, &mag_reset);
	if (status != ICM_20948_Stat_Ok) {
		LOG_ERR("复位 AK09916 失败: status=%d", status);
		return status;
	}

	for (uint8_t attempt = 1U; attempt <= ICM20948_MAG_START_ATTEMPTS;
	     ++attempt) {
		status = ICM_20948_i2c_master_single_r(&ctx->device,
			MAG_AK09916_I2C_ADDR, AK09916_REG_WIA1,
			&mag_who_am_i_1);
		if (status == ICM_20948_Stat_Ok) {
			status = ICM_20948_i2c_master_single_r(&ctx->device,
				MAG_AK09916_I2C_ADDR, AK09916_REG_WIA2,
				&mag_who_am_i_2);
		}

		if ((status == ICM_20948_Stat_Ok) &&
		    (mag_who_am_i_1 == (MAG_AK09916_WHO_AM_I >> 8)) &&
		    (mag_who_am_i_2 == (MAG_AK09916_WHO_AM_I & 0xFF))) {
			LOG_INF("AK09916 WHO_AM_I=0x%02x%02x，尝试次数=%u",
				mag_who_am_i_1, mag_who_am_i_2, attempt);
			return ICM_20948_Stat_Ok;
		}

		last_mag_status = (status == ICM_20948_Stat_Ok) ?
			ICM_20948_Stat_WrongID : status;
		status = ICM_20948_i2c_master_reset(&ctx->device);
		if (status != ICM_20948_Stat_Ok) {
			last_mag_status = status;
		}
		k_msleep(10);
	}

	LOG_ERR("AK09916 探测失败: got=0x%02x%02x status=%d",
		mag_who_am_i_1, mag_who_am_i_2, last_mag_status);
	return last_mag_status;
}
