#include "EPD_7in3e.h"
#include "GUI_Paint.h"
#include "ImageData.h"
#include "DEV_Config.h"
#include <stdlib.h> // malloc() free()
#include <string.h>
#include "waveshare_PCF85063.h"

float measureVBAT(void)
{
    float Voltage=0.0;
    const float conversion_factor = 3.3f / (1 << 12);
    uint16_t result = adc_read();
    Voltage = result * conversion_factor * 3;
    Serial.printf("Raw value: 0x%03x, voltage: %f V\n", result, Voltage);
    return Voltage;
}

int EPD_7in3e_display(float vol)
{
    printf("e-Paper Init and Clear...\r\n");
    EPD_7IN3E_Init();

    //Create a new image cache
    UBYTE *BlackImage;
    UDOUBLE Imagesize = ((EPD_7IN3E_WIDTH % 2 == 0)? (EPD_7IN3E_WIDTH / 2 ): (EPD_7IN3E_WIDTH / 2 + 1)) * EPD_7IN3E_HEIGHT;
    if((BlackImage = (UBYTE *)malloc(Imagesize)) == NULL) {
        printf("Failed to apply for black memory...\r\n");
        return -1;
    }
    Serial.printf("Paint_NewImage\r\n");
    Paint_NewImage(BlackImage, EPD_7IN3E_WIDTH, EPD_7IN3E_HEIGHT, 0, EPD_7IN3E_WHITE);
    Paint_SetScale(6);

    printf("Display BMP\r\n");
    Paint_SelectImage(BlackImage);
    Paint_Clear(EPD_7IN3E_WHITE);
    
    Paint_DrawBitMap(Image6color);

    Paint_SetRotate(180);
    char strvol[21] = {0};
    sprintf(strvol, "%f V", vol);
    if(vol < 5.1) {
        Paint_DrawString_EN(10, 10, "Low voltage, please charge in time.", &Font16, EPD_7IN3E_BLACK, EPD_7IN3E_WHITE);
        Paint_DrawString_EN(10, 26, strvol, &Font16, EPD_7IN3E_BLACK, EPD_7IN3E_WHITE);
    }

    Serial.printf("EPD_Display\r\n");
    EPD_7IN3E_Display(BlackImage);

    Serial.printf("Goto Sleep...\r\n\r\n");
    EPD_7IN3E_Sleep();
    free(BlackImage);
    BlackImage = NULL;

    return 0;
}

int EPD_7in3e_test(void)
{
    printf("e-Paper Init and Clear...\r\n");
    EPD_7IN3E_Init();

    //Create a new image cache
    UBYTE *BlackImage;
    UDOUBLE Imagesize = ((EPD_7IN3E_WIDTH % 2 == 0)? (EPD_7IN3E_WIDTH / 2 ): (EPD_7IN3E_WIDTH / 2 + 1)) * EPD_7IN3E_HEIGHT;
    if((BlackImage = (UBYTE *)malloc(Imagesize)) == NULL) {
        printf("Failed to apply for black memory...\r\n");
        return -1;
    }
    printf("Paint_NewImage\r\n");
    Paint_NewImage(BlackImage, EPD_7IN3E_WIDTH, EPD_7IN3E_HEIGHT, 0, EPD_7IN3E_WHITE);
    Paint_SetScale(6);

//#if 1   // Drawing on the image
    //1.Select Image
    printf("SelectImage:BlackImage\r\n");
    Paint_SelectImage(BlackImage);
    Paint_Clear(EPD_7IN3E_WHITE);

    int hNumber, hWidth, vNumber, vWidth;
    hNumber = 20;
	hWidth = EPD_7IN3E_HEIGHT/hNumber; // 800/20
    vNumber = 10;
	vWidth = EPD_7IN3E_WIDTH/vNumber; // 480/10
	
    // 2.Drawing on the image
    printf("Drawing:BlackImage\r\n");
	for(int i=0; i<vNumber; i++) {
		Paint_DrawRectangle(1, 1+i*vWidth, 800, vWidth*(i+1), EPD_7IN3E_GREEN + (i % 5), DOT_PIXEL_1X1, DRAW_FILL_FULL);
	}
	for(int i=0, j=0; i<hNumber; i++) {
		if(i%2) {
			j++;
			Paint_DrawRectangle(1+i*hWidth, 1, hWidth*(1+i), 480, j%2 ? EPD_7IN3E_BLACK : EPD_7IN3E_WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
		}
	}

    printf("EPD_Display\r\n");
    EPD_7IN3E_Display(BlackImage);
//#endif

    printf("Goto Sleep...\r\n");
    EPD_7IN3E_Sleep();
    free(BlackImage);
    BlackImage = NULL;

    return 0;
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  delay(2000);
  Serial.printf("Init...\r\n");
  if(DEV_Module_Init() != 0) {  // DEV init
    Serial.printf("Failed to init...\r\n");
    return;
  }
  //watchdog_enable(8*1000, 1);   // 8s
  DEV_Delay_ms(1000);
  PCF85063_init();    // RTC init
  //PCF85063_test();
  PCF85063_clear_alarm_flag();    // clear RTC alarm flag
  PCF85063_alarm_Time_Disable();
  EPD_7in3e_display(measureVBAT());
  //EPD_7in3e_test();

}

void loop() {
  // put your main code here, to run repeatedly:

}
