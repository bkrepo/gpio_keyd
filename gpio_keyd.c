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
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <fcntl.h>
#include <time.h>
#include <syslog.h>
#include <signal.h>

#include <linux/input.h>
#include <linux/uinput.h>

#include <wiringPi.h>

#include "gpio_keyd.h"

#define ADC_MAX	4095

static int uidev_fd = -1;
static int pid_fd = -1;
const char *pid_file_name = "/tmp/gpio_keyd.pid";
static bool running = false;
const char *conf_file_name = "/etc/gpio_keyd.conf";
static int range = 100;

static int init_uinput(void)
{
	int fd;
	struct uinput_user_dev uidev;
	int i;

	fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
	if(fd < 0)
		goto err;

	if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0)
		goto err;
	if (ioctl(fd, UI_SET_EVBIT, EV_REP) < 0)
		goto err;
	if (ioctl(fd, UI_SET_KEYBIT, BTN_LEFT) < 0)
		goto err;
	if (ioctl(fd, UI_SET_EVBIT, EV_REL) < 0)
		goto err;
	if (ioctl(fd, UI_SET_RELBIT, REL_X) < 0)
		goto err;
	if (ioctl(fd, UI_SET_RELBIT, REL_Y) < 0)
		goto err;

	/* don't forget to add all the keys! */
	for (i = 0; i < 256; i++) {
		if (ioctl(fd, UI_SET_KEYBIT, i) < 0)
			goto err;
	}

	memset(&uidev, 0, sizeof(uidev));
	snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "uinput-gpio_keyd");
	uidev.id.bustype = BUS_HOST;
	uidev.id.vendor  = 0x1;
	uidev.id.product = 0x1;
	uidev.id.version = 1;

	if (write(fd, &uidev, sizeof(uidev)) < 0)
		goto err;

	if (ioctl(fd, UI_DEV_CREATE) < 0)
		goto err;

	uidev_fd = fd;

	return 0;
err:
	syslog(LOG_ERR, "%s: Failed initialize uinput", __func__);
	return -errno;
}

static int close_uinput(void)
{
	sleep(2);

	if (uidev_fd != -1) {
		if (ioctl(uidev_fd, UI_DEV_DESTROY) < 0) {
			syslog(LOG_ERR, "%s: Failed ioctl()", __func__);
			return -errno;
		}

		close(uidev_fd);
	}

	return 0;
}

static int sendSync(void)
{
	struct input_event uidev_ev;

	memset(&uidev_ev, 0, sizeof(struct input_event));
	gettimeofday(&uidev_ev.time, NULL);
	uidev_ev.type = EV_SYN;
	uidev_ev.code = SYN_REPORT;
	uidev_ev.value = 0;
	if (write(uidev_fd, &uidev_ev, sizeof(struct input_event)) < 0) {
		syslog(LOG_ERR, "%s: Failed wirte event", __func__);
		return -errno;
	}
	return 0;
}

static int sendKey(int key_code, int value)
{
	struct input_event uidev_ev;

	memset(&uidev_ev, 0, sizeof(struct input_event));
	gettimeofday(&uidev_ev.time, NULL);
	uidev_ev.type = EV_KEY;
	uidev_ev.code = key_code;
	uidev_ev.value = value;

	if (write(uidev_fd, &uidev_ev, sizeof(struct input_event)) < 0) {
		syslog(LOG_ERR, "%s: Failed write event", __func__);
		return -errno;
	}

	return 0;
}

static void gpio_key_poll(void)
{
	struct gpio_key *p;

	for (p = gpio_key_head.lh_first; p != NULL; p = p->list.le_next) {
		if (p->gpio_type == DIGITAL) {
			p->val = digitalRead(p->pin);
			if ((p->val == p->act_val) && (p->val != p->pre_val))
				sendKey((int)p->key_code, 1);
			else if ((p->val != p->act_val) && (p->val != p->pre_val))
				sendKey((int)p->key_code, 0);
		} else if (p->gpio_type == ANALOG) {
			p->val = analogRead(p->pin);
			if (((p->val <= p->act_val + range) && (p->val >= p->act_val - range)) &&
					((p->val > p->pre_val + range) || (p->val < p->pre_val - range)))
				sendKey((int)p->key_code, 1);
			else if ((p->val > ADC_MAX - range) &&
					((p->val > p->pre_val + range) || (p->val < p->pre_val - range)))
				sendKey((int)p->key_code, 0);
		}
		p->pre_val = p->val;
	}

	sendSync();
}

