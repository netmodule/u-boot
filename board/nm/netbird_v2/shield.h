/**@file	/home/eichenberger/projects/nbhw16/u-boot/board/nm/netbird_v2/shield.h
 * @author	eichenberger
 * @version	704
 * @date
 * 	Created:	Wed 31 May 2017 02:56:16 PM CEST \n
 * 	Last Update:	Wed 31 May 2017 02:56:16 PM CEST
 */
#ifndef SHIELD_H
#define SHIELD_H

struct shield_t{
    char name[64];
    int (*setmode)(char * const argv[], int argc);
};

int shield_setmode(int mode);
void shield_register(struct shield_t *shield);

int shield_gpio_request_as_input(unsigned int gpio, const char *label);

#endif // SHIELD_H
