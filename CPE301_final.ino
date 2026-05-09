// CPE 301 Final Project Rough Draft
// Arduino Mega 2560 Thermistor Temperature Regulator
//
// Pin setup:
// A0  = thermistor voltage divider signal
// D2  = start button with interrupt
// D3  = reset button
// D4  = off button
// D8  = RGB red
// D9  = RGB green
// D10 = RGB blue
// D11 = L293D motor enable
//
// Button wiring:
// D2 ---- start button ---- GND
// D3 ---- reset button ---- GND
// D4 ---- off button ---- GND
//
// Restrictions followed:
// No pinMode()
// No digitalRead()
// No digitalWrite()
// No delay()
// No analogRead()
// No Serial library functions
//
// States:
// OFF     = RGB off, motor off
// IDLE    = green LED, motor off
// COOLING = blue LED, motor on
// RESET   = red LED briefly, motor off, then OFF
//
// Serial Monitor updates every 1 minute using millis().

#include <math.h>

#define TBE 0x20

// UART registers
volatile unsigned char *myUCSR0A = (unsigned char *)0x00C0;
volatile unsigned char *myUCSR0B = (unsigned char *)0x00C1;
volatile unsigned char *myUCSR0C = (unsigned char *)0x00C2;
volatile unsigned int  *myUBRR0  = (unsigned int  *)0x00C4;
volatile unsigned char *myUDR0   = (unsigned char *)0x00C6;

// ADC registers
volatile unsigned char *my_ADMUX    = (unsigned char *)0x7C;
volatile unsigned char *my_ADCSRB   = (unsigned char *)0x7B;
volatile unsigned char *my_ADCSRA   = (unsigned char *)0x7A;
volatile unsigned int  *my_ADC_DATA = (unsigned int  *)0x78;

// Arduino Mega 2560 port registers
volatile unsigned char *pinB  = (unsigned char *)0x23;
volatile unsigned char *ddrB  = (unsigned char *)0x24;
volatile unsigned char *portB = (unsigned char *)0x25;

volatile unsigned char *pinE  = (unsigned char *)0x2C;
volatile unsigned char *ddrE  = (unsigned char *)0x2D;
volatile unsigned char *portE = (unsigned char *)0x2E;

volatile unsigned char *pinG  = (unsigned char *)0x32;
volatile unsigned char *ddrG  = (unsigned char *)0x33;
volatile unsigned char *portG = (unsigned char *)0x34;

volatile unsigned char *pinH  = (unsigned char *)0x100;
volatile unsigned char *ddrH  = (unsigned char *)0x101;
volatile unsigned char *portH = (unsigned char *)0x102;

// Arduino Mega pin-to-port mapping:
// D8  = PH5 = RGB red
// D9  = PH6 = RGB green
// D10 = PB4 = RGB blue
// D11 = PB5 = L293D motor enable
// D2  = PE4 = start button interrupt
// D3  = PE5 = reset button
// D4  = PG5 = off button
// A0  = ADC0 = thermistor voltage divider signal

const float TRIGGER_TEMP_F = 75.0;

// Thermistor constants for common 10k NTC thermistor
const float SERIES_RESISTOR = 10000.0;
const float THERMISTOR_NOMINAL = 10000.0;
const float TEMP_NOMINAL_C = 25.0;
const float B_COEFFICIENT = 3950.0;

enum State {
  OFF_STATE,
  IDLE_STATE,
  COOLING_STATE,
  RESET_STATE
};

volatile bool startInterruptFlag = false;

State currentState = OFF_STATE;

float temperatureF = 0.0;

unsigned long lastLogTime = 0;
const unsigned long logInterval = 60000; // 1 minute

unsigned long lastStartTime = 0;
unsigned long lastResetTime = 0;
unsigned long lastOffTime = 0;
const unsigned long debounceTime = 250;

unsigned long resetStartTime = 0;
const unsigned long resetDisplayTime = 1000; // red LED for 1 second

void setup()
{
  U0init(9600);
  adc_init();

  // D8 red and D9 green as outputs
  *ddrH |= (1 << 5); // D8 red
  *ddrH |= (1 << 6); // D9 green

  // D10 blue and D11 motor enable as outputs
  *ddrB |= (1 << 4); // D10 blue
  *ddrB |= (1 << 5); // D11 motor enable

  // D2 start button as input
  *ddrE &= ~(1 << 4);

  // D3 reset button as input
  *ddrE &= ~(1 << 5);

  // D4 off button as input
  *ddrG &= ~(1 << 5);

  // Enable internal pull-up resistors for buttons
  *portE |= (1 << 4); // D2 start pull-up
  *portE |= (1 << 5); // D3 reset pull-up
  *portG |= (1 << 5); // D4 off pull-up

  // On Arduino Mega 2560, interrupt 0 is digital pin D2
  attachInterrupt(0, startISR, FALLING);

  setOffStateOutputs();

  U0putstr("Mega thermistor temperature regulator initialized.\n\r");
}

void loop()
{
  bool resetPressed = !(*pinE & (1 << 5)); // D3 active LOW
  bool offPressed   = !(*pinG & (1 << 5)); // D4 active LOW

  if (startInterruptFlag) {
    startInterruptFlag = false;

    if (millis() - lastStartTime > debounceTime) {
      lastStartTime = millis();

      if (currentState == OFF_STATE) {
        currentState = IDLE_STATE;
        U0putstr("Start button pressed. System ON.\n\r");
      }
    }
  }

  if (resetPressed && millis() - lastResetTime > debounceTime) {
    lastResetTime = millis();
    currentState = RESET_STATE;
    resetStartTime = millis();
    U0putstr("Reset button pressed.\n\r");
  }

  if (offPressed && millis() - lastOffTime > debounceTime) {
    lastOffTime = millis();
    currentState = OFF_STATE;
    U0putstr("Off button pressed. System OFF.\n\r");
  }

  temperatureF = readThermistorF();

  switch (currentState)
  {
    case OFF_STATE:
      setOffStateOutputs();
      break;

    case IDLE_STATE:
      setIdleStateOutputs();

      if (temperatureF > TRIGGER_TEMP_F) {
        currentState = COOLING_STATE;
      }
      break;

    case COOLING_STATE:
      setCoolingStateOutputs();

      if (temperatureF <= TRIGGER_TEMP_F) {
        currentState = IDLE_STATE;
      }
      break;

    case RESET_STATE:
      setResetStateOutputs();

      if (millis() - resetStartTime >= resetDisplayTime) {
        currentState = OFF_STATE;
      }
      break;
  }

  if (millis() - lastLogTime >= logInterval) {
    lastLogTime = millis();
    printStatus();
  }
}

void startISR()
{
  startInterruptFlag = true;
}

void adc_init()
{
  // Enable ADC
  *my_ADCSRA |= 0x80;

  // ADC prescaler = 128
  *my_ADCSRA |= 0x07;

  // AVCC reference, ADC0 selected
  *my_ADMUX = 0x40;

  // Free running mode off, MUX5 cleared
  *my_ADCSRB &= 0xF8;
}

unsigned int adc_read(unsigned char channel)
{
  // Keep reference bits and clear channel bits
  *my_ADMUX &= 0xE0;

  // Select ADC channel
  *my_ADMUX |= (channel & 0x1F);

  // Clear MUX5 for ADC0-ADC7
  *my_ADCSRB &= ~(1 << 3);

  // Start conversion
  *my_ADCSRA |= 0x40;

  // Wait until conversion is complete
  while (*my_ADCSRA & 0x40);

  return *my_ADC_DATA;
}

