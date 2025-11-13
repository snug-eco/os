

upload: compile
	avrdude -P /dev/ttyACM0 -c stk500v2 -p m1284p -v -U flash:w:build/main.hex


compile:
	-mkdir build
	avr-gcc -mmcu=atmega1284p -Os -o build/main.elf src/main.c
	avr-objcopy -O ihex build/main.elf build/main.hex

