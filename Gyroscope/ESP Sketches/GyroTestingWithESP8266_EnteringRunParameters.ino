/*GyroTestingWithESP8266_EnteringRunParameters    12/30/2016
 * MJL - thepiandi.blogspot.com
 * 
 * First of two sketchs to investigate MPU9250 gyro operation
 *  other sketch is GyroTestingWithESP8266_MakingRunResultsToBrowser.ino
 *  
 * Specifically, investigates the Gyro Offset setting.
 * 
 * To determine when the gyros are providing meaningful results it is necessary
 *  to make measurements at a couple of known conditions. 
 * 
 * The test methodology here is to test with the MPU9250 at rest, and spinning at
 *  a known rate.  A phonograph turntable will be used provide the second condition.
 *  
 * This requires that the MPU9250 be connected with a device with WiFi capability.  
 *  Therefore, an ESP8266 (ESP12) mounted on a NodeMCU developement board is used.
 *  This also requires the MPU9250 to be untethered from the computer USB cable, 
 *  and power to be supplied by batteries.  Under those conditions, the device 
 *  can be placed on the spinning turntable.
 *  
 * For every different run condition this sketch is run first.  It alows the user to
 *  set the full scale setting and the bandwidth.  Furthermore, it allows the user 
 *  to input the following run conditions that will be printed out with the test
 *  results:
 *    axis under test
 *    whether or device is orientated in line with the direction of the positive
 *      axis, or not
 *    whether or not the device is spinning.
 *  
 *  The full scale setting and bandwidth will also be printed out wiht the test
 *    results.
 *    
 *  Those five items mentioned above will be stored within the ESP8266's EEPROM so
 *    that these settings can be retrieved from the othr program.
 *  
 *  The last job of this sketch is to connect to WiFi and assertain the local IP
 *    address of the browser page that will have the results.  This is printed to
 *    to Serial Monitor.
 */
 
#include <Wire.h>
#include <My_MPU9250.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>

/*------------------------------------------Global Variables-------------------------------*/
uint8_t gyroConfig;
uint16_t range;
uint16_t bandwidth;
uint8_t gyroBW;
uint8_t gyroFS;

/*---------------------------------------------yes_or_no-------------------------------------*/
boolean yes_or_no(){
  boolean yes_no = false;
  char keyboard_entry[5];
  do{
    Serial.println("Please enter Y or y, or N or n");        
    while (Serial.available() == 0) delay(0);
    Serial.readBytes(keyboard_entry, 1);
    if (keyboard_entry[0] == 'Y' || keyboard_entry[0] == 'y'){
      yes_no = true;
    }
  }while((keyboard_entry[0] != 'y') && (keyboard_entry[0] != 'Y') && (keyboard_entry[0] != 'n') && (keyboard_entry[0] != 'N'));

  while (Serial.read() != 13);
  return yes_no;
}
/*-------------------------------------I2Cread--------------------------------*/
void I2Cread(uint8_t Address, uint8_t Register, uint8_t Nbytes, uint8_t* Data){
  uint8_t index=0;
  
  // Set register address
  Wire.beginTransmission(Address);
  Wire.write(Register);
  Wire.endTransmission();
  
  // Read Nbytes
  Wire.requestFrom(Address, Nbytes); 
  while (Wire.available())
    Data[index++] = Wire.read();
}

/*-------------------------------------I2CwriteByte------------------------------*/
void I2CwriteByte(uint8_t Address, uint8_t Register, uint8_t Data){
  // Set register address
  Wire.beginTransmission(Address);
  Wire.write(Register);
  Wire.write(Data);
  Wire.endTransmission();
}


/*---------------------------------Read Gyro FullScale-------------------*/
void readGyroFS() {

  //read gyro full scale settingI;
  I2Cread(MPU9250_ADDRESS, GYRO_CONFIG, 1, &gyroConfig);
  
  gyroFS = (gyroConfig & 0x18) >> 3;
  range = 250.0 * pow(2.0, float(gyroFS)) + 0.1;

}

/*-------------------------------Set Gyro FullScale---------------------*/
void setGyroFS(int fullScale){
  //read present gyro configuration
  I2Cread(MPU9250_ADDRESS, GYRO_CONFIG, 1, &gyroConfig);
  
  //write Gyro Full Scale to MPU9250
  //for gyro full scale equal to +/-250dps
  if (fullScale == 0) { 
    gyroConfig = gyroConfig & 0b11100111;
  }

  //for gyro full scale equal to +/-500dps
  else if (fullScale == 1) { 
    gyroConfig = gyroConfig & 0b11100111 | 0b00001000;
  }

  //for gyro full scale equal to +/-1000dps
  else if (fullScale == 2) {
    gyroConfig = gyroConfig & 0b11100111 | 0b00010000;
  }

  //for gyro full scale equal to +/-2000dps
  else {
    gyroConfig = gyroConfig | 0b00011000;
  }

  //Now write it out
  I2CwriteByte(MPU9250_ADDRESS, GYRO_CONFIG, gyroConfig);
  
}

