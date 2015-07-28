hl-stnucleo-mqtt-c
==================

This C sample shows how to use MQTT to communicate with AirVantage.

This source code is based on http://git.eclipse.org/c/paho/org.eclipse.paho.mqtt.embedded-c.git/ sample.

Scenario
--------

This version sends and receives data/command to/from AirVantage. It detects if the ambient light is under a trigger level.  

Configuration
-------------
-In main.cpp:
change with your password (line 223)
change the apn (line 221)
change the server to be connect (line 220)

Build
-----

Import source in the mbed.org compiler. Compile it for a STM32L053 Nucleo. Download the code in the board.

Run
---

-Plug the two boards on the demo kit. 
It will be connected to AirVantage automatically.