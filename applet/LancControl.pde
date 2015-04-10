/*

 LANC Control - v1.0
 (C) Ariel Rocholl - 2008, Madrid Spain
 Feel free to share this source code, but include explicit mention to the author.
 Licensed under creative commons - see http://creativecommons.org/licenses/by-sa/3.0/
 
 */

int ledPin = 13;                // LED connected to digital pin 13
int PinLANC = 3;                // 5V limited input signal from LANC data
int PinCMD = 2;                 // Command to send to LANC
int PinDEBUG = 12;              // Used for timing and debug as a signal output for a logic analyzer
int PinRS232 = 4;               // When set to LOW will dump RS232 info, although several frames will be lost

int PinLEDRecord = 6;           // Will be ON when recording
int PinLEDPlay = 7;             // Will be ON when playing
int PinLEDActivity = 8;         // Will be ON anytime there is status different than STOP

int PinSWRecord = 10;
int PinSWStop = 9;
int PinSWPlay = 11;

//Commands to send to camera (note this is not the same value as coded received on status)
const int eSTOP = 0x30;
const int ePLAY = 0x34;
const int eFFWD = 0x38;
const int eREWD = 0x36;
const int eREC  = 0x3A;

int eCommand;                   //Current LANC command to send to video camera
int nDetectedStatus;            //Status returned by LANC bus
byte nCommandTimes;             //LANC requires at least 5 times to repeat the command to video camera. This will count that.
int nFlashCounter;              //Counter to flash leds so they save battery power (as opposed to be always ON)

void setup()                    // run once, when the sketch starts
{
  pinMode(ledPin, OUTPUT);      
  pinMode(PinLANC, INPUT);
  pinMode(PinDEBUG, OUTPUT);
  pinMode(PinCMD, OUTPUT);
  pinMode(PinLEDRecord, OUTPUT);
  pinMode(PinLEDActivity,OUTPUT);

  //set input pins and activate internal weak pullup resistors
  pinMode(PinRS232,INPUT);
  digitalWrite(PinRS232,HIGH);
  pinMode(PinSWRecord,INPUT);
  digitalWrite(PinSWRecord,HIGH);
  pinMode(PinSWStop,INPUT);
  digitalWrite(PinSWStop,HIGH);
  pinMode(PinSWPlay,INPUT);
  digitalWrite(PinSWPlay,HIGH);

  nFlashCounter=0;
  nCommandTimes=0;

  Serial.begin(9600);
}

void FlashLED()
{
  boolean bLED_ON=false;

  nFlashCounter++;
  if (nFlashCounter>=50)
    nFlashCounter=0;
  else if (nFlashCounter>=45)
    bLED_ON=true;

  if (bLED_ON)
  {
    switch(nDetectedStatus)
    {
    case 0x02: //stop
      digitalWrite(PinLEDPlay,LOW);
      digitalWrite(PinLEDActivity,LOW);
      digitalWrite(PinLEDRecord,LOW);
      break;
    case 0x06: //PLAY
      digitalWrite(PinLEDActivity,HIGH);
      digitalWrite(PinLEDPlay,HIGH);
      break;
    case 0x04: //REC
      digitalWrite(PinLEDActivity,HIGH);
      digitalWrite(PinLEDRecord,HIGH);
      break;
    default: //any other activity except STOP
      digitalWrite(PinLEDActivity,HIGH);
      break;
    }
  }
  else
  {
    digitalWrite(PinLEDPlay,LOW);
    digitalWrite(PinLEDActivity,LOW);
    digitalWrite(PinLEDRecord,LOW);
  }
}

void BlinkLong()
{
  digitalWrite(ledPin, HIGH);   // sets the LED on
  delay(1000);                  // waits for a second
  digitalWrite(ledPin, LOW);    // sets the LED off
  delay(1000);                  // waits for a second
  digitalWrite(ledPin, HIGH);   // sets the LED on
  delay(1000);                  // waits for a second
  digitalWrite(ledPin, LOW);    // sets the LED off
  delay(1000);                  // waits for a second
  digitalWrite(ledPin, HIGH);   // sets the LED on
  delay(1000);                  // waits for a second
  digitalWrite(ledPin, LOW);    // sets the LED off
  delay(1000);                  // waits for a second
}

