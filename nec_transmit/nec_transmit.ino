#define IRLEDpin  6              //the arduino pin connected to IR LED to ground. HIGH=LED ON
#define BITtime   562            //length of the carrier bit in microseconds

char text[10];
boolean nec_ok = 0;
byte  i, nec_state = 0, command, inv_command;
unsigned int address;
unsigned long nec_code;
  String code;
  unsigned long val;

void remote_read() {
unsigned int timer_value;
  if(nec_state != 0){
    timer_value = TCNT1;                         // Store Timer1 value
    TCNT1 = 0;                                   // Reset Timer1
  }
  switch(nec_state){
   case 0 :                                      // Start receiving IR data (we're at the beginning of 9ms pulse)
    TCNT1  = 0;                                  // Reset Timer1
    TCCR1B = 2;                                  // Enable Timer1 module with 1/8 prescaler ( 2 ticks every 1 us)
    nec_state = 1;                               // Next state: end of 9ms pulse (start of 4.5ms space)
    i = 0;
    return;
   case 1 :                                      // End of 9ms pulse
    if((timer_value > 19000) || (timer_value < 17000)){         // Invalid interval ==> stop decoding and reset
      nec_state = 0;                             // Reset decoding process
      TCCR1B = 0;                                // Disable Timer1 module
    }
    else
      nec_state = 2;                             // Next state: end of 4.5ms space (start of 562µs pulse)
    return;
   case 2 :                                      // End of 4.5ms space
    if((timer_value > 10000) || (timer_value < 8000)){
      nec_state = 0;                             // Reset decoding process
      TCCR1B = 0;                                // Disable Timer1 module
    }
    else
      nec_state = 3;                             // Next state: end of 562µs pulse (start of 562µs or 1687µs space)
    return;
   case 3 :                                      // End of 562µs pulse
    if((timer_value > 1400) || (timer_value < 800)){           // Invalid interval ==> stop decoding and reset
      TCCR1B = 0;                                // Disable Timer1 module
      nec_state = 0;                             // Reset decoding process
    }
    else
      nec_state = 4;                             // Next state: end of 562µs or 1687µs space
    return;
   case 4 :                                      // End of 562µs or 1687µs space
    if((timer_value > 3600) || (timer_value < 800)){           // Time interval invalid ==> stop decoding
      TCCR1B = 0;                                // Disable Timer1 module
      nec_state = 0;                             // Reset decoding process
      return;
    }
    if( timer_value > 2000)                      // If space width > 1ms (short space)
      bitSet(nec_code, (31 - i));                // Write 1 to bit (31 - i)
    else                                         // If space width < 1ms (long space)
      bitClear(nec_code, (31 - i));              // Write 0 to bit (31 - i)
    i++;
    if(i > 31){                                  // If all bits are received
      nec_ok = 1;                                // Decoding process OK
      detachInterrupt(0);                        // Disable external interrupt (INT0)
      return;
    }
    nec_state = 3;                               // Next state: end of 562µs pulse (start of 562µs or 1687µs space)
  }
}
 
ISR(TIMER1_OVF_vect) {                           // Timer1 interrupt service routine (ISR)
  nec_state = 0;                                 // Reset decoding process
  TCCR1B = 0;                                    // Disable Timer1 module
}



  
void setup()
{
   Serial.begin(9600);
    pinMode(IRLEDpin, OUTPUT);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  TCCR1A = 0;
  TCCR1B = 0;                                    // Disable Timer1 module
  TCNT1  = 0;                                    // Set Timer1 preload value to 0 (reset)
  TIMSK1 = 1;                                    // enable Timer1 overflow interrupt
  attachInterrupt(0, remote_read, CHANGE);  
}


void IRsetup(void)
{
  pinMode(IRLEDpin, OUTPUT);
  digitalWrite(IRLEDpin, LOW);    //turn off IR LED to start
}

// Ouput the 38KHz carrier frequency for the required time in microseconds
// This is timing critial and just do-able on an Arduino using the standard I/O functions.
// If you are using interrupts, ensure they disabled for the duration.
void IRcarrier(unsigned int IRtimemicroseconds)
{
   digitalWrite(IRLEDpin, HIGH);
   delayMicroseconds(IRtimemicroseconds); 
   digitalWrite(IRLEDpin, LOW);
//  for(int i=0; i < (IRtimemicroseconds / 26); i++)
//    {
//    digitalWrite(IRLEDpin, HIGH);   //turn on the IR LED
//    //NOTE: digitalWrite takes about 3.5us to execute, so we need to factor that into the timing.
//    delayMicroseconds(9);          //delay for 13us (9us + digitalWrite), half the carrier frequnecy
//    digitalWrite(IRLEDpin, LOW);    //turn off the IR LED
//    delayMicroseconds(9);          //delay for 13us (9us + digitalWrite), half the carrier frequnecy
//    }
}

//Sends the IR code in 4 byte NEC format
void IRsendCode(unsigned long code)
{
   
  //send the leading pulse
  IRcarrier(9000);            //9ms of carrier
  delayMicroseconds(4500);    //4.5ms of silence
  
  //send the user defined 4 byte/32bit code
  for (int i=0; i<32; i++)            //send all 4 bytes or 32 bits
    {
    IRcarrier(BITtime);               //turn on the carrier for one bit time
    if (code & 0x80000000)            //get the current bit by masking all but the MSB
      delayMicroseconds(3 * BITtime); //a HIGH is 3 bit time periods
    else
      delayMicroseconds(BITtime);     //a LOW is only 1 bit time period
     code<<=1;                        //shift to the next bit for this byte
    }
  IRcarrier(BITtime);                 //send a single STOP bit.
}


void resetDecoder()
{
    nec_ok = 0;                                 
    nec_state = 0;
    TCCR1B = 0;                                  
    address = nec_code >> 16;
    command = nec_code >> 8;
    inv_command = nec_code;
    attachInterrupt(0, remote_read, CHANGE);
}

void loop()                           //some demo main code
{
  if(nec_ok){  
    Serial.print("IR input(long): ");                   
    Serial.println(nec_code);
    Serial.print("IR input(BIN): ");  
    Serial.println(nec_code,BIN);
    Serial.print("IR input(HEX): "); 
    Serial.println(nec_code,HEX); 
    Serial.println("Transmit IR"); 
    IRsendCode(16591063);
    resetDecoder();
  }
}
