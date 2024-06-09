MCU=attiny3227
F_CPU=5000000UL

ifneq (, $(filter $(F_CPU), 20000000UL 10000000UL 5000000UL))
	CFUSE = fuse2:w:0x02:m
endif
ifneq (, $(filter $(F_CPU), 16000000UL 8000000UL 4000000UL 2000000UL 1000000UL))
	CFUSE = fuse2:w:0x01:m
endif

clean: main.elf
	rm main.elf

build: main.c
	avr-gcc main.c -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os -o main.elf

flash: main.elf
	avrdude -c serialupdi -P /dev/tty.usbserial-* -p $(MCU) -U $(CFUSE) -U flash:w:main.elf

all: build flash

