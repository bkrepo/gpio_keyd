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
GPIO pin means the wiringPi GPIO pin number. Digital GPIO can configure 'active high' or 'active low' by 'Active value'

gpio_keyd also support analog GPIO input. Analog GPIO only works with 'active low'.

* (Example: /etc/gpio_keyd.conf)
```
# Digital input
# <Key code>	<GPIO type>	<GPIO pin>	<Active value>
KEY_LEFT	digital		1		0
KEY_RIGHT	digital		4		0
KEY_UP		digital		16		0
KEY_DOWN	digital		15		0
KEY_ENTER	digital		23		0
KEY_ESC		digital		22		0
KEY_A		digital		31		0
KEY_B		digital		11		0
KEY_C		digital		10		0
KEY_D		digital		6		0

# Analog input
# <Key code>	<GPIO type>	<ADC port>	<ADC active value>
KEY_E		analog		0		0
KEY_F		analog		0		2045
KEY_G		analog		1		0
KEY_H		analog		1		2045
```

gpio_keyd need key code header file for the key code parsing.
This header file is different each system (default: '/usr/include/linux/input.h'). If the header file is wrong in your system, then change the header file with '-k' option.
```
$ gpio_keyd -k /usr/include/linux/input-event-codes.h
```
