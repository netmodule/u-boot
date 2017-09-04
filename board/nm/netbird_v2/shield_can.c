#undef DEBUG

#include <common.h>
#include <asm/gpio.h>
#include <asm/arch/mux.h>

#include "shield.h"
#include "board.h"

#define NETBIRD_GPIO_RST_SHIELD_N GPIO_TO_PIN(0, 27)
#define NETBIRD_GPIO_LATCH GPIO_TO_PIN(0, 7)
#define NETBIRD_GPIO_MODE_0 GPIO_TO_PIN(1, 8)
#define NETBIRD_GPIO_MODE_1 GPIO_TO_PIN(1, 10)


static int shield_slot_initialized = 0;


static struct module_pin_mux can_shield_netbird_pin_mux_config[] = {
	/* Leave UART0 unconfigured because we want to configure it as needed by linux (can/spi/uart/etc) */
	{OFFSET(uart0_ctsn), (MODE(7) | PULLUDEN | PULLUP_EN)},	/* CAN1 tx */
	{OFFSET(uart0_rxd), (MODE(7) | PULLUDEN | PULLUP_EN)},			/* CAN0 tx */
	{OFFSET(ecap0_in_pwm0_out), (MODE(7) | PULLUDEN | PULLUP_EN)},			/* Latch EN */
	{-1},
};

static struct module_pin_mux can_shield_netbird_pin_mux_final[] = {
	/* Leave UART0 unconfigured because we want to configure it as needed by linux (can/spi/uart/etc) */
	{OFFSET(uart0_ctsn), (MODE(2) | PULLUDEN | PULLUP_EN)},	/* CAN1 tx */
	{OFFSET(uart0_rtsn), (MODE(2) | PULLUDDIS | RXACTIVE)},	/* CAN1 rx */
	{OFFSET(uart0_txd), (MODE(2) | PULLUDDIS | RXACTIVE)},	/* CAN0 rx */
	{OFFSET(uart0_rxd), (MODE(2) | PULLUDEN | PULLUP_EN)},			/* CAN0 tx */
	{-1},
};

static int request_gpios(void)
{
	int ret;

	debug("Shield configure gpios\n");
 	ret = shield_gpio_request_as_input(NETBIRD_GPIO_RST_SHIELD_N, "shield-rst");
	if ((ret < 0))
		return -1;
	ret = shield_gpio_request_as_input(NETBIRD_GPIO_LATCH, "shield-load");
	if ((ret < 0))
		return -1;
	ret = shield_gpio_request_as_input(NETBIRD_GPIO_MODE_0, "shield-mode0");
	if ((ret < 0))
		return -1;
	ret = shield_gpio_request_as_input(NETBIRD_GPIO_MODE_1, "shield-mode1");
	if ((ret < 0))
		return -1;

	shield_slot_initialized = 1;
	return 0;
}

static int configure_shieldmode(int mode)
{
	int ret;

	if (mode < 0 || mode > 3) {
		debug("Invalid shield mode %d\n", mode);
		return -1;
	}

	debug("Shield type dualcan\n");
	debug ("Set shield mode to %d\n", mode);

	if (!shield_slot_initialized) {
		if (request_gpios()) {
			puts("Failed to request gpios\n");
			return -1;
		}
	}

	debug("Configure shield pin muxing for configuration\n");
	configure_module_pin_mux(can_shield_netbird_pin_mux_config);

	debug("Make sure shield module is in reset\n");
	ret = gpio_direction_output(NETBIRD_GPIO_RST_SHIELD_N, 0);
	if (ret < 0) {
		puts("Can not set shield-rst as output\n");
		return -1;
	}
	udelay(10);

	debug("Set latch to high\n");
	ret = gpio_direction_output(NETBIRD_GPIO_LATCH, 1);
	if (ret < 0) {
		puts("Can not set shield-load as output\n");
		return -1;
	}
	udelay(10);

	debug("Write mode to GPIOs\n");
	ret = gpio_direction_output(NETBIRD_GPIO_MODE_0, mode & 0x01);
	if (ret < 0) {
		puts("Can not set shield-mode0 as output\n");
		return -1;
	}
	ret = gpio_direction_output(NETBIRD_GPIO_MODE_1, mode & 0x02);
	if (ret < 0) {
		puts("Can not set shield-mode1 as output\n");
		return -1;
	}
	udelay(10);

	debug("Set latch to low\n");
	gpio_set_value(NETBIRD_GPIO_LATCH, 0);
	udelay(10);

	debug("Set mode0 and mode1 to highz again\n");
	ret = gpio_direction_input(NETBIRD_GPIO_MODE_0);
	if ((ret < 0)) {
		puts("Could not configure shield slot mode0 gpio as input\n");
		return -1;
	}

	ret = gpio_direction_input(NETBIRD_GPIO_MODE_1);
	if ((ret < 0)) {
		puts("Could not configure shield slot mode1 gpio as input\n");
		return -1;
	}
	udelay(10);

	debug("Take shield out of reset\n");
	gpio_set_value(NETBIRD_GPIO_RST_SHIELD_N, 1);
	udelay(10);

	debug("Set final can shield muxing\n");
	configure_module_pin_mux(can_shield_netbird_pin_mux_final);

	return 0;

}

static int get_termination(const char* termination)
{
	if (strcmp("on", termination) == 0) {
		return 1;
	}
	else if (strcmp("off", termination) == 0)  {
		return 0;
	}

	debug ("Invalid termination mode %s (falling back to off)", termination);
	return -1;
}

static int get_mode_from_args(char * const argv[], int argc)
{
#define CAN_PORTS	2
	int terminations[CAN_PORTS];
	int i;

	assert(argc == (CAN_PORTS + 1));

	if (strcmp ("termination", argv[0])) {
		debug("The only option for dualcan is terminations\n");
		return -1;
	}

	for (i = 0; i < CAN_PORTS; i ++) {
		terminations[i] = get_termination(argv[i + 1]);
		if (terminations[i] < 0) {
			return -1;
		}
	}

	/* Termination is inverse */
	return (!terminations[0] << 0) | (!terminations[1] << 1);
}

static int set_shieldmode(char * const argv[], int argc)
{
	if (argc != 3) {
		debug("Too few arguments for dualcan\n");
		return -1;
	}

	return configure_shieldmode(get_mode_from_args(argv, argc));
}

struct shield_t can_shield = {
	"dualcan", set_shieldmode
};

void can_shield_init(void)
{
	shield_register(&can_shield);
}

