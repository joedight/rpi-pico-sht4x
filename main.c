#define _GNU_SOURCE

#include <stdint.h>
#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "hardware/i2c.h"

#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/apps/mdns.h"

#include "/usr/local/include/util/string.h"

#include "config.h"

void led_on(bool x)
{
	cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, x);
}

struct measurement {
	const char *name;
	const char *type;
	char *value;
};

void measure_init(void);
struct measurement *measure_take(void);

enum error {
	ERROR_GENERIC = 1,
	ERROR_INIT,
	ERROR_WLAN,
	ERROR_MDNS,
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
	ERROR_SERVICE_TXT,
	ERROR_FINAL,
};

void flash_error(int err)
{
	for (int i = 0; i < err; i++) {
		led_on(1);
		sleep_ms(500);
		led_on(0);
		sleep_ms(500);
	}
}

void fatal_error(int err)
{
	while (1) {
		flash_error(err);
		sleep_ms(4500);
	}
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
	return ERR_OK;
}

err_t server_accept(void *, struct tcp_pcb *pcb, err_t err)
{
	if (err != ERR_OK || !pcb) {
		fatal_error(ERROR_ACCEPT);
		return ERR_VAL;
	}

	/* Take measurement and store. */
	struct session *arg = calloc(1, sizeof(struct session));
	const struct measurement *ms = measure_take();
	arg->data = rstrcpy(
		NULL, 
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: application/openmetrics-text; version=1.0.0; charset=utf-8\r\n"
		"\r\n"
	);

	while (ms->name) {
		arg->data = rsprintf(
			arg->data,
			"%s"
			"# TYPE %s %s\n"
			"%s %s\n",
			arg->data,
			ms->name, ms->type,
			ms->name, ms->value
		);
		ms++;
	}
	arg->data = rstrcat(arg->data, "# EOF\n");

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

	return ERR_OK;
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
//	i2c_init(i2c0, 100 * 1000);
//	gpio_set_function(our_sda_pin, GPIO_FUNC_I2C);
//	gpio_set_function(our_clk_pin, GPIO_FUNC_I2C);
//	gpio_pull_up(our_sda_pin);
//	gpio_pull_up(our_clk_pin);

//	measure_init();

	if (cyw43_arch_init_with_country(CYW43_COUNTRY_UK)) {
		fatal_error(ERROR_INIT);
	}

	cyw43_arch_enable_sta_mode();

	led_on(1);

	while (cyw43_arch_wifi_connect_blocking(wlan_ssid, wlan_pass, CYW43_AUTH_WPA2_AES_PSK) != 0) {
		flash_error(ERROR_WLAN);
		sleep_ms(300'000);
	}

	mdns_resp_init();
	if (mdns_resp_add_netif(&cyw43_state.netif[CYW43_ITF_STA], CYW43_HOST_NAME) != ERR_OK) {
		fatal_error(ERROR_MDNS);
	}
	/*
	if (mdns_resp_add_service(netif_default, MDNS_SERVICE_NAME, "_prometheus-http", DNSSD_PROTO_TCP, 80, srv_txt, NULL) != ERR_OK) {
		fatal_error(ERROR_MDNS);
	}
	*/
	// mdns_resp_announce(netif_default);

	/*
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
	*/

	led_on(0);

	uint64_t next_announce = time_us_64();
	while (1) {
		cyw43_arch_poll();
		sleep_ms(1);
		/* Should only be on addr change... */
		if (time_us_64() >= next_announce) {
			next_announce = time_us_64() + 1000ul * 1000 * 60 * 10;
			mdns_resp_announce(netif_default);
		}	
	}

	cyw43_arch_deinit();
	fatal_error(ERROR_FINISH);
	return 0;
}

