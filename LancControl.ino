int PinLANC = 3;                // 5V limited input signal from LANC data
int PinCMD = 2;                 // Command to send to LANC
int PinLEDRecord = 4;           // Will be ON when recording
int PinLEDActivity = 5;         // Will be ON anytime there is status different than STOP
int PinSWRecord = 6;

int FrameDuration = 94;       //ms

//Commands to send to camera (note this is not the same value as coded received on status)
const int eSTOP = 0x5E;
const int eSTART = 0x05;
const int eREC  = 0x33;

int eCommand;                   //Current LANC command to send to video camera
int nDetectedStatus;            //Status returned by LANC bus
int nCommandTimes;             //LANC requires at least 5 times to repeat the command to video camera. This will count that.
bool KeepAlive;                //counter for sending keepalive packet

void setup(){   
  pinMode(PinLANC, INPUT);
  pinMode(PinCMD, OUTPUT);
  pinMode(PinLEDRecord, OUTPUT);
  pinMode(PinLEDActivity,OUTPUT);

  pinMode(PinSWRecord,INPUT);
  digitalWrite(PinSWRecord,HIGH);
}

void FlashLED(){
    switch(nDetectedStatus){
    case 0x14: //stop
      digitalWrite(PinLEDActivity,HIGH);
      digitalWrite(PinLEDRecord,LOW);
      break;
    case 0x04: //REC
      digitalWrite(PinLEDActivity,LOW);
      digitalWrite(PinLEDRecord,HIGH);
      break;
    default: //any other activity except STOP
      digitalWrite(PinLEDActivity,HIGH);
      digitalWrite(PinLEDRecord,HIGH);
      break;
    }
}

void AfterLongGap(){
  boolean bLongEnough=false;
  int nInd;
  
  while (!bLongEnough){
    for (nInd=0; nInd<150; nInd++){
      delayMicroseconds(25);
      if (digitalRead(PinLANC)==LOW)
        break;
    }
    if (nInd==150)
      bLongEnough=true;
  }

  //Now wait till we get the first start bit (low)
  while (digitalRead(PinLANC)==HIGH)   delayMicroseconds(25);
}

void SendCommand(unsigned char nCommand){
  delayMicroseconds(FrameDuration); //ignore start bit
  digitalWrite(PinCMD,nCommand  & 0x01);

  delayMicroseconds(FrameDuration); 
  digitalWrite(PinCMD,(nCommand  & 0x02) >> 1);

  delayMicroseconds(FrameDuration); 
  digitalWrite(PinCMD,(nCommand  & 0x04) >> 2);

  delayMicroseconds(FrameDuration); 
  digitalWrite(PinCMD,(nCommand  & 0x08) >> 3);

  delayMicroseconds(FrameDuration); 
  digitalWrite(PinCMD,(nCommand  & 0x10) >> 4);

  delayMicroseconds(FrameDuration); 
  digitalWrite(PinCMD,(nCommand  & 0x20) >> 5);

  delayMicroseconds(FrameDuration); 
  digitalWrite(PinCMD,(nCommand  & 0x40) >> 6);

  delayMicroseconds(FrameDuration); 
  digitalWrite(PinCMD,(nCommand  & 0x80) >> 7);

  delayMicroseconds(FrameDuration); 
  digitalWrite(PinCMD,LOW); //free the LANC bus after writting the whole command
}

byte GetNextByte(){
  unsigned char nByte=0;

  delayMicroseconds(FrameDuration + 15); //ignore start bit
  nByte|= digitalRead(PinLANC);

  delayMicroseconds(FrameDuration + 15); 
  nByte|= digitalRead(PinLANC) << 1;

  delayMicroseconds(FrameDuration + 15); 
  nByte|= digitalRead(PinLANC) << 2;

  delayMicroseconds(FrameDuration + 15); 
  nByte|= digitalRead(PinLANC) << 3;

  delayMicroseconds(FrameDuration + 15); 
  nByte|= digitalRead(PinLANC) << 4;

  delayMicroseconds(FrameDuration + 15); 
  nByte|= digitalRead(PinLANC) << 5;

  delayMicroseconds(FrameDuration + 15); 
  nByte|= digitalRead(PinLANC) << 6;

  delayMicroseconds(FrameDuration + 15); 
  nByte|= digitalRead(PinLANC) << 7;

  nByte = nByte ^ 255;  //invert bits, we got LANC LOWs for logic HIGHs

  return nByte;
}

void NextStartBit(){
  for(;;){
    //this will look for the first LOW signal with abou 5uS precission
    while (digitalRead(PinLANC)==HIGH)
      delayMicroseconds(10); 

    //And this guarantees it was actually a LOW, to ignore glitches and noise
    delayMicroseconds(10);
    if (digitalRead(PinLANC)==LOW) 
      break;
  }
}

int IsSwitchEnabled(int nPin){
  if (digitalRead(nPin)==LOW){
    delayMicroseconds(100);
    int time = millis();
    while(digitalRead(nPin)==LOW){
      if((millis() - time) >= 2000){
       nCommandTimes=0; //Push button status change, so think on send command now
       return 2;
     }
   
    }     
      nCommandTimes=0;
      return 1; 
  }
  return 0;
}

void GetCommand(){
  switch(IsSwitchEnabled(PinSWRecord)){
    case 1:
      eCommand=eREC;
      break;
    case 2:
      eCommand=eSTOP;
      break;
    default:
      break;
  }
}

void loop()                     // run over and over again
{
  byte LANC_Frame[8];
  byte nByte;
  int nInd;
  KeepAlive=false;
  int StartFrame=2;
  int Counter=0;

  for(;;){
    if(nCommandTimes >= 8)GetCommand();         //get button press
    AfterLongGap();       //sync

    if (nCommandTimes<8){
      nCommandTimes++;

      SendCommand(0x18); 
      
      NextStartBit();
      SendCommand(eCommand);
    }
    else{
      GetNextByte();
      NextStartBit();
      GetNextByte();
    } 
    if(eCommand==eSTOP && nCommandTimes >= 8){
      delay(5000);
    }    
    if(KeepAlive){
        Counter++;
        switch(Counter){
          case 4:
          case 5:
            StartFrame=4;
            NextStartBit();
            SendCommand(0x46);
             NextStartBit();
              SendCommand(0x10);
            break;
          case 6:
            StartFrame=3;
            NextStartBit();
            SendCommand(0x56);
            break;
           case 7:
           NextStartBit();
            SendCommand(0x56);
            KeepAlive=false;
            StartFrame=2;
            Counter=0;
            break;
          default: 
            break;
        }
    }    
    
    //Get next 6 bytes remaining
    for (nInd=StartFrame; nInd<8; nInd++){
      NextStartBit();
 
      LANC_Frame[nInd]=GetNextByte();
    }
    
    nDetectedStatus=LANC_Frame[4];
    switch(LANC_Frame[2]){
       case 0x49: 
                KeepAlive=true;
                LANC_Frame[2]=0x00;
                break;
     default: break;
    }
    
    FlashLED();
  }

}
