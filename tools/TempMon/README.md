TempMon
=======

A tiny temperature monitor program which in responce to a certain temperature
value in degrees C can clear or set output GPIO pins.

The pins are specified using wiringPi numbers (execute `gpio readall` to
obtain them).
During initialisation all of the specified pins will be set in OUTPUT mode,
having their state cleared (low/false/0).

The temperature is read from a file in millidegrees C resolution as an
integer value, and later gets converted to a floating point one in degrees.

For configuration example refer to the configuration template file
'config.json.template'. Then create a copy of it called 'config.json' with your
actual settings, which will be used with TempMon.


How to Compile
--------------

`make`

How to Install as a System Service
---------------------------------

Create your config.json file in the current directory. Then execute:

`sudo make install`

To start the service execute: `sudo service tempmon start`

To enable the service at system's startup: `sudo systemctl enable tempmon.service`

How to Uninstall the System Service
-----------------------------------

`sudo make -i uninstall`


Sample Applications
------------------

* Smart cooling fan (if combined with a MOSFET)
* Controlling other circuity depending on temperature
* Controlling devices based on pseudo-temperature values
