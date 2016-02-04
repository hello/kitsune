Kitsune
=======

Kitsune is the image that runs on the application processor of the CC3200.

Setup
=====

This project is built with a vendor supplied toolchain that's derived from gcc. To run the debugger requires windows 7 or greater.

* Install code composer studio v6
* Create a workspace and add all the projects in kitsune
* Go to debug configurations and add the cc3200 config located at kitsune/tools/ccs_patch/cc3200.ccxml
* Build all the projects except kitsune
* Build kitsune

Debugger
========

Debug is via SWD JTAG on a FT2232, we simply pull the signals off the launchpad board. Four wire JTAG in a VM isn't stable probably due to timing requirements.

For use as a debugger, remove all the jumpers from the launchpad and connect the TCK, TMX, RX, TX, 5V, and GND. For best results power the FTDI chip off the 1.8V supply on the bottom board (jumper J13, pin is labelled BRD PWR).

After loading code and connecting to the UART the "boot" command is required to start the background tasks, and "led stop" is needed to dismiss the debug cable animation.

Note: In the above configuration 3.3v is supplied on the opposite pin of J13 creating a convenient supply for a pill board.

Architecture
============

Basic building blocks are FreeRTOS, FatFS, Nanopb, and TI's network stack.

Kitsune does not exist in a vacuum. The system is composed internally of two mcu's, the cc3200 on the middle board and the nRF51422 on the top board which is the focus of kodobannin. Beyond this the Pill is running another nRF which communicates via ANT to the top board, the mobile apps on iOS or Android, and the backend server.

This complexity is managed at an interface level using protobuf. The periodic data protobuf contains enviromnental data for each minute, and the log protobuf contains chunks of debug text. The communication between the phone and sense is via the monolithic protobuf morpheus_ble, which internally is composed of many optional fields and a command field which is used to determine the intent and what content to expect. This message is also used in account pairing during which the phone will initiate the process by sending it to the sense, which will annotate it and forward it to the server. All data between the server and Kitsune is transferred using serialized, signed protobuf over HTTP. The signing process is done by computing the SHA of the message and encrypting that SHA with a per-device AES key which is generated on the device during manufacturing in a process referred to as provisioning.

Internally a few important threads exist:
* networktask exists to serialze the accesses to our single secure socket
* fast and slow sampling threads record the sensor data
* a logging thread captures debug output
* a spi thread listens for data coming via the SPI connection to the top board
* a command task handles initialization of many of the other tasks and brings up a uart debug terminal if on a debug cable
* an audio task handles recording and playback

There is also a cross connect UART (CCU) which runs between the nRF on the top board and the cc3200 on the middle board. This exists to enable remote updates of the nRF but also allows for the debug stream to be captured and merged with the debug stream uploaded to the server.

Files to read for rapid familiarity:
* commands.c has the debug command line, new commands should be added to the table near the end. It also contains the fast/slow sampling tasks and alarm logic.
* wifi_cmd.c handles the nuts and bolts of signing and sending messages to the server
* fatfs_cmd.c handles the OTA process
* audio_task.c handles the playback and record buffering
* ble_proto.c handles the phone interface logic
* sys_time.c handles managing the system time (fetching from NTP and interfacing with the RTC)

Further Reading
===============

Datasheets, technical manuals, and current schematics are included in the reference folder.