This is a barebones example for using [ddlsmurf/ESP32-ANCS-AMS-Notifications](https://github.com/ddlsmurf/ESP32-ANCS-AMS-Notifications) on a [Watchy](https://watchy.sqfmi.com/).

There is no effort to make it pretty, nor energy efficient, the main goal was to get the BLE library working.

Requirements:

- [Platformio](https://platformio.org/install)

Instructions:

    git clone or download to some folder
    pio run -t upload && pio device monitor --port /dev/cu.SLAB_USBtoUART # or whatever is your port name
