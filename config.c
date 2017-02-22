/*
 * Copyright (C) 2017 Brian Kim <brian.kim@hardkernel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <syslog.h>
#include <unistd.h>

#include "gpio_keyd.h"

struct key_code {
	char name[32];
	int code;
} key_codes[] = {
#include "key_defs.h"
};

static int find_key_code(const char *name)
{
	int i = 0;

	for(;i < sizeof(key_codes) / sizeof(key_codes[0]); ++i) {
		if (!strcmp(key_codes[i].name, name))
			return key_codes[i].code;
	}

	return -1;
}

int parse_config(const char *conf_file_name)
{
	FILE *cfp;
	static char buf[255] = {0,};
	static char ckey_name[64] = {0,};
	static char gpio_type[16] = {0,};
	int pin, val, key_code, line = 0;
	struct gpio_key *pgpio_key;

	if ((cfp = fopen(conf_file_name, "r")) == NULL)
		goto err;

	LIST_INIT(&gpio_key_head);

	while (fgets(buf, 255, (FILE *)cfp)) {
		++line;
		if (buf[0] == '#')
			continue;
		sscanf(buf, "%s %s %d %d", ckey_name, gpio_type, &pin, &val);

		/* Find key code value from key code header file */
		key_code = find_key_code(ckey_name);
		if (key_code == -1) {
			syslog(LOG_ERR, "%s[%d]: Unknown key code = \"%s\"\n",
				conf_file_name, line, ckey_name);
			fclose(cfp);
			errno = EINVAL;
			goto err;
		}
		pgpio_key = (struct gpio_key *)malloc(sizeof(struct gpio_key));
		pgpio_key->pin = pin;
		pgpio_key->key_code = key_code;
		if (!strncmp("digital", gpio_type, 7)) {
			pgpio_key->gpio_type = DIGITAL;
			pgpio_key->val = !val;
			pgpio_key->pre_val = !val;
			pgpio_key->act_val = val;
		} else if(!strncmp("analog", gpio_type, 6)) {
			pgpio_key->gpio_type = ANALOG;
			pgpio_key->val = 0;
			pgpio_key->pre_val = 0;
			pgpio_key->act_val = val;
		} else {
			syslog(LOG_ERR, "%s[%d]: Unknown GPIO typecode = \"%s\"\n",
					conf_file_name, line, gpio_type);
			fclose(cfp);
			errno = EINVAL;
			goto err;
		}

		LIST_INSERT_HEAD(&gpio_key_head, pgpio_key, list);
	}

	fclose(cfp);

	return 0;
err:
	syslog(LOG_ERR, "%s: Failed config file parsing", __func__);
	return -errno;
}
