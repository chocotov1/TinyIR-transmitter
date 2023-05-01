#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>

byte ir_output_pin = 1;

volatile bool button_press_temp;
bool button_pressed;
byte button_pin = 3; // attiny85 pin 2

ISR (PCINT0_vect){
    if (!button_state()){
      button_press_temp = 1;
   }
}

// watchdog interrupt
ISR (WDT_vect){
   wdt_disable();  // disable watchdog
}

void setup(){  
  pinMode(ir_output_pin, OUTPUT);
  digitalWrite(ir_output_pin, LOW);

  pinMode(button_pin, INPUT_PULLUP);
  GIMSK = 1<<PCIE;       // pin change interrupt
  PCMSK = 1<<button_pin; // pin change interrupt

  // custom 36 kHz PWM at 1 MHz system clock
  // datasheet chapters:
  // 12.2 Counter and Compare Units
  // 12.3.1 TCCR1 – Timer/Counter1 Control Register
  
  TCCR1 &= 0xF0;                    // turn off timer clock / reset all prescaler bits (some are set by default)
  //TCCR1 |= (1<<CS12) | (1<<CS11); // prescaler: 32
  //TCCR1 |= (1<<CS12) | (1<<CS10); // prescaler: 16
  //TCCR1 |= (1<<CS12);             // prescaler: 8
  TCCR1 |= (1<<CS10);               // no prescaler

  //12.3.1 TCCR1 – Timer/Counter1 Control Register
  // Table 12-1. Compare Mode Select in PWM Mode
  // 
  //TCCR1 |= (1<<COM1A1);               // OC1x cleared on compare match. Set when TCNT1 = $00. Inverted OC1x not connected.
  //TCCR1 |= (1<<COM1A1) | (1<<COM1A0); // OC1x Set on compare match. Cleared when TCNT1= $00.  Inverted OC1x not connected.
  //
  // PWM output is set in IR_carrier_on() IR_carrier_off()

  // this is how far the counter goes before it starts over again (255 max):
  OCR1C = 26;

  // theoretical frequency 38461 Hz: timer clock source (1 MHz) / prescaler (1) / OCR1C (26)
  // will differ in reality due to no accurate clock source, in my test this setup produced a frequency of 36.7 kHz
  // duty cycle 33%:
  OCR1A = 9;

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
}

void go_to_sleep(){
  // this saved a bit more than 0.2 mA during sleep when I tested:
  ADCSRA &= ~(1<<ADEN);
  // tested current consumption during sleep was 0.2 µA (no watchdog)
  sleep_mode(); // sleep

  // watchdog not used at the moment
  //
  // from the ATTinyCore docs:
  // When using the WDT as a reset source and NOT using a bootloader remember that after reset the WDT will be enabled with minimum timeout.
  // The very first thing your application must do upon restart is reset the WDT (wdt_reset()),  clear WDRF flag in MCUSR (MCUSR&=~(1<<WDRF))
  // and then turn off or configure the WDT for your desired settings. If using the Optiboot bootloader, this is already done for you by the bootloader.
  //
  //wdt_reset();
  //MCUSR&=~(1<<WDRF);
  //wdt_disable(); 
}

bool button_state() {
   return PINB & 1<<button_pin;
}

void IR_carrier_on(){
   TCCR1 |= (1<<COM1A1);                 // OC1x cleared on compare match. Set when TCNT1 = $00. Inverted OC1x not connected.
   //TCCR1 |= (1<<COM1A1) | (1<<COM1A0); // OC1x Set on compare match. Cleared when TCNT1= $00.  Inverted OC1x not connected.
}

void IR_carrier_off(){
   // turn off PWM mode OC1A:
   TCCR1 &= ~(1<<COM1A1 | 1<<COM1A0);
}

void send_philips_rc5(uint16_t ir_code){
   // philips rc5 bits:
   // bit 1-2 : start -> '1', '1'
   // bit 3   : toggle
   // bit 4-8 : adress
   // bit 9-14: command

   for (int8_t i = 13; i >= 0; i--){
      uint16_t mask = 1<<i;

      if (ir_code & mask){
        // bit set
        IR_carrier_off();
        philips_rc5_pause();
        IR_carrier_on();
        philips_rc5_pause();
      } else {
        // bit not set
        IR_carrier_on();
        philips_rc5_pause();
        IR_carrier_off();
        philips_rc5_pause();
      }
   }

   IR_carrier_off();
}

void philips_rc5_pause(){
  delayMicroseconds(815); // roughly results in the required 889 µS burst / low periods
}

void loop() {
  if (button_press_temp){
     // at the moment the button only wakes up the MCU, no other functionality implemented
     button_press_temp = 0;
     delay(40); // primitive debounce
  }


  send_philips_rc5(0b11110100110101);
  go_to_sleep();  
}
