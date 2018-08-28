#!/bin/bash

set -e

if [ ! -f "/usr/include/libudev.h" ]; then
    apt-get install libudev-dev
fi

if [ ! -d "host" ]; then
    echo "Run this script in the folder containing build.sh" >&2
    exit 1
fi

if [ ! -d "build" ]; then
    mkdir build
fi
cd build
cmake ../host
make

cat << EOF > /etc/systemd/system/zynthian-stm32controller-host.service
[Unit]
Description=I/O expander for Zynthian using USB connected STM32 microcontroller

[Service]
Environment=HOME=/root
WorkingDirectory=/root
ExecStart=/zynthian/zynthian-stm32controller/build/zynthian-stm32controller-host
Restart=always
RestartSec=1

[Install]
WantedBy=multi-user.target

EOF
systemctl start zynthian-stm32controller-host.service
systemctl enable zynthian-stm32controller-host.service
