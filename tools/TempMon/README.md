TempMon
=======

A tiny temperature monitor program which in responce to a certain temperature
value in degrees C can clear or set output GPIO pins.

The pins are specified using wiringPi numbers (execute `gpio readall` to
obtain them).
During initialisation all of the specified pins will be set in OUTPUT mode,
having their state cleared (low/false/0).

The temperature is read from a file in millidegrees C resolution.

For configuration example refer to the configuration template file
'config.json.template'. Then create a copy of it called 'config.json' with your
actual settings, which will be used with TempMon.


How to Compile
--------------

`make`

How to Install as a System Service
---------------------------------

`sudo make install`

To start the service: `sudo service tempmon start`
To enable the service at system startup `sudo systemctl enable tempmon.service`



Sample Applications
------------------

* Smart cooling fan (if combined with MOSFET)
* Controlling other circuity depending on temperature
* Controlling devices based on pseudo-temperature values
