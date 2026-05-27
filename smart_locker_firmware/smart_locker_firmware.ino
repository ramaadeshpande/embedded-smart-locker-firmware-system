#include <EEPROM.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <avr/power.h>
#include <avr/wdt.h>
const int pin = 9;  //pin 9 used on arduino
//from sg90 datasheet(lock(-90) = 1ms, unlock(90) = 2ms, position0(0) = 1.5ms)
const int lockNeg90Deg = 1000;
const int unlock90Deg = 2000;
const int pos0Deg = 1500;              //centered
const int period = 20000;              //20ms period(from datasheet)
unsigned long movementStartTime = 0;   //time since servo started to move
const unsigned long normalTime = 750;  //time taken for normal servo movement
const unsigned long maxTime = 1000;    //max time allowed before signaling blockage


int buttCount = 0;


const int salt = 0;
const int transform = 4;
const char pepper[] = "ECE2804";


static unsigned long lastKeypressTime = 0;
#define SLEEP_TIMEOUT_MS 5000UL


//creates a 20ms PWM cycle
//sets high then low for remaining time to reach total of 20ms/period
void pulse(int time) {
  digitalWrite(pin, HIGH);
  delayMicroseconds(time);  //keeps high
  digitalWrite(pin, LOW);
  delayMicroseconds(period - time);  //keeps low for remaining time
}


//sends pulse to servo for set time cycles
void holdServo(int pulseWidth, int cycles) {
  for (int i = 0; i < cycles; i++) {
    pulse(pulseWidth);
  }
}


//when right password is entered
void unlockServo() {
  holdServo(unlock90Deg, 100);
}


//when lock button is pressed
void lockServo() {
  holdServo(lockNeg90Deg, 100);
}


//make pin 2 lock button
const int lockButtonPin = 2;




//state machine(locked/unlocked and busy states)
enum State { LOCKED,
             UNLOCKED,
             UNLOCK_TO_LOCK,
             LOCK_TO_UNLOCK };
State state = LOCKED;




//Creates a random salt value using analog noise from an input pin
uint32_t generateSalt() {
  //random using noise from analog pins
  randomSeed(analogRead(A0));
  uint32_t saltVal = random(1, 100000);  //avoids 0 which weakens transformation
  return saltVal;
}


//Converts pepper defined as string to ASCII
uint32_t getPepper() {
  uint32_t pepperVal = 0;


  for (int i = 0; pepper[i] != '\0'; i++) {
    pepperVal += pepper[i];  //adds all ASCII values
  }


  return pepperVal;
}
//Converts a 4-digit password into a single integer value and combines with the salt
uint32_t transformPassword(int digits[4], uint32_t saltVal) {
  uint32_t password = digits[0] * 1000 + digits[1] * 100 + digits[2] * 10 + digits[3];  //convert array to 4 digit num
  //salt
  uint32_t transformedPassword = password + saltVal;
  //pepper
  uint32_t pepperVal = getPepper();
  transformedPassword ^= pepperVal;
  transformedPassword = transformedPassword ^ (transformedPassword << 5);  //shift left 5 and XOR
  transformedPassword = transformedPassword ^ (transformedPassword >> 3);  //shift right 3 and XOR
  transformedPassword = transformedPassword ^ (transformedPassword << 7);  //shift left 7 and XOR
  transformedPassword ^= (transformedPassword << 11);
  transformedPassword += 12345;
  transformedPassword ^= (transformedPassword >> 7);
  return transformedPassword;
}


