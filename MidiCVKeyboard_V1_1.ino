//#include <MsTimer2.h>

/*  
Arduino Mini CV Keyboard firmware v1.1
Dave Guenther, 2017


This sketch uses the Arduino 10-bit DAC to scan a keyboard 
and produce precise control voltages on pin D6, D9, D10 and D11 and octave 
information on pins D2, D3, and D4.  

Pin D6, D9, D10, and D11:
These pins are set to Analog (PWM) mode.  Setting precise 
output CV requires 10 bit resolution to achieve approximately 
1mV/step increment.  Each pin in PWM Out mode provides only 8 
bit access to a 5V scale.  We can take that to scale down to 
be between 0-1 Volt, but will need better resolution still. 
We can chain together the four pins (D6, D9, D10, D11) in PWM mode in
such a way so that instead of scaling the output voltage to 
a single analog pin correlating to 0-1 Volt, we further scale 
so that each analog pin equates to 0-250mV.  We then sum the 
voltages of all 4 pins after scaling to produce a 10bit DAC 
that provides 1V in 1mV increments.  This turns out to be a scale 
down by factor of 20 (5 to get the initial 5v scale down to 
1v and the 4 to get the 1v scale down to 250mV per pin).  
Once these are scaled using an op amp, we can then sum them 
together using another op amp set up as a summer.  This 
gives us the ability to write Arduino code to programmatically 
increment values of pins D6, D9, D10, and D11 to gain the 
CV required.    

Pin D2, D3, and D4:
These pins are is set to Digital mode.  The program uses the
pins to select the octave or whole volt to add to the CV on 
pin D6, D9, D10 and D11.  Pins D2, D3, and D4 are the parallel binary 
representation of the octave.  Pin D4 is MSB and pin D2 is LSB.  
The binary information will be red by a CD4051 3 bit Analog 
MUX to switch appropriate lines (there are 8 total lines 
with voltages from 0-7 Volts).  The bits correspond to a
voltage and are summed with the summed voltage from pins  D6, D9, 
D10, and D11 to produce a complete CV that is sent to other 
synth modules such as VCOs, VCAs, VCFs, etc.

Pin A0 controls the Arpeggio Rate

Pins D5, D8 and D7 are used to scan the Physical Keyboard. They
are the data, latch and clock pins respectively and are used 
with shift registers to read all keys on the keyboard and several
other switch positions on the control panel

Pin D1(RX) is used to Recieve Midi Data.

Pin D12:  This pin is used to send ouy Gate information. When
a key is pressed either via MIDI or by scanning the keyboard,
This pin will be set to high.  When no keys are pressed the pin 
will be set to low.

Future Enhancements:
Pins A6, A3, A2, and A1 will be used to produce the CV generation 
currently used on pins D6, D9, D10, and D11

*/

byte commandByte;
byte noteByte;
byte velocityByte;
int ArpSpeed;

bool wasKeyBoardKeyPressed=false;
bool wasMIDIKeyPressed=false;
bool ArppeggioOn=false;

const int CV_Out_A = 6;  
const int CV_Out_B = 9;  
const int CV_Out_C = 10; 
const int CV_Out_D = 11;
const int Oct_Out_A = 2;  //LSB
const int Oct_Out_B = 3;
const int Oct_Out_C = 4;  //MSB
const int Gate = 12; // Implements Gate
const int ArpSpeedPin = A0; // 0-5V to determine speed of arppegiator


// Shift In Data Elements
const int dataPin = 5;
const int latchPin = 8;
const int clockPin = 7;
byte switchVar1 = 72;  //01001000
byte switchVar2 = 159; //10011111
byte switchVar3 = 163;  //10100011
byte switchVar4 = 47; //00101111
byte switchVar5 = 210; //11010010
byte switchVar6 = 21; //00010101
 
struct KeyboardNote{
  int Note=0;     // Note is an int representing a single note on 
                 // a 12 half-step scale 0=C and 11-B before the 
                 // next C
  int Octave=0;    // Octave is a value 0<=octave<=5
  int NoteCode=0; // this is a value 0<=NoteCode<=83 
                  // (7 octaves * 12 notes).  It represents 
                  // an absolute value for the note
  int PitchBend=0; // PitchBend is a value -1.00<PitchBend<1.0 
                // where -1.0 is the next halfstep down and +1.0 
                // is the next halfstep up.
};

bool MIDIBOARD[84];
bool KEYBOARD[84];
int KeyboardOctaveSelect=0; // 0=Base Octave, 1= Shift up 1 Octave, 2=Shift up two octaves, 3=Shift up 3 octaves 
int LastAnalogValue=0; // Used to keep the SetAnalogValue function from repeating the same value if there is no change since last scan.

/////Arpeggio Variables
int Arppeggiator[16]={-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
int ArpDelay=50;  //Milliseconds
int ArpLocation=0;
int ArpKeysDown[4]={-1,-1,-1,-1};
int NumArpKeysDown=0;
bool HasArppeggioChanged=true;
int ArpType=2; //0=Regular Up, 1=Regular Down, 2=Inversions Up, 3=Inversions Down based on pin read
int NumOctaves=4; // 1,2,3, or 4  Based on Pin Read for Arppegiator
bool MasterArp=false; // When set to true, Arppegiator will rerun regardless of other conditions


void setup()
{
  pinMode(CV_Out_A, OUTPUT);
  pinMode(CV_Out_B, OUTPUT);
  pinMode(CV_Out_C, OUTPUT);
  pinMode(CV_Out_D, OUTPUT);
  pinMode(Oct_Out_A, OUTPUT);
  pinMode(Oct_Out_B, OUTPUT);
  pinMode(Oct_Out_C, OUTPUT);
  pinMode(Gate, OUTPUT);
  pinMode(ArpSpeed, INPUT);
 
// Shift In Setup
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT); 
  pinMode(dataPin, INPUT);

// End Shift In Setup
  

  pinMode(13,OUTPUT);
  digitalWrite(13,LOW); 
  Serial.begin(31250);  
  commandByte=0;
  noteByte=0;
  velocityByte=0;

  for (int i=0;i<=83;i++)
  {
    MIDIBOARD[i]=false;
  }
  //Serial.print("Number of keys currently pressed: ");
  //Serial.println(NumKeysPressedMIDIBoard());
  for (int i=0;i<=83;i++)
  {
    KEYBOARD[i]=false;
  }
  //ArpSpeed=GetArpSpeed();
  //Timer1.initialize(1000000/*ArpSpeed*100*/); // set a timer of length 0-1024 * 1000 microseconds = 0-1024 milliseconds = 0-1.024 Seconds
  //Timer1.attachInterrupt( timerIsr ); // attach the service routine here

  //MsTimer2::set(50, TimerPulse); // 50ms period
  //MsTimer2::start();

}

