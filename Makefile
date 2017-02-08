TARGET:=gpio_keyd

CC := $(CROSS_COMPILE)gcc

CFLAGS = -O2 -Wstrict-prototypes -Wmissing-prototypes
SRC := $(wildcard *.c)
OBJ := $(patsubst %.c,%.o,$(SRC))
LIBS = -lwiringPi -lpthread

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

install: all
	sudo install gpio_keyd /usr/bin/
	sudo install gpio_keyd.conf /etc/

uninstall:
	sudo rm /usr/bin/gpio_keyd
	sudo rm /etc/gpio_keyd.conf

clean:
	rm -f $(TARGET) *.o *~
