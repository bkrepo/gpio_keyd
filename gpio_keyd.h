#ifndef _GPIO_KEYD_H
#define _GPIO_KEYD_H

#include <sys/queue.h>

struct gpio_key {
	int pin;	/* wiringPi number */
	int key_code;	/* KEY code */
	int val;	/* GPIO value */
	int pre_val;	/* GPIO previous value */
	LIST_ENTRY(gpio_key) list;
};

LIST_HEAD(listhead, gpio_key) gpio_key_head;

extern int parse_config(const char *conf_file_name, const char *key_code_header);

#endif
