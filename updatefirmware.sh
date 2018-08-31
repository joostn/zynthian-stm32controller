#!/bin/bash

set -e

if [ ! -f "/usr/bin/dfu-util" ]; then
    apt-get install dfu-util
fi

CHOICE=$(dialog --clear --backtitle "Flash firmware" --title "Select firmware" --menu "Select firmware to flash::" 15 40 4 \
1 "For linear potentiometer" \
2 "For logarithmic potentiometer" 2>&1 >/dev/tty)
clear

if [ $CHOICE = "1" ]; then
	FIRMWAREFILE=firmware/zynthian-stm32controller/for_linear_potentiometer.bin
elif [ $CHOICE = "2" ]; then
	FIRMWAREFILE=firmware/zynthian-stm32controller/for_logarithmic_potentiometer.bin
else
	echo "Invalid input: $CHOICE"
	exit 1
fi

if [ ! -f $FIRMWAREFILE ]; then
	echo "File not found: $FIRMWAREFILE"
	exit 1
fi

while true; do
	set +e  # don't abort if error
	dfu-util --reset --device , --alt 2 --download $FIRMWAREFILE
	if [ $? -eq 0 ]
	then
		echo "Success!"
   		break
	fi
	echo "Please connect or reset the microcontroller. Retrying..."
	sleep 0.3
done
