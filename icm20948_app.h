/*
 * 版权所有 (c) 2026 XyShen
 *
 * SPDX-License-Identifier: MIT
 *
 * 本文件属于 Nordic 适配范围。许可证范围见 LICENSE-NORDIC.md。
 * SparkFun 派生代码与固件继续适用其原有许可证。
 */

#ifndef ICM20948_APP_H_
#define ICM20948_APP_H_

#include <zephyr/drivers/i2c.h>

#include "ICM_20948_C.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	ICM_20948_Device_t device;
	ICM_20948_Serif_t serif;
} icm20948_t;

ICM_20948_Status_e icm20948_init(icm20948_t *ctx,
				  const struct i2c_dt_spec *i2c);
ICM_20948_Status_e icm20948_initialize_dmp(ICM_20948_Device_t *device);

#ifdef __cplusplus
}
#endif

#endif /* ICM20948_APP_H_ */