void setup() {
  // put your setup code here, to run once:
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delay(2000);


  pinMode(A0, INPUT);
  pinMode(A1, INPUT);
  pinMode(A2, INPUT);
  pinMode(A3, INPUT);
  pinMode(A4, INPUT);
  pinMode(13, OUTPUT);
  pinMode(12, OUTPUT);
  pinMode(3, OUTPUT);
  Serial.begin(9600);
  Serial.println("test");


  PCICR |= (1 << PCIE1);
  PCMSK1 |= (1 << PCINT8) | (1 << PCINT9) | (1 << PCINT10) | (1 << PCINT11);
  lastKeypressTime = millis();


  pinMode(lockButtonPin, INPUT_PULLUP);  //pressed button is LOW
  lockServo();                           //start at locked position


  pinMode(10, INPUT_PULLUP);
}




bool checkBattery() {
  bool good = true;
  digitalWrite(3, HIGH);


  double batVol = analogRead(4);


  digitalWrite(3, LOW);


  batVol = (batVol * 5) / 1024;


  if (batVol <= 3.5) {
    good = false;
  } else {
    good = true;
  }


  return good;
}


void redLight() {
  digitalWrite(12, HIGH);
  delay(100);
  digitalWrite(12, LOW);
  delay(100);
  digitalWrite(12, HIGH);
  delay(100);
  digitalWrite(12, LOW);
  delay(100);
  digitalWrite(12, HIGH);
  delay(100);
  digitalWrite(12, LOW);
}


void greenLight() {
  digitalWrite(13, HIGH);
  delay(100);
  digitalWrite(13, LOW);
  delay(100);
  digitalWrite(13, HIGH);
  delay(100);
  digitalWrite(13, LOW);
  delay(100);
  digitalWrite(13, HIGH);
  delay(100);
  digitalWrite(13, LOW);
}


void bothLight() {
  digitalWrite(13, HIGH);
  digitalWrite(12, HIGH);
  delay(100);
  digitalWrite(13, LOW);
  digitalWrite(12, LOW);
  delay(100);
  digitalWrite(13, HIGH);
  digitalWrite(12, HIGH);
  delay(100);
  digitalWrite(13, LOW);
  digitalWrite(12, LOW);
  delay(100);
  digitalWrite(13, HIGH);
  digitalWrite(12, HIGH);
  delay(100);
  digitalWrite(13, LOW);
  digitalWrite(12, LOW);
}


void lockedStateLight() {
  digitalWrite(12, HIGH);  //turn on red
  digitalWrite(13, LOW);   //turn off green
}


void unlockedStateLight() {
  digitalWrite(12, LOW);
  digitalWrite(13, HIGH);
}


void busyStateLight() {
  digitalWrite(12, HIGH);
  digitalWrite(13, HIGH);
}


void offSleepStateLight() {
  digitalWrite(12, LOW);
  digitalWrite(13, LOW);
}


static int resetting = 1;
static int resCount = 0;
static int Rcode[4];
static int Tcode[4];
static bool Rpreviousin = false;
static bool Rlastloop = false;
static int Rtimer = 0;




