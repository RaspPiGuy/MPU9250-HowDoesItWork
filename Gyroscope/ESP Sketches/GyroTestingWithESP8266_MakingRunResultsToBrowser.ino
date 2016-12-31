/*GyroTestingWithESP8266_MakingRunResultsToBrowser    12/30/2016
 * MJL - thepiandi.blogspot.com
 * 
 * Second of two sketchs to investigate MPU9250 gyro operation
 *  other sketch is GyroTestingWithESP8266_EnteringParameters.ino
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
 * When this sketch is run, the following information is retrieved from EEPROM:
 *  1.  Gyro Full Scale Setting
 *  2.  Gyro Bandwith
 *  3.  axis under test
 *  4.  whether or device is orientated in line with the direction of the positive
 *      axis, or not
 *  5.  whether or not the device is spinning.
 *  
 * The first two items are placed in the MPU9250's registers
 * 
 * A run is made, setting the offset from 0 to 60000 in steps of 1000. and measuring the 
 *  gyro output of the axis in question
 *  
 * When the run is complete, the values, along with the test conditions can be obtained
 *  from the browser page at the local IP address ascertained from the first sketch.
 *  
 * Since, when on the turntable, no serial monitor will be available, the red LED on the ESP
 * device will be used to signal the operator of test progress as follows:
 * 
 *  1.  LED flashes on and off at one half second rate for 10 seconds, so operator can
 *    prepare hardware and turntable.
 *    
 *  2.  LED is extinguished while test run is in progress.
 *  
 *  3.  LED turns on to signal operator to check results
 *  
 *  4.  When operator clicks on web page, the LED goes out.
 *  
 */
 
#include <Wire.h>
#include <My_MPU9250.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

/*------------------------------------------Global Variables-------------------------------*/
uint8_t gyroConfig;
uint16_t range;
uint16_t bandwidth;
uint16_t offset[3];
float measurement[3];
uint16_t raw_measurement[3];
boolean raw;
float results[60];
uint8_t axis;
String output;
boolean orientation, spinning;

const char* ssid = "-----------";
const char* password = "-------------";

boolean debug = true;

ESP8266WebServer server(80);

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


/*---------------------------------Gyro FullScale-------------------------*/
void gyroFullScale() {
uint8_t gyroFS;
  
  gyroFS = EEPROM.read(4);
  
  //read present gyro configuration
  I2Cread(MPU9250_ADDRESS, GYRO_CONFIG, 1, &gyroConfig);

  //write Gyro Full Scale to MPU9250
  //for gyro full scale equal to +/-250dps
  if (gyroFS == 0) { 
    gyroConfig = gyroConfig & 0b11100111;
  }

  //for gyro full scale equal to +/-500dps
  else if (gyroFS == 1) { 
    gyroConfig = gyroConfig & 0b11100111 | 0b00001000;
  }

  //for gyro full scale equal to +/-1000dps
  else if (gyroFS == 2) { 
    gyroConfig = gyroConfig & 0b11100111 | 0b00010000;
  }

  //for gyro full scale equal to +/-2000dps
  else { 
    gyroConfig = gyroConfig | 0b00011000;
  }

  //Now write it out
  I2CwriteByte(MPU9250_ADDRESS, GYRO_CONFIG, gyroConfig);
    
  range = 250.0 * pow(2.0, float(gyroFS)) + 0.1;
}

