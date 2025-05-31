#include "EPD_7in3e.h"
#include "GUI_Paint.h"
//#include "ImageData.h"
#include "DEV_Config.h"
#include <SPI.h>
#include <SD.h>
#include <stdlib.h>  // malloc() free()
#include <string.h>
#include "waveshare_PCF85063.h"
#include "pico/rand.h"

String localImagePath = "/BMP";
UBYTE Palette[21]= { 0 };
char FileName[100];
// line buffer
uint8_t buff[2400] = { 0 };
uint8_t* BlackImage;

// color mapping from prepared BMP to EPD colors
uint8_t color6(uint16_t red, uint16_t green, uint16_t blue) {
  for (uint8_t cv6 = 0; cv6 < 7; cv6++) {
    if ((red == Palette[3 * cv6 + 0]) && (green == Palette[3 * cv6 + 1]) && (blue == Palette[3 * cv6 + 2])) return cv6;
  }
  return 7;  //clear as fallback
}

float measureVBAT(void) {
  float Voltage = 0.0;
  const float conversion_factor = 3.3f / (1 << 12);
  uint16_t result = adc_read();
  Voltage = result * conversion_factor * 3;
  Serial.printf("Raw value: 0x%03x, voltage: %f V\n", result, Voltage);
  return Voltage;
}

int EPD_7in3e_display_BMP(const char *path, float vol) {
  Serial.printf("e-Paper Init and Clear...\r\n");
  EPD_7IN3E_Init();

  //Create a new image cache
  BlackImage = (uint8_t*)malloc(192000);
  Serial.printf("Paint_NewImage\r\n");
  Paint_NewImage(BlackImage, EPD_7IN3E_WIDTH, EPD_7IN3E_HEIGHT, 0, EPD_7IN3E_WHITE);
  Paint_SetScale(6);
  Paint_SelectImage(BlackImage);

  Serial.print("Display BMP: ");
  Serial.println(path);

  File file = SD.open(path);
  if (file) {
    uint32_t startTime = millis();
    Serial.println("Skip BMP Header");
    file.read(buff, 54);                      // skip BMP header
    Serial.println("Start reading");
    for (uint16_t row = 0; row < 480; row++)  // for each line
    {
      yield();                //to avoid WDT
      file.read(buff, 2400);  // read a line
      if (row == 0) {         //get the palette from the first 21 bytes
        for (uint8_t f = 0; f < 21; f++) {
          Palette[f] = buff[f];
          //Serial.printf("got Palette %d : %d\r\n",f,Palette[f]);
        }
      }
      for (uint16_t col = 0; col < 400; col++)  // for each pixel, in groups of two
      {
        //Serial.printf("Column :%d\r\n",col);
        //image rotated and byte order flipped because connector is at the top
        BlackImage[row * 400 + (399-col)] = (color6(buff[col * 6 + 5], buff[col * 6 + 4], buff[col * 6 + 3])<<4) | (color6(buff[col * 6 + 2], buff[col * 6 + 1], buff[col * 6 + 0]) );
      }  // end pixel
      //Serial.printf("Row :%d\r\n",row);
    }    // end line
    Serial.print("loaded in ");
    Serial.print(millis() - startTime);
    Serial.println(" ms");
    file.close();
  }
  char strvol[21] = { 0 };
  sprintf(strvol, "%.2fV", vol);
  Paint_SetRotate(180);
  Paint_DrawString_EN(0, 464, strvol, &Font16, EPD_7IN3E_BLUE, EPD_7IN3E_WHITE);
  if (vol < 3.3) {
    Paint_DrawString_EN(300, 192, "Low voltage, please charge.", &Font16, EPD_7IN3E_RED, EPD_7IN3E_WHITE);
  }

  Serial.printf("EPD_Display\r\n");
  EPD_7IN3E_Display(BlackImage);

  Serial.printf("Goto Sleep...\r\n\r\n");
  EPD_7IN3E_Sleep();
  free(BlackImage);
  BlackImage = NULL;

  return 0;
}


String Random_File() {
  Serial.print("Listing directory: ");
  Serial.println(localImagePath);
  int ct = 0;
  File dir = SD.open(localImagePath);
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) break;
    Serial.println(entry.name());
    ct++;
    entry.close();
  }
  //pick a random number
  uint8_t randValue = (uint8_t)get_rand_32();
  Serial.printf("randvalue = %d\r\n", randValue);
  int rn = 1 + randValue * ct / 256;
  Serial.printf("Picking number %d of %d files\r\n", rn, ct);
  dir.rewindDirectory();
  ct = 0;
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) break;
    ct++;
    if (rn == ct) return entry.name();
    entry.close();
  }
  return "";
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  while (!Serial) {
    delay(1);  // wait for serial port to connect. Needed for native USB port only
  }
  Serial.printf("Init...\r\n");
  if (DEV_Module_Init() != 0) {  // DEV init
    Serial.printf("Failed to init DEV...\r\n");
    return;
  }
  SPI.setRX(SD_MISO_PIN);
  SPI.setTX(SD_MOSI_PIN);
  SPI.setSCK(SD_CLK_PIN);
  if (!SD.begin(SD_CS_PIN)) {  // using spi0
    Serial.printf("Init SD failed...\r\n");
    return;
  }
  
  Serial.print("Card size:  ");
  Serial.println((float)SD.size64() / 1000);
  Serial.print("Picked File: ");
  strcpy(FileName, localImagePath.c_str());
  strcat(FileName, "/");
  strcat(FileName, Random_File().c_str());

  Serial.println(FileName);
  
  //watchdog_enable(8*1000, 1);   // 8s
  //DEV_Delay_ms(1000);
  PCF85063_init();  // RTC init
  Time_data Time = {2025-2000, 5, 31, 0, 0, 0};
  Time_data alarmTime = Time;
  // alarmTime.seconds += 10;
  alarmTime.minutes += 5; 
  //alarmTime.hours +=12;
  rtcRunAlarm(Time, alarmTime);  // RTC run alarm
  EPD_7in3e_display_BMP(FileName,measureVBAT());
  Serial.printf("power off ...\r\n");
  powerOff(); // BAT off
}

void loop() {
  // put your main code here, to run repeatedly:
}
