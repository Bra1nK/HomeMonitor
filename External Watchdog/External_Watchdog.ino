//Watchdog
#define ResetDetect 8       // watchdog detect pin, HIGH if a watchdog reset has occured
#define heartbeat 9         // heartbeat pin
int pulseState = LOW;       // pulseState used to set the heartbeat pin
long lastbeat = 0;          // will store last time the heartbeat pin was updated
long HeartBeatFreq = 500;   // interval at which to blink (milliseconds)
boolean ResetHappened;      // Set to true if a watchdog reset has occurred

void setup(){
  pinMode(ResetDetect, INPUT);    // set Watchdog Reset sensing pin as input
  digitalWrite(ResetDetect, HIGH);// and turn on pullup  
  pinMode(heartbeat, OUTPUT);     // set the heartbeat pin as output:
}

void loop(){
    // Check if Restarting after Watchdog Reset
    // NB must come before heartbeat resets external counter
    int ResetSet = digitalRead(ResetDetect);
    if (ResetSet == HIGH){
      ResetHappened = true;
    }
    else {
      ResetHappened = false;
    }
  
  // Heartbeat resets external watchdog when pin goes high
  if ((long)( millis() - (lastbeat + HeartBeatFreq)) >= 0) {    
    lastbeat = millis();    
    // if the LED is off turn it on and vice-versa:
    if (pulseState == LOW)
      pulseState = HIGH;
    else
      pulseState = LOW;
    // set the LED with the ledState of the variable:
    digitalWrite(heartbeat, pulseState);
  }
}
