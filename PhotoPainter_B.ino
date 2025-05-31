#include "EPD_7in3e.h"
#include "GUI_Paint.h"
#include "DEV_Config.h"
#include <SPI.h>
#include <SD.h>
#include <stdlib.h>  // malloc() free()
#include <string.h>
#include "waveshare_PCF85063.h"
#include <time.h>
#include "pico/rand.h"
#include "led.h"

String localImagePath = "/BMP";
UBYTE Palette[21] = { 0 };
char FileName[100];
// line buffer
uint8_t buff[2400] = { 0 };
uint8_t* BlackImage;
Time_data Time;
Time_data alarmTime;
char hasCard = 0;

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

int EPD_7in3e_display_BMP(const char* path, float vol) {
  //Serial.printf("e-Paper Init and Clear...\r\n");
  EPD_7IN3E_Init();

  //Create a new image cache
  BlackImage = (uint8_t*)malloc(192000);
  //Serial.printf("Paint_NewImage\r\n");
  Paint_NewImage(BlackImage, EPD_7IN3E_WIDTH, EPD_7IN3E_HEIGHT, 0, EPD_7IN3E_WHITE);
  Paint_SetScale(6);
  Paint_SelectImage(BlackImage);

  //Serial.print("Display BMP: ");
  //Serial.println(path);

  File file = SD.open(path);
  if (file) {
    uint32_t startTime = millis();
    //Serial.println("Skip BMP Header");
    Serial.println("Start reading");
    file.read(buff, 54);  // skip BMP header
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
        //image rotated and byte order flipped because connector is at the top - different from regular setup
        BlackImage[row * 400 + (399 - col)] = (color6(buff[col * 6 + 5], buff[col * 6 + 4], buff[col * 6 + 3]) << 4) | (color6(buff[col * 6 + 2], buff[col * 6 + 1], buff[col * 6 + 0]));
      }  // end pixel
    }  // end line
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

int EPD_7in3e_display_Error(float vol) {
  //Serial.printf("e-Paper Init and Clear...\r\n");
  EPD_7IN3E_Init();

  //Create a new image cache
  BlackImage = (uint8_t*)malloc(192000);
  //Serial.printf("Paint_NewImage\r\n");
  Paint_NewImage(BlackImage, EPD_7IN3E_WIDTH, EPD_7IN3E_HEIGHT, 0, EPD_7IN3E_WHITE);
  Paint_SetScale(6);
  Paint_SelectImage(BlackImage);
  Paint_SetRotate(180);
  for (uint8_t f = 0; f < 8; f++) // test pattern
    Paint_DrawRectangle((f % 4) * 200, (f / 4) * 240, (f % 4) * 200 + 200, (f / 4) * 240 + 240, f, DOT_PIXEL_1X1, DRAW_FILL_FULL);
  char strvol[21] = { 0 };
  sprintf(strvol, "%.2fV", vol);
  Paint_DrawString_EN(0, 464, strvol, &Font16, EPD_7IN3E_BLUE, EPD_7IN3E_WHITE);
  if (vol < 3.3) {
    Paint_DrawString_EN(50, 464, "Low voltage, please charge.", &Font16, EPD_7IN3E_RED, EPD_7IN3E_WHITE);
  }
  if (!hasCard) {
    Paint_DrawString_EN(200, 232, "Please insert SD card with pictures.", &Font16, EPD_7IN3E_RED, EPD_7IN3E_WHITE);
  }

  Serial.printf("EPD_Display\r\n");
  EPD_7IN3E_Display(BlackImage);

  Serial.printf("Goto Sleep...\r\n\r\n");
  EPD_7IN3E_Sleep();
  free(BlackImage);
  BlackImage = NULL;

  return 0;
}

void chargeState_callback(unsigned int, long unsigned int) {
  if (DEV_Digital_Read(VBUS)) {
    if (!DEV_Digital_Read(CHARGE_STATE)) {  // is charging
      Serial.printf("Charging...\r\n");
      ledCharging();
    } else {  // charge complete
      ledCharged();
      Serial.printf("Charging complete.\r\n");
    }
  }
}