float readThermistorF()
{
  unsigned int adcValue = adc_read(0);

  if (adcValue <= 0) {
    adcValue = 1;
  }

  if (adcValue >= 1023) {
    adcValue = 1022;
  }

  // This equation assumes:
  // 5V -> thermistor -> A0 -> 10k fixed resistor -> GND
  float resistance = SERIES_RESISTOR * ((1023.0 / adcValue) - 1.0);

  float steinhart;
  steinhart = resistance / THERMISTOR_NOMINAL;
  steinhart = log(steinhart);
  steinhart /= B_COEFFICIENT;
  steinhart += 1.0 / (TEMP_NOMINAL_C + 273.15);
  steinhart = 1.0 / steinhart;
  steinhart -= 273.15;

  float tempC = steinhart;
  float tempF = (tempC * 9.0 / 5.0) + 32.0;

  return tempF;
}

void setOffStateOutputs()
{
  // OFF = RGB off, motor off
  *portH &= ~(1 << 5); // red off
  *portH &= ~(1 << 6); // green off
  *portB &= ~(1 << 4); // blue off
  *portB &= ~(1 << 5); // motor off
}

void setIdleStateOutputs()
{
  // IDLE = temperature <= 75 F
  // Green LED on, motor off
  *portH &= ~(1 << 5); // red off
  *portH |=  (1 << 6); // green on
  *portB &= ~(1 << 4); // blue off
  *portB &= ~(1 << 5); // motor off
}

void setCoolingStateOutputs()
{
  // COOLING = temperature > 75 F
  // Blue LED on, motor on
  *portH &= ~(1 << 5); // red off
  *portH &= ~(1 << 6); // green off
  *portB |=  (1 << 4); // blue on
  *portB |=  (1 << 5); // motor on
}

void setResetStateOutputs()
{
  // RESET = red LED on, motor off
  *portH |=  (1 << 5); // red on
  *portH &= ~(1 << 6); // green off
  *portB &= ~(1 << 4); // blue off
  *portB &= ~(1 << 5); // motor off
}

void printStatus()
{
  U0putstr("Temperature: ");
  U0printFloat(temperatureF);
  U0putstr(" F | System State: ");

  if (currentState == OFF_STATE) {
    U0putstr("OFF");
    U0putstr(" | Fan State: IDLE");
  }
  else if (currentState == IDLE_STATE) {
    U0putstr("IDLE");
    U0putstr(" | Fan State: IDLE");
  }
  else if (currentState == COOLING_STATE) {
    U0putstr("COOLING");
    U0putstr(" | Fan State: COOLING");
  }
  else if (currentState == RESET_STATE) {
    U0putstr("RESET");
    U0putstr(" | Fan State: IDLE");
  }

  U0putstr("\n\r");
}

void U0init(unsigned long baud)
{
  unsigned long FCPU = 16000000;
  unsigned int tbaud = (FCPU / 16 / baud - 1);

  *myUCSR0A = 0x20;
  *myUCSR0B = 0x18;
  *myUCSR0C = 0x06;
  *myUBRR0 = tbaud;
}

void U0putchar(unsigned char data)
{
  while (!(*myUCSR0A & TBE));
  *myUDR0 = data;
}

void U0putstr(const char *str)
{
  while (*str) {
    U0putchar(*str++);
  }
}

void U0printNumber(int num)
{
  char buffer[10];
  int i = 0;

  if (num == 0) {
    U0putchar('0');
    return;
  }

  if (num < 0) {
    U0putchar('-');
    num = -num;
  }

  while (num > 0) {
    buffer[i++] = (num % 10) + '0';
    num /= 10;
  }

  while (i > 0) {
    U0putchar(buffer[--i]);
  }
}

void U0printFloat(float value)
{
  int whole = (int)value;
  int decimal = (int)((value - whole) * 10);

  if (decimal < 0) {
    decimal = -decimal;
  }

  U0printNumber(whole);
  U0putchar('.');
  U0printNumber(decimal);
}