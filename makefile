
upload: compile
	avrdude -P /dev/ttyACM0 -c stk500v2 -p m1284p -v -U flash:w:build/main.hex

size: compile
	avr-size -A -C -x --mcu=atmega1284p build/main.elf

compile:
	-mkdir build
	avr-gcc -mmcu=atmega1284p -Os -o build/main.elf src/main.c -Wl,-Map=build/mem.map 
	avr-objcopy -O ihex build/main.elf build/main.hex

