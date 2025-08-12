/*
  Inkplate10_Set_VCOM sketch for Soldered Inkplate 10.
  For this sketch you will need USB and Inkplate 10.
  Select "e-radionica Inkplate 10" or "Soldered Inkplate10" from Tools -> Board menu.
  Don't have "e-radionica Inkplate10" or "Soldered Inkplate10" option? Follow our tutorial and add it:
  https://soldered.com/learn/add-inkplate-6-board-definition-to-arduino-ide/

  WARNING! - VCOM voltage is written in EEPROM, which means it can be set a limited number of 
  times, so don't run this sketch repeteadly! VCOM should be set once and then left as is.
  Use with caution!

  Want to learn more about Inkplate? Visit https://soldered.com/documentation/inkplate/
  Looking to get support? Write on our forums: https://forum.soldered.com/
  29 July 2025 by Soldered
*/

// Next 3 lines are a precaution, you can ignore those, and the example would also work without them
#if !defined(ARDUINO_INKPLATE10) && !defined(ARDUINO_INKPLATE10V2)
#error                                                                                                                 \
    "Wrong board selection for this example, please select e-radionica Inkplate10 or Soldered Inkplate10 in the boards menu."
#endif

#include <EEPROM.h>
#include <Inkplate.h>
#include <Wire.h>

Inkplate display(INKPLATE_3BIT); // Create an object on Inkplate library and also set the grayscale to 3bit.


double currentVCOM; //Stores the current VCOM value stored on EEPROM
const int EEPROMAddress=0; //Should leave the address as it is for correct EEPROM reading later
double vcomVoltage;

double readPanelVCOM();
double getVCOMFromSerial(double *_vcom);
uint8_t writeVCOMToEEPROM(double v);
void displayTestImage();
void writeReg(uint8_t reg, float data);
uint8_t readReg(uint8_t reg);

void setup() {
  Serial.begin(115200);     //Start serial at 115200 baud
  EEPROM.begin(512);        //Initialize EEPROM
  Wire.begin();             //Initialize I2C buss
  display.begin();          //Initialize the Inkplate 
  Serial.println("The optimal VCOM Voltage for your Inkplate's panel can sometimes be found written on the flat cable connector");
  Serial.println("Write VCOM voltage from epaper panel. \r\nDon't forget negative (-) sign!\r\nUse dot as the decimal point.\r\nFor example -1.23\n");
  displayTestImage();
}

void loop() {
  
  if (Serial.available()){
    //Serial.println("Enter VCOM value, it must be [-5, 0]");
    do{
      getVCOMFromSerial(&vcomVoltage);
      Serial.println(vcomVoltage, 2);
      display.display();
      if(vcomVoltage < -5.0 || vcomVoltage > 0.0){
        Serial.println("VCOM out of range! [-5, 0]");
      }
    }while(vcomVoltage <-5.0 || vcomVoltage > 0.0);

    //Program the panel EEPROM
    display.pinModeInternal(IO_INT_ADDR, display.ioRegsInt, 6, INPUT_PULLUP);
    if(writeVCOMToEEPROM(vcomVoltage)){
      EEPROM.put(EEPROMAddress, vcomVoltage);
      EEPROM.commit();
    }
    displayTestImage();
  }

}

double readPanelVCOM(){
  delay(10); //Wake up TPS65186 so registers respond
  uint8_t vcomL=readReg(0x03); // REad low 8 bits from register 0x03
  uint8_t vcomH = readReg(0x04) & 0x01; // Read full byte, mask off all but bit 0 (MSB)
  delay(10); //Power down driver
  int raw=(vcomH << 8) | vcomL; //Value between 0 - 511
  return -(raw/100.0);
}

double getVCOMFromSerial(double *_vcom){
  double vcom=0;
  char buff[32];
  unsigned long start;
  while (!Serial.available());
  start=millis();
  int idx=0;
  while ((millis()-start)<500 && idx<sizeof(buff)-1){
    if(Serial.available()){
      char c=Serial.read();
      buff[idx++]=c;
      start=millis();
    }
  }
  buff[idx]='\0';
  sscanf(buff, "%lf", &vcom);
  *_vcom=vcom;
  return vcom;
}

uint8_t writeVCOMToEEPROM(double v){
  //Build a 9-bit raw value (0 - 511)
  int raw=int(abs(v)*100)&0x1FF;
  uint8_t lsb=raw & 0xFF;
  uint8_t msb=(raw >> 8)&0x01;
  
  display.einkOn();
  delay(10);

  writeReg(0x03, lsb);
  uint8_t r4=readReg(0x04)&~0x01;
  writeReg(0x04, r4 | msb);
  writeReg(0x04, (r4 | msb) | (1 << 6));
  while ( display.digitalReadInternal(IO_INT_ADDR, display.ioRegsInt, 6) ) {
    delay(1);
  }
  readReg(0x07);  // clear interrupt flag
  writeReg(0x03, 0);
  writeReg(0x04, 0);
  display.einkOff(); // WAKEUP low
  delay(10);
  display.einkOn();  // WAKEUP high
  delay(10);
  uint8_t vL = readReg(0x03);
  uint8_t vH = readReg(0x04) & 0x01;
  int check = (vH << 8) | vL;
  if (check != raw) {
    Serial.printf("Verification failed: got %d, want %d\n", check, raw);
    return 0;
  }
  Serial.println("VCOM EEPROM PROGRAMMING OK");
  return 1;
}
void writeReg(uint8_t reg, float data){
  Wire.beginTransmission(0x48);
  Wire.write(reg);
  Wire.write((uint8_t)data);
  Wire.endTransmission();
}
uint8_t readReg(uint8_t reg){
  Wire.beginTransmission(0x48);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)0x48, (uint8_t)1);
  return Wire.read();
}
void displayTestImage() {
    display.clearDisplay();
    currentVCOM = readPanelVCOM();

    display.setTextColor(BLACK);
    display.setTextSize(2);
    display.setCursor(5, 5);
    display.print("Current VCOM: ");
    display.print(currentVCOM, 2);
    display.print(" V");

    for (int i = 0; i < 8; i++) {
        int x = (display.width() / 8) * i;
        display.drawRect(x, 40, display.width() / 8, display.height(), i);
        display.fillRect(x, 40, display.width() / 8, display.height(), i);
    }
    display.display();
}