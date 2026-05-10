# CPE301_Final
CPE301 Final project
# CPE 301 Final Project: Temperature Regulator System

## Group Information

Group Name: 6

Team Members:
Evan Schank

## Project Description

This project is an automatic temperature regulator built using an Arduino Mega 2560. The system monitors temperature using a thermistor voltage divider connected to analog pin A0. When the measured temperature rises above 75°F, the system activates a motor through an L293D motor driver to represent a cooling fan. When the temperature is 75°F or lower, the fan remains off.

The system uses an RGB LED to indicate the current state:
- Standby / Monitoring: RGB LED off, fan off
- Idle / Active: green LED on, fan off
- Cooling: blue LED on, fan on
- Reset: red LED on briefly, fan off

The project uses register-level programming for the ADC, digital outputs, and UART serial output. It avoids restricted Arduino functions such as pinMode(), digitalRead(), digitalWrite(), delay(), analogRead(), and Serial library functions.

## Pin Connections

| Component | Arduino Mega Pin |
|---|---|
| Thermistor voltage divider signal | A0 |
| Start button | D2 |
| Reset button | D3 |
| Off button | D4 |
| RGB LED red pin | D8 |
| RGB LED green pin | D9 |
| RGB LED blue pin | D10 |
| motor | D11 |

## Hardware Components

- Arduino Mega 2560
- 10k NTC thermistor
- 10k resistor
- RGB LED
- Three current-limiting resistors for RGB LED
- Three push buttons
- 3.5v motor
- Breadboard
- Jumper wires

## System Behavior

The system is controlled by a state machine with four states:

1. Standby / Monitoring  
   The system monitors temperature, but the RGB LED and fan remain off.

2. Idle / Active  
   The system is on, temperature is 75°F or lower, green LED is on, and the fan is off.

3. Cooling  
   The temperature is above 75°F, blue LED is on, and the motor/fan is activated.

4. Reset  
   The red LED turns on briefly, the fan turns off, and the system returns to standby.

## Controls

- Start button: Starts the system using an interrupt.
- Reset button: Sends the system to the reset state.
- Off button: Turns the system off and returns it to standby.

## Serial Monitor Output

The system prints the temperature, system state, and fan state to the Serial Monitor once every minute using millis() timing.

Example output:

Temperature: 74.8 F | System State: IDLE | Fan State: IDLE
Temperature: 78.2 F | System State: COOLING | Fan State: COOLING
