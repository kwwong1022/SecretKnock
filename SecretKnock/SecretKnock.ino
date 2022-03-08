/** Requirements:                                                                    */
/** Proper debouncing without a blocking delay                                       */
/** Use an external interrupt and call the function above                            */
/** Make the solenoid move as the green led goes on, or do something original        */
/** Press a button to enter ”record mode”                                            */
/** You have, say, 5 seconds to create your sequence of taps                         */
/** Wait until a first knock, then start recording the time intervals between knocks */
/** using millis(), and save them into an unsigned long array                        */
/** If there is no knock detected after 5 sec (for instance), go to listening mode.  */

// library
#include <LCD.h>                // for LCD
#include <Wire.h>               // for LCD
#include <LiquidCrystal_I2C.h>  // for LCD
LiquidCrystal_I2C lcd(0x20, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  // LCD: A4, A5
#include <Servo.h>              // for servo motor
Servo lockServo;
#include <Servo.h>

// pin
int buzzerPin = 8;
int servoPin = A3;
int btnOKPin = 7;
int led[] = {3, 5, 6};  // [0]: green - correct/unlocked, [1]: red - wrong, [2]: yellow - recording/listening

// system mode
const int MODE_UNLOCK = 0, MODE_RECORD = 1, MODE_LOCK = 2, MODE_LISTEN = 3;
int mode = MODE_UNLOCK;

// click detection with external interrupt debounce, ref: lecture 5 note
const int MAX_BOUNCE_TIME = 100;
int pinInterrupt = 2;  // btn pin
volatile bool buttonHasBeenPressed = false;  // global variable that will changed by the ISR

// global const & variable
const String LCD_EMPTY = "                ";
const int MSG_INIT = -1, MSG_EMPTY = 0, MSG_UNLOCK = 1, MSG_LOCK = 2, MSG_RECORD = 3, MSG_LISTEN = 4;
const int ANGLE_UNLOCK = 0, ANGLE_LOCK = 90;
unsigned long pms = 0, ms = 0;

String recordedPattern = "", inputPattern = "";
int noInputCount = 0;
int patternThreshold = 400;  // ms

bool warming = false;


void setup() {
  Serial.begin(9600);
  
  // click detection with external interrupt debounce, ref: lecture 5 note
  attachInterrupt(digitalPinToInterrupt(pinInterrupt), checkButtonISR, FALLING);
  
  initialization();
}

int getPatternLength(String pattern) {
  int len = 1;
  for (int i=0; i<pattern.length(); i++) {
    if (pattern.charAt(i) == ' ') {
      len++;
    }
  }
  return len;
}

int getMinNum(int param1, int param2) {
  return param1<param2? param1:param2;
}

int getMaxNum(int param1, int param2) {
  return param1>param2? param1:param2;
}

bool patternIsValid() {
  // split pattern with space (" "), extract recordedPattern & input pattern into an array
  // comparison between recorded pattern & input pattern
  
  // 1. get length
  int recordedPatternLen = getPatternLength(recordedPattern);
  int inputPatternLen = getPatternLength(inputPattern);
  
  // 2. create new array with same length
  long recordedArr[recordedPatternLen];
  long inputArr[inputPatternLen];
  
  // 3. reassign time in to each array elements
  int counter = 0;  // patternArr[i]
  String timeStr = "";
  
  // 3.1 recordedArr
  for (int i=0; i<recordedPattern.length(); i++) {
    if (recordedPattern.charAt(i) != ' ') {
      timeStr += recordedPattern.charAt(i);  // concat timeStr
    } else if (recordedPattern.charAt(i) == ' ') {
      unsigned long temp = timeStr.toInt();
      recordedArr[counter] = temp;  // save the time to array
      timeStr = "";  // reset timeStr
      counter++;     // next timeStr
    }
  }
  counter = 0;
  for (int i=0; i<inputPattern.length(); i++) {
    if (inputPattern.charAt(i) != ' ') {
      timeStr += inputPattern.charAt(i);  // concat timeStr
    } else if (inputPattern.charAt(i) == ' ') {
      unsigned long temp = timeStr.toInt();
      inputArr[counter] = temp;  // save the time to array
      timeStr = "";  // reset timeStr
      counter++;     // next timeStr
    }
  }

  Serial.println("recordedStr: " + recordedPattern);
  Serial.println("inputStr: " + inputPattern);
  
  // 4. comparison
  // 4.1 - length
  if (recordedPatternLen == inputPatternLen) {
    // check each time different: (arr1[i+1] - arr1[i]) - (arr0[i+1] - arr0[i]) if they are within the threshold
    int match = 0;
    for (int i=0; i<recordedPatternLen-2; i++) {
      int timeDiffRecord = recordedArr[i+1] - recordedArr[i];
      int timeDiffInput = inputArr[i+1] - inputArr[i];
      Serial.println("");
      Serial.print(getMaxNum(timeDiffRecord, timeDiffInput));
      Serial.print(" - ");
      Serial.print(getMinNum(timeDiffRecord, timeDiffInput));
      Serial.print(" = ");
      Serial.print(getMaxNum(timeDiffRecord, timeDiffInput) - getMinNum(timeDiffRecord, timeDiffInput));
      if (getMaxNum(timeDiffRecord, timeDiffInput) - getMinNum(timeDiffRecord, timeDiffInput) < patternThreshold) match++;
    }

    if (match == recordedPatternLen-2) {
      Serial.println("UNLOCKED");
      return true;
    }
  }
  
  return false;
}

void loop() {
  ms = millis();
  
  // LED indicator
  ledIndicator();

  // timer - 1s
  if (ms - pms > 1000) {
    //Serial.println("1 second passed");
    if (mode == MODE_RECORD || mode == MODE_LISTEN) {
      noInputCount++;
      // if no more input in 5 seconds, save the pattern and lock
      if (noInputCount >= 5) {
        switch (mode) {
          case MODE_RECORD:
            // to make sure there are enough tap to cal the time different
            if (getPatternLength(recordedPattern) > 3) { 
              mode = MODE_LOCK;
              lock();
              lcdMsg(MSG_EMPTY);
              lcdMsg(MSG_LOCK);
 
            } else {
              // if not enough tap to form a pattern
              mode = MODE_UNLOCK;  // back to unlock mode, let the user record pattern again
              lcdMsg(MSG_EMPTY);
              lcdMsg(MSG_UNLOCK);
            }
            break;
            
          case MODE_LISTEN:
            // pattern validation - patternIsValid()
            // if input pattern correct (input pattern == recorded pattern)
            if (patternIsValid()) {  // vaild -> mode = MODE_UNLOCK, unlock()
              mode = MODE_UNLOCK;
              lcdMsg(MSG_EMPTY);
              lcdMsg(MSG_UNLOCK);
              unlock();
            } else {
              mode = MODE_LOCK;    // invalid -> mode = MODE_LOCK, beep()
              lcdMsg(MSG_EMPTY);
              lcdMsg(MSG_LOCK);
            }
            break;
        }
        noInputCount = 0;
      }
    }
    
    pms = ms;
  }

  // click detection with external interrupt debounce, ref: lecture 5 note
  if (buttonHasBeenPressed) {
    //Serial.println("button 2 pressed");

    switch (mode) {
      case MODE_UNLOCK:
        mode = MODE_RECORD;    // when user start recording pattern
        recordedPattern = String(ms) + " ";  // record the first tap
        lcdMsg(MSG_EMPTY);
        lcdMsg(MSG_RECORD);
        break;
        
      case MODE_LOCK:
        mode = MODE_LISTEN;    // when user start inputting pattern
        inputPattern = String(ms) + " ";     // record the first tap
        lcdMsg(MSG_EMPTY);
        lcdMsg(MSG_LISTEN);
        break;
        
      case MODE_RECORD:
        noInputCount = 0;
        // pattern concat
        recordedPattern += String(ms) + " ";
        break;

      case MODE_LISTEN:
        noInputCount = 0;
        // pattern concat
        inputPattern += String(ms) + " ";
        break;
    }

    buttonHasBeenPressed = false;
  }

  // beep while clicked
  int toneLv = digitalRead(pinInterrupt)==HIGH? 1:0;
  digitalWrite(buzzerPin, toneLv);
}

void lock() {
  lockServo.write(ANGLE_LOCK);  // rotate to lock angle
  mode = MODE_LOCK;             // change mode to locked
}

void unlock() {
  lockServo.write(ANGLE_UNLOCK);  // rotate to unlock angle
  mode = MODE_UNLOCK;             // change mode to unlocked
}

// ISR called when pin 2 goes from HIGH to LOW, ref: lecture 5 note
void checkButtonISR() {
  static unsigned long last_time_pressed = 0;
  unsigned long timeNow = millis();

  // condition to debounce:
  if (timeNow - last_time_pressed > MAX_BOUNCE_TIME) {
    buttonHasBeenPressed = true;
    last_time_pressed = timeNow;
  }
}

void lcdMsg(int msg) {
  switch (msg) {
    case MSG_INIT:
      lcd.setCursor(0, 0);
      lcd.print("SM3610");
      lcd.setCursor(0, 1);
      lcd.print("SRCRET KNOCK");
      break;
      
    case MSG_EMPTY:
      lcd.setCursor(0, 0);
      lcd.print(LCD_EMPTY);
      lcd.setCursor(0, 1);
      lcd.print(LCD_EMPTY);
      break;
      
    case MSG_UNLOCK:
      lcd.setCursor(0, 0);
      lcd.print("PRESS BTN_2 TO");
      lcd.setCursor(0, 1);
      lcd.print("RECORD PATTERN");
      break;

    case MSG_LOCK:
      lcd.setCursor(0, 0);
      lcd.print("PRESS BTN_1 TO");
      lcd.setCursor(0, 1);
      lcd.print("UNLOCK");
      break;

    case MSG_RECORD:
      lcd.setCursor(0, 0);
      lcd.print("RECORDING");
      lcd.setCursor(0, 1);
      lcd.print("PRESS BTN_2");
      break;

    case MSG_LISTEN:
      lcd.setCursor(0, 0);
      lcd.print("LISTENING");
      lcd.setCursor(0, 1);
      lcd.print("PRESS BTN_2");
      break;
  }
}

void ledIndicator() {
  switch (mode) {
    case MODE_UNLOCK:
      analogWrite(led[0], 255);
      analogWrite(led[1], 0);
      analogWrite(led[2], 0);
      break;
      
    case MODE_LOCK:
      analogWrite(led[0], 0);
      analogWrite(led[1], 255);
      analogWrite(led[2], 0);
      break;
      
    case MODE_RECORD:
      analogWrite(led[2], 255);
      break;
      
    case MODE_LISTEN:
      analogWrite(led[2], 255);
      break;
  }
}

void initialization() {
  // init LCD: (cols, rows)
  lcd.begin(16, 2); 
  lcd.backlight();
  
  // init servo object pin
  lockServo.attach(servoPin);
  pinMode(servoPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  
  // init notification
  lcdMsg(MSG_INIT);
  delay(1000);
  lcdMsg(MSG_EMPTY);
  lcdMsg(MSG_UNLOCK);
  
  // init lock servo angle
  unlock();
}
