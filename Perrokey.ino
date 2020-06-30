#include      <map> 
#include      <M5Stack.h>
#include      <IridiumSBD.h>

#define       MAX_MSG_LEN           100
// Keyboard
#define       KEYBOARD_I2C_ADDR     0X08
#define       KEYBOARD_INT          5
// GSM Module
#define       GSM_SERIAL_BAUD       9600
#define       GSM_RX_PIN            16
#define       GSM_TX_PIN            17
// RockBlock Sat Module
#define       SAT_SERIAL_BAUD       19200
#define       SAT_RX_PIN            3
#define       SAT_TX_PIN            1

// -----------------------------
//           DATA TYPES
// -----------------------------
typedef void (*ScreenFunc)(void);

typedef struct Contact {
  String      name;
  String      number;
} Contact;

typedef struct Screen {
  String      title;
  String      btnA;
  String      btnB;
  String      btnC;
  ScreenFunc  show;
  ScreenFunc  update;
} Screen;

typedef std::map<String, Screen> ScreenMap;

// -----------------------------
//           VARIABLES
// -----------------------------
IridiumSBD    modem(Serial);
ScreenMap     screens;
Screen        *currentScreen;
Contact       contacts[] = {
                { "Nicolas", "+33667534050" },
                { "Mathilde", "+33651807010" }
              };

int           positionIndex = 0,
              strIndex = 0;
String        strBuffer = "";
boolean       isSending = false;

              
// -----------------------------
//    INIT & MAIN FUNCTIONS
// -----------------------------
void setup() {
  M5.begin();
  Wire.begin();
  pinMode(KEYBOARD_INT, INPUT_PULLUP);
  M5.Lcd.setTextSize(3);
  initScreens();
  showScreen("home");
}

void initScreens() {
  screens["home"]     = (Screen) { "Home", "UP", "OK", "DOWN", &homeScreen_SHOW, &homeScreen_UPDATE };
  screens["write"]    = (Screen) { "Write", "BACK", "", "SEND", &writeScreen_SHOW, &writeScreen_UPDATE};
  screens["contacts"] = (Screen) { "Contacts", "UP", "OK", "DOWN", &contactsScreen_SHOW, &contactsScreen_UPDATE };
  screens["sendMode"] = (Screen) { "Send Mode", "UP", "OK", "DOWN", &sendModeScreen_SHOW, &sendModeScreen_UPDATE };
  screens["sendGSM"]  = (Screen) { "Send GSM", "", "", "", &sendGSMScreen_SHOW, &sendGSMScreen_UPDATE };
  screens["sendSat"]  = (Screen) { "Send Sat", "", "", "", &sendSatScreen_SHOW, &sendSatScreen_UPDATE };
}

void showScreen(String scrKey) {

  currentScreen = &screens[scrKey];
  M5.Lcd.clear(BLACK);
  M5.Lcd.setCursor(0,0);
  M5.update();
  
  currentScreen->show();

  // Bottom bar
  showBottomBar();
}

void loop() {
  M5.update();
  currentScreen->update();
}

// -----------------------------
//          HOME SCREEN
// -----------------------------
void homeScreen_SHOW() {
  M5.Lcd.setTextSize(3);
  String items[] = {"WRITE MSG"};
  showList(items, 1);
}

void homeScreen_UPDATE() {
   if (M5.BtnA.wasReleased() && positionIndex > 0) {
    positionIndex--;
    showScreen("home");
   }
   
   if (M5.BtnB.wasReleased()) {
    if (positionIndex == 0) showScreen("write");
    
   }

   if (M5.BtnC.wasReleased() && positionIndex < 1) {
    positionIndex++;
    showScreen("home");
   }
}

// -----------------------------
//      WRITE MSG SCREEN
// -----------------------------
void writeScreen_SHOW() {  
  M5.Lcd.setTextSize(3);
  M5.Lcd.setCursor(0,0);
  M5.Lcd.println(strBuffer);
  M5.update();
}

