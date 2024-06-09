/*
 * Hackaday 2024 Business Card Challenge
 * Designed by: Henry Cheung (e-tinkers)
 * Date: 27 May, 2024
 * 
 * Notes:
 *   When running at 3v 5MHz, the battery consumption: 2.5mA when LED is on, 1uA during powerDown mode.
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #define USE_INTERNAL_OSC
// #define AUTO_SHOWTIME

#define LEDS                12

const uint16_t TWELVE_HOUR = 43200;    // 12 * 3600 seconds
const uint16_t SLEEP_INTERVAL = 60;    // 60 seconds
const uint32_t DISPLAY_TIME = 5000;  // 5000ms

// Charlieplexing configuration and state matrix
typedef struct {
  uint8_t pinConfig;
  uint8_t pinState;
} Mux_t;

const Mux_t mux[LEDS] = {
  {0x90, 0x80}, // 0
  {0x30, 0x10}, // 1
  {0x30, 0x20}, // 2
  {0x60, 0x20}, // 3
  {0x60, 0x40}, // 4
  {0xc0, 0x40}, // 5
  {0xc0, 0x80}, // 6
  {0x50, 0x10}, // 7
  {0x50, 0x40}, // 8
  {0xA0, 0x20}, // 9
  {0xA0, 0x80}, // 10
  {0x90, 0x10}  // 11
};

// state machine for LED blinking states
enum States {BEGIN, LED_ON, LED_OFF, END};

volatile uint16_t timeCount = 0;
volatile uint32_t t_millis = 0;

volatile uint32_t displayStart = 0;
volatile uint8_t showTime = 0;

volatile uint8_t sw1Pressed = 0;
volatile uint8_t sw2Pressed = 0;

uint32_t millis() {
    while (TCA0.SINGLE.INTFLAGS & TCA_SINGLE_OVF_bm);
    return t_millis;
}

ISR (RTC_PIT_vect) {
  timeCount = (timeCount + 1) % TWELVE_HOUR; // count up to 12-hour (12 * 3600) = 43200
#ifdef AUTO_SHOWTIME
  if ((timeCount % SLEEP_INTERVAL) == 0) {              // only show time once in every 60 seconds
    showTime = 1;
    displayStart = millis();
  }
#endif
  RTC.PITINTFLAGS = RTC_PI_bm;
}

ISR(TCA0_OVF_vect) {
    t_millis++;
    TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
}

ISR(PORTC_PORT_vect) {
 if (PORTC.INTFLAGS & PORT_INT5_bm) {  // PC5 (SW1) for show time
    PORTC.PIN5CTRL = 0;                // disable trigger
    PORTC.INTFLAGS = PORT_INT5_bm;     // Clear PC5 interrupt flag
    sw1Pressed = 1;
    displayStart = millis();
  }
  if (PORTC.INTFLAGS & PORT_INT4_bm) { // PC4 (SW2) for show time
    PORTC.PIN4CTRL = 0;                // disable trigger
    PORTC.INTFLAGS = PORT_INT4_bm;     // Clear PC4 interrupt flag
    sw2Pressed = 1;
    displayStart = millis();
  }
}

void disableAllPins() {

  // set all pin to input
  PORTA.DIRCLR = PIN0_bm | PIN1_bm | PIN2_bm | PIN3_bm | PIN4_bm | PIN5_bm | PIN6_bm | PIN7_bm;
  PORTB.DIRCLR = PIN0_bm | PIN1_bm | PIN2_bm | PIN3_bm | PIN4_bm | PIN5_bm | PIN6_bm | PIN7_bm;
  PORTC.DIRCLR = PIN0_bm | PIN1_bm | PIN2_bm | PIN3_bm | PIN4_bm | PIN5_bm;

  // disable input buffer to lower power consumption in sleep mode
  PORTA.PIN0CTRL = PORT_ISC_INPUT_DISABLE_gc;
  PORTA.PIN1CTRL = PORT_ISC_INPUT_DISABLE_gc;
  PORTA.PIN2CTRL = PORT_ISC_INPUT_DISABLE_gc;
  PORTA.PIN3CTRL = PORT_ISC_INPUT_DISABLE_gc;
  PORTA.PIN4CTRL = PORT_ISC_INPUT_DISABLE_gc;
  PORTA.PIN5CTRL = PORT_ISC_INPUT_DISABLE_gc;
  PORTA.PIN6CTRL = PORT_ISC_INPUT_DISABLE_gc;
  PORTA.PIN7CTRL = PORT_ISC_INPUT_DISABLE_gc;

  PORTB.PIN0CTRL = PORT_ISC_INPUT_DISABLE_gc;
  PORTB.PIN1CTRL = PORT_ISC_INPUT_DISABLE_gc;
  PORTB.PIN2CTRL = PORT_ISC_INPUT_DISABLE_gc;
  PORTB.PIN3CTRL = PORT_ISC_INPUT_DISABLE_gc;
  PORTB.PIN4CTRL = PORT_ISC_INPUT_DISABLE_gc;
  PORTB.PIN5CTRL = PORT_ISC_INPUT_DISABLE_gc;
  PORTB.PIN6CTRL = PORT_ISC_INPUT_DISABLE_gc;
  PORTB.PIN7CTRL = PORT_ISC_INPUT_DISABLE_gc;

  PORTC.PIN0CTRL = PORT_ISC_INPUT_DISABLE_gc;
  PORTC.PIN1CTRL = PORT_ISC_INPUT_DISABLE_gc;
  PORTC.PIN2CTRL = PORT_ISC_INPUT_DISABLE_gc;
  PORTC.PIN3CTRL = PORT_ISC_INPUT_DISABLE_gc;
  // PORTC.PIN4CTRL = PORT_ISC_INPUT_DISABLE_gc;
  // PORTC.PIN5CTRL = PORT_ISC_INPUT_DISABLE_gc;
}

void turnOnLED(uint8_t led) {

  // enable input buffer
  PORTB.PIN4CTRL = 0;
  PORTB.PIN5CTRL = 0;
  PORTB.PIN6CTRL = 0;
  PORTB.PIN7CTRL = 0;

  // rest all pins to input and set it to low
  PORTB.DIRCLR = (PIN4_bm | PIN5_bm | PIN6_bm | PIN7_bm);
  PORTB.OUTCLR = (PIN4_bm | PIN5_bm | PIN6_bm | PIN7_bm);

  // set pin(s) based on mux.pinConfig and mux.pinState value
  PORTB.DIRSET = mux[led].pinConfig;
  PORTB.OUTSET = mux[led].pinState;

}

void turnOffLED() {

  PORTB.DIRCLR = PIN4_bm | PIN5_bm | PIN6_bm | PIN7_bm;

}

void flashLED(uint8_t theLED, uint8_t flashes) {

  static uint8_t flashState = BEGIN;
  static uint8_t cycle = 0;
  static uint32_t onTimer = 0;
  static uint32_t offTimer = 0;
  static uint32_t intervalTimer = 0;

  switch (flashState) {
    case BEGIN:
      onTimer = millis();
      flashState = LED_ON;
      break;
    case LED_ON:
      {
        turnOnLED(theLED);
        if (flashes == 0) {  // flash once for 450ms On/50ms Off
          if (millis() - onTimer > 450) {
            flashState = LED_OFF;
          }
        }
        else {
          if (millis() - onTimer > 50) {  // flash once for 50ms On
            flashState = LED_OFF;
            offTimer = millis();
          }
        }
      }
      break;
    case LED_OFF:
      {
        turnOffLED();
        if (flashes == 0) {
          if (millis() - offTimer > 50) {
            intervalTimer = millis();
            flashState = END;
          }
        }
        else {
          if (millis() - offTimer > (500UL - 50 * flashes) / flashes) { // Off varies based on number of flashes
            if (++cycle < flashes) {
              flashState = BEGIN;
            }
            else {
              flashState = END;
              intervalTimer = millis();
            }
          }
        }
      }
      break;
    case END:
      if (millis() - intervalTimer > 1000UL) {
        flashState = BEGIN;
        cycle = 0;
      }
      break;
    default:
      break;
  }

}

// Generate a 1ms output for millis()
void configTCA() {
    TCA0.SINGLE.CTRLB = TCA_SINGLE_WGMODE_NORMAL_gc;
    TCA0.SINGLE.PER = 625 - 1;                          // (1ms * F_CPU ) / 8 -1 (i.e. (0.001 * 5000000 / 8) - 1 )
    TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm;
    TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV8_gc | TCA_SINGLE_ENABLE_bm;
}

void configRTC() {

#if defined(USE_INTERNAL_OSC)
  /* Using ULP internal 32.768kHz clcok */
  while (RTC.STATUS > 0);
  RTC.CLKSEL = RTC_CLKSEL_INT32K_gc;                    // 32.768kHz Internal Oscillator