int GetArpSpeed()
{
  // returns the arppegio speed in milliseconds
  return analogRead(ArpSpeedPin);
}

int NumKeysPressedMIDIBoard()
// This function scans through MIDIBOARD[] and returns an in value
// equal to the total number of keys that are currently pressed
{
  int TotalKeysPressed=0;
  for (int i=0;i<=83;i++)
  {
    if (MIDIBOARD[i]) TotalKeysPressed+=1;
  }
  return TotalKeysPressed;
}

int NumKeysPressedKeyBoard()
// This function scans through KEYBOARD[] and returns an int value
// equal to the total number of keys that are currently pressed
{
  int TotalKeysPressed=0;
  for (int i=0;i<=83;i++)
  {
    if (KEYBOARD[i]>0) TotalKeysPressed+=1;
  }
  return TotalKeysPressed;
}

void NoteOff_ResetPortamento()
{
   if (digitalRead(Gate)==HIGH)
   {
    
   digitalWrite(Gate,LOW);
   //Serial.println("Notes OFF");
   }
}

bool checkMIDI(KeyboardNote *thisNote){
if (Serial.available() >=2)
    {
      commandByte = Serial.read();//read first byte
      //Serial.println(commandByte);
      if(commandByte==144) // MIDI Byte: 1001000  --> Note On
      {
        digitalWrite(13,HIGH);
        noteByte = Serial.read();//read first byte
        noteByte-=12;
        velocityByte = Serial.read();//read first byte
        //Debug Serial Print

        
        MIDIBOARD[noteByte]=true;

      
        thisNote->Note = noteByte % 12;
        thisNote->Octave = int(noteByte/12);
        thisNote->PitchBend=0;
        if(digitalRead(Gate)==LOW) 
        {
          digitalWrite(Gate,HIGH);
          //Serial.println("Note ON");
        }
        //Serial.print("Note ON,  noteByte: ");
        //Serial.print(noteByte);
        //Serial.print("    velocityByte: ");
        //Serial.println(velocityByte);      
        //Serial.print("NumKeysPressedMIDIBoard(): ");
        //Serial.println(NumKeysPressedMIDIBoard());      
        wasMIDIKeyPressed=true;
      }
      if(commandByte==128) // MIDI Byte: 1001000  --> Note Off
      {
        digitalWrite(13,HIGH);
        noteByte = Serial.read();//read first byte
        noteByte-=12;
        velocityByte = Serial.read();//read first byte
        //Debug Serial Print      
        //Serial.print("Note OFF,  Data: ");
        //Serial.print(noteByte);
        //Serial.print("    Velocity: ");
        //Serial.println(velocityByte);
        MIDIBOARD[noteByte]=false;
        HasArppeggioChanged=true;
        //Serial.print("Number of keys currently pressed: ");
        //Serial.println(NumKeysPressedMIDIBoard());
        

        if(NumKeysPressedMIDIBoard()==0)
        {
          NoteOff_ResetPortamento();
        }
        wasMIDIKeyPressed=false;
      }
      digitalWrite(13,LOW);
    return true;  
    }else
    {
      return false;
    }
    return false;
}

