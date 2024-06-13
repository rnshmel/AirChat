# AirChat

Author and original creator: Richard Shmel

AirChat is the project name for a low-cost, easily exploitable/reversible, RF transceiver. It is used as a capstone exercise for my Software Defined Radio 101 (SDR 101) course I am teaching at DEF CON Trainings.

##### TODO
Firmware is not optimized due to time constraints, and I would still call it a “beta” release. Refactor to use a more state-machine and interrupt-driven solution.

##### BUGS  
Carrier sense collision avoidance (CSCA) will fail to detect a transmission until the preamble is finished. OOK modulation has cross talk issue at close range (1-2 meters).

# Overview

Project consists of 3 main components:
* custom PCB using a STM32 and a TI CC1101
* Firmware for the STM32 (C)
* Client software to interface with the board (Python)

System has two modulation types: 2FSK at 9600 baud and OOK at 4800 baud and is designed to transmit only ASCII characters as a simple “chat program”. Operates in the 915 ISM band with 16 separate channels, 8 for FSK and 8 for OOK. Interfacing with the chipset is done via UART and the USB-to-TTL chip on the PCB. Air Chat has no encryption, checksums, or nonces that would ensure data integrity or attribution; it is intentionally vulnerable as a learning tool for students.

# Hardware

### Schematic 
PCB schematic can be found in the *hardware* folder. Key sections are:
* STM32-G03 microcontroller. Communicates with the client via USART 1 and with the RF chip via SPI 1. Has two LED outputs for TX and RX. Has two GPIO inputs from the RF chip which are used as signals to show specific information.
* Texas Instruments CC1101 RF Transceiver. Uses SPI to send/receive data from the STM32. Uses an SMA antenna to send/receive RF packets.
* LM1117 linear voltage regulator. Converts 5VDC from USB to 3.3VDC.
*CP2102 USB-to-TTL: Converts USB data to UART data. Original version omitted this and used a special cable. Newest hardware iteration adds this chip to the PCB.
* 915 MHz impedance matching network. PCB is configured to use the 915 ISM band. While the CC1101 can use other bands, the firmware for AirChat only allows 915 MHz.

![Image](images/STM32_pins.PNG)  
*STM32 pin diagram from CubeMX*

### PCB
PCB design was done in both KiCAD and EasyEDA. Production files (Gerber files, materials, and pick-and-place) are included in this repo. Top and bottom copper layers are located in the images folder. PCB measure 60mm by 80mm and has three size M3 screw mounting holes. Antenna attachment is SMA-F, so any 915 MHz SMA-M antenna will work. USB cable is a standard USB-A port.
![Image](images/PCB_1.jpg)  
*production version of the PCB*
![Image](images/PCB_2.jpg)  
*PCB next to antenna and USB cable*
