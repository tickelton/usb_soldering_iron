PRJNAME    = Soldering
DEVICE     = attiny85
CLOCK      = 16500000
PROGRAMMER = arduino
TTY        = /dev/ttyUSB0
BAUDRATE   = 19200
USBDRV     = v-usb/usbdrv
SRC        = src

AVRDUDE = avrdude -c $(PROGRAMMER) -p $(DEVICE) -b $(BAUDRATE) -P $(TTY)
COMPILE = avr-gcc -Wall -Os -I$(USBDRV) -I$(SRC) -DF_CPU=$(CLOCK) -mmcu=$(DEVICE)

USBDRV_OBJECTS = usbdrv/usbdrv.o usbdrv/usbdrvasm.o usbdrv/oddebug.o
OBJECTS = $(USBDRV_OBJECTS) $(patsubst %.c,%.o,$(wildcard $(SRC)/*.c))

all: $(SRC)/$(PRJNAME).hex

.c.o:
	$(COMPILE) -c $< -o $@

.S.o:
	$(COMPILE) -x assembler-with-cpp -c $< -o $@

.c.s:
	$(COMPILE) -S $< -o $@

flash: all
	$(AVRDUDE) -U flash:w:$(SRC)/$(PRJNAME).hex:i

load: all
	sudo micronucleus --run $(SRC)/$(PRJNAME).hex

readcal:
	$(AVRDUDE) -U calibration:r:/dev/stdout:i | head -1

clean:
	rm -rf $(SRC)/$(PRJNAME).hex $(SRC)/$(PRJNAME).elf $(OBJECTS) usbdrv

usbdrv:
	cp -r $(USBDRV) usbdrv

$(SRC)/$(PRJNAME).elf: $(OBJECTS)
	$(COMPILE) -o $(SRC)/$(PRJNAME).elf $(OBJECTS)

$(SRC)/$(PRJNAME).hex: usbdrv $(SRC)/$(PRJNAME).elf
	rm -f $(SRC)/$(PRJNAME).hex
	avr-objcopy -j .text -j .data -O ihex $(SRC)/$(PRJNAME).elf $(SRC)/$(PRJNAME).hex
	avr-size --format=avr --mcu=$(DEVICE) $(SRC)/$(PRJNAME).elf

disasm: $(SRC)/$(PRJNAME).elf
	avr-objdump -d $(SRC)/$(PRJNAME).elf

bootloader: config/micronucleus/firmware micronucleus/firmware/
	@cp -r config/micronucleus/firmware/* micronucleus/firmware/
	@cd micronucleus/firmware && \
		make CONFIG=usb_soldering_iron

micronucleus: micronucleus/commandline/
	@cd micronucleus/commandline && make