void checkKeyboard(KeyboardNote *thisNote)
// Used to scan keyboard using Shift Registers.  Keys pressed 
// here would override the MIDIBOARD array
{
  int TotalKeysOn=0;
    //Pulse the latch pin:
  //set it to 1 to collect parallel data
  digitalWrite(latchPin,1);
  //set it to 1 to collect parallel data, wait
  //delayMicroseconds(20);
  //set it to 0 to transmit data serially  
  digitalWrite(latchPin,0);
//Serial.println("Shifting In");
  //while the shift register is in serial mode
  //collect each shift register into a byte
  //the register attached to the chip comes in first 
  switchVar1 = shiftIn(dataPin, clockPin);
  switchVar2 = shiftIn(dataPin, clockPin);
  switchVar3 = shiftIn(dataPin, clockPin);
  switchVar4 = shiftIn(dataPin, clockPin);
  switchVar5 = shiftIn(dataPin, clockPin);
  switchVar6 = shiftIn(dataPin, clockPin);
//Serial.println(switchVar1);
//Serial.println(switchVar2);
//Serial.println(switchVar3);
//Serial.println(switchVar4);
//Serial.println(switchVar5);
//Serial.println(switchVar6);  
  // turn remaining shift ins on as CD4021's are placed on board
  /*switchVar3 = shiftIn(dataPin, clockPin);
  switchVar4 = shiftIn(dataPin, clockPin);
  switchVar5 = shiftIn(dataPin, clockPin);  */
/*
if(switchVar1 & 128) KEYBOARD[0]=true; else KEYBOARD[0]=false;
if(switchVar1 & 64) KEYBOARD[1]=true; else KEYBOARD[1]=false;
if(switchVar1 & 32) KEYBOARD[2]=true; else KEYBOARD[2]=false;
if(switchVar1 & 16) KEYBOARD[3]=true; else KEYBOARD[3]=false;
if(switchVar1 & 8) KEYBOARD[4]=true; else KEYBOARD[4]=false;
if(switchVar1 & 4) KEYBOARD[5]=true; else KEYBOARD[5]=false;
if(switchVar1 & 2) KEYBOARD[6]=true; else KEYBOARD[6]=false;
if(switchVar1 & 1) KEYBOARD[7]=true; else KEYBOARD[7]=false;
if(switchVar2 & 128) KEYBOARD[8]=true; else KEYBOARD[8]=false;
if(switchVar2 & 64) KEYBOARD[9]=true; else KEYBOARD[9]=false;
if(switchVar2 & 32) KEYBOARD[10]=true; else KEYBOARD[10]=false;
if(switchVar2 & 16) KEYBOARD[11]=true; else KEYBOARD[11]=false;
if(switchVar2 & 8) KEYBOARD[12]=true; else KEYBOARD[12]=false;
if(switchVar2 & 4) KEYBOARD[13]=true; else KEYBOARD[13]=false;
if(switchVar2 & 2) KEYBOARD[14]=true; else KEYBOARD[14]=false;
if(switchVar2 & 1) KEYBOARD[15]=true; else KEYBOARD[15]=false;
*/
AddKeysToArray(0);
AddKeysToArray(1);
AddKeysToArray(2);
AddKeysToArray(3);
AddKeysToArray(4);
AddKeysToArray(5);

TotalKeysOn = NumKeysPressedKeyBoard();
if(TotalKeysOn>0)
{
  //At least one Keyboard Key is pressed
  //Serial.print(" Keys Pressed: ");
  int ArpKeysDownCtr=0;
  for (int i=0;i<=47;i++)
  {
    if(KEYBOARD[i]>0)
    {
        ArpKeysDown[ArpKeysDownCtr]=i;
        ArpKeysDownCtr+=1;
        thisNote->Note = i % 12;
        thisNote->Octave = int(i/12);
        thisNote->PitchBend=0;

      //Serial.print(i);
      //Serial.print(" ");
    }
  
  }
  //Serial.println();
  //Serial.print("NumKeysPressedKeyBoard(): ");
  //Serial.println(TotalKeysOn);
  if (TotalKeysOn!=NumArpKeysDown)
  {
    ClearArppeggiator();  
    NumArpKeysDown=TotalKeysOn;  
    //Serial.print("TotalKeysOn = ");
    //Serial.println(TotalKeysOn);
    //Serial.print("NumArpKeysDown = ");
    //Serial.println(NumArpKeysDown);
    
    //Serial.println("Arppegiator Cleared in Keyboard Scanner"); 
  }
  wasKeyBoardKeyPressed=true;
  //Serial.flush();
//void serialFlush(){
  while(Serial.available() > 0) {
    char t = Serial.read();
  }
//}   
  
  if(digitalRead(Gate)==LOW) 
  // Set the Gate High
  {
    digitalWrite(Gate,HIGH);
    //Serial.println("Note ON1");
  }

}else
{
  wasKeyBoardKeyPressed=false; 
  if(!wasMIDIKeyPressed)
  {
     NoteOff_ResetPortamento();
     ClearArppeggiator();
     NumArpKeysDown=0;
    //Serial.println("Note OFF");

  }
}
}

void ClearArppeggiator()
{
  for(int i=0; i<16;i++)
  {
    Arppeggiator[i]=-1;
  }
}