void writeScreen_UPDATE() {
  if (M5.BtnA.wasReleased()) {
    showScreen("home");
  }

  if (M5.BtnC.wasReleased()) {
    showScreen("sendMode");
  }
  
  // Keyboard text handling
  if(digitalRead(KEYBOARD_INT) == LOW) {
    
    Wire.requestFrom(KEYBOARD_I2C_ADDR, 1);  // request 1 byte from keyboard
    while (Wire.available()) { 
      uint8_t key_val = Wire.read();                  // receive a byte as character
      if (key_val != 0) {
        
        // ASCII Chars
        if(key_val >= 0x20 && key_val < 0x7F && strIndex < MAX_MSG_LEN) { 
            strBuffer += (char) key_val;
            strIndex++;
            showScreen("write");
          
        } else {
          // backspace
          if (key_val == 0x08) {
            strBuffer.remove(strBuffer.length() - 1);
            strIndex--;
          }
        }          

        // Refresh screen
        showScreen("write");
      }
    }
  }
}

// -----------------------------
//        SEND MODE SCREEN
// -----------------------------
void sendModeScreen_SHOW() {
  M5.Lcd.setTextSize(3);
  String items[] = {"GSM", "SAT"};
  showList(items, 2);
}

void sendModeScreen_UPDATE() {
   if (M5.BtnA.wasReleased() && positionIndex > 0) {
    positionIndex--;
    showScreen("sendMode");
   }
   
   if (M5.BtnB.wasReleased()) {
    if (positionIndex == 0) showScreen("sendGSM");
    if (positionIndex == 1) showScreen("sendSat");
   }

   if (M5.BtnC.wasReleased() && positionIndex < 1) {
    positionIndex++;
    showScreen("sendMode");
   }
}

// -----------------------------
//        CONTACTS SCREEN
// -----------------------------
void contactsScreen_SHOW() {
  M5.Lcd.setTextSize(3);
  String items[] = {"GSM", "SAT"};
  showList(items, 2);
}

void contactsScreen_UPDATE() {
   if (M5.BtnA.wasReleased() && positionIndex > 0) {
    positionIndex--;
    showScreen("contacts");
   }
   
   if (M5.BtnB.wasReleased()) {
    if (positionIndex == 0) showScreen("sendGSM");
   }

   if (M5.BtnA.wasReleased() && positionIndex < 3) {
    positionIndex++;
    showScreen("contacts");
   }
}

// -----------------------------
//       SEND GSM SCREEN
// -----------------------------
void sendGSMScreen_SHOW() {
  M5.Lcd.setCursor(0,0);
  M5.Lcd.setTextSize(2);
  M5.Lcd.println("Sending message via GSM ...");
  sendMessage_GSM();
}

void sendGSMScreen_UPDATE() {}

// -----------------------------
//       SEND SAT SCREEN
// -----------------------------
void sendSatScreen_SHOW() {
  M5.Lcd.setCursor(0,0);
  M5.Lcd.setTextSize(2);
  M5.Lcd.println("Sending message via Satellite ...");
  sendMessage_Sat();
}

void sendSatScreen_UPDATE() {}

// -----------------------------
//             UTILS
// -----------------------------

void sendMessage_GSM() {
  Serial.begin(GSM_SERIAL_BAUD, SERIAL_8N1, GSM_RX_PIN, GSM_TX_PIN);
  delay(2000);
  String number = "+33667534050";
  String numberCommandSMS = "AT+CMGS=\"" + number + "\"\r";
  String atCmd = sendCommand("AT", false);
  if (atCmd.indexOf("ERROR") >= 0) {
    M5.Lcd.println("Error : GSM module not responding");
    delay(2000);
    return showScreen("write");
  }
  delay(500);
  String csqCmd = sendCommand("AT+CSQ", false);
  if (csqCmd.indexOf("ERROR") >= 0) {
    M5.Lcd.println("Error : No Signal ?");
    delay(2000);
    return showScreen("write");
  }
  delay(500);
  String copsCmd = sendCommand("AT+COPS?", false);
  if (copsCmd.indexOf("ERROR") >= 0) {
    M5.Lcd.println("Error : No operator ?");
    delay(2000);
    return showScreen("write");
  }
  delay(500);
  String cmgfCmd = sendCommand("AT+CMGF=1", false);
  if (cmgfCmd.indexOf("ERROR") >= 0) {
    M5.Lcd.println("Error : couldn't switch to text mode");
    delay(2000);
    return showScreen("write");
  }
  delay(500);
  String numCmd = sendCommand(numberCommandSMS, false);
  if (numCmd.indexOf("ERROR") >= 0) {
    M5.Lcd.println("Error : couldn't prepare number");
    delay(2000);
    return showScreen("write");
  }
  delay(500);
  String sendCmd = sendCommand(strBuffer, true);
  if (sendCmd.indexOf("ERROR") >= 0) {
    M5.Lcd.println("Error : send command failed");
    delay(2000);
    return showScreen("write");
  }
  
  delay(500);
  M5.Lcd.println("Sent!");
  delay(2000);
  showScreen("home");
}

