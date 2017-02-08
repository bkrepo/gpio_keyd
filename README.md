gpio_keyd
========

gpio_keyd is a maaping program between GPIO and key inputs by uinput.
gpio_keyd needs wiringPi library in order to control GPIO.

###Installation:

```
$ git clone https://github.com/hardkernel/wiringPi.git
$ cd wiringPi
$ sudo ./build
$ cd
$ git clone https://github.com/bkrepo/gpio_keyd.git
$ cd gpio_keyd
$ make
$ sudo make install
```

###Usage:
```
Usage: gpio_keyd [option]
Options:
  -c <config file>           set the configuration file (default: "/etc/gpio_keyd.conf")
  -k <key code header>       set the key code header file (default: "/usr/include/linux/input.h")
  -i <polling interval>      set polling interval time (default: 10000 us)
  -d                         run as deamon
  -h                         help
```

###Configuration:
gpio_keyd uses the configuration file which is including the GPIO-key mapping information.
GPIO pin means the wiringPi GPIO pin number.

* /etc/gpio_keyd.conf
```
#<key code>	<GPIO pin>
KEY_LEFT	1
KEY_RIGHT	4
KEY_UP		16
KEY_DOWN	15
KEY_ENTER	23
KEY_ESC		22
KEY_A		31
KEY_B		11
KEY_C		10
KEY_D		6
```

gpio_keyd need key code header file for the key code parsing.
This header file is different each system (default: '/usr/include/linux/input.h'). If the header file is wrong in your system, then change the header file with '-k' option.
```
$ gpio_keyd -k /usr/include/linux/input-event-codes.h
```
