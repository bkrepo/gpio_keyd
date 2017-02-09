#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <syslog.h>

#include "gpio_keyd.h"

static int find_key_code(const char *fname, const char *key_name)
{
	FILE *hfp;
	static char buf[255] = {0,};
	static char hkey_name[64] = {0,};
	static char key_code_str[16] = {0,};
	int key_code;

	if ((hfp = fopen(fname, "r")) == NULL) {
		syslog(LOG_ERR, "%s: Failed key code header file", __func__);
		return -errno;
	}

	while (fgets(buf, 255, (FILE *)hfp)) {
		if (!strncmp("#define BTN_", buf, 11) ||
		    !strncmp("#define KEY_", buf, 11)) {
			sscanf(buf+8, "%s %s", hkey_name, key_code_str);

			if (!strncmp(hkey_name, key_name,
			   	     strlen(hkey_name) >= strlen(key_name) ? strlen(hkey_name):strlen(key_name))
				    ) {
				fclose(hfp);
				return (int)strtol(key_code_str, NULL, 0);
			}
		}
	}
	fclose(hfp);
	return -1;
}

int parse_config(const char *conf_file_name, const char *key_code_header)
{
	FILE *cfp;
	static char buf[255] = {0,};
	static char ckey_name[64] = {0,};
	static char gpio_type[16] = {0,};
	int pin, key_code, val, line = 0;
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
		key_code = find_key_code(key_code_header, ckey_name);
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