void sendMessage_Sat() {
  Serial.begin(SAT_SERIAL_BAUD, SERIAL_8N1, SAT_RX_PIN, SAT_TX_PIN);
  isSending = true;
  int signalQuality = -1;
  int err;
  // Begin satellite modem operation
  M5.Lcd.println("Starting modem...");
  err = modem.begin();
  M5.Lcd.println("Modem started");
  if (err != ISBD_SUCCESS) {
    M5.Lcd.println("Begin failed: error ");
    M5.Lcd.println(err);
    if (err == ISBD_NO_MODEM_DETECTED)
      M5.Lcd.println("No modem detected: check wiring.");
     
    Serial.end();
    return showScreen("write");
  }

  // Example: Print the firmware revision
  char version[12];
  err = modem.getFirmwareVersion(version, sizeof(version));
  if (err != ISBD_SUCCESS) {
     M5.Lcd.println("FirmwareVersion failed: error ");
     M5.Lcd.println(err);
     isSending = false;
      Serial.end();
     return;
  }
  
  //M5.Lcd.println("Firmware Version is ");
  //M5.Lcd.println(version);
  //M5.Lcd.println(".");
  err = modem.getSignalQuality(signalQuality);
  if (err != ISBD_SUCCESS) {
    M5.Lcd.println("SignalQuality failed: error ");
    M5.Lcd.println(err);
    isSending = false;
    delay(2000);
    Serial.end();
    return showScreen("write");
  }

  M5.Lcd.print("Signal quality is currently ");
  M5.Lcd.print(signalQuality);
  M5.Lcd.println("/5");

  // Send the message
  M5.Lcd.println("Trying to send the message.\r\nThis might take several minutes.\r\n");
  char charBuf[100];
  strBuffer.toCharArray(charBuf, 100);
  err = modem.sendSBDText(charBuf);
  if (err != ISBD_SUCCESS) {
    M5.Lcd.println("sendSBDText failed: error ");
    M5.Lcd.println(err);
    if (err == ISBD_SENDRECEIVE_TIMEOUT)
      M5.Lcd.println("Try again with a better view of the sky.");
      delay(2000);
      Serial.end();
      return showScreen("write");
  } else {
    M5.Lcd.println("Message sucessfully sent!");
  }

  
      Serial.end();
  isSending = false;
}

String sendCommand(String command, boolean finish) {
  char charBuf[50];
  command.toCharArray(charBuf, 50);
  return sendCommandChars(charBuf, finish);
}

String sendCommandChars(char command[], boolean finish) {
  Serial2.println(command);
  if (finish) 
    Serial2.println(char(26));
  
  delay(2000);
  if (Serial2.available() > 0) {
    return Serial2.readString();
  } else {
    return "ERROR 404" + String(command);
  }
}

void showBottomBar() {
  M5.Lcd.setTextSize(3);
  M5.Lcd.fillRect(0, 200, 340, 40, TFT_DARKGREY);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setCursor(40, 210);
  M5.Lcd.println(currentScreen->btnA);
  M5.Lcd.setCursor(130, 210);
  M5.Lcd.println(currentScreen->btnB);
  M5.Lcd.setCursor(230, 210);
  M5.Lcd.println(currentScreen->btnC);
  
  M5.update();
}

void showList(String items[], int len) {
  for (int i = 0; i < len; i++) {
    if (i == positionIndex) 
      M5.Lcd.setTextColor(RED);
    else
      M5.Lcd.setTextColor(WHITE);
       
    M5.Lcd.println(items[i]);
  }
}
