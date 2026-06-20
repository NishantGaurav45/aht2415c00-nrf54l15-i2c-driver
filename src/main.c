/*
 * AHT2415C00 (AHT20-family) temperature & humidity probe driver
 * for nRF54L15 / nRF Connect SDK (Zephyr).
 *
 * The AHT2415C00 doesn't have a built-in Zephyr sensor driver, so this
 * file talks to it directly over I2C using the documented command set:
 *
 *   I2C address            : 0x38
 *   Status read command    : 0x71
 *   Init command            : 0xBE 0x08 0x00
 *   Trigger measurement cmd : 0xAC 0x33 0x00
 *   Result                 : 6 bytes -> [status, hum_H, hum_M, hum/temp_X, temp_M, temp_L]
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(aht2415c, LOG_LEVEL_INF);

/* Grab the I2C device + address straight from the devicetree node
 * declared in boards/nrf54l15dk_nrf54l15_cpuapp.overlay
 */
static const struct i2c_dt_spec aht20 = I2C_DT_SPEC_GET(DT_NODELABEL(aht20));

#define AHT20_CMD_STATUS   0x71
#define AHT20_CMD_INIT0    0xBE
#define AHT20_CMD_INIT1    0x08
#define AHT20_CMD_INIT2    0x00
#define AHT20_CMD_TRIGGER0 0xAC
#define AHT20_CMD_TRIGGER1 0x33
#define AHT20_CMD_TRIGGER2 0x00

#define AHT20_STATUS_BUSY_BIT 0x80
#define AHT20_STATUS_CAL_BIT  0x08

static int aht20_read_status(uint8_t *status)
{
	uint8_t cmd = AHT20_CMD_STATUS;

	return i2c_write_read_dt(&aht20, &cmd, 1, status, 1);
}

static int aht20_init(void)
{
	int ret;
	uint8_t status;

	/* Datasheet: wait >=100ms after power-on before talking to the sensor */
	k_msleep(100);

	ret = aht20_read_status(&status);
	if (ret < 0) {
		LOG_ERR("Failed to read status register (err %d)", ret);
		return ret;
	}

	if ((status & AHT20_STATUS_CAL_BIT) == 0) {
		uint8_t init_cmd[3] = {
			AHT20_CMD_INIT0,
			AHT20_CMD_INIT1,
			AHT20_CMD_INIT2,
		};

		LOG_INF("Sensor not calibrated yet, sending init command");

		ret = i2c_write_dt(&aht20, init_cmd, sizeof(init_cmd));
		if (ret < 0) {
			LOG_ERR("Failed to send init command (err %d)", ret);
			return ret;
		}

		k_msleep(10);
	}

	return 0;
}

/* Triggers a measurement and converts the raw bytes into °C / %RH. */
static int aht20_read(float *temperature_c, float *humidity_pct)
{
	int ret;
	uint8_t trigger_cmd[3] = {
		AHT20_CMD_TRIGGER0,
		AHT20_CMD_TRIGGER1,
		AHT20_CMD_TRIGGER2,
	};
	uint8_t data[6];

	ret = i2c_write_dt(&aht20, trigger_cmd, sizeof(trigger_cmd));
	if (ret < 0) {
		LOG_ERR("Failed to trigger measurement (err %d)", ret);
		return ret;
	}

	/* Datasheet says wait 80ms, then poll the busy bit just to be safe */
	k_msleep(80);

	for (int tries = 0; tries < 5; tries++) {
		ret = i2c_read_dt(&aht20, data, sizeof(data));
		if (ret < 0) {
			LOG_ERR("Failed to read measurement (err %d)", ret);
			return ret;
		}

		if ((data[0] & AHT20_STATUS_BUSY_BIT) == 0) {
			break; /* measurement ready */
		}

		k_msleep(20);
	}

	if (data[0] & AHT20_STATUS_BUSY_BIT) {
		LOG_WRN("Sensor still busy after retries, data may be stale");
	}

	uint32_t raw_humidity = ((uint32_t)data[1] << 12) |
				 ((uint32_t)data[2] << 4) |
				 (data[3] >> 4);

	uint32_t raw_temperature = (((uint32_t)data[3] & 0x0F) << 16) |
				    ((uint32_t)data[4] << 8) |
				    data[5];

	*humidity_pct = ((float)raw_humidity / 1048576.0f) * 100.0f;
	*temperature_c = (((float)raw_temperature / 1048576.0f) * 200.0f) - 50.0f;

	return 0;
}

int main(void)
{
	int ret;

	LOG_INF("AHT2415C00 + nRF54L15 demo starting");

	if (!device_is_ready(aht20.bus)) {
		LOG_ERR("I2C bus %s is not ready", aht20.bus->name);
		return 0;
	}

	ret = aht20_init();
	if (ret < 0) {
		LOG_ERR("Sensor init failed (err %d), halting", ret);
		return 0;
	}

	LOG_INF("Sensor ready, reading every 2 seconds");

	while (1) {
		float temperature_c = 0.0f;
		float humidity_pct = 0.0f;

		ret = aht20_read(&temperature_c, &humidity_pct);
		if (ret == 0) {
			/* printk does not support %f on every config, so split
			 * the float into integer + 2 decimal places manually.
			 */
			int temp_i = (int)temperature_c;
			int temp_f = (int)((temperature_c - temp_i) * 100);
			int hum_i = (int)humidity_pct;
			int hum_f = (int)((humidity_pct - hum_i) * 100);

			if (temp_f < 0) temp_f = -temp_f;
			if (hum_f < 0) hum_f = -hum_f;

			LOG_INF("Temperature: %d.%02d C  |  Humidity: %d.%02d %%RH",
				 temp_i, temp_f, hum_i, hum_f);
		} else {
			LOG_ERR("Sensor read failed (err %d)", ret);
		}

		k_sleep(K_SECONDS(2));
	}

	return 0;
}