/*------------------------------------Gyro Bandwidth-----------------------*/
void gyroBandwidth(){
uint8_t gyroBW;

  //read present gyro configuration
  gyroBW = EEPROM.read(3);
  
  //write Gyro Bandwidth to MPU9250
  
  I2Cread(MPU9250_ADDRESS, CONFIG, 1, &gyroConfig);

  gyroConfig &= 0xF8;
  gyroConfig += gyroBW;

  //Now write it out
  I2CwriteByte(MPU9250_ADDRESS, CONFIG, gyroConfig);  
  

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

/*-----------------------------------Change Gyro Offset------------------*/
void changeGyroOffsets(byte axis, uint16_t gyroOffset) {
 
  uint8_t toRegister[2];
  
  toRegister[0] = gyroOffset >> 8;
  toRegister[1] = gyroOffset;
  
  //x offset
  if (axis == 120) { //axis = x
    I2CwriteByte(MPU9250_ADDRESS, XG_OFFSET_H, toRegister[0]);
    I2CwriteByte(MPU9250_ADDRESS, XG_OFFSET_L, toRegister[1]);
  }

  //y offset
  if (axis == 121) { //axis = y
    I2CwriteByte(MPU9250_ADDRESS, YG_OFFSET_H, toRegister[0]);
    I2CwriteByte(MPU9250_ADDRESS, YG_OFFSET_L, toRegister[1]);
  }

  //z offset
  if (axis == 122) { //axis = z
    I2CwriteByte(MPU9250_ADDRESS, ZG_OFFSET_H, toRegister[0]);
    I2CwriteByte(MPU9250_ADDRESS, ZG_OFFSET_L, toRegister[1]);
  }
}

/*------------------------------------Read Gyro Values-------------------*/
void readGyroValues(uint8_t axis) {
  uint8_t interrupt;
  uint8_t fromRegister[2];
  
  do{
    //wait for interrupt
    I2Cread(MPU9250_ADDRESS, INT_STATUS, 1, &interrupt);
    delay(0);    
  }while (!(interrupt & 0x01));
    

  if (axis == 120){
    //read gyro X offset from MPU9250
    I2Cread(MPU9250_ADDRESS, GYRO_XOUT_H, 2, fromRegister);
        
    raw_measurement[0] = (fromRegister[0] << 8) + fromRegister[1];
    measurement[0] = (2.0 * float(range) * float(raw_measurement[0]) / 65536.0 - float(range)) / 6.0;
  }

  if (axis == 121){
    //read gyro Y offset from MPU9250
    I2Cread(MPU9250_ADDRESS, GYRO_YOUT_H, 2, fromRegister);
        
    raw_measurement[1] = (fromRegister[0] << 8) + fromRegister[1];
    measurement[1] = (2.0 * float(range) * float(raw_measurement[1]) / 65536.0 - float(range)) / 6.0;
  }
  
  if (axis == 122){
    //read gyro Z offset from MPU9250
    I2Cread(MPU9250_ADDRESS, GYRO_ZOUT_H, 2, fromRegister);
        
    raw_measurement[2] = (fromRegister[0] << 8) + fromRegister[1];
    measurement[2] = (2.0 * float(range) * float(raw_measurement[2]) / 65536.0 - float(range)) / 6.0;
  }
}

/*_______________________________________Setup________________________________*/
void setup() {
  byte keyboardBuffer[5];
  boolean goodEntry;
  String output;

  EEPROM.begin(5);
  
  if (debug){
    Serial.begin(115200);
  }
  
  pinMode(16, OUTPUT);
  digitalWrite(16, HIGH);
  Wire.begin();

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if (debug){
      Serial.print(".");
    }
  }
  if (debug){
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }

 
  //**************************************** Disable Accelerometer and set Fchoice_b
  //disable accelerometer
  I2Cread(MPU9250_ADDRESS, PWR_MGMT_2, 1, &gyroConfig);
  gyroConfig &= 0x38;
  //Now write it out
  I2CwriteByte(MPU9250_ADDRESS, GYRO_CONFIG, gyroConfig);  

  //set FCHOICE_B = 00 
  I2Cread(MPU9250_ADDRESS, GYRO_CONFIG, 1, &gyroConfig);
  gyroConfig &= 0xFC;
  //Now write it out
  I2CwriteByte(MPU9250_ADDRESS, GYRO_CONFIG, gyroConfig);   
  
  //**************************************** Retrieve Bandwdith and Full Scale
  gyroBandwidth();  //function will read stored value from EEPROM first
  gyroFullScale();  //function will read stored value from EEPROM first
    
  //**************************************** Retrieve Axis, Orientation, and Spinning From EEPROM
  axis = EEPROM.read(0);
  orientation = EEPROM.read(1);
  spinning = EEPROM.read(2);
  //EEPROM.end();

  if(debug){
    Serial.print("\naxis: ");
    Serial.println(axis);
    Serial.print("orientation: ");
    Serial.println(orientation);
    Serial.print("spinning: ");
    Serial.println(spinning);
    Serial.print("bandwidth: ");
    Serial.println(bandwidth);
    Serial.print("full scale: ");
    Serial.println(range);
  }
  
  //**************************************** 10 Second Delay
  //Gives 10 second delay
  for (int i = 0; i < 10; i++){
    digitalWrite(16, LOW);
    delay(500);
    digitalWrite(16, HIGH);
    delay(500);      
  }
  
  //**************************************** Run the test
  if (debug){
    Serial.println("Running Test");
  }
  
  for (int i = 0; i < 61; i++){
    changeGyroOffsets(axis, (i * 1000));
    readGyroValues(axis);
    results[i] = measurement[axis - 120];
    delay(100);
  } 
  digitalWrite(16, LOW);  //Turn LED ON to signal uset to evoke browser  
  if (debug){
    Serial.println("Test Done");
  }


  //**************************************** Setup Server
  server.begin();
  if (debug){
    Serial.println("HTTP server started");
  }
  
  server.on("/", [](){
    String output;
  
    //Prepare the Result String
    output = "\n  Testing ";
    output += String(char(axis -32));
    output += " Axis     ***     ";
    if (orientation){
      output += "Testing In the Direction Of the Postive Axis     ***     ";
    }
    else{
      output += "Testing In the Direction Of the Negative Axis     ***     ";      
    }
    if (spinning){
      output += "Device Is Spinning\n";    
    }
    else{
      output += "Device Is NOT Spinning\n";    
    }
      output += "  Full Scale: ";
      output += String(float(range)/6.0);
      output += " RPM     ***     ";
      output += "Bandwidth: ";
      output += String(bandwidth);
      output +=" Hz\n\n";
          
    for (int i = 0; i < 61; i++){
      output += "\tOffset: ";
      output += String(i * 1000);
      output += "\tValue: ";
      output += String(results[i]);
      output += " RPM\n";
    }
    
    server.send(200, "text/plain", output);
   digitalWrite(16, HIGH);  //Turn LED OFF  

  });
  


}

/*_______________________________________loop________________________________*/

void loop() {
  server.handleClient();
}
