const int pin = 9; //pin 9 used on arduino
//from sg90 datasheet(lock(-90) = 1ms, unlock(90) = 2ms, position0(0) = 1.5ms)
const int lockNeg90Deg = 1000;
const int unlock90Deg = 2000;
const int pos0Deg = 1500; //centered
const int period = 20000; //20ms period(from datasheet)

//creates a 20ms PWM cycle
//sets high then low for remaining time to reach total of 20ms/period
void pulse(int time)
{
  digitalWrite(pin, HIGH);
  delayMicroseconds(time); //keeps high
  digitalWrite(pin, LOW);
  delayMicroseconds(period - time); //keeps low for remaining time
}

//sends pulse to servo for set time cycles
void holdServo(int pulseWidth, int cycles)
{
  for (int i = 0; i < cycles; i++)
  {
    pulse(pulseWidth);
  }
}

//when right password is entered
void unlockServo()
{
  holdServo(unlock90Deg, 100);
}

//when lock button is pressed
void lockServo()
{
  holdServo(lockNeg90Deg, 100);
}

//make pin 2 lock button
const int lockButtonPin = 2;


//state machine(only locked/unlocked)
enum State { LOCKED, UNLOCKED };
State state = LOCKED;

//from Athan's code
static int correctcode[4] = {1,2,3,4};
static int counter = 0;
static int code[4];
static bool previousin = false;
static bool lastloop = false;

void setup() {
  // put your setup code here, to run once:
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delay(2000);

  pinMode(A0, INPUT);
  pinMode(A1, INPUT);
  pinMode(A2, INPUT);
  pinMode(A3, INPUT);
  pinMode(13, OUTPUT);
  pinMode(12, OUTPUT);
  Serial.begin(9600);
  Serial.println("test");


pinMode(lockButtonPin, INPUT_PULLUP); //pressed button is LOW
lockServo();   //start at locked position
}

void loop() {
  if (state == UNLOCKED)
  {
    if (digitalRead(lockButtonPin) == LOW)
    {
      Serial.println("Lock button pressed -> LOCKED");
      lockServo();
      state = LOCKED;
      counter = 0; //reset counter
    }
    return; //to ignore keypad when unlocked
  }

  //locked state (Athan's code)
  int pin1 = analogRead(A0);
  int pin2 = analogRead(A1);
  int pin3 = analogRead(A2);
  int pin4 = analogRead(A3);


  int inv1 = pin1 / 9;
  int inv2 = pin2 / 9;
  int inv3 = pin3 / 9;
  int inv4 = pin4 / 9;


  int val = 0;
  if(inv4 > 5)
  {
    val = inv4;
  }
  else if(inv3 > 5)
  {
    val = inv3;
  }
  else if(inv2 > 5)
  {
    val = inv2;
  }
  else if(inv1 > 5)
  {
    val = inv1;
  }




  if(val > 0 && !previousin)
  {
    previousin = true;
  }


  if(previousin && !lastloop){
    bool pound = false;
  //row 1
  if(inv4 > 5 && inv4 < 28)
  {
    code[counter] = 1;
  }
  else if(inv4 > 28 && inv4 < 60)
  {
    code[counter] = 2;
  }
  else if(inv4 > 60 && inv4 < 110)
  {
    code[counter] = 3;
  }//row 2
  else if(inv3 > 3 && inv3 < 20)
  {
    code[counter] = 4;
  }
  else if(inv3 > 20 && inv3 < 44)
  {
    code[counter] = 5;
  }
  else if(inv3 > 44 && inv3 < 80)
  {
    code[counter] = 6;
  }//row 3
  else if(inv2 > 0 && inv2 < 15)
  {
    code[counter] = 7;
  }
  else if(inv2 > 15 && inv2 < 42)
  {
    code[counter] = 8;
  }
  else if(inv2 > 42 && inv2 < 80)
  {
    code[counter] = 9;
  }//row 4
  else if(inv1 > 5 && inv1 < 28)
  {
    //
  }
  else if(inv1 > 28 && inv1 < 60)
  {
    code[counter] = 0;
  }
  else if(inv1 > 60 && inv1 < 110)
  {
    counter = -1;
    Serial.println("");
    Serial.println("----- Reset -----");
    pound = true;
  }

  if(!pound){
  Serial.print(code[counter]);
  }
  counter++;
  pound = false;
  }


  if(val < 5)
  {
    previousin = false;
  }


  if(counter == 4)
  {
    counter = 0;
    int check = 0;
    for(int i = 0; i < 4; i++)
    {
      if(code[i] != correctcode[i])
      {
        check = 0;
      }
      else
      {
        check++;
      }
    }


      if(check == 4)
      {
          Serial.println("");
          Serial.println("----- Correct -----");
          Serial.println("----- UNLOCKED -----");
          digitalWrite(13, HIGH);
          delay(300);
          digitalWrite(13, LOW);
          delay(300);
          digitalWrite(13, HIGH);
          delay(300);
          digitalWrite(13, LOW);
          delay(300);
          digitalWrite(13, HIGH);
          delay(300);
          digitalWrite(13, LOW);
          unlockServo();
          state = UNLOCKED;
      }
      else
      {
        Serial.println("");
        Serial.println("----- Incorrect -----");
        digitalWrite(12, HIGH);
        delay(300);
        digitalWrite(12, LOW);
        delay(300);
        digitalWrite(12, HIGH);
        delay(300);
        digitalWrite(12, LOW);
        delay(300);
        digitalWrite(12, HIGH);
        delay(300);
        digitalWrite(12, LOW);
      }
  }
  lastloop = previousin;
  delay(100);
  
  
}