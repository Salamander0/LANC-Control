int PinLANC = 3;                // vstupni signal LANC data
int PinCMD = 2;                 // vystupni signal LANC data
int PinLEDRecord = 4;           // LED pro nahravani
int PinLEDActivity = 5;         // LED pro standby
int PinSWRecord = 6;            // tlacitko REC/STOP

int FrameDuration = 94;         // delka jednoho ramce pro synchronizaci

const int eSTOP = 0x5E;
const int eREC  = 0x33;
const int sSTOP = 0x14;
const int sREC = 0x04;


int eCommand;                   // prikaz pro odeslani do kamery
int nDetectedStatus;            // detekovany status kamery
int nCommandTimes;              // pocet zbyvajicich prikazu k zopakovani
bool KeepAlive;                 // indikator KeepAlive packetu

void setup(){   
  pinMode(PinLANC, INPUT);
  pinMode(PinCMD, OUTPUT);
  pinMode(PinLEDRecord, OUTPUT);
  pinMode(PinLEDActivity,OUTPUT);

  pinMode(PinSWRecord,INPUT);
  digitalWrite(PinSWRecord,HIGH);
  
  KeepAlive=false;
}

void FlashLED(){
    switch(nDetectedStatus){
    case sSTOP: //stop
      digitalWrite(PinLEDActivity,HIGH);
      digitalWrite(PinLEDRecord,LOW);
      break;
    case sREC: //REC
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
  digitalWrite(PinCMD,LOW);
}

byte GetNextByte(){
  unsigned char nByte=0;

  delayMicroseconds(FrameDuration + 15);
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

  nByte = nByte ^ 255;

  return nByte;
}

void NextStartBit(){
  for(;;){
    while (digitalRead(PinLANC)==HIGH)
      delayMicroseconds(10); 
    if (digitalRead(PinLANC)==LOW) 
      break;
  }
}

int IsSwitchEnabled(int nPin){
  if (digitalRead(nPin) == LOW){
  unsigned long time = millis();
   delay(200); //debounce
   // check if the switch is pressed for longer than 1 second.
    if(digitalRead(nPin) == LOW && (millis() - time > 2000 UL)) 
     {
       return 2;
     } else
     return 1; 
    }
    return 0;
}

void GetCommand(){
  switch(IsSwitchEnabled(PinSWRecord)){
    case 1:
      nCommandTimes=0;
      eCommand=eREC;
      break;
    case 2:
      nCommandTimes=0;
      eCommand=eSTOP;
      break;
    default:
      break;
  }
}

void loop()                    
{
  byte LANC_Frame[8];
  byte nByte;
  int nInd;
  int StartFrame=2;
  int Counter=0;

  for(;;){
    if(nCommandTimes >= 8)GetCommand();
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
