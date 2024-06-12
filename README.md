# AirChat

Author and original creator: Richard Shmel

AirChat is the project name for a low-cost, easily exploitable/reversible, RF transceiver as a capstone exercise for my Software Defined Radio 101 (SDR 101) course I am teaching at DEF CON Trainings.

##### TODO
Firmware is not optimized due to time constraints, and I would still call it a “beta” release. Refactor to use a more state-machine and interrupt-driven solution.

##### BUGS  
carrier sense collision avoidance (CSCA) will fail to detect a transmission until the preamble is finished.

# Overview

Project consists of 3 main components:
* custom PCB using a STM32 and a TI CC1101
* Firmware for the STM32 (C)
* Client software to interface with the board (Python)

System has two modulation types: 2FSK at 9600 baud and OOK at 4800 baud and is designed to transmit only ASCII characters as a simple “chat program”. Operates in the 915 ISM band with 16 separate channels, 8 for FSK and 8 for OOK. Interfacing with the chipset is done via UART and the USB-to-TTL chip on the PCB. Air Chat has no encryption, checksums, or nonces that would insure data integrity or attribution; it is intentionally vulnerable as a learning tool for students.