void AddKeysToArray(int ShiftRegisterNumber)
{
  byte RegisterByte;
  int ArpUpDown=0;
  int ArpRegInv=0;
  switch (ShiftRegisterNumber) {
    case 0:
      RegisterByte = switchVar1;
      break;
    case 1:
      RegisterByte = switchVar2;
      break;
    case 2:
      RegisterByte = switchVar3;
      break;
    case 3:
      RegisterByte = switchVar4;
      break;
    case 4:
      {
        RegisterByte = switchVar5;
      ShiftRegisterNumber *= 8;
    if(RegisterByte & 128) 
    {
      if(KEYBOARD[ShiftRegisterNumber+0]==false) HasArppeggioChanged=true;
      KEYBOARD[ShiftRegisterNumber+0]=true; 
    }else 
    {
      if(KEYBOARD[ShiftRegisterNumber+0]==true) HasArppeggioChanged=true;
      KEYBOARD[ShiftRegisterNumber+0]=false;
    }
    if(RegisterByte & 64) 
    {
      if(KEYBOARD[ShiftRegisterNumber+1]==false) HasArppeggioChanged=true;
      KEYBOARD[ShiftRegisterNumber+1]=true; 
    }else 
    {
      if(KEYBOARD[ShiftRegisterNumber+1]==true) HasArppeggioChanged=true;
      KEYBOARD[ShiftRegisterNumber+1]=false;
    }
    if(RegisterByte & 32) 
    {
    if(KEYBOARD[ShiftRegisterNumber+2]==false) HasArppeggioChanged=true;
    KEYBOARD[ShiftRegisterNumber+2]=true; 
    }else 
    {
      if(KEYBOARD[ShiftRegisterNumber+2]==true) HasArppeggioChanged=true;
      KEYBOARD[ShiftRegisterNumber+2]=false;
    }
    if(RegisterByte & 16) 
    {
      if(KEYBOARD[ShiftRegisterNumber+3]==false) HasArppeggioChanged=true;
      KEYBOARD[ShiftRegisterNumber+3]=true; 
    }else 
    {
      if(KEYBOARD[ShiftRegisterNumber+3]==true) HasArppeggioChanged=true;
      KEYBOARD[ShiftRegisterNumber+3]=false;
    }
    if(RegisterByte & 8) 
    {
      ArppeggioOn=true;
    }else
    {
      ArppeggioOn=false;
    }
    if(RegisterByte & 4) 
    {
      ArpUpDown=1; // Arppegio Ascends  
    }else
    {
      ArpUpDown=0; // Arppegio Descends
    }
    if(RegisterByte & 2) 
    {
      ArpRegInv=1; //Regular Arppegio
    }else
    {
      ArpRegInv=0; // Inversions Arppegio
    }
    if(RegisterByte & 1) 
    {
      NumOctaves=1;
    }
      if(ArpUpDown==1 && ArpRegInv==1) 
      {
        if(ArpType!=0) MasterArp=true; //Recreate Arppegio
        ArpType=0; // Regular Arppegio Ascending
      }
      if(ArpUpDown==0 && ArpRegInv==1) 
      {
        if(ArpType!=1) MasterArp=true; //Recreate Arppegio
        ArpType=1; // Regular Arppegio Descending
      }
      if(ArpUpDown==1 && ArpRegInv==0) 
      {
        if(ArpType!=2) MasterArp=true; //Recreate Arppegio
        ArpType=2; // Inversions Arppegio Ascending
      }
      if(ArpUpDown==0 && ArpRegInv==0) 
      {
        if(ArpType!=3) MasterArp=true; //Recreate Arppegio
        ArpType=3; // Inversions Arppegio Descending  
      }
      break;
      }
    case 5:
    {
      RegisterByte = switchVar6;
      if(RegisterByte & 128) 
      {
        NumOctaves=2;
      }
      if(RegisterByte & 64) 
      {
        NumOctaves=3;
      }
      if(RegisterByte & 32) 
      {
        NumOctaves=4;
      }
      if(RegisterByte & 16) 
      {
        KeyboardOctaveSelect=0;
      }
      if(RegisterByte & 8) 
      {
        KeyboardOctaveSelect=1;
      }
      if(RegisterByte & 4) 
      {
        KeyboardOctaveSelect=2;
      }
      if(RegisterByte & 2) 
      {
        KeyboardOctaveSelect=3;
      }
      if(RegisterByte & 1) 
      {
        // Do Nothing -- Empty Register
      }
      break;
    }
    default:
      RegisterByte = switchVar6;
      break;
      
  }
  if(ShiftRegisterNumber<4)
  {
    ShiftRegisterNumber *= 8;
    if(RegisterByte & 128) 
    {
      if(KEYBOARD[ShiftRegisterNumber+0]==false) HasArppeggioChanged=true;
      KEYBOARD[ShiftRegisterNumber+0]=true; 
    }else 
    {
      if(KEYBOARD[ShiftRegisterNumber+0]==true) HasArppeggioChanged=true;
      KEYBOARD[ShiftRegisterNumber+0]=false;
    }
    if(RegisterByte & 64) 
    {
      if(KEYBOARD[ShiftRegisterNumber+1]==false) HasArppeggioChanged=true;
      KEYBOARD[ShiftRegisterNumber+1]=true; 
    }else 
    {
      if(KEYBOARD[ShiftRegisterNumber+1]==true) HasArppeggioChanged=true;
      KEYBOARD[ShiftRegisterNumber+1]=false;
    }
    if(RegisterByte & 32) 
    {
    if(KEYBOARD[ShiftRegisterNumber+2]==false) HasArppeggioChanged=true;
    KEYBOARD[ShiftRegisterNumber+2]=true; 
    }else 
    {
      if(KEYBOARD[ShiftRegisterNumber+2]==true) HasArppeggioChanged=true;
      KEYBOARD[ShiftRegisterNumber+2]=false;
    }
    if(RegisterByte & 16) 
    {
      if(KEYBOARD[ShiftRegisterNumber+3]==false) HasArppeggioChanged=true;
      KEYBOARD[ShiftRegisterNumber+3]=true; 
    }else 
    {
      if(KEYBOARD[ShiftRegisterNumber+3]==true) HasArppeggioChanged=true;
      KEYBOARD[ShiftRegisterNumber+3]=false;
    }
    if(RegisterByte & 8) 
    {
      if(KEYBOARD[ShiftRegisterNumber+4]==false) HasArppeggioChanged=true;
      KEYBOARD[ShiftRegisterNumber+4]=true; 
    }else 
    {
      if(KEYBOARD[ShiftRegisterNumber+4]==true) HasArppeggioChanged=true;
      KEYBOARD[ShiftRegisterNumber+4]=false;
    }
    if(RegisterByte & 4) 
    {
      if(KEYBOARD[ShiftRegisterNumber+5]==false) HasArppeggioChanged=true;
      KEYBOARD[ShiftRegisterNumber+5]=true; 
    }else 
    {
      if(KEYBOARD[ShiftRegisterNumber+5]==true) HasArppeggioChanged=true;
      KEYBOARD[ShiftRegisterNumber+5]=false;
    }
    if(RegisterByte & 2) 
    {
      if(KEYBOARD[ShiftRegisterNumber+6]==false) HasArppeggioChanged=true;
      KEYBOARD[ShiftRegisterNumber+6]=true; 
    }else 
    {
      if(KEYBOARD[ShiftRegisterNumber+6]==true) HasArppeggioChanged=true;
      KEYBOARD[ShiftRegisterNumber+6]=false;
    }
    if(RegisterByte & 1) 
    {
      if(KEYBOARD[ShiftRegisterNumber+7]==false) HasArppeggioChanged=true;
      KEYBOARD[ShiftRegisterNumber+7]=true; 
    }else 
    {
      if(KEYBOARD[ShiftRegisterNumber+7]==true) HasArppeggioChanged=true;
      KEYBOARD[ShiftRegisterNumber+7]=false;
    }
  }
}




