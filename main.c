#define _GNU_SOURCE

#include <stdint.h>
#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "hardware/i2c.h"

#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/apps/mdns.h"

static const char *const wlan_ssid = WLAN_SSID;
static const char *const wlan_pass = WLAN_PASS;

static const uint16_t tcp_port = 80;

enum error {
	ERROR_GENERIC = 1,
	ERROR_INIT,
	ERROR_WLAN,
	ERROR_CREATE_PCB,
	ERROR_BIND,
	ERROR_LISTEN,
	ERROR_FINISH,
	ERROR_ACCEPT,
	ERROR_WRITE_PART,
	ERROR_WRITE_PART_MEM,
	ERROR_WRITE_BEGIN,
	ERROR_WRITE_BEGIN_MEM,
	ERROR_CHECKSUM_TEST,
	ERROR_SHT_CHECKSERIAL_READ,
	ERROR_SHT_CHECKSERIAL_WRITE,
	ERROR_SHT_CHECKSERIAL_CHECKSUM,
	ERROR_SHT_READ,
	ERROR_SHT_WRITE,
	ERROR_SHT_CHECKSUM,
	ERROR_SERVICE_TXT,
};

#define led_on(x) cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, x)

void fatal_error(enum error err)
{
	while (1) {
		for (int i = 0; i < err; i++) {
			led_on(1);
			sleep_ms(500);
			led_on(0);
			sleep_ms(500);
		}
		sleep_ms(4500);
	}
}

enum {
	SHT_CMD_MEASURE_HP		= 0xFD,
	SHT_CMD_MEASURE_MP		= 0xF6,
	SHT_CMD_MEASURE_LP		= 0xE0,
	SHT_CMD_READSERIAL		= 0x89,
	SHT_CMD_SOFTRESET		= 0x94,
	SHT_CMD_HEAT_200mW_1000ms	= 0x39,
	SHT_CMD_HEAT_200mW_100ms	= 0x32,
	SHT_CMD_HEAT_110mW_1000ms	= 0x2F,
	SHT_CMD_HEAT_110mW_100ms	= 0x24,
	SHT_CMD_HEAT_20mW_1000ms	= 0x1E,
	SHT_CMD_HEAT_20mW_100ms		= 0x15,

	/* Delay for any command except heater.
	 * Spec. says max is 8ms, so this is plenty. */
	SHT_DELAY_MEASURE = 25,

	OUR_SDA_PIN = 0,
	OUR_CLK_PIN = 1,

	OUR_I2C_ADDR = 0x44,
};

#define OUR_I2C i2c0

uint8_t crc8(uint8_t *data)
{
	uint8_t crc = 0xFF;
	for (int i = 0; i < 2; i++) {
		crc ^= data[i];
		for (int j = 0; j < 8; j++) {
			if (crc & 0x80)
				crc = (crc << 1) ^ 0x31;
			else
				crc = crc << 1;
		}
	}
	return crc;
}

void sht_cmd_blocking(uint8_t cmd, uint16_t *buf)
{
	if (i2c_write_blocking(OUR_I2C, OUR_I2C_ADDR, &cmd, 1, false) != 1) {
		fatal_error(ERROR_SHT_READ);
	}

	sleep_ms(SHT_DELAY_MEASURE);

	uint8_t data[6] = { 0 };
	if (i2c_read_blocking(OUR_I2C, OUR_I2C_ADDR, data, sizeof(data), false) != sizeof(data)) {
		fatal_error(ERROR_SHT_WRITE);
	}

	if (crc8(data) != data[2])
		fatal_error(ERROR_SHT_CHECKSUM);
	if (crc8(data + 3) != data[5])
		fatal_error(ERROR_SHT_CHECKSUM);

	buf[0] = (data[0] << 8) | data[1];
	buf[1] = (data[3] << 8) | data[4];
}

struct session {
	u16_t rem_to_send;
	u16_t queued;
	u16_t rem_to_queue;
	char *data;
};

#define min(x, y) ( (x) < (y) ? (x) : (y) )

err_t server_sent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
	struct session *session = (struct session*)arg;
	session->rem_to_send -= len;
	if (session->rem_to_send == 0) {
		free(session->data);
		free(session);
		tcp_arg(pcb, NULL);
		tcp_sent(pcb, NULL);
		tcp_close(pcb);
	} else {
		u16_t to_queue = min(tcp_sndbuf(pcb), session->rem_to_queue);
		err_t err = tcp_write(pcb, session->data + session->queued, to_queue, 0);
		tcp_output(pcb);
		if (err == ERR_OK) {
			session->queued += to_queue;
			session->rem_to_queue -= to_queue;
		} else if (err != ERR_MEM) {
			fatal_error(ERROR_WRITE_PART);
		}
	}
}

