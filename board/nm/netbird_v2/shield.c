#undef DEBUG

#include <common.h>
#include <asm/gpio.h>
#include <asm/arch/mux.h>

#include "shield.h"
#include "board.h"

#define MAX_SHIELDS 16

static struct shield_t *shields[MAX_SHIELDS];
static int shield_count = 0;

/* Perhaps this function shouldn't leave in shields.c? */
int shield_gpio_request_as_input(unsigned int gpio, const char *label)
{
	int ret;

 	ret = gpio_request(gpio, label);
	if ((ret < 0)) {
		printf("Could not request shield slot %s gpio\n", label);
		return -1;
	}

	ret = gpio_direction_input(gpio);
	if ((ret < 0)) {
		printf("Could not configure shield slot %s gpio as input\n", label);
		return -1;
	}

	return 0;
}

void shield_register(struct shield_t *shield)
{
    if (shield_count >= MAX_SHIELDS) {
        printf("Max shield count reached (%d), please increment MAX_SHIELDS\n", MAX_SHIELDS);
        return;
    }
    shields[shield_count++] = shield;
}

int shield_set_mode(const char* shield_type, int argc, char * const argv[])
{
    int i;

    for (i = 0; i < shield_count; i++) {
        if (strcmp(shield_type, shields[i]->name) == 0) {
            return shields[i]->setmode(argv, argc);
        }
    }
    printf("Shield %s is unknown\n", shield_type);
    return -1;
}

static int do_shieldmode(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
    if (argc < 3) {
        puts("Invalid command (see help)\n");
		return -1;
    }

	return shield_set_mode(argv[1], argc - 2, &argv[2]);
}

U_BOOT_CMD(
	shield,	6,	1,	do_shieldmode,
	"Set the shield mode",
	"dualcan termination [on|off] [on|off]\n"
	"shield comio mode [rs232|rs485] termination [on|off]\n"
);