byte shiftIn(int myDataPin, int myClockPin) 
// Code pulled from Arduino.cc Shift In examples
{ 
  int i;
  int temp = 0;
  int pinState;
  byte myDataIn = 0;

  pinMode(myClockPin, OUTPUT);
  pinMode(myDataPin, INPUT);

//we will be holding the clock pin high 8 times (0,..,7) at the
//end of each time through the for loop

//at the begining of each loop when we set the clock low, it will
//be doing the necessary low to high drop to cause the shift
//register's DataPin to change state based on the value
//of the next bit in its serial information flow.
//The register transmits the information about the pins from pin 7 to pin 0
//so that is why our function counts down
  for (i=7; i>=0; i--)
  {
    digitalWrite(myClockPin, 0);
    //delayMicroseconds(2);
    temp = digitalRead(myDataPin);
    if (temp) {
      pinState = 1;
      //set the bit to 0 no matter what
      myDataIn = myDataIn | (1 << i);
    }
    else {
      //turn it off -- only necessary for debuging
     //print statement since myDataIn starts as 0
      pinState = 0;
    }

    //Debuging print statements
    //Serial.print(pinState);
    //Serial.print("     ");
    //Serial.println (dataIn, BIN);

    digitalWrite(myClockPin, 1);

  }
  //debuging print statements whitespace
  //Serial.println();
  //Serial.println(myDataIn, BIN);
  return myDataIn;

  
}
int GetAnalogValueForNote(KeyboardNote thisNote)
//Returns analog value normalized to 1024 per Octave of the note 
//passed in.
{
  //Get note matched to AnalogValue=1024/Oct Scale 
  int AnalogValue=0;
  double HalfStep=0;
  
  // Get note value for specific halfstep;
  HalfStep=thisNote.Note*83.17;
  AnalogValue+=int(HalfStep);
  AnalogValue+=int(thisNote.PitchBend*83.17);
  return AnalogValue;
}

void SetAnalogValue(int Value)  //  0 <= Value <= 1023
// This function used for pins in PWM Mode to create a 10 bit DAC.
// It fills up the pins based on Value.
{
  /*// Debug Serial Print Info
  Serial.print("\nAnalogValue (0<Value,1024): ");
  Serial.print(Value);
  */
  
  if(LastAnalogValue!=Value)
  {
  analogWrite(CV_Out_A, 0);
  analogWrite(CV_Out_B, 0);
  analogWrite(CV_Out_C, 0);
  analogWrite(CV_Out_D, 0);
  int Val_A=0;
  int Val_B=0;
  int Val_C=0;
  int Val_D=0;

  if(Value<=255)
  {
    //Value is <256 (<250mV) and therefore set directly here
  Val_A=Value;
  analogWrite(CV_Out_A, Val_A);
}else
  {
    //Value is >=256 (>=250mV) and therefore this pin is maxed.
  Val_A=255;
  analogWrite(CV_Out_A, Val_A);
  }
  
  if(Value<=511)
  {
    //Value is <512 (<500mV) and therefore set directly here
  Val_B=Value-256;
  if(Val_B<0) Val_B=0;
  analogWrite(CV_Out_B, Val_B);
  }else
  {
    //Value is >=512 (500mV) and therefore this pin is maxed.
  Val_B=255;
  analogWrite(CV_Out_B, Val_B);
  }
  
  if(Value<=767)
  {
    //Value is <768 (<750mV) and therefore set directly here
  Val_C=Value-512;
  if(Val_C<0) Val_C=0;
  analogWrite(CV_Out_C, Val_C);
  }else
  {
    //Value is >=768 (750mV) and therefore this pin (pin 8) is maxed.
  Val_C=255;
  Val_D=Value-768;
  if(Val_D<0) Val_D=0;
  analogWrite(CV_Out_C, Val_C);
  analogWrite(CV_Out_D, Val_D);
  }
  /*// Debug Serial Print Info
  Serial.print("\nCV_Out_A: ");
  Serial.print(Val_A);
  Serial.print("\nCV_Out_B: ");
  Serial.print(Val_B);
  Serial.print("\nCV_Out_C: ");
  Serial.print(Val_C);
  Serial.print("\nCV_Out_D: ");
  Serial.print(Val_D);  
  */
  LastAnalogValue=Value;
  }
}

void SetOctave(int Octave)
{
  int A;
  int B;
  int C;
  switch (Octave)
  {
    case 0:
      digitalWrite(Oct_Out_A,LOW);
      digitalWrite(Oct_Out_B,LOW);
      digitalWrite(Oct_Out_C,LOW);

      break;

    case 1:

      digitalWrite(Oct_Out_A,LOW);
      digitalWrite(Oct_Out_B,LOW);
      digitalWrite(Oct_Out_C,HIGH);
      break;

    case 2:

      digitalWrite(Oct_Out_A,LOW);
      digitalWrite(Oct_Out_B,HIGH);
      digitalWrite(Oct_Out_C,LOW);

      break;

    case 3:

      digitalWrite(Oct_Out_A,LOW);
      digitalWrite(Oct_Out_B,HIGH);
      digitalWrite(Oct_Out_C,HIGH);

      break;

    case 4:

      digitalWrite(Oct_Out_A,HIGH);
      digitalWrite(Oct_Out_B,LOW);
      digitalWrite(Oct_Out_C,LOW);

      break;

    case 5:

      digitalWrite(Oct_Out_A,HIGH);
      digitalWrite(Oct_Out_B,LOW);
      digitalWrite(Oct_Out_C,HIGH);

      break;

    case 6:

      digitalWrite(Oct_Out_A,HIGH);
      digitalWrite(Oct_Out_B,HIGH);
      digitalWrite(Oct_Out_C,LOW);

      break;

    case 7:

      digitalWrite(Oct_Out_A,HIGH);
      digitalWrite(Oct_Out_B,HIGH);
      digitalWrite(Oct_Out_C,HIGH);

      break;

    default:
      digitalWrite(Oct_Out_A,LOW);
      digitalWrite(Oct_Out_B,LOW);
      digitalWrite(Oct_Out_C,LOW);

  }
}

void SetLoadedAnalogValue (KeyboardNote thisNote)
{
  //This function reads a value between 0-calls SetOctave and 
  //SetAnalogValue
  SetAnalogValue(GetAnalogValueForNote(thisNote)); //0-1024
  SetOctave(thisNote.Octave);
}