static void init_gpio_keyd(void)
{
	struct gpio_key *p;

	wiringPiSetup();

	for (p = gpio_key_head.lh_first; p != NULL; p = p->list.le_next) {
		pinMode(p->pin, INPUT);
		if (p->act_val)
			pullUpDnControl(p->pin, PUD_DOWN);
		else
			pullUpDnControl(p->pin, PUD_UP);
	}
}

static void handle_signal(int sig)
{
	if ((sig == SIGINT) || (sig == SIGTERM)) {
		/* Unlock and close lockfile */
		if (pid_fd != -1) {
			if (lockf(pid_fd, F_ULOCK, 0) < 0)
				syslog(LOG_ERR, "%s: unlocking pid file failed", __func__);
			close(pid_fd);
		}

		unlink(pid_file_name);
		running = false;

		/* Reset signal handling to default behavior */
		signal(sig, SIG_DFL);
	} else if (sig == SIGHUP) {
		parse_config(conf_file_name);
	}
}

static void daemonize(void)
{
	pid_t pid = 0;
	int fd;
	char pid_str[256] = {0,};

	pid = fork();

	if (pid < 0)
		exit(EXIT_FAILURE);

	if (pid > 0)
		exit(EXIT_SUCCESS);

	if (setsid() < 0)
		exit(EXIT_FAILURE);

	signal(SIGCHLD, SIG_IGN);

	pid = fork();

	if (pid < 0)
		exit(EXIT_FAILURE);

	if (pid > 0)
		exit(EXIT_SUCCESS);

	umask(0);
	if (chdir("/") < 0)
		syslog(LOG_ERR, "%s: chdir() failed", __func__);

	for (fd = sysconf(_SC_OPEN_MAX); fd > 0; fd--)
		close(fd);

	stdin = fopen("/dev/null", "r");
	stdout = fopen("/dev/null", "w+");
	stderr = fopen("/dev/null", "w+");

	pid_fd = open(pid_file_name, O_RDWR|O_CREAT, 0640);
	if (pid_fd < 0)
		exit(EXIT_FAILURE);

	if (lockf(pid_fd, F_TLOCK, 0) < 0)
		exit(EXIT_FAILURE);

	sprintf(pid_str, "%d\n", getpid());
	if (write(pid_fd, pid_str, strlen(pid_str)) < 0)
		syslog(LOG_ERR, "%s: write pid failed", __func__);
}

static void print_usage(void)
{
	printf("Usage: gpio_keyd [option]\n");
	printf("Options:\n");
	printf("  -c <config file>           set the configuration file (default: \"/etc/gpio_keyd.conf\")\n");
	printf("  -i <polling interval>      set polling interval time (default: 10000 us)\n");
	printf("  -d                         run as deamon\n");
	printf("  -h                         help\n");
}

int main(int argc, char **argv)
{
	bool is_daemon = false;
	unsigned int interval = 10000;
	int ret = EXIT_SUCCESS;
	int opt;

	while ((opt = getopt(argc, argv, "hc:k:di:")) != -1) {
		switch (opt) {
			case 'h':
				print_usage();
				return 0;
			case 'd':
				is_daemon = true;
				break;
			case 'c':
				conf_file_name = optarg;
				break;
			case 'i':
				interval = atoi(optarg);
				break;
		}
	}

	/* Open system log and write message to it */
	openlog("gpio_keyd", LOG_PID|LOG_CONS, LOG_DAEMON);
	syslog(LOG_INFO, "Started gpio_keyd");

	if (is_daemon)
		daemonize();

	parse_config(conf_file_name);
	init_gpio_keyd();

	/* Register signal handler */
	signal(SIGINT, handle_signal);
	signal(SIGTERM, handle_signal);
	signal(SIGHUP, handle_signal);

	if ((ret = init_uinput()) < 0)
		goto out;

	sleep(1);

	if (!is_daemon)
		printf("Press ^C to exit.\n");

	/* Polling */
	running = true;
	while (running) {
		gpio_key_poll();
		usleep(interval);
	}

out:
	syslog(LOG_INFO, "Stopped gpio_keyd");
	closelog();
	close_uinput();

	return ret;
}
