#undef DEBUG

#include <common.h>
#include <asm/gpio.h>
#include <asm/arch/mux.h>

#include "shield.h"
#include "board.h"

#define NETBIRD_GPIO_RST_SHIELD_N GPIO_TO_PIN(0, 27)
#define NETBIRD_GPIO_LOAD GPIO_TO_PIN(1, 9)
#define NETBIRD_GPIO_MODE_0 GPIO_TO_PIN(1, 11)
#define NETBIRD_GPIO_MODE_1 GPIO_TO_PIN(1, 10)


static int shield_slot_initialized = 0;


/* V2OK */
static struct module_pin_mux shield_gpio_safe_netbird_pin_mux[] = {
	/* Leave UART0 unconfigured because we want to configure it as needed by linux (can/spi/uart/etc) */
	{OFFSET(uart0_rxd), (MODE(7) | PULLUDDIS | RXACTIVE)},	/* (E15) UART0_RXD */
	{OFFSET(uart0_txd), (MODE(7) | PULLUDDIS | RXACTIVE)},			/* (E16) UART0_TXD */
	{-1},
};

static struct module_pin_mux shield_gpio_netbird_pin_mux[] = {
	/* Leave UART0 unconfigured because we want to configure it as needed by linux (can/spi/uart/etc) */
	{OFFSET(uart0_rxd), (MODE(7) | PULLUDDIS)},	/* (E15) UART0_RXD */
	{OFFSET(uart0_txd), (MODE(7) | PULLUDEN | PULLUP_EN)},			/* (E16) UART0_TXD */
	{-1},
};

static int request_gpios(void)
{
	int ret;

	debug("Extension slot init\n");
 	ret = shield_gpio_request_as_input(NETBIRD_GPIO_RST_SHIELD_N, "shield-rst");
	if ((ret < 0))
		return -1;
	ret = shield_gpio_request_as_input(NETBIRD_GPIO_LOAD, "shield-load");
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
		debug ("Invalid shield mode %d\n", mode);
		return -1;
	}

	debug("Shield type comio\n");
	debug ("Set shield mode to %d\n", mode);

	if (!shield_slot_initialized) {
		if (request_gpios()) {
			puts("Failed to request gpios\n");
			return -1;
		}
	}

	debug("Make sure shield module is in reset\n");
	ret = gpio_direction_output(NETBIRD_GPIO_RST_SHIELD_N, 0);
	if (ret < 0) {
		puts("Can not set shield-rst as output\n");
		return -1;
	}
	udelay(10);

	debug("Enable gpio pull-ups\n");
	configure_module_pin_mux(shield_gpio_netbird_pin_mux);

	debug("Set load to low\n");
	ret = gpio_direction_output(NETBIRD_GPIO_LOAD, 0);
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

	debug("Set load to high\n");
	gpio_set_value(NETBIRD_GPIO_LOAD, 1);
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

	debug("Disable pullups on shield gpios\n");
	configure_module_pin_mux(shield_gpio_safe_netbird_pin_mux);
	udelay(10);

	debug("Take shield out of reset\n");
	gpio_set_value(NETBIRD_GPIO_RST_SHIELD_N, 1);
	udelay(10);

	debug("Set gpio load as input again\n");
	ret = gpio_direction_input(NETBIRD_GPIO_LOAD);
	if (ret < 0) {
		puts("Can not configure shield slot load as input");
		return -1;
	}

	return 0;
}

enum mode_nr {
	RS232,
	RS485,
	UNKNOWN
};

struct mode {
	enum mode_nr nr;
	const char* name;
	int argc;
};

struct mode modes[] = {
	{RS232, "rs232", 0},
	{RS485, "rs485", 2}
};

static const struct mode *get_mode(const char *mode)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(modes); i++) {
		if (strcmp(modes[i].name, mode) == 0) {
			return &modes[i];
		}
	}
	return NULL;
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
	int termination = 0;
	int rs232 = 0;
	const struct mode *selected_mode;

	assert(argc >= 2);

	if (strcmp ("mode", argv[0])) {
		debug("Invalid arguments (see help)\n");
		return -1;
	}

	selected_mode = get_mode(argv[1]);
	if (selected_mode == NULL) {
		debug("Mode %s not supported\n", argv[1]);
		return -1;
	}

	debug ("Mode %s, index %d, argc %d\n", selected_mode->name,
			selected_mode->nr, selected_mode->argc);

	if (selected_mode->argc != argc - 2) {
		debug("Invalid argument count for mode %s (should %d is %d)\n",
				argv[1],  selected_mode->argc, argc - 2);
		return -1;
	}

	if (selected_mode->nr == RS485) {
		if (strcmp("termination", argv[2])) {
			debug("Invalid arguments, do not configure termination\n");
			return -1;
		}

		termination = get_termination(argv[3]);
		if (termination < 0) {
			debug("Invalid termination %s\n", argv[3]);
			return -1;
		}
	}
	else {
		rs232 = 1;
	}

	/* Termination is inverse */
	return (rs232 << 0) | ((!termination) << 1);
}

int set_shieldmode(char * const argv[], int argc)
{
	if (argc < 2) {
		debug("Too few arguments for comio\n");
		return -1;
	}

	/* -1 will make configure_shieldmode to faile and is okay therefore */
	return configure_shieldmode(get_mode_from_args(argv, argc));
}

struct shield_t comio_shield = {
	"comio", set_shieldmode
};

void comio_shield_init(void)
{
	shield_register(&comio_shield);
}