void Arppegiate()
{
  
  //////   Determine up to 4 notes from the keyboard Array that will be used in the arppegiator //////

  int MaxKeysToApply=4;  // total number of keys possible to add to the arppeggiator
  int OriginalKeysApplied[MaxKeysToApply]={-1,-1,-1,-1};
  int KEYBOARD_Ctr=0; // Counter variable to reference indeces in KEYBOARD[]
  int KeysAppliedCtr=0; // Counter variable to reference indeces in OriginalKeysApplied[]
  while(KEYBOARD_Ctr<84 && KeysAppliedCtr <4)
  {
    if(KEYBOARD[KEYBOARD_Ctr]>0)
    {
      OriginalKeysApplied[KeysAppliedCtr]=KEYBOARD_Ctr;
      KeysAppliedCtr+=1;
    }
    KEYBOARD_Ctr+=1;
  }

  /////  Up to 4 original Notes to be used for Arppeggiator stored in OriginalKeysApplied[]
  /////  Array Positions unused will be populated with -1  
   /*Serial.println("Original Keys Applied");
  
  for (KeysAppliedCtr=0;KeysAppliedCtr<MaxKeysToApply;KeysAppliedCtr++)
  {
    Serial.print(OriginalKeysApplied[KeysAppliedCtr]);
    Serial.print(", ");
  }
  Serial.println("End"); 
  */
  /////  Preform Modulo 12 on all notes to get up to 4 notes scaled to same octave
  int ScaledKeysApplied[MaxKeysToApply]={-1,-1,-1,-1};
  KeysAppliedCtr=0; // Reset counter variable so it can be used to traverse ScaledKeysApplied[]
  while(KeysAppliedCtr<MaxKeysToApply &&OriginalKeysApplied[KeysAppliedCtr]>-1)
  {
    ScaledKeysApplied[KeysAppliedCtr]=OriginalKeysApplied[KeysAppliedCtr]%12;
    KeysAppliedCtr+=1;
  }
  /////  Up to 4 scaled notes in same octave stored in ScaledKeysApplied[]
  /////  Array Positions Unused will be populated with -1

   
   /*Serial.println("ModuloKeysApplied WITHOUT Dups Removed");
  
  for (KeysAppliedCtr=0;KeysAppliedCtr<MaxKeysToApply;KeysAppliedCtr++)
  {
    Serial.print(ScaledKeysApplied[KeysAppliedCtr]);
    Serial.print(", ");
  }
  Serial.println("End"); 
  */
  /////  Sort ScaledKeysApplied[] from smallest to largest
  //bubble sort
   int i, j, flag = 1;    // set flag to 1 to start first pass
      int temp;             // holding variable
      for(i = 1; (i <= MaxKeysToApply) && flag; i++)
     {
          flag = 0;
          for (j=0; j < (MaxKeysToApply -1); j++)
         {
               if (ScaledKeysApplied[j+1] < ScaledKeysApplied[j] 
                   && ScaledKeysApplied[j]>-1 && ScaledKeysApplied[j+1]>-1)      // ascending order simply changes to <
              { 
                    temp = ScaledKeysApplied[j];             // swap elements
                    ScaledKeysApplied[j] = ScaledKeysApplied[j+1];
                    ScaledKeysApplied[j+1] = temp;
                    flag = 1;               // indicates that a swap occurred.
               }
          }
     }
  

    /*Serial.println("ModuloKeysApplied SORTED WITHOUT Dups Removed");
  
  for (KeysAppliedCtr=0;KeysAppliedCtr<MaxKeysToApply;KeysAppliedCtr++)
  {
    Serial.print(ScaledKeysApplied[KeysAppliedCtr]);
    Serial.print(", ");
  }
  Serial.println("End");  
  */
  
  ///// Remove Duplicates from ScaledKeysApplied (This would occur when kids play an octave)
  int TempKeyArray[MaxKeysToApply]={-1,-1,-1,-1};
  KeysAppliedCtr=0; // Reset counter var for reuse traversing TempKeyArray[] and ScaledKeysApplied[]

  
  ///  Swap ScaledKeysApplied[] and TempKeyArray[]
  for(KeysAppliedCtr=0;KeysAppliedCtr<MaxKeysToApply;KeysAppliedCtr++)
  // Copy ScaledKeysApplied[] to TempKeyArray[]
  {
    TempKeyArray[KeysAppliedCtr]=ScaledKeysApplied[KeysAppliedCtr];
  }
  KeysAppliedCtr=0;// Reset counter again for next block
   

  
  for(KeysAppliedCtr=0;KeysAppliedCtr<MaxKeysToApply;KeysAppliedCtr++)
  // Reset ScaledKeysApplied[]
  {
    ScaledKeysApplied[KeysAppliedCtr]=-1;
  }
  KeysAppliedCtr=0;// Reset counter again for next block
  int DuplicatesArrayCtr=0;

/*Serial.println("TempKeyArray Swapped with ScaledKeysArray Values");
  
  for (KeysAppliedCtr=0;KeysAppliedCtr<MaxKeysToApply;KeysAppliedCtr++)
  {
    Serial.print(TempKeyArray[KeysAppliedCtr]);
    Serial.print(", ");
  }
  Serial.println("End");  
  
   
   Serial.println("ScaledKeysArray Reset");
  
  for (KeysAppliedCtr=0;KeysAppliedCtr<MaxKeysToApply;KeysAppliedCtr++)
  {
    Serial.print(ScaledKeysApplied[KeysAppliedCtr]);
    Serial.print(", ");
  }
  Serial.println("End");  
*/
  KeysAppliedCtr=0;
  while (KeysAppliedCtr<MaxKeysToApply && TempKeyArray[KeysAppliedCtr]>-1)
  {
    if(KeysAppliedCtr==0)
    // Allows first record to pass through
    {
       ScaledKeysApplied[DuplicatesArrayCtr]=TempKeyArray[KeysAppliedCtr];
       KeysAppliedCtr+=1;
       DuplicatesArrayCtr+=1;
    }
    
    if(TempKeyArray[KeysAppliedCtr]>TempKeyArray[KeysAppliedCtr-1])
    // Only Copied Record if it is greater than the record one index value back (Array is pre-sorted)
    {
       ScaledKeysApplied[DuplicatesArrayCtr]=TempKeyArray[KeysAppliedCtr];
       DuplicatesArrayCtr+=1;
    }
    KeysAppliedCtr+=1;
  }

  ///// Up to 4 scaled notes with duplicates removed stored in ScaledKeysApplied[]
  /*Serial.println("ScaledKeysApplied with Dups Removed");
  
  for (KeysAppliedCtr=0;KeysAppliedCtr<MaxKeysToApply;KeysAppliedCtr++)
  {
    Serial.print(ScaledKeysApplied[KeysAppliedCtr]);
    Serial.print(", ");
  }
  Serial.println("End");  
  */
///// Get new number of Keys Down with Duplicates removed (temporarily used in place of NumArpKeysDown)
  KeysAppliedCtr=0;
  while (KeysAppliedCtr<MaxKeysToApply && ScaledKeysApplied[KeysAppliedCtr]>-1)
  {
    //Serial.print("NumArpKeysDownNoDups = ");
    //Serial.println(KeysAppliedCtr);
    KeysAppliedCtr+=1;
  }
  int NumArpKeysDownNoDups=KeysAppliedCtr;
    //Serial.print("NumArpKeysDownNoDups = ");
    //Serial.println(NumArpKeysDownNoDups);

  ///// Establish ArpIntervals as Array of Intervals rather than absolute notes
  int ArpIntervals[MaxKeysToApply]={-1,-1,-1,-1};
  KeysAppliedCtr=0; // Reset counter variable so it can be used to traverse ScaledKeysApplied[]
  if(ScaledKeysApplied[KeysAppliedCtr]>-1 /* && ScaledKeysApplied[KeysAppliedCtr+1]>-1 */)
  {
    if(ScaledKeysApplied[KeysAppliedCtr+1]==-1)
    {
      ArpIntervals[KeysAppliedCtr]=ScaledKeysApplied[0]+12-ScaledKeysApplied[KeysAppliedCtr];
      KeysAppliedCtr+=1;      
    }else
    { //ScaledKeysApplied[KeysAppliedCtr+1]>-1  or there is still another record in front
      ArpIntervals[KeysAppliedCtr]=ScaledKeysApplied[KeysAppliedCtr+1]-ScaledKeysApplied[KeysAppliedCtr];
      KeysAppliedCtr+=1;      
    }

    while(KeysAppliedCtr<MaxKeysToApply &&ScaledKeysApplied[KeysAppliedCtr]>-1
          /*&& ScaledKeysApplied[KeysAppliedCtr+1]>-1*/)
    {
      if(ScaledKeysApplied[KeysAppliedCtr+1]==-1 || KeysAppliedCtr+1>=MaxKeysToApply)
      {
        ArpIntervals[KeysAppliedCtr]=ScaledKeysApplied[0]+12-ScaledKeysApplied[KeysAppliedCtr];
        KeysAppliedCtr+=1;      
      }else
      { //ScaledKeysApplied[KeysAppliedCtr+1]>-1  or there is still another record in front
        ArpIntervals[KeysAppliedCtr]=ScaledKeysApplied[KeysAppliedCtr+1]-ScaledKeysApplied[KeysAppliedCtr];
        KeysAppliedCtr+=1;      
      }
      /*ArpIntervals[KeysAppliedCtr]=ScaledKeysApplied[KeysAppliedCtr+1]-ScaledKeysApplied[KeysAppliedCtr];
      KeysAppliedCtr+=1;
      */
    }
  }

  /*Serial.println("ArpIntervals");
  
  for (KeysAppliedCtr=0;KeysAppliedCtr<MaxKeysToApply;KeysAppliedCtr++)
  {
    Serial.print(ArpIntervals[KeysAppliedCtr]);
    Serial.print(", ");
  }
  Serial.println("End");  
  */
  //ArpType=0; //0=Regular Up, 1=Regular Down, 2=Inversions Up, 3=Inversions Down based on pin read
  //NumOctaves=3; // 1,2,3, or 4  Based on Pin Read
  int StartingOctave=floor(OriginalKeysApplied[0]/12);
  KeysAppliedCtr=0;


  if(ArpType==0 || ArpType==1) // Regular Arppegio
  {
    ///// Build Regular Arppeggio Array
    for(i=0;i<NumOctaves;i++)
    {
      for(j=0;j<MaxKeysToApply;j++)
      {
        if(ScaledKeysApplied[j]>-1)
        {
          Arppeggiator[KeysAppliedCtr]=ScaledKeysApplied[j]+(i*12)+(StartingOctave*12);
          KeysAppliedCtr+=1;
        }
      }
    }
    //Serial.println("Final Arppeggiator Array in Regular Mode");
  }else // ArpType = 2 or 3 // Inversions Arppegio
  {
    ///// Build Inversions Array    
    j=0;  // ScaledKeysIndex
    int l=0;
    int offset=0; //inversion offset
    int prev_j=0;
    int k=0; //counts to MaxKaysToApply
    if(ScaledKeysApplied[j]>-1) 
    {
      Arppeggiator[KeysAppliedCtr]=ScaledKeysApplied[j]+(StartingOctave*12);
      j+=1;
      k+=1;
      KeysAppliedCtr+=1;
    }
    for(i=0;i<NumOctaves;i++)
    {
      while(k<NumArpKeysDownNoDups)
      {
      /*  Serial.print("i = ");
        Serial.print(i);
        Serial.print(",   j = ");
        Serial.print(j);
        Serial.print(",   prev_j = ");
        Serial.print(prev_j);
        Serial.print(",   k = ");
        Serial.print(k);
        Serial.print(",   l = ");
        Serial.print(l);  
        Serial.print(",   NumArpKeysDownNoDups = ");
        Serial.println(NumArpKeysDownNoDups);  
        Serial.print(",   Arppeggiator Index = ");
        Serial.println(KeysAppliedCtr);  
        */      
        if(j>=NumArpKeysDownNoDups) 
        {
          j=0;
        }
                
        if(ScaledKeysApplied[j]>-1 && KeysAppliedCtr<(NumOctaves*NumArpKeysDownNoDups))
        {
          if(j==0)
          {
            Arppeggiator[KeysAppliedCtr]=Arppeggiator[KeysAppliedCtr-1]+ArpIntervals[NumArpKeysDown-1];
            KeysAppliedCtr+=1;
            k+=1;
            prev_j=0;
            j+=1;
          }else
          {
            Arppeggiator[KeysAppliedCtr]=Arppeggiator[KeysAppliedCtr-1]+ArpIntervals[prev_j];
            KeysAppliedCtr+=1;
            k+=1;
            prev_j+=1;
            j+=1;
            //Serial.println("K Should be Incrememting");
          }          
        }
      }
     offset=offset+ArpIntervals[l];
     l+=1; // execute chord inversion
     j=l;
     k=0;
     //Serial.println("K is Reset");
      if(ScaledKeysApplied[j]>-1 && KeysAppliedCtr<(NumOctaves*NumArpKeysDownNoDups)) 
      {
        Arppeggiator[KeysAppliedCtr]=ScaledKeysApplied[j-l]+(StartingOctave*12)+offset;
        j+=1;
        prev_j=j-1;
        k+=1;
        KeysAppliedCtr+=1;
      }
 
    }    
    //Serial.println("Final Arppeggiator Array in Inversions Mode");
  }
//  Serial.println("Final Arppeggiator Array in Regular Mode");
  for(KeysAppliedCtr=0;KeysAppliedCtr<16;KeysAppliedCtr++)
  {
    Serial.print(Arppeggiator[KeysAppliedCtr]);
    Serial.print(" ");
  }
  Serial.println();
  
  




  
  HasArppeggioChanged=false;
}

