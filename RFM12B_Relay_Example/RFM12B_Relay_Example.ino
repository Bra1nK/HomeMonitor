//----------------------------------------------------------------------------------------------------------------------
// Simple RFM12B Relaying Example
// Relays from one Network Group to another maintaining the Node ID
// By Brian Kelly. 
// 14th January 2014
// Avoid using any code with interrupts (e.g powersaving watchdogs)
// as it seems to cause resets and the setup code to execute again
// Power Saving doesn't really make sense for a Relay as it needs to be listening all the time
// and I would expect it to be running from a mains PSU or similar.
//----------------------------------------------------------------------------------------------------------------------

#include <JeeLib.h> // https://github.com/jcw/jeelib

// Fixed RF12 Settings
#define freq RF12_868MHZ     //frequency

// RF12 Receive settings
#define MYNODE 30            //node ID of the receiver
#define group 211            //network group to relay

//RF12 Transmit settings
#define Tgroup 210        // Network group for retransmission
#define USE_ACK           // Enable ACKs, comment out to disable
#define RETRY_PERIOD 2    // How soon to retry (in seconds) if ACK didn't come in
#define RETRY_LIMIT 5     // Maximum number of times to retry
#define ACK_TIME 10       // Number of milliseconds to wait for an ack

 byte rx[66];    // Buffer for received data (max size is 66 Bytes)

 int nodeID;    //node ID of tx, extracted from RF datapacket. Not transmitted as part of Payload structure

// Wait a few milliseconds for proper ACK
 #ifdef USE_ACK
  static byte waitForAck() {
   MilliTimer ackTimer;
   while (!ackTimer.poll(ACK_TIME)) {
     if (rf12_recvDone() && rf12_crc == 0 &&
        rf12_hdr == (RF12_HDR_DST | RF12_HDR_CTL | nodeID))
        return 1;
     }
   return 0;
  }
 #endif

//--------------------------------------------------------------------------------------------------
// Send payload data via RF
//-------------------------------------------------------------------------------------------------
// length is size of received data just transmit that part of the buffer

 static void rfwrite(byte length){
  #ifdef USE_ACK
   for (byte i = 0; i <= RETRY_LIMIT; ++i) {  // tx and wait for ack up to RETRY_LIMIT times
      while (!rf12_canSend())
      rf12_recvDone();
      rf12_sendStart(RF12_HDR_ACK, &rx, length, 1); //4th parameter is wait for completion
      byte acked = waitForAck();  // Wait for ACK
      if (acked) { return; }      // Return if ACK received  
   delay(RETRY_PERIOD * 1000);    // If no ack received wait and try again
   }
  #else
     while (!rf12_canSend())
     rf12_recvDone();
     rf12_sendStart(0, &rx, length, 1); //4th parameter is wait for completion
     return;
  #endif
 }

void setup () {
 
 rf12_initialize(MYNODE, freq,group); // Initialise the RFM12B

}

void loop() {

 if (rf12_recvDone() && rf12_crc == 0 && (rf12_hdr & RF12_HDR_CTL) == 0) {
  byte hdr = rf12_hdr, len = rf12_len;
  nodeID = hdr & 0x1F;  // get node ID

  memcpy(rx, (void*) rf12_data, len);
   
   if (RF12_WANTS_ACK) {                  // Send ACK if requested
     rf12_sendStart(RF12_ACK_REPLY, 0, 0, 1); //4th parameter is wait for completion
   }   

   rf12_initialize(nodeID, freq, Tgroup); // Initialise the RFM12B for relaying
   rfwrite(len); // Send received data via RF
   rf12_initialize(MYNODE, freq, group); // Initialise the RFM12B for receiving again
 }
}