void AfterLongGap()
{
  boolean bLongEnough=false;
  int nInd;

  digitalWrite(PinDEBUG,HIGH);

  //search for 4ms gap -at least- being high continuosly.
  //This worked extraordinarily well in my European Sony video camera
  //but it may not work on a NTSC camera, you may need to reduce the 
  //loop limit for that (hopefully not). Please send me details
  //in that case to make it available to the community.

  while (!bLongEnough)
  {
    for (nInd=0; nInd<150; nInd++)
    {
      delayMicroseconds(25);
      if (digitalRead(PinLANC)==LOW)
        break;
    }
    if (nInd==150)
      bLongEnough=true;
  }

  //Now wait till we get the first start bit (low)
  while (digitalRead(PinLANC)==HIGH)
    delayMicroseconds(25);

  digitalWrite(PinDEBUG,LOW);
}

void SendCommand(unsigned char nCommand)
{
  //Note bits are inverted already by the NPN on open collector
  //(when it is switched ON by a vbe>0.7v, the vce will be close to zero thus low state for LANC)
  //This is exactly what we want and how LANC bus works (tie to GND to indicate "1")

  //Note timming here is critical, I used a logic analyzer to rev engineering right delay times.
  //This guarantees closer to LANC standard so it increases chances to work with any videocamera,
  //but you may need to finetune these numbers for yours (hopefully not). Please send me details
  //in that case to make it available to the community.

  delayMicroseconds(84); //ignore start bit
  digitalWrite(PinCMD,nCommand  & 0x01);

  delayMicroseconds(94); 
  digitalWrite(PinCMD,(nCommand  & 0x02) >> 1);

  delayMicroseconds(94); 
  digitalWrite(PinCMD,(nCommand  & 0x04) >> 2);

  delayMicroseconds(94); 
  digitalWrite(PinCMD,(nCommand  & 0x08) >> 3);

  delayMicroseconds(94); 
  digitalWrite(PinCMD,(nCommand  & 0x10) >> 4);

  delayMicroseconds(94); 
  digitalWrite(PinCMD,(nCommand  & 0x20) >> 5);

  delayMicroseconds(94); 
  digitalWrite(PinCMD,(nCommand  & 0x40) >> 6);

  delayMicroseconds(94); 
  digitalWrite(PinCMD,(nCommand  & 0x80) >> 7);

  delayMicroseconds(94); 
  digitalWrite(PinCMD,LOW); //free the LANC bus after writting the whole command
}

byte GetNextByte()
{
  unsigned char nByte=0;

  //Timming here is not as critical as when writing, actually it is a bit delayed
  //when compared to start edge of the signal, but this is good as it increases
  //chances to read in the center of the bit status, thus it is more immune to noise
  //Using this I didn't need any hardware filter such as RC bass filter.

  delayMicroseconds(104); //ignore start bit
  nByte|= digitalRead(PinLANC);

  delayMicroseconds(104); 
  nByte|= digitalRead(PinLANC) << 1;

  delayMicroseconds(104); 
  nByte|= digitalRead(PinLANC) << 2;

  delayMicroseconds(104); 
  nByte|= digitalRead(PinLANC) << 3;

  delayMicroseconds(104); 
  nByte|= digitalRead(PinLANC) << 4;

  delayMicroseconds(104); 
  nByte|= digitalRead(PinLANC) << 5;

  delayMicroseconds(104); 
  nByte|= digitalRead(PinLANC) << 6;

  delayMicroseconds(104); 
  nByte|= digitalRead(PinLANC) << 7;

  nByte = nByte ^ 255;  //invert bits, we got LANC LOWs for logic HIGHs

  return nByte;
}

void NextStartBit()
{
  digitalWrite(PinDEBUG,HIGH);

  //Now wait till we get the first start bit (low)
  while(1)
  {
    //this will look for the first LOW signal with abou 5uS precission
    while (digitalRead(PinLANC)==HIGH)
      delayMicroseconds(5); 

    //And this guarantees it was actually a LOW, to ignore glitches and noise
    delayMicroseconds(5);
    if (digitalRead(PinLANC)==LOW) 
      break;
  }
  digitalWrite(PinDEBUG,LOW);
}

