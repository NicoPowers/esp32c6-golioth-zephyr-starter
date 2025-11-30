/*
 * Copyright (c) 2022-2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(golioth_rd_template, LOG_LEVEL_DBG);

#include <stdio.h>
#include <app_version.h>
#include "app_rpc.h"
#include "app_settings.h"
#include "app_state.h"
#include "app_sensors.h"
#include <golioth/client.h>
#include <golioth/fw_update.h>
// #include <samples/common/net_connect.h>
// #include <samples/common/sample_credentials.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/devicetree.h>
#include <zephyr/device.h>
#include <zephyr/fs/fs.h>


#define CRT_PATH "/lfs1/credentials/crt.der"
#define KEY_PATH "/lfs1/credentials/key.der"

/* The devicetree node identifiers for the LEDs */
#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)

/* Current firmware version; update in VERSION */
static const char *_current_version =
	STRINGIFY(APP_VERSION_MAJOR) "." STRINGIFY(APP_VERSION_MINOR) "." STRINGIFY(APP_PATCHLEVEL);

static const struct gpio_dt_spec green_led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec red_led = GPIO_DT_SPEC_GET(LED1_NODE, gpios);


/* Get I2C bus device */
#define I2C_NODE DT_NODELABEL(i2c0)
static const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);


static const uint8_t tls_ca_crt[] = {
#include "golioth-systemclient-ca_crt.inc"
};

static const uint8_t tls_secondary_ca_crt[] = {
#if CONFIG_GOLIOTH_SECONDARY_CA_CRT
#include "golioth-systemclient-secondary_ca_crt.inc"
#endif
};

static struct golioth_client *client;
K_SEM_DEFINE(connected, 0, 1);

static k_tid_t _system_thread = 0;

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

static int load_credential_from_fs(const char *path, uint8_t **buf_p, size_t *buf_len)
{
    struct fs_file_t file;
    struct fs_dirent dirent;

    fs_file_t_init(&file);

    int err = fs_stat(path, &dirent);

    if (err < 0)
    {
        LOG_WRN("Could not stat %s, err: %d", path, err);
        goto finish;
    }
    if (dirent.type != FS_DIR_ENTRY_FILE)
    {
        LOG_ERR("%s is not a file", path);
        err = -EISDIR;
        goto finish;
    }
    if (dirent.size == 0)
    {
        LOG_ERR("%s is an empty file", path);
        err = -EINVAL;
        goto finish;
    }


    err = fs_open(&file, path, FS_O_READ);

    if (err < 0)
    {
        LOG_ERR("Could not open %s", path);
        goto finish;
    }

    /* NOTE: *buf_p is used directly by the TLS Credentials library, and so must remain
     * allocated for the life of the program.
     */

    free(*buf_p);
    *buf_p = malloc(dirent.size);

    if (*buf_p == NULL)
    {
        LOG_ERR("Could not allocate space to read credential");
        err = -ENOMEM;
        goto finish_with_file;
    }

    err = fs_read(&file, *buf_p, dirent.size);

    if (err < 0)
    {
        LOG_ERR("Could not read %s, err: %d", path, err);
        free(*buf_p);
        goto finish_with_file;
    }

    LOG_INF("Read %d bytes from %s", dirent.size, path);

    /* Set the size of the allocated buffer */
    *buf_len = dirent.size;

finish_with_file:
    fs_close(&file);

finish:
    return err;
}

static void start_golioth_client(const struct golioth_client_config *client_config)
{
	/* Get the client configuration from auto-loaded settings */
	// const struct golioth_client_config *client_config = golioth_sample_credentials_get();

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
	gpio_pin_set_dt(&green_led, pin_state);
}

int main(void)
{
	int ret;

	LOG_DBG("Start Reference Design Template sample");

	LOG_INF("Firmware version: %s", _current_version);

	/* Get system thread id so loop delay change event can wake main */
	_system_thread = k_current_get();

	ret = gpio_pin_configure_dt(&green_led, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		printk("Error: failed to configure green LED GPIO\n");
		return 0;
	}

	/* Run WiFi/DHCP if necessary */
	if (IS_ENABLED(CONFIG_GOLIOTH_SAMPLE_COMMON)) {
		net_connect();
	}

	uint8_t *tls_client_crt = NULL;
    uint8_t *tls_client_key = NULL;
    size_t tls_client_crt_len, tls_client_key_len;

	while (1)
    {
        int ret = load_credential_from_fs(CRT_PATH, &tls_client_crt, &tls_client_crt_len);
        if (ret > 0)
        {
            ret = load_credential_from_fs(KEY_PATH, &tls_client_key, &tls_client_key_len);
            if (ret > 0)
            {
                break;
            }
        }

        k_sleep(K_SECONDS(5));
    }

	if (tls_client_crt != NULL && tls_client_key != NULL)
    {
        struct golioth_client_config client_config = {
            .credentials =
                {
                    .auth_type = GOLIOTH_TLS_AUTH_TYPE_PKI,
                    .pki =
                        {
                            .ca_cert = tls_ca_crt,
                            .ca_cert_len = sizeof(tls_ca_crt),
                            .public_cert = tls_client_crt,
                            .public_cert_len = tls_client_crt_len,
                            .private_key = tls_client_key,
                            .private_key_len = tls_client_key_len,
                            .secondary_ca_cert = tls_secondary_ca_crt,
                            .secondary_ca_cert_len = sizeof(tls_secondary_ca_crt),
                        },
                },
        };

        /* Start Golioth client */
		start_golioth_client(&client_config);
    }
    else
    {
        LOG_ERR("Error reading certificate credentials from filesystem");
    }
	

	/* Block until connected to Golioth */
	k_sem_take(&connected, K_FOREVER);

	while (true) {
		app_sensors_read_and_stream();

		k_sleep(K_SECONDS(get_loop_delay_s()));
	}
}