#else
  /* Using external 32.768kHz clock on PB2 and PB3 */
  _PROTECTED_WRITE(CLKCTRL.XOSC32KCTRLA, CLKCTRL_RUNSTDBY_bm | CLKCTRL_ENABLE_bm);
  while (RTC.STATUS > 0);
  RTC.CLKSEL = RTC_CLKSEL_TOSC32K_gc;                   // clock from  XOSC32K (PB2 & PB3) or TSOC1 pin (PB3)
#endif

  RTC.PITCTRLA = RTC_PERIOD_CYC32768_gc | RTC_PITEN_bm; // RTC clock cycles between each interrupt
  RTC.PITINTCTRL = RTC_PI_bm;

}

void configButtons() {
    PORTC.DIRCLR = PIN4_bm;
    PORTC.PIN4CTRL = PORT_PULLUPEN_bm | PORT_ISC_LEVEL_gc;  // Enable PC4(SW2) PULLUP and interrupt trigger on LOW
    PORTC.DIRCLR = PIN5_bm;
    PORTC.PIN5CTRL = PORT_PULLUPEN_bm | PORT_ISC_LEVEL_gc;  // Enable PC5(SW1) PULLUP and interrupt trigger on LOW
}


void configTime() {
    char timeStr[10];
    strcpy(timeStr, __TIME__);
    
    uint16_t h = atoi(strtok(timeStr, ":"));
    uint16_t m = atoi(strtok(NULL, ":"));
    uint16_t s = atoi(strtok(NULL, ":"));
    timeCount = ( h * 3600 +  m * 60 + s ) % TWELVE_HOUR; //round it to 12-hour
}