void resetPass() {
  Rpreviousin = false;
  Rlastloop = false;
  Rtimer = 0;
  resCount = 0;
  Rcode[0] = Tcode[0];
  Rcode[1] = Tcode[1];
  Rcode[2] = Tcode[2];
  Rcode[3] = Tcode[3];


  Serial.println("Resetting Password");
  busyStateLight();
  while (resetting == 1) {
    int pin1 = analogRead(A0);
    int pin2 = analogRead(A1);
    int pin3 = analogRead(A2);
    int pin4 = analogRead(A3);


    int inv1 = pin1 / 9;
    int inv2 = pin2 / 9;
    int inv3 = pin3 / 9;
    int inv4 = pin4 / 9;


    int val = 0;
    if (inv4 > 5) {
      val = inv4;
    } else if (inv3 > 5) {
      val = inv3;
    } else if (inv2 > 5) {
      val = inv2;
    } else if (inv1 > 5) {
      val = inv1;
    }


    if (val > 0 && !Rpreviousin) {
      Rpreviousin = true;
      Rtimer = 0;
    }


    if (Rpreviousin && !Rlastloop) {
      bool pound = false;


      //row 1
      if (inv4 > 100 && inv4 < 111) {
        Rcode[resCount] = 1;
      } else if (inv4 > 111 && inv4 < 120) {
        Rcode[resCount] = 2;
      } else if (inv4 > 84 && inv4 < 87) {
        Rcode[resCount] = 3;
      }  //row 2
      else if (inv3 > 68 && inv3 < 71) {
        Rcode[resCount] = 4;
      } else if (inv3 > 74 && inv3 < 78) {
        Rcode[resCount] = 5;
      } else if (inv3 > 57 && inv3 < 62) {
        Rcode[resCount] = 6;
      }  //row 3
      else if (inv2 > 87 && inv2 < 91) {
        Rcode[resCount] = 7;
      } else if (inv2 > 98 && inv2 < 103) {
        Rcode[resCount] = 8;
      } else if (inv2 > 69 && inv2 < 75) {
        Rcode[resCount] = 9;
      }  //row 4
      else if (inv1 > 108 && inv1 < 111) {


        //if pressing * again, cancel the reset.
        Serial.println("");
        Serial.println("Exiting Password Reset: Exit Button");
        resCount = 0;
        return;
      } else if (inv1 > 111 && inv1 < 115) {
        Rcode[resCount] = 0;
      } else if (inv1 > 84 && inv1 < 89) {
        resCount = -1;
        Serial.println("");
        Serial.println("----- Reset -----");
        pound = true;
      }


      if (!pound) {
        Serial.print(Rcode[resCount]);
      }
      resCount++;
      pound = false;
    }


    if (val < 5) {
      Rpreviousin = false;
    }


    if (Rtimer >= 150) {
      Serial.println("");
      Serial.println("Exiting Password Reset: Timeout");
      bothLight();
      Rtimer = 0;
      resCount = 0;
      return;
    }


    if (resCount == 4) {
      resCount = 0;
      int Rcheck = 0;
      //check if valid code
      for (int i = 0; i < 4; i++) {
        if (Rcode[i] >= 0 && Rcode[i] <= 9) {
          Rcheck++;
        } else {
          Rcheck = 0;
        }
      }


      //if yes store in mem
      if (Rcheck == 4) {
        Serial.println("");
        Serial.println("Password Reset Successfully");
        greenLight();


        //for testing purposes
        uint32_t saltVal = generateSalt();
        uint32_t transformedPassword = transformPassword(Rcode, saltVal);
        //UPDATED FOR WEEK 12
        uint32_t transformedPassword2 = transformPassword(Rcode, saltVal + 12345);
        //prints each entered digit
        Serial.print("Entered password: ");
        Serial.print(Rcode[0]);
        Serial.print(Rcode[1]);
        Serial.print(Rcode[2]);
        Serial.println(Rcode[3]);


        uint32_t pepperVal = getPepper();


        EEPROM.update(salt, (saltVal >> 24) & 0xFF);      //first byte(most significant)
        EEPROM.update(salt + 1, (saltVal >> 16) & 0xFF);  //second byte
        EEPROM.update(salt + 2, (saltVal >> 8) & 0xFF);   //third byte
        EEPROM.update(salt + 3, saltVal & 0xFF);          //fourth byte(least significant)




        EEPROM.update(transform + 4, (transformedPassword2 >> 24) & 0xFF);  //first byte(most significant)
        EEPROM.update(transform + 5, (transformedPassword2 >> 16) & 0xFF);  //second byte
        EEPROM.update(transform + 6, (transformedPassword2 >> 8) & 0xFF);   //third byte
        EEPROM.update(transform + 7, transformedPassword2 & 0xFF);          //fourth byte


        EEPROM.update(transform, (transformedPassword >> 24) & 0xFF);
        EEPROM.update(transform + 1, (transformedPassword >> 16) & 0xFF);
        EEPROM.update(transform + 2, (transformedPassword >> 8) & 0xFF);
        EEPROM.update(transform + 3, transformedPassword & 0xFF);


        bothLight();
        return;
      } else {
        Serial.println("");
        Serial.println("Try a different Password");
        resCount = 0;
      }
      //if not reset counter and new code red
    }


    Rlastloop = Rpreviousin;
    delay(100);
    Rtimer++;
  }
}


