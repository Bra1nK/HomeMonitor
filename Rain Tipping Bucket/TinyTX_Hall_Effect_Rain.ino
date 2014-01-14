//----------------------------------------------------------------------------------------------------------------------
// TinyTX_Hall_Effect_Rain - An ATtiny84 Rainfall Monitoring Node
//
// Sends whenever there is a new pulse or when a timeout occurs
//
// Using an A3214EUA-T or A3213EUA-T Hall Effect Sensor
// Power (pin 1) of sensor to D9, output (pin 3) to D10, 0.1uF capactior across power & ground
//
//
// Requires Arduino IDE with arduino-tiny core: http://code.google.com/p/arduino-tiny/
//----------------------------------------------------------------------------------------------------------------------

#include <JeeLib.h> // https://github.com/jcw/jeelib
#include <PinChangeInterrupt.h> // http://code.google.com/p/arduino-tiny/downloads/list
#include <avr/sleep.h>
#include <avr/wdt.h>

#define myNodeID 1        // RF12 node ID in the range 1-30
#define network 210       // RF12 Network group
#define freq RF12_868MHZ  // Frequency of RFM12B module

#define USE_ACK           // Enable ACKs, comment out to disable
#define RETRY_PERIOD 5    // How soon to retry (in seconds) if ACK didn't come in
#define RETRY_LIMIT 5     // Maximum number of times to retry
#define ACK_TIME 10       // Number of milliseconds to wait for an ack

#define powerPin 9        // Sensors power pin is on D9 (ATtiny pin 12)
#define sensorPin 10      // Sensors data pin is on D10 (ATtiny pin 13)

//Delays
int TimeOut = 415;            // how many Watchdog Timeouts to wait ( x8 secs approx + loop time) e.g 450 = 1hr

// These are modified in the interrupt
volatile int pulse_seen = 0;   // Pulse was seen in interrupt
volatile int WDEventCnt = 0;   // Count of Watchdog Events used for longer delays
volatile int timer_int = 1;    // Timer timed out, run timeout code. 
                               // Set to 1 so that an initial voltage reading is provided at switch on


//--------------------------------------------------------------------------------------------------
//Data Structure to be sent
//--------------------------------------------------------------------------------------------------

 typedef struct {
  	  int rain;	// Rainfall
          int supplyV;	// Supply voltage
 } Payload;

 Payload tinytx;
 
//--------------------------------------------------------------------------------------------------
// Wait for an ACK
//--------------------------------------------------------------------------------------------------
 #ifdef USE_ACK
  static byte waitForAck() {
   MilliTimer ackTimer;
   while (!ackTimer.poll(ACK_TIME)) {
     if (rf12_recvDone() && rf12_crc == 0 &&
        rf12_hdr == (RF12_HDR_DST | RF12_HDR_CTL | myNodeID))
        return 1;
     }
   return 0;
  }
 #endif

//--------------------------------------------------------------------------------------------------
// Send payload data via RF
//-------------------------------------------------------------------------------------------------
 static void rfwrite(){
  #ifdef USE_ACK
   for (byte i = 0; i <= RETRY_LIMIT; ++i) {  // tx and wait for ack up to RETRY_LIMIT times
     rf12_sleep(-1);              // Wake up RF module
      while (!rf12_canSend())
      rf12_recvDone();
      rf12_sendStart(RF12_HDR_ACK, &tinytx, sizeof tinytx); 
      rf12_sendWait(2);           // Wait for RF to finish sending while in standby mode
      byte acked = waitForAck();  // Wait for ACK
      rf12_sleep(0);              // Put RF module to sleep
      if (acked) { return; }      // Return if ACK received
      
   Sleepy::loseSomeTime(RETRY_PERIOD * 1000);     // If no ack received wait and try again
   }
  #else
     rf12_sleep(-1);              // Wake up RF module
     while (!rf12_canSend())
     rf12_recvDone();
     rf12_sendStart(0, &tinytx, sizeof tinytx); 
     rf12_sendWait(2);            // Wait for RF to finish sending while in standby mode
     rf12_sleep(0);               // Put RF module to sleep
     return;
  #endif
 }

//--------------------------------------------------------------------------------------------------
// Read current supply voltage
//--------------------------------------------------------------------------------------------------
 long readVcc() {
   bitClear(PRR, PRADC); ADCSRA |= bit(ADEN); // Enable the ADC
   long result;
   // Read 1.1V reference against Vcc
   #if defined(__AVR_ATtiny84__) 
    ADMUX = _BV(MUX5) | _BV(MUX0); // For ATtiny84
   #else
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);  // For ATmega328
   #endif 
   delay(2); // Wait for Vref to settle
   ADCSRA |= _BV(ADSC); // Convert
   while (bit_is_set(ADCSRA,ADSC));
   result = ADCL;
   result |= ADCH<<8;
   result = 1126400L / result; // Back-calculate Vcc in mV
   ADCSRA &= ~ bit(ADEN); bitSet(PRR, PRADC); // Disable the ADC to save power
   return result;
} 
  
    
//########################################################################################################################

void setup() {
  
  rf12_initialize(myNodeID,freq,network); // Initialize RFM12 with settings defined above 
  rf12_sleep(0);                          // Put the RFM12 to sleep

  pinMode(powerPin, OUTPUT);     // set power pin as output
  digitalWrite(powerPin, HIGH);  // and set high

  pinMode(sensorPin, INPUT);     // set sensor pin as input
  digitalWrite(sensorPin, HIGH); // and turn on pullup
  
  attachPcInterrupt(sensorPin,RainPulse,FALLING); // attach a PinChange Interrupt on the falling edge
  
  PRR = bit(PRTIM1); // only keep timer 0 going
  
  ADCSRA &= ~ bit(ADEN); bitSet(PRR, PRADC); // Disable the ADC to save power

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);         // Set sleep mode
  sleep_mode();                                // Sleep now
}

// watchdog timer interrupt
ISR (WDT_vect) 
{
 //   wdt_disable();  // disable watchdog
   ++WDEventCnt;   // increment counter
   if (WDEventCnt >= TimeOut){
   timer_int = 1;}
}  // end of WDT_vect

void RainPulse(){
   ++pulse_seen;  // loop() sees pulse_seen >= 1 and takes action
}

void loop() {
 // Pulse interrupt happened
 if (pulse_seen >= 1) {
   // Something woke us up so reset wake up event counter
   WDEventCnt = 0;
   SendData(pulse_seen);
   pulse_seen = 0;
 }
 
    // Watchdog Timeout Interrupt happened
 if (timer_int == 1) {
   SendData(0);  // Not A Rain Pulse so Rain = 0
   timer_int = 0;
  // Something woke us up so reset wake up event counter
   WDEventCnt = 0;}
  
    // disable ADC
  ADCSRA = 0; 
  // clear various "reset" flags
  MCUSR = 0;     
  // allow changes, disable reset
  WDTCSR = bit (WDCE) | bit (WDE);
  // set interrupt mode and an interval 
  WDTCSR = bit (WDIE) | bit (WDP3) | bit (WDP0);    // set WDIE, and 8 seconds delay
  wdt_reset();  // pat the dog
   
  set_sleep_mode(SLEEP_MODE_PWR_DOWN); // Set sleep mode
  sleep_mode(); // Sleep now

  // cancel sleep as a precaution
  sleep_disable();  //This will be first line executed after wake up event
}

void SendData(int Rain){
    tinytx.rain = Rain;
    tinytx.supplyV = readVcc(); // Get supply voltage
    rfwrite(); // Send data via RF   
    Sleepy::loseSomeTime(1000); 
}

