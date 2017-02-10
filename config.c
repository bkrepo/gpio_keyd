#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <syslog.h>
#include <unistd.h>

#define _GNU_SOURCE
#define __USE_GNU
#include <search.h>

#include "gpio_keyd.h"

struct hsearch_data htab;
#define HTABLE_COUNT	1024

static int create_hash_table(const char *fname)
{
	FILE *hfp;
	static char buf[255] = {0,};
	static char hkey_name[64] = {0,};
	static char key_code_str[16] = {0,};
	static char *keys[HTABLE_COUNT];
	ENTRY e, *ep;
	int i=0, slen;

	printf("header file: %s\n", fname);
	if ((hfp = fopen(fname, "r")) == NULL) {
		syslog(LOG_ERR, "%s: Failed key code header file", __func__);
		return -errno;
	}

	hcreate_r(HTABLE_COUNT, &htab);
	while (fgets(buf, 255, (FILE *)hfp)) {
		if (!strncmp("#define BTN_", buf, 11) ||
		    !strncmp("#define KEY_", buf, 11)) {
			sscanf(buf+8, "%s %s", hkey_name, key_code_str);
			slen = strlen(hkey_name);
			keys[i] = (char *)malloc(slen+1);
			keys[i][slen] = '\0';
			strncpy(keys[i], hkey_name, slen);
			e.key = keys[i++];
			e.data = (void *)strtol(key_code_str, NULL, 0);
			if (hsearch_r(e, ENTER, &ep, &htab) == 0) {
				syslog(LOG_ERR, "%s: hash table entry failed", __func__);
				fclose(hfp);
				return -errno;
			}
		}
	}
	fclose(hfp);

	return 0;
}

static void destroy_hash_table(void)
{
	hdestroy_r(&htab);
}

int parse_config(const char *conf_file_name)
{
	FILE *cfp;
	static char buf[255] = {0,};
	static char ckey_name[64] = {0,};
	static char gpio_type[16] = {0,};
	int pin, val, line = 0;
	struct gpio_key *pgpio_key;
	const char *key_code_header = "/usr/include/linux/input-event-codes.h";
	ENTRY e, *ep;

	/* Find key code header file. */
	if (access(key_code_header, F_OK) == -1)
		key_code_header = "/usr/include/linux/input.h";
	if (access(key_code_header, F_OK) == -1) {
		syslog(LOG_ERR, "%s: could not find key code header file.
				('/usr/include/linux/input-event-codes.h' or '/usr/include/linux/input.h')
				", __func__);
		goto err;
	}

	if (create_hash_table(key_code_header) < 0)
		goto err;

	if ((cfp = fopen(conf_file_name, "r")) == NULL)
		goto err;

	LIST_INIT(&gpio_key_head);

	while (fgets(buf, 255, (FILE *)cfp)) {
		++line;
		if (buf[0] == '#')
			continue;
		sscanf(buf, "%s %s %d %d", ckey_name, gpio_type, &pin, &val);

		/* Find key code value from key code header file */
		e.key = ckey_name;
		if (hsearch_r(e, FIND, &ep, &htab) == 0) {
			syslog(LOG_ERR, "%s[%d]: Unknown key code = \"%s\"\n",
				conf_file_name, line, ckey_name);
			fclose(cfp);
			errno = EINVAL;
			goto err;
		}
		pgpio_key = (struct gpio_key *)malloc(sizeof(struct gpio_key));
		pgpio_key->pin = pin;
		pgpio_key->key_code = (long)ep->data;
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
	destroy_hash_table();

	return 0;
err:
	syslog(LOG_ERR, "%s: Failed config file parsing", __func__);
	return -errno;
}