ISR(PCINT1_vect) {
}


void goToSleep() {
  digitalWrite(13, LOW);
  digitalWrite(12, LOW);
  power_adc_disable();
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sei();
  sleep_cpu();
  sleep_disable();
  power_adc_enable();
  lastKeypressTime = millis();
}


static int counter = 0;
static int code[4];  //user entered password
static bool previousin = false;
static bool lastloop = false;
static int pinPCount = 0;
static int RESBUT = 0;


void loop() {


  if (state == LOCK_TO_UNLOCK) {  //move servo to unlocked position
    busyStateLight();
    if (movementStartTime == 0) {
      movementStartTime = millis();
    }
    pulse(unlock90Deg);
    unsigned long time = millis() - movementStartTime;
    //timing evidence
    /*
      //fake blocking condition for testing
      if (time >= 500 && time < 700) {
        Serial.println("Block detected. Relocking.");
        state = LOCKED;
        movementStartTime = 0;
        return;
      }
      */
    if (time >= maxTime)  //check for block
    {
      Serial.println("Block detected. Relocking.");
      state = LOCKED;  //return to locked state
      movementStartTime = 0;
      return;
    } else if (time >= normalTime && time < maxTime) {
      Serial.println("Unlocked.");
      state = UNLOCKED;
      movementStartTime = 0;
      return;
    }
  }


  if (state == UNLOCK_TO_LOCK) {  //move servo to locked position
    busyStateLight();
    if (movementStartTime == 0) {
      movementStartTime = millis();
    }
    pulse(lockNeg90Deg);
    unsigned long time = millis() - movementStartTime;


    if (time >= maxTime)  //check for block
    {
      Serial.println("Block detected. Unable to lock.");
      state = UNLOCKED;  //return to unlocked state
      movementStartTime = 0;
      return;
    } else if (time >= normalTime && time < maxTime) {
      Serial.println("Locked.");
      state = LOCKED;
      movementStartTime = 0;
      return;
    }
  }


  if (state == UNLOCKED) {
    unlockedStateLight();


    if (pinPCount >= 2) {
      pinPCount = 0;
      bothLight();
      resetPass();
    }
    int pinP = analogRead(A0) / 9;
    if (pinP > 5 && pinP < 28) {
      pinPCount++;
      delay(1000);
    }


    if (digitalRead(lockButtonPin) == LOW) {
      Serial.println("Lock button pressed -> LOCKED");
      movementStartTime = millis();
      state = UNLOCK_TO_LOCK;
      counter = 0;  //reset counter
    }
    return;  //to ignore keypad when unlocked
  }


  lockedStateLight();
  //locked state (Athan's code)
  int pin1 = analogRead(A0);
  int pin2 = analogRead(A1);
  int pin3 = analogRead(A2);
  int pin4 = analogRead(A3);




  if (digitalRead(10) == LOW) {
    buttCount++;
    delay(1000);
  }




  if (buttCount >= 2) {
    buttCount = 0;
    resetPass();
  }


  int inv1 = pin1 / 9;
  int inv2 = pin2 / 9;
  int inv3 = pin3 / 9;
  int inv4 = pin4 / 9;


  int val = 0;
  if (inv4 > 5) {
    val = inv4;
  } else if (inv3 > 5) {
    val = inv3;
  } else if (inv2 > 5) {
    val = inv2;
  } else if (inv1 > 5) {
    val = inv1;
  }




  if (val > 0 && !previousin) {


    previousin = true;
    lastKeypressTime = millis();
    Serial.println(val);
  }




  if (previousin && !lastloop) {
    bool pound = false;
    //row 1
    if (inv4 > 100 && inv4 < 111) {
      code[counter] = 1;
    } else if (inv4 > 111 && inv4 < 120) {
      code[counter] = 2;
    } else if (inv4 > 84 && inv4 < 87) {
      code[counter] = 3;
    }  //row 2
    else if (inv3 > 68 && inv3 < 71) {
      code[counter] = 4;
    } else if (inv3 > 74 && inv3 < 78) {
      code[counter] = 5;
    } else if (inv3 > 57 && inv3 < 62) {
      code[counter] = 6;
    }  //row 3
    else if (inv2 > 87 && inv2 < 91) {
      code[counter] = 7;
    } else if (inv2 > 98 && inv2 < 103) {
      code[counter] = 8;
    } else if (inv2 > 69 && inv2 < 75) {
      code[counter] = 9;
    }  //row 4
    else if (inv1 > 108 && inv1 < 111) {
      //
    } else if (inv1 > 111 && inv1 < 115) {
      code[counter] = 0;
    } else if (inv1 > 84 && inv1 < 89) {
      counter = -1;
      Serial.println("");
      Serial.println("----- Reset -----");
      pound = true;
    }




    if (!pound) {
      Serial.print(code[counter]);
    }
    counter++;
    pound = false;
  }




  if (val < 5) {
    previousin = false;
  }


  if (counter == 1) {
    bool battery = checkBattery();


    if (!battery) {
      bothLight();
    }
  }


  if (counter == 4) {
    counter = 0;


    uint32_t saltVal = 0;
    saltVal |= ((uint32_t)EEPROM.read(salt) << 24);
    saltVal |= ((uint32_t)EEPROM.read(salt + 1) << 16);
    saltVal |= ((uint32_t)EEPROM.read(salt + 2) << 8);
    saltVal |= (uint32_t)EEPROM.read(salt + 3);


    uint32_t transformedPassword = 0;
    transformedPassword |= ((uint32_t)EEPROM.read(transform) << 24);
    transformedPassword |= ((uint32_t)EEPROM.read(transform + 1) << 16);
    transformedPassword |= ((uint32_t)EEPROM.read(transform + 2) << 8);
    transformedPassword |= (uint32_t)EEPROM.read(transform + 3);


    uint32_t transformedPassword2 = 0;
    transformedPassword2 |= ((uint32_t)EEPROM.read(transform + 4) << 24);
    transformedPassword2 |= ((uint32_t)EEPROM.read(transform + 5) << 16);
    transformedPassword2 |= ((uint32_t)EEPROM.read(transform + 6) << 8);
    transformedPassword2 |= (uint32_t)EEPROM.read(transform + 7);


    uint32_t enteredTransformedPass = transformPassword(code, saltVal);
    //UPDATED FOR WEEK 12
    uint32_t enteredTransformedPass2 = transformPassword(code, saltVal + 12345);


    Serial.println("");
    Serial.println("----- Attempt -----");


    // print entered password
    Serial.print("Entered password: ");
    Serial.print(code[0]);
    Serial.print(code[1]);
    Serial.print(code[2]);
    Serial.println(code[3]);


    if (enteredTransformedPass == transformedPassword && enteredTransformedPass2 == transformedPassword2) {
      Serial.println("----- Correct -----");
      Serial.println("FORCING UNLOCK NOW");
      unlockServo();     // directly send 2000 us pulses
      state = UNLOCKED;  // mark it unlocked after movement
      counter = 0;
    } else {
      Serial.println("----- Incorrect -----");
      redLight();
    }
  }


  if ((millis() - lastKeypressTime >= SLEEP_TIMEOUT_MS) && state != UNLOCKED && state != UNLOCK_TO_LOCK) {
    Serial.println("Sleeping...");
    Serial.flush();
    goToSleep();
  }


  lastloop = previousin;
  delay(100);
}
