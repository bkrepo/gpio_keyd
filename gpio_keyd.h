#ifndef _GPIO_KEYD_H
#define _GPIO_KEYD_H

#include <sys/queue.h>

typedef enum {
	DIGITAL,
	ANALOG,
} gpio_type_t;

struct gpio_key {
	int pin;		/* wiringPi pin number */
	long key_code;		/* KEY code */
	int val;		/* GPIO value */
	int pre_val;		/* GPIO previous value */
	gpio_type_t gpio_type;	/* GPIO type */
	int act_val;		/* Active value */
	LIST_ENTRY(gpio_key) list;
};

LIST_HEAD(listhead, gpio_key) gpio_key_head;

int parse_config(const char *conf_file_name);

#endif
