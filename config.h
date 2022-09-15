#pragma once

#include <stdlib.h>
#include <stdint.h>

#ifdef WLAN_SSID
static const char *const wlan_ssid = WLAN_SSID;
static const char *const wlan_pass = WLAN_PASS;
#else
#error Should define WLAN_SSID
static const char *const wlan_ssid = NULL;
static const char *const wlan_pass = NULL;
#endif

enum {
	tcp_port = 80,

	our_sda_pin = 0,
	our_clk_pin = 1,
};

#define our_i2c i2c0

