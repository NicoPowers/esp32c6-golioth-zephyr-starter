/*
 * Copyright (c) 2022-2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_sensors, LOG_LEVEL_DBG);

#include <golioth/client.h>
#include <golioth/stream.h>
#include <zcbor_encode.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>

#include "app_sensors.h"

#ifdef CONFIG_LIB_OSTENTUS
#include <libostentus.h>
static const struct device *o_dev = DEVICE_DT_GET_ANY(golioth_ostentus);
#endif
#ifdef CONFIG_ALUDEL_BATTERY_MONITOR
#include <battery_monitor.h>
#endif

static struct golioth_client *client;

/* Add Sensor structs here */
struct sensor_value pressure, temp;
static unsigned int obs;

const struct device *pressure_sensor = DEVICE_DT_GET(DT_NODELABEL(pressure_sensor));

/* Callback for LightDB Stream */
static void async_error_handler(struct golioth_client *client, enum golioth_status status,
				const struct golioth_coap_rsp_code *coap_rsp_code, const char *path,
				void *arg)
{
	if (status != GOLIOTH_OK) {
		LOG_ERR("Async task failed: %d", status);
		return;
	}
}

/* This will be called by the main() loop */
/* Do all of your work here! */
void app_sensors_read_and_stream(void)
{
	int err;
	int ret;

	/* Send sensor data to Golioth */
	/* Fetch all sensor channels */
	ret = sensor_sample_fetch(pressure_sensor);
	if (ret < 0) {
		printf("Sensor sample update error: %d\n", ret);
		return;
	}

	/* Get pressure channel */
	ret = sensor_channel_get(pressure_sensor, SENSOR_CHAN_PRESS, &pressure);
	if (ret < 0) {
		printf("Cannot read pressure channel: %d\n", ret);
		return;
	}

	/* Get temperature channel */
	ret = sensor_channel_get(pressure_sensor, SENSOR_CHAN_AMBIENT_TEMP, &temp);
	if (ret < 0) {
		printf("Cannot read temperature channel: %d\n", ret);
		return;
	}

	/* Only stream sensor data if connected */
	if (golioth_client_is_connected(client)) {
		/* Encode sensor data using CBOR serialization */
		uint8_t cbor_buf[13];

		ZCBOR_STATE_E(zse, 1, cbor_buf, sizeof(cbor_buf), 1);

		bool ok = zcbor_map_start_encode(zse, 1) && zcbor_tstr_put_lit(zse, "counter") &&
			  zcbor_float16_put(zse, sensor_value_to_float(&pressure)) && zcbor_map_end_encode(zse, 1);

		if (!ok) {
			LOG_ERR("Failed to encode CBOR.");
			return;
		}

		size_t cbor_size = zse->payload - cbor_buf;

		LOG_DBG("Streaming observation: %d", obs);

		/* Stream data to Golioth */
		err = golioth_stream_set_async(client, "sensor", GOLIOTH_CONTENT_TYPE_CBOR,
					       cbor_buf, cbor_size, async_error_handler, NULL);
		if (err) {
			LOG_ERR("Failed to send sensor data to Golioth: %d", err);
		}
	} else {
		LOG_DBG("No connection available, skipping streaming observation: %d", obs);
	}

	/* Increment for the next run */
	++obs;
}

void app_sensors_set_client(struct golioth_client *sensors_client)
{
	client = sensors_client;
}
