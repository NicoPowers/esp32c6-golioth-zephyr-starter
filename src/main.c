/*
 * Copyright (c) 2022-2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(golioth_rd_template, LOG_LEVEL_DBG);

#include <app_version.h>
#include "app_rpc.h"
#include "app_settings.h"
#include "app_state.h"
#include "app_sensors.h"
#include <golioth/client.h>
#include <golioth/fw_update.h>
#include <samples/common/net_connect.h>
#include <samples/common/sample_credentials.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>


/* Current firmware version; update in VERSION */
static const char *_current_version =
	STRINGIFY(APP_VERSION_MAJOR) "." STRINGIFY(APP_VERSION_MINOR) "." STRINGIFY(APP_PATCHLEVEL);

static struct golioth_client *client;
K_SEM_DEFINE(connected, 0, 1);

static k_tid_t _system_thread = 0;

#if DT_NODE_EXISTS(DT_ALIAS(golioth_led))
static const struct gpio_dt_spec golioth_led = GPIO_DT_SPEC_GET(DT_ALIAS(golioth_led), gpios);
#endif /* DT_NODE_EXISTS(DT_ALIAS(golioth_led)) */

/* forward declarations */
void golioth_connection_led_set(uint8_t state);

void wake_system_thread(void)
{
	k_wakeup(_system_thread);
}

static void on_client_event(struct golioth_client *client, enum golioth_client_event event,
			    void *arg)
{
	bool is_connected = (event == GOLIOTH_CLIENT_EVENT_CONNECTED);

	if (is_connected) {
		k_sem_give(&connected);
		golioth_connection_led_set(1);
	}
	LOG_INF("Golioth client %s", is_connected ? "connected" : "disconnected");
}

static void start_golioth_client(void)
{
	/* Get the client configuration from auto-loaded settings */
	const struct golioth_client_config *client_config = golioth_sample_credentials_get();

	/* Create and start a Golioth Client */
	client = golioth_client_create(client_config);

	/* Register Golioth on_connect callback */
	golioth_client_register_event_callback(client, on_client_event, NULL);

	/* Initialize DFU components */
	golioth_fw_update_init(client, _current_version);

	/*** Call Golioth APIs for other services in dedicated app files ***/

	/* Observe State service data */
	app_state_observe(client);

	/* Set Golioth Client for streaming sensor data */
	app_sensors_set_client(client);

	/* Register Settings service */
	app_settings_register(client);

	/* Register RPC service */
	app_rpc_register(client);
}


void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	LOG_DBG("Button pressed at %d", k_cycle_get_32());
	/* This function is an Interrupt Service Routine. Do not call functions that
	 * use other threads, or perform long-running operations here
	 */
	k_wakeup(_system_thread);
}

/* Set (unset) LED indicators for active Golioth connection */
void golioth_connection_led_set(uint8_t state)
{
	uint8_t pin_state = state ? 1 : 0;
	ARG_UNUSED(pin_state); /* silence warning if no LED/Ostentus present */

	/* Turn on Golioth logo LED once connected */
	IF_ENABLED(DT_NODE_EXISTS(DT_ALIAS(golioth_led)),
		(gpio_pin_set_dt(&golioth_led, pin_state);));

	/* Change the state of the Golioth LED on Ostentus */
	IF_ENABLED(CONFIG_LIB_OSTENTUS, (ostentus_led_golioth_set(o_dev, pin_state);));
}

int main(void)
{
	int err;

	LOG_DBG("Start Reference Design Template sample");

	LOG_INF("Firmware version: %s", _current_version);

	/* Get system thread id so loop delay change event can wake main */
	_system_thread = k_current_get();

#if DT_NODE_EXISTS(DT_ALIAS(golioth_led))
	/* Initialize Golioth logo LED */
	err = gpio_pin_configure_dt(&golioth_led, GPIO_OUTPUT_INACTIVE);
	if (err) {
		LOG_ERR("Unable to configure LED for Golioth Logo");
	}
#endif /* #if DT_NODE_EXISTS(DT_ALIAS(golioth_led)) */


	/* Run WiFi/DHCP if necessary */
	if (IS_ENABLED(CONFIG_GOLIOTH_SAMPLE_COMMON)) {
		net_connect();
	}

	/* Start Golioth client */
	start_golioth_client();

	/* Block until connected to Golioth */
	k_sem_take(&connected, K_FOREVER);

	while (true) {
		app_sensors_read_and_stream();

		k_sleep(K_SECONDS(get_loop_delay_s()));
	}
}
