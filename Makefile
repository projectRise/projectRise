CC	:= pio

.PHONY: all
all:
	$(CC) run

.PHONY: upload
upload:
	$(CC) run --target upload

.PHONY: config
config:
	# Run this in the terminal before running this target
	# Syntax:	export SP_PORT="<portname>"
	# Windows:	export SP_PORT="\\.\COMn"
	# *NIX:		export SP_PORT="/dev/ttySn"
	@python ./config_device.py "$(SP_PORT)"

.PHONY: test
test:
	$(MAKE) -C test

.PHONY: clean
clean: clean
	$(CC) run --target clean