String Random_File() {
  //Serial.print("Listing directory: ");
  //Serial.println(localImagePath);
  int ct = 0;
  File dir = SD.open(localImagePath);
  while (true) {
    DEV_Digital_Write(LED_ACT, ct % 2);
    File entry = dir.openNextFile();
    if (!entry) break;
    //Serial.println(entry.name());
    ct++;
    entry.close();
  }
  //pick a random number - Pico2 hardware implemented
  uint8_t randValue = (uint8_t)get_rand_32();
  //Serial.printf("randvalue = %d\r\n", randValue);
  int rn = 1 + randValue * ct / 256;
  Serial.printf("Picking number %d of %d files\r\n", rn, ct);
  dir.rewindDirectory();
  ct = 0;
  while (true) {
    DEV_Digital_Write(LED_ACT, ct % 2);
    File entry = dir.openNextFile();
    if (!entry) break;
    ct++;
    if (rn == ct) return entry.name();
    entry.close();
  }
  return "";
}

void run_display() {
  if (!SD.begin(SD_CS_PIN)) {  // using spi0
    Serial.printf("Init SD failed...\r\n");
    EPD_7in3e_display_Error(measureVBAT());
    PCF85063_clear_alarm_flag();   // clear RTC alarm flag
    rtcRunAlarm(Time, alarmTime);  // RTC run alarm
    return;
  }
  hasCard = 1;
  Serial.print("Card size:  ");
  Serial.println((float)SD.size64() / 1000);
  String bmp = Random_File();
  strcpy(FileName, localImagePath.c_str());
  strcat(FileName, "/");
  strcat(FileName, bmp.c_str());
  Serial.println(FileName);
  // TODO: error screen for low battery
  if (bmp == "") {
    Serial.printf("No card inserted or no images present...\r\n");
    EPD_7in3e_display_Error(measureVBAT());
  } else {
    EPD_7in3e_display_BMP(FileName, measureVBAT());
  }
  DEV_Delay_ms(100);
  PCF85063_clear_alarm_flag();   // clear RTC alarm flag
  rtcRunAlarm(Time, alarmTime);  // RTC run alarm
  SD.end();
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
  Serial.printf("Setting callback for charge state...\r\n");
  gpio_set_irq_enabled_with_callback(CHARGE_STATE, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, chargeState_callback);

  Time = { 2025 - 2000, 5, 31, 0, 0, 0 };
  alarmTime = Time;
  // alarmTime.seconds += 10;
  alarmTime.minutes += 5;
  //alarmTime.hours +=12;
  Serial.printf("Starting RTC...\r\n");
  PCF85063_init();  // RTC init

  //set SD pins
  SPI.setRX(SD_MISO_PIN);
  SPI.setTX(SD_MOSI_PIN);
  SPI.setSCK(SD_CLK_PIN);

  //watchdog_enable(60*1000, 1);   // 60s
  // DEV_Delay_ms(1000);

  // rtcRunAlarm(Time, alarmTime);  // RTC run alarm
  //  run_display(Time, alarmTime, hasCard);


  //Serial.printf("power off ...\r\n");
  //https://github.com/raspberrypi/pico-extras/blob/master/src/rp2_common/pico_sleep/sleep.c
  //powerOff(); // BAT off
}

void loop() {
  if (!DEV_Digital_Read(VBUS)) {  // no charging cable connected
    Serial.printf("Running without charger.\r\n");
    run_display();
  } else {  // charging cable connected
    //chargeState_callback(0, 0);
    Serial.printf("Running with charger.\r\n");
    while (DEV_Digital_Read(VBUS)) {
      //measureVBAT();
      if (!DEV_Digital_Read(RTC_INT)) {  // RTC interrupt trigger
        Serial.printf("rtc interrupt\r\n");
        run_display();
      }

      if (!DEV_Digital_Read(BAT_STATE)) {  // KEY pressed
        Serial.printf("key interrupt\r\n");
        run_display();
      }
      DEV_Delay_ms(2000);
    }
  }

  Serial.printf("power off loop...\r\n");
  powerOff();  // BAT off
}
