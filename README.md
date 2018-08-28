# zynthian-stm32controller
I/O expander for Zynthian using USB connected STM32 microcontroller

# Install host software
Connect via SSH to your zythian box and enter:

```
cd /zynthian
git clone --recursive https://github.com/joostn/zynthian-stm32controller.git
cd zynthian-stm32controller
./build.sh
```

This will build the host software and install as /etc/systemd/system/zynthian-stm32controller-host.service.
