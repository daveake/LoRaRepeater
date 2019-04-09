This is a LoRa repeater that repeats any LoRa HAB packet that it receives.  The receive and transmit parameters (frequency, mode etc.) are programmable via serial/USB and are stored in EEPROM; a companion application is available to make this easy and is available for Windows.

Hardware
========

Connections are:

Arduino 0 (Tx) - Serial for configuration
Arduino 1 (Rx) - Serial for configuration

Arduino  4 - LoRa RST
Arduino  7 - LoRa DIO0
Arduino  8 - LoRa NSS
Arduino 11 - LoRa MOSI
Arduino 12 - LoRa MISO
Arduino 13 - LoRa SCK

Serial Protocol
===============

The serial connection is 57600 baud and will send status information and incoming LoRa packets without being polled.  Each of these is of the form

something = value <CR\><LF\>

The "something"s are:

	- CurrentRSSI=<RSSI>
	- Message=<telemetry>
	- Hex=<hex packet e.g. SSDV>
	- FreqErr=<error in kHz>
	- PacketRSSI=<RSSI>
	- PacketSNR=<SNR>

The serial interface accepts a few commands, each of the form

~ Command [value] <CR\>

(a trailing <LF\> can be sent but is ignored).  Accepted commands are responded to with an OK (* <CR\> <LF\>) and rejected commands (unknown command, or invalid command value) with a WTF (? <CR\> <LF\>)

Commands that set radio parameters are in upper case for the receiver or lower case for the transmitter.  e.g. ~F434.250 sets the receive frequency to 434.250, and ~f434/450 sets the transmit frequency to 434.450.

The radio commands (upper case for receiver, lower case for transmitter) are:

	- F<frequency in MHz>
	- B<bandwidth>
	- E<error coding from 5 to 8>
	- S<spreading factor from 6 to 11>
	- I<1=implicit mode, 0=explicit mode>
	- L(low data rate optimisation: 1=enable, 0=disable)

The other configuration commands are:

	- P<PPM>
	- R<1=repeating on, 0=repeating off>
	
Finally, "~!" stores the current settings in EEPROM, "~^" resets all settings to those in EEPROM, and then lists those settings, "~*" resets all settings to defaults, stores those in EEPROM and lists them, and "~?" lists the current values of all settings.

Bandwidth value strings can be 7K8, 10K4, 15K6, 20K8, 31K25, 41K7, 62K5, 125K, 250K, 500K

History
=======
				
09/04/2019	V1.0.1	- Updated README only

05/04/2019	V1.0.1	- First version