err_t server_accept(void *, struct tcp_pcb *pcb, err_t err)
{
	if (err != ERR_OK || !pcb) {
		fatal_error(ERROR_ACCEPT);
		return ERR_VAL;
	}

	double temp, hum;
	{
		uint16_t buf[2] = {0, 0};
		sht_cmd_blocking(SHT_CMD_MEASURE_HP, buf);

		temp = (175.0 * ((double)buf[0] / 65535.0)) - 45.0;
		hum = (125.0 * ((double)buf[1] / 65535.0)) - 6.0;
	}

	struct session *arg = malloc(sizeof(struct session));
	asprintf(&arg->data, 
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: application/openmetrics-text; version=1.0.0; charset=utf-8\r\n"
		"\r\n"
		"# TYPE humid gauge\n"
		"humid %f\n"
		"# TYPE temp gauge\n"
		"temp %f\n"
		"# EOF\n",
		hum,
		temp
	);
	arg->rem_to_send = strlen(arg->data);
	u16_t to_queue = min(arg->rem_to_send, tcp_sndbuf(pcb));
	arg->queued = to_queue;
	arg->rem_to_queue = arg->rem_to_send - to_queue;
	tcp_arg(pcb, arg);
	tcp_sent(pcb, server_sent);
	err_t newerr = tcp_write(pcb, arg->data, to_queue, 0);
	if (newerr == ERR_MEM) {
		fatal_error(ERROR_WRITE_BEGIN_MEM);
	} else if (newerr != ERR_OK) {
		fatal_error(ERROR_WRITE_BEGIN);
	}
	tcp_output(pcb);
}

static void srv_txt(struct mdns_service *service, void *)
{
	const char *txt = "path=/";
	err_t res = mdns_resp_add_service_txtitem(service, txt, strlen(txt));
	if (res != ERR_OK) {
		fatal_error(ERROR_SERVICE_TXT);
	}
}

int main()
{
	{
		/* Test vector from spec. */
		uint8_t data[2] = {0xBE, 0xEF};
		if (crc8(data) != 0x92)
			fatal_error(ERROR_CHECKSUM_TEST);
	}

	i2c_init(i2c0, 100 * 1000);
	gpio_set_function(OUR_SDA_PIN, GPIO_FUNC_I2C);
	gpio_set_function(OUR_CLK_PIN, GPIO_FUNC_I2C);
	gpio_pull_up(OUR_SDA_PIN);
	gpio_pull_up(OUR_CLK_PIN);

	{
		/* Test sensor by reading serial */
		uint8_t cmd = SHT_CMD_READSERIAL;
		if (i2c_write_blocking(OUR_I2C, OUR_I2C_ADDR, &cmd, 1, false) != 1) {
			fatal_error(ERROR_SHT_CHECKSERIAL_READ);
		}

		sleep_ms(SHT_DELAY_MEASURE);

		uint8_t data[6] = { 0 };
		if (i2c_read_blocking(OUR_I2C, OUR_I2C_ADDR, data, sizeof(data), false) != sizeof(data)) {
			fatal_error(ERROR_SHT_CHECKSERIAL_WRITE);
		}

		if (crc8(data) != data[2])
			fatal_error(ERROR_SHT_CHECKSERIAL_CHECKSUM);
		if (crc8(data + 3) != data[5])
			fatal_error(ERROR_SHT_CHECKSERIAL_CHECKSUM);
	}

	if (cyw43_arch_init_with_country(CYW43_COUNTRY_UK)) {
		fatal_error(ERROR_INIT);
	}

	cyw43_arch_enable_sta_mode();

	led_on(1);
	if (cyw43_arch_wifi_connect_blocking(wlan_ssid, wlan_pass, CYW43_AUTH_WPA2_AES_PSK) != 0) {
		fatal_error(ERROR_WLAN);
	}
	led_on(0);

	struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
	if (!pcb) {
		fatal_error(ERROR_CREATE_PCB);
	}

	if (tcp_bind(pcb, IP_ANY_TYPE, tcp_port)) {
		fatal_error(ERROR_BIND);
	}

	pcb = tcp_listen_with_backlog(pcb, 1);
	if (!pcb) {
		fatal_error(ERROR_LISTEN);
	}

	tcp_accept(pcb, server_accept);

	sleep_ms(5000);
	led_on(1);

	mdns_resp_add_service(netif_default, MDNS_SERVICE_NAME, "_prometheus-http", DNSSD_PROTO_TCP, 80, srv_txt, NULL);
	mdns_resp_announce(netif_default);

	while (1) {
		cyw43_arch_poll();
		sleep_ms(1);
	}

	cyw43_arch_deinit();
	fatal_error(ERROR_FINISH);
	return 0;
}