boolean IsSwitchEnabled(int nPin)
{
  if (digitalRead(nPin)==LOW)
  {
    delayMicroseconds(100);
    if (digitalRead(nPin)==LOW)
    {
      nCommandTimes=0; //Push button status change, so think on send command now
      return true;
    }
  }
  return false;
}

void GetCommand()
{
  if (IsSwitchEnabled(PinSWStop))
    eCommand=eSTOP;
  else if (IsSwitchEnabled(PinSWRecord))
    eCommand=eREC;
  else if (IsSwitchEnabled(PinSWPlay))
    eCommand=ePLAY;
}

void loop()                     // run over and over again
{
  byte LANC_Frame[8];
  byte nByte;
  int nInd, nInd0;

  digitalWrite(PinDEBUG,LOW);
  BlinkLong();

  eCommand = eSTOP;

  //infinite loop, I do not want to get out of here ever.
  //It is more efficient than let loop() function to do
  //that, as it requires a function call and then a loop
  //like this one. Well, I prefer my own efficient loop
  //instead. This increase chances of AfterLongGap() having
  //time enough for what it needs to be done.

  while (1) 
  {
    //Which command do I have to send from my push buttons?
    GetCommand();

    //Sinchronize to get start of next frame
    AfterLongGap();

    //I will send the command 8 times, LANC requires at least 5 times
    if (nCommandTimes<8) 
    {
      nCommandTimes++;

      SendCommand(0x18); //0x18 indicates normal command to videocamera.
      //you have to change this for a photo camera.
      NextStartBit();
      SendCommand(eCommand);
    }
    else
    {
      //Command already sent 8 times so, just ignore next 2 bytes
      GetNextByte();
      NextStartBit();
      GetNextByte();
    }
    
    //Get next 6 bytes remaining
    for (nInd=2; nInd<8; nInd++)
    {
      NextStartBit();

      LANC_Frame[nInd]=GetNextByte();
    }
    
    nDetectedStatus=LANC_Frame[4];
    FlashLED();

    if (digitalRead(PinRS232)==HIGH) ///----------------------------------- RS232 dump below this point
      continue; //ignore RS232 dump 

    Serial.println();
    Serial.print("LANC Data:");
    for (nInd=4; nInd<8; nInd++)
    {
      Serial.print("[");
      Serial.print(LANC_Frame[nInd],BIN);
      Serial.print("]");
    }
    Serial.print("-");
    Serial.print(LANC_Frame[4],HEX);

    Serial.print("-");
    Serial.print(LANC_Frame[6],HEX);
    Serial.print(",");
    Serial.print(LANC_Frame[7],HEX);

    Serial.print("-");
    Serial.print(LANC_Frame[6],DEC);
    Serial.print(",");
    Serial.print(LANC_Frame[7],DEC);

    switch(LANC_Frame[4])
    {
    case 0x02: 
      Serial.print(" - STOP"); 
      break;
    case 0x04: 
      Serial.print(" - REC"); 
      break;
    case 0x06: 
      Serial.print(" - PLAY"); 
      break;
    case 0x46: 
      Serial.print(" - PLAY FFWD"); 
      break;
    case 0x56: 
      Serial.print(" - PLAY REWD"); 
      break;
    case 0x07: 
      Serial.print(" - PLAY PAUSE"); 
      break;
    case 0x83: 
      Serial.print(" - REWD"); 
      break;
    case 0x03: 
      Serial.print(" - FFWD"); 
      break;
    }

  }

  Serial.println("****Unexpected Loop here, if you reach here it is an ERROR!");
}

/* 
 
 HEX values rev engineered from my Sony video camera in byte #4.
 You can get yours by simply activating the RS232 dump and manually
 playing with your camera, the codes you see in byte #4 will let you
 check for a particular value programmatically.
 
 2 -stop
 4 - rec
 6 -play
 46 -ffwd with play
 56 -rew with play
 7 -play pause
 83 -rew
 3 -ffwd
 
 */