int sizeofArpeggio()
{
  int i=0;
  while(i<16 && Arppeggiator[i]>-1)
  {
    i++;
  }  
  return i-1;
}

void loop()
{

  ArpDelay=GetArpSpeed()/3;

  int Note_Out = 0;
  int Octave=-1;
  KeyboardNote MyNote;
  MyNote.Octave = Octave;
  MyNote.PitchBend = 0;
  MyNote.NoteCode = Octave*12+Note_Out;
  bool isMIDIKeyOn=false;
  //ArppeggioOn=true;
  bool ArppegioForward=true; // Determines type of Arppegio Pass (Forward or Backward)
  if(ArpType==0 || ArpType==2) 
  {
    ArppegioForward=true;
  }else // This would occur is ArpType = 1 or 3 (Backwards)
  {
    ArppegioForward=false;
  }

  
  //delay(2000);
  if(!wasKeyBoardKeyPressed)
  {
     isMIDIKeyOn=checkMIDI(&MyNote);
     
     //if(checkMIDI(&MyNote)&&MyNote.Octave!=-1) 
  }
  checkKeyboard(&MyNote);
  if(!ArppeggioOn)
  {   
    
    if((isMIDIKeyOn &&MyNote.Octave!=-1)||(wasKeyBoardKeyPressed))
    {
      if(wasKeyBoardKeyPressed) MyNote.Octave+=KeyboardOctaveSelect;
      //Serial.print("MyNote->Note = ");
      //Serial.print(MyNote.Note);
      //Serial.print("     MyNote->Octave = ");
      //Serial.println(MyNote.Octave);
    
      SetLoadedAnalogValue(MyNote);
    
      //delay (2000);
    }
  }else
  {
    /*Serial.print("NumArpKeysDown = ");
    Serial.println(NumArpKeysDown);
    Serial.print("Arppeggiator[0] = ");
    Serial.println(Arppeggiator[0]);
    Serial.print("HasArppeggioChanged = ");
    Serial.println(HasArppeggioChanged);
    */
    if(NumArpKeysDown>0 && Arppeggiator[0]==-1 && HasArppeggioChanged==true || MasterArp==true) 
    {
      //Serial.print("Arppegiating...");
    
      Arppegiate();  //Builds Arppegio if a note has changed and a key is down
      MasterArp=false;
    }
    if (ArppegioForward)
      {
        int noteByte=0;
        noteByte=Arppeggiator[ArpLocation];
        if(noteByte<0 || ArpLocation>=(NumOctaves*NumArpKeysDown))
        {
          ArpLocation=0;
        }else
        {
          MyNote.Note = noteByte % 12;
          MyNote.Octave = int(noteByte/12)+KeyboardOctaveSelect;
          MyNote.PitchBend=0;
          if(noteByte>-1 || ArpLocation<(NumOctaves*NumArpKeysDown))
          {
            SetLoadedAnalogValue(MyNote);
            delay(ArpDelay);
            ArpLocation+=1;

          }
        }
      }else
      {
        int noteByte=0;
        noteByte=Arppeggiator[ArpLocation];
        if(noteByte<0)
        {
          ArpLocation=sizeofArpeggio();
        }else
        {
          MyNote.Note = noteByte % 12;
          MyNote.Octave = int(noteByte/12)+KeyboardOctaveSelect;
          MyNote.PitchBend=0;
          if(noteByte>-1)
          {
            SetLoadedAnalogValue(MyNote);
            delay(ArpDelay);
           //Serial.print("ArpLocation = ");
            //Serial.print(ArpLocation);
            //Serial.print (", Size of Arppegio = ");
            //Serial.println(sizeofArpeggio());
            
            if(ArpLocation==0) 
            {
              ArpLocation=sizeofArpeggio();
            }else
            {
              ArpLocation-=1;
            }
          }
        }
      }
        
    //PlayArppegio(&MyNote);
  
  }
  //  SetLoadedAnalogValue(MyNote);

}