/*------------------------------------Read Gyro Bandwidth---------------------*/
void readGyroBandwidth(){

  I2Cread(MPU9250_ADDRESS, CONFIG, 1, &gyroConfig);

  gyroBW = gyroConfig & 0x07;

  switch (gyroBW) {
    case 0:
      bandwidth = 250;
      break;
    case 1:
      bandwidth = 184;
      break;
    case 2:
      bandwidth = 92;
      break;
    case 3:
      bandwidth = 41;
      break;
    case 4:
      bandwidth = 20;
      break;
    case 5:
      bandwidth = 10;
      break;
    case 6:
      bandwidth = 5;
      break;
    case 7:
      bandwidth = 3600;      
  }  
}

/*----------------------------------Set Gyro Bandwidth---------------------*/
void setGyroBandwidth(uint8_t bandWTH){
    
  //read present configuration
  I2Cread(MPU9250_ADDRESS, CONFIG, 1, &gyroConfig);
  
  gyroConfig &= 0xF8;
  gyroConfig += bandWTH;
      
  //write Gyro Bandwidth to MPU9250

  //Now write it out
  I2CwriteByte(MPU9250_ADDRESS, CONFIG, gyroConfig);  
}
/*_______________________________________Setup________________________________*/
void setup() {
  byte keyboardBuffer[5];
  boolean goodEntry;
  uint8_t axis;
  boolean orientation, spinning;
  const char* ssid = "------------";
  const char* password = "--------------";
     
  Wire.begin();
  Serial.begin(115200);

  EEPROM.begin(5);
 
 
  //************************************* Change Bandwidth?
  setGyroBandwidth(EEPROM.read(3));
  readGyroBandwidth();  
  Serial.print("\n\nCurrent Bandwidth: ");
  Serial.print(bandwidth);
  Serial.println(" Hz");
  Serial.print("Change It?  ");
  if (yes_or_no()){
    goodEntry = false;
    do{
      Serial.println("Enter Value Between 1 and 8");
      while (Serial.available() == 0) delay(0);
      Serial.readBytesUntil('\n', keyboardBuffer, 2);
      if (keyboardBuffer[0] > 48 && keyboardBuffer[0] < 57) { //1 - 8
        setGyroBandwidth(keyboardBuffer[0] - 49);
        readGyroBandwidth();  
        Serial.print("New Bandwidth: ");
        Serial.print(bandwidth);
        Serial.println(" hZ");
        goodEntry = true;
      }
      else{
        Serial.println("Bad Entry");
      }
    }while (!goodEntry);    
  }

  //********************************* Change Full Scale?
  setGyroFS(EEPROM.read(4));
  readGyroFS();
  Serial.print("\nCurrent Full Scale : ");
  Serial.print(float(range)/6.0);
  Serial.println(" RPM");
  Serial.print("Change It?  ");
  if (yes_or_no()){
    goodEntry = false;
    do{
      Serial.println("Enter Value Between 1 and 4");
      while (Serial.available() == 0) delay(0);
      Serial.readBytesUntil('\n', keyboardBuffer, 2);
      if (keyboardBuffer[0] > 48 && keyboardBuffer[0] < 53) { //1 - 4
        setGyroFS(keyboardBuffer[0] - 49);
        readGyroFS();
        Serial.print("New Full Scale: ");
        Serial.print(float(range)/6.0);
        Serial.println(" RPM");
        goodEntry = true;
      }
      else{
        Serial.println("Bad Entry");
      }
    }while (!goodEntry);    
  }

  //**************************************** Which Axis To Test?
  goodEntry = false;
  do{
    Serial.println("\nWhich Axis To Test?  Enter x, y, or z");
    while (Serial.available() == 0) delay(0);
    Serial.readBytesUntil('\n', keyboardBuffer, 2);
    if (keyboardBuffer[0] > 119 && keyboardBuffer[0] < 123) { //x, y, or z
      axis = keyboardBuffer[0];
      Serial.print("Testing The ");
      Serial.print (char(axis - 32));
      Serial.println(" Axis");
      goodEntry = true;
    }
    else{
      Serial.println("Bad Entry");
    }
  }while (!goodEntry);    
  
  //**************************************** Which Axis Orientation?
  Serial.println("\nTesting In The Direction Of Positive Axis?");
  orientation = yes_or_no();
  if (orientation){
    Serial.println("Testing In the Direction Of the Postive Axis");
  }
  else{
    Serial.println("Testing In the Direction Of the Negative Axis");      
  }

  //Is the Device Spinning?
  Serial.println("\nWill the Device Be Spinning?");
  spinning = yes_or_no();
  if (spinning){
    Serial.println("The Device Will Be Spinning");
  }
  else{
    Serial.println("The Device Will Not Be Spinning");      
  }

  //**************************************** Write Out To EEPROM
  EEPROM.write(0, axis);
  EEPROM.write(1, orientation);
  EEPROM.write(2, spinning);
  EEPROM.write(3, gyroBW);
  EEPROM.write(4, gyroFS);
  EEPROM.end();

  //**************************************** Find URL

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("Done");
}

/*_______________________________________Loop________________________________*/
void loop() {
  // put your main code here, to run repeatedly:

}