void testLED() {
    for(int l = 0; l < LEDS; l++) {
        turnOnLED( l );
        _delay_ms( DISPLAY_TIME / LEDS );
    }
}

int main() {

    _PROTECTED_WRITE(CLKCTRL_MCLKCTRLB, (CLKCTRL_PEN_bm | CLKCTRL_PDIV_4X_gc)); // set prescaler to 4 for running at 5MHz
    // _PROTECTED_WRITE(CLKCTRL.MCLKCTRLA, CLKCTRL_CLKOUT_bm);  // output clock on CLKOUT(PB5) for testing

    configTime();
    configRTC();
    configTCA();
    configButtons();
    SLPCTRL.CTRLA |= SLPCTRL_SMODE_PDOWN_gc;    // config sleep controller to PowerDown mode 
    sei();

    while(1) {

      // testLED();

      if (showTime || sw1Pressed || sw2Pressed) {
          cli();
          uint16_t seconds = timeCount;
          sei();

          uint8_t hours = (seconds / 3600) % 12;
          uint8_t minutes = (seconds / 60) % 60;
          uint8_t fiveMinuteInterval = (minutes / 5) % 12;
          uint8_t flashes = minutes % 5;

          turnOnLED(hours);
          flashLED(fiveMinuteInterval, flashes);

          if (millis() - displayStart >= DISPLAY_TIME) {
              turnOffLED();
              if (sw1Pressed) {
                sw1Pressed = 0;
                PORTC.PIN5CTRL = PORT_PULLUPEN_bm | PORT_ISC_LEVEL_gc;
              }
              if (sw2Pressed) {
                sw2Pressed = 0;
                PORTC.PIN4CTRL = PORT_PULLUPEN_bm | PORT_ISC_LEVEL_gc;
              }
              showTime = 0;
              displayStart = millis();
          }
      }
      else {
          // this put MCU in PowerDown mode and consumed only 1uA
          disableAllPins();
          TCA0.SINGLE.CTRLA &= ~TCA_SINGLE_ENABLE_bm; // disable TCA
          SLPCTRL.CTRLA |= SLPCTRL_SEN_bm;            // sleep enable
          __asm("sleep");
          SLPCTRL.CTRLA &= ~SLPCTRL_SEN_bm;           // sleep disable
          __asm("nop");
          TCA0.SINGLE.CTRLA |= TCA_SINGLE_ENABLE_bm;  // re-enable TCA
      }

    }

    return 0;

}
