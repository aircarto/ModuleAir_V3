#include <WString.h>
#include <pgmspace.h>

#define SOFTWARE_VERSION_STR "ModuleAirV3-V1-062023-com"
#define SOFTWARE_VERSION_STR_SHORT "V1-062023"
String SOFTWARE_VERSION(SOFTWARE_VERSION_STR);
String SOFTWARE_VERSION_SHORT(SOFTWARE_VERSION_STR_SHORT);

#include <Arduino.h>
#include "PxMatrix.h"
#include <SPI.h>

/*****************************************************************
 * IMPORTANT                                          *
 *****************************************************************/

//On force l'utilisation des 2 SPI

//Dans SPI.cpp

// #if CONFIG_IDF_TARGET_ESP32
// SPIClass SPI(VSPI);
// SPIClass SPI_H(HSPI);
// #else
// SPIClass SPI(FSPI);
// #endif

// Dans SPI.h

// extern SPIClass SPI_H; en bas

//PxMatrix utilise le SPI_H 

//SD utilise le SPI

//on remplace la glcdfont.c original dans AdaFruitGFX => mod dans le dossier Fonts

/*****************************************************************
 * IMPORTANT FIN                                          *
 *****************************************************************/

#include <MHZ16_uart.h> // CO2
#include <MHZ19.h>

#include "ccs811.h" // CCS811

#include "ca-root.h"

// includes ESP32 libraries
#define FORMAT_SPIFFS_IF_FAILED true
#include <FS.h>
#include <SD.h>   //REVOIR QUI SUR SPI QUI SUR SPI_H
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <HardwareSerial.h>

#if ESP_IDF_VERSION_MAJOR >= 4
#if (ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(1, 0, 6))
#include "sha/sha_parallel_engine.h"
#else
#include <esp32/sha.h>
#endif
#else
//#include <hwcrypto/sha.h>
#endif

// includes external libraries

#include "./Fonts/Font4x7Fixed.h" // modified Pour l'affichage des unités
#include "./Fonts/Font4x5Fixed.h" //pour l'affichage des infos de debug

#define ARDUINOJSON_ENABLE_ARDUINO_STREAM 0
#define ARDUINOJSON_ENABLE_ARDUINO_PRINT 0
#define ARDUINOJSON_DECODE_UNICODE 0
#include <ArduinoJson.h>
#include <StreamString.h>
#include "./bmx280_i2c.h"
#include "./configuration.h"

// includes files
#include "./bmx280_i2c.h"
#include "./cairsens.h"
#include "./intl.h"
#include "./utils.h"
#include "defines.h"
#include "ext_def.h"
#include "./html-content.h"


// define size of the config JSON
#define JSON_BUFFER_SIZE 2300
// define size of the AtmoSud Forecast API JSON
#define JSON_BUFFER_SIZE2 500

// test variables
long int sample_count = 0;
bool bmx280_init_failed = false;
bool ccs811_init_failed = false;
bool moduleair_selftest_failed = false;
bool sdcard_found = false;
bool file_created = false;

namespace cfg
{
	unsigned debug = 5;  //default
	unsigned sending_intervall_ms = 120000;

	// main config
	bool has_sdcard = false;
	bool has_matrix = false;
	bool has_wifi = false;
	bool has_lora = false;
	bool has_ethernet = false;

	// (in)active sensors
	bool npm_read = false;
	bool bmx280_read = false;
	bool mhz16_read = false;
	bool mhz19_read = false;
	bool ccs811_read = false;
	bool enveano2_read = false;

	bool display_measure = false;
	bool display_forecast = false;

	unsigned utc_offset = 0;
	bool show_nebuleair = false;
	uint16_t height_above_sealevel = 0;
}


/*****************************************************************
 * Display definitions                                           *
 *****************************************************************/

//For the matrix
hw_timer_t *timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
#define matrix_width 64
#define matrix_height 32
uint8_t display_draw_time = 30; //10-50 is usually fine
PxMATRIX display(64, 32, P_LAT, P_OE, P_A, P_B, P_C, P_D, P_E);

uint8_t logos[6] = {0, 0, 0, 0, 0, 0};
uint8_t logo_index = -1;
bool has_logo;

extern const uint8_t gamma8[]; //for gamma correction

struct RGB
{
	byte R;
	byte G;
	byte B;
};

struct RGB displayColor
{
	0, 0, 0
};

uint16_t myRED = display.color565(255, 0, 0);
uint16_t myGREEN = display.color565(0, 255, 0);
uint16_t myBLUE = display.color565(0, 0, 255);
uint16_t myCYAN = display.color565(0, 255, 255);
uint16_t myWHITE = display.color565(255, 255, 255);
uint16_t myYELLOW = display.color565(255, 255, 0);
uint16_t myMAGENTA = display.color565(255, 0, 255);
uint16_t myBLACK = display.color565(0, 0, 0);
uint16_t myCUSTOM = display.color565(displayColor.R, displayColor.G, displayColor.B);
uint16_t myCOLORS[8] = {myRED, myGREEN, myCYAN, myWHITE, myYELLOW, myCYAN, myMAGENTA, myBLACK};

void IRAM_ATTR display_updater()
{
	// Increment the counter and set the time of ISR
	portENTER_CRITICAL_ISR(&timerMux);
	display.display(display_draw_time);
	portEXIT_CRITICAL_ISR(&timerMux);
}

void display_update_enable(bool is_enable)
{
	Debug.print("Call display_update_enable function with:");
	if (is_enable)
	{
		Debug.println("true");
		//timer = timerBegin(0, 80, true);
		timerAttachInterrupt(timer, &display_updater, true);
		timerAlarmWrite(timer, 4000, true);
		timerAlarmEnable(timer);
	}
	else
	{
		Debug.println("false");
		timerDetachInterrupt(timer);
		timerAlarmDisable(timer);
	}
}

void drawImage(int x, int y, int h, int w, uint16_t image[])
{
	int imageHeight = h;
	int imageWidth = w;
	int counter = 0;
	for (int yy = 0; yy < imageHeight; yy++)
	{
		for (int xx = 0; xx < imageWidth; xx++)
		{
			display.drawPixel(xx + x, yy + y, image[counter]);
			counter++;
		}
	}
}

bool gamma_correction = GAMMA; //Gamma correction

//REVOIR TOUS LES GRADIENTS

struct RGB interpolateint(float valueSensor, int step1, int step2, int step3, bool correction)
{

	struct RGB result;
	uint16_t rgb565;

	if (valueSensor == 0)
	{

		result.R = 0;
		result.G = 255; // VERT
		result.B = 0;
	}
	else if (valueSensor > 0 && valueSensor < step1)
	{

		result.R = 0;
		result.G = 255; // VERT
		result.B = 0;
	}
	else if (valueSensor >= step1 && valueSensor < step2)
	{
		result.R = 255;
		result.G = 255; // jaune
		result.B = 0;
	}
	else if (valueSensor >= step2 && valueSensor < step3)
	{
		result.R = 255;
		result.G = 140; // orange
		result.B = 0;
	}
	else if (valueSensor >= step3)
	{

		result.R = 255;
		result.G = 0; // ROUGE
		result.B = 0;
	}
	else
	{
		result.R = 0;
		result.G = 0;
		result.B = 0;
	}

	if (correction == true)
	{
		result.R = pgm_read_byte(&gamma8[result.R]);
		result.G = pgm_read_byte(&gamma8[result.G]);
		result.B = pgm_read_byte(&gamma8[result.B]);
	}

	rgb565 = ((result.R & 0b11111000) << 8) | ((result.G & 0b11111100) << 3) | (result.B >> 3);
	return result;
}

struct RGB interpolateindice(int valueIndice, bool correction)
{

	struct RGB result;
	uint16_t rgb565;

	switch (valueIndice)
	{
	case 1:
		result.R = 80;
		result.G = 240; //blue
		result.B = 230;
		break;
	case 2:
		result.R = 80;
		result.G = 204; //green
		result.B = 170;
		break;
	case 3:
		result.R = 237;
		result.G = 230; //yellow
		result.B = 97;
		break;
	case 4:
		result.R = 237;
		result.G = 94; //orange
		result.B = 88;
		break;
	case 5:
		result.R = 136;
		result.G = 26; //red
		result.B = 51;
		break;
	case 6:
		result.R = 115;
		result.G = 40; //violet
		result.B = 125;
		break;
	default:
		result.R = 0;
		result.G = 0;
		result.B = 0;
	}

	if (correction == true)
	{
		result.R = pgm_read_byte(&gamma8[result.R]);
		result.G = pgm_read_byte(&gamma8[result.G]);
		result.B = pgm_read_byte(&gamma8[result.B]);
	}

	rgb565 = ((result.R & 0b11111000) << 8) | ((result.G & 0b11111100) << 3) | (result.B >> 3);
	//Debug.println(rgb565); // to get list of color if drawGradient is acitvated
	return result;
}

struct RGB interpolate(float valueSensor, int step1, int step2, int step3, int step4, int step5, bool correction)
{

	byte endColorValueR;
	byte startColorValueR;
	byte endColorValueG;
	byte startColorValueG;
	byte endColorValueB;
	byte startColorValueB;

	int valueLimitHigh;
	int valueLimitLow;
	struct RGB result;
	uint16_t rgb565;

	if (valueSensor == 0)
	{

		result.R = 80;
		result.G = 240; //blue
		result.B = 230;
	}
	else if (valueSensor > 0 && valueSensor <= step5)
	{
		if (valueSensor <= step1)
		{
			valueLimitHigh = step1;
			valueLimitLow = 0;
			endColorValueR = 80;
			startColorValueR = 80; //blue to green
			endColorValueG = 204;
			startColorValueG = 240;
			endColorValueB = 170;
			startColorValueB = 230;
		}
		else if (valueSensor > step1 && valueSensor <= step2)
		{
			valueLimitHigh = step2;
			valueLimitLow = step1;
			endColorValueR = 237;
			startColorValueR = 80;
			endColorValueG = 230; //green to yellow
			startColorValueG = 204;
			endColorValueB = 97;
			startColorValueB = 170;
		}
		else if (valueSensor > step2 && valueSensor <= step3)
		{
			valueLimitHigh = step3;
			valueLimitLow = step2;
			endColorValueR = 237;
			startColorValueR = 237;
			endColorValueG = 94; //yellow to orange
			startColorValueG = 230;
			endColorValueB = 88;
			startColorValueB = 97;
		}
		else if (valueSensor > step3 && valueSensor <= step4)
		{

			valueLimitHigh = step4;
			valueLimitLow = step3;
			endColorValueR = 136;
			startColorValueR = 237;
			endColorValueG = 26; // orange to red
			startColorValueG = 94;
			endColorValueB = 51;
			startColorValueB = 88;
		}
		else if (valueSensor > step4 && valueSensor <= step5)
		{
			valueLimitHigh = step5;
			valueLimitLow = step4;
			endColorValueR = 115;
			startColorValueR = 136;
			endColorValueG = 40; // red to violet
			startColorValueG = 26;
			endColorValueB = 125;
			startColorValueB = 51;
		}

		result.R = (byte)(((endColorValueR - startColorValueR) * ((valueSensor - valueLimitLow) / (valueLimitHigh - valueLimitLow))) + startColorValueR);
		result.G = (byte)(((endColorValueG - startColorValueG) * ((valueSensor - valueLimitLow) / (valueLimitHigh - valueLimitLow))) + startColorValueG);
		result.B = (byte)(((endColorValueB - startColorValueB) * ((valueSensor - valueLimitLow) / (valueLimitHigh - valueLimitLow))) + startColorValueB);
	}
	else if (valueSensor > step5)
	{
		result.R = 115;
		result.G = 40; //violet
		result.B = 125;
	}
	else
	{
		result.R = 0;
		result.G = 0;
		result.B = 0;
	}

	//Gamma Correction

	if (correction == true)
	{
		result.R = pgm_read_byte(&gamma8[result.R]);
		result.G = pgm_read_byte(&gamma8[result.G]);
		result.B = pgm_read_byte(&gamma8[result.B]);
	}

	rgb565 = ((result.R & 0b11111000) << 8) | ((result.G & 0b11111100) << 3) | (result.B >> 3);
	//Debug.println(rgb565); // to get list of color if drawGradient is acitvated
	return result;
}

struct RGB interpolateint2(float valueSensor, int step1, int step2, bool correction)
{

	struct RGB result;
	uint16_t rgb565;

	if (valueSensor == 0)
	{

		result.R = 0;
		result.G = 255; // Green entre 0 et 800
		result.B = 0;
	}
	else if (valueSensor > 0 && valueSensor < step1)
	{

		result.R = 0;
		result.G = 255; // Green entre 0 et 800
		result.B = 0;
	}
	else if (valueSensor >= step1 && valueSensor < step2)
	{
		result.R = 255;
		result.G = 140; // Orange entre 800 et 1500
		result.B = 0;
	}
	else if (valueSensor >= step2)
	{
		result.R = 255;
		result.G = 0; // Rouge supérieur à 1500
		result.B = 0;
	}
	else
	{
		result.R = 0;
		result.G = 0;
		result.B = 0;
	}

	if (correction == true)
	{
		result.R = pgm_read_byte(&gamma8[result.R]);
		result.G = pgm_read_byte(&gamma8[result.G]);
		result.B = pgm_read_byte(&gamma8[result.B]);
	}

	rgb565 = ((result.R & 0b11111000) << 8) | ((result.G & 0b11111100) << 3) | (result.B >> 3);
	//Debug.println(rgb565); // to get list of color if drawGradient is acitvated
	return result;
}

struct RGB interpolateint3(float valueSensor, int step1, int step2, bool correction) // Humi
{

	struct RGB result;
	uint16_t rgb565;

	if (valueSensor == 0)
	{

		result.R = 255;
		result.G = 0; // red
		result.B = 0;
	}
	else if (valueSensor > 0 && valueSensor < step1)
	{
		result.R = 255;
		result.G = 0; // red
		result.B = 0;
	}
	else if (valueSensor >= step1 && valueSensor < step2)
	{
		result.R = 0;
		result.G = 255; // green
		result.B = 0;
	}
	else if (valueSensor > step2)
	{
		result.R = 255;
		result.G = 0; // red
		result.B = 0;
	}
	else
	{
		result.R = 0;
		result.G = 0;
		result.B = 0;
	}

	if (correction == true)
	{
		result.R = pgm_read_byte(&gamma8[result.R]);
		result.G = pgm_read_byte(&gamma8[result.G]);
		result.B = pgm_read_byte(&gamma8[result.B]);
	}

	rgb565 = ((result.R & 0b11111000) << 8) | ((result.G & 0b11111100) << 3) | (result.B >> 3);
	//Debug.println(rgb565); // to get list of color if drawGradient is acitvated
	return result;
}

struct RGB interpolateint4(float valueSensor, int step1, int step2, bool correction) // temp
{

	struct RGB result;
	uint16_t rgb565;

	if (valueSensor >= -128 && valueSensor < step1)
	{
		result.R = 0;
		result.G = 0; // Bleu / Trop froid inférieur à 19 (step1)
		result.B = 255;
	}
	else if (valueSensor >= step1 && valueSensor < step2)
	{

		result.R = 0;
		result.G = 255; // Green ok
		result.B = 0;
	}
	else if (valueSensor >= step2)
	{
		result.R = 255;
		result.G = 0; // RED / trop chaud supérieur à 28
		result.B = 0;
	}
	else
	{
		result.R = 0;
		result.G = 0;
		result.B = 0;
	}

	if (correction == true)
	{
		result.R = pgm_read_byte(&gamma8[result.R]);
		result.G = pgm_read_byte(&gamma8[result.G]);
		result.B = pgm_read_byte(&gamma8[result.B]);
	}

	rgb565 = ((result.R & 0b11111000) << 8) | ((result.G & 0b11111100) << 3) | (result.B >> 3);
	//Debug.println(rgb565); // to get list of color if drawGradient is acitvated
	return result;
}

//You can use drawGradient once in order to get the list of colors and then create an image which is much faster to display


struct RGB colorNO2(int valueSensor, int step1, int step2, int step3, int step4, int step5, bool correction)
{
	struct RGB result;
	uint16_t rgb565;

	if (valueSensor == 0)
	{
		result.R = 80;
		result.G = 240; //blue
		result.B = 230;
	}
	else if (valueSensor > 0 && valueSensor <= step5)
	{
		if (valueSensor <= step1)
		{
			result.R = 80;
			result.G = 240; //blue
			result.B = 230;
		}
		else if (valueSensor > step1 && valueSensor <= step2)
		{
			result.R = 80;
			result.G = 204; //green
			result.B = 170;
		}
		else if (valueSensor > step2 && valueSensor <= step3)
		{
			result.R = 237;
			result.G = 230; //yellow
			result.B = 97;
		}
		else if (valueSensor > step3 && valueSensor <= step4)
		{
			result.R = 237;
			result.G = 94; //orange
			result.B = 88;
		}
		else if (valueSensor > step4 && valueSensor <= step5)
		{
			result.R = 136;
			result.G = 26; //red
			result.B = 51;
		}
	}
	else if (valueSensor > step5)
	{
		result.R = 115;
		result.G = 40; //violet
		result.B = 125;
	}
	else
	{
		result.R = 0;
		result.G = 0;
		result.B = 0;
	}

	//Gamma Correction

	if (correction == true)
	{
		result.R = pgm_read_byte(&gamma8[result.R]);
		result.G = pgm_read_byte(&gamma8[result.G]);
		result.B = pgm_read_byte(&gamma8[result.B]);
	}

	rgb565 = ((result.R & 0b11111000) << 8) | ((result.G & 0b11111100) << 3) | (result.B >> 3);
	//Debug.println(rgb565); // to get list of color if drawGradient is acitvated
	return result;
}

struct RGB interpolateNO2(float valueSensor, int step1, int step2, int step3, int step4, int step5, bool correction)
{

	byte endColorValueR;
	byte startColorValueR;
	byte endColorValueG;
	byte startColorValueG;
	byte endColorValueB;
	byte startColorValueB;

	int valueLimitHigh;
	int valueLimitLow;
	struct RGB result;
	uint16_t rgb565;

	if (valueSensor == 0)
	{

		result.R = 80;
		result.G = 240; //blue
		result.B = 230;
	}
	else if (valueSensor > 0 && valueSensor <= step5)
	{
		if (valueSensor <= step1)
		{
			valueLimitHigh = step1;
			valueLimitLow = 0;
			endColorValueR = 80;
			startColorValueR = 80; //blue to green
			endColorValueG = 204;
			startColorValueG = 240;
			endColorValueB = 170;
			startColorValueB = 230;
		}
		else if (valueSensor > step1 && valueSensor <= step2)
		{
			valueLimitHigh = step2;
			valueLimitLow = step1;
			endColorValueR = 237;
			startColorValueR = 80;
			endColorValueG = 230; //green to yellow
			startColorValueG = 204;
			endColorValueB = 97;
			startColorValueB = 170;
		}
		else if (valueSensor > step2 && valueSensor <= step3)
		{
			valueLimitHigh = step3;
			valueLimitLow = step2;
			endColorValueR = 237;
			startColorValueR = 237;
			endColorValueG = 94; //yellow to orange
			startColorValueG = 230;
			endColorValueB = 88;
			startColorValueB = 97;
		}
		else if (valueSensor > step3 && valueSensor <= step4)
		{

			valueLimitHigh = step4;
			valueLimitLow = step3;
			endColorValueR = 136;
			startColorValueR = 237;
			endColorValueG = 26; // orange to red
			startColorValueG = 94;
			endColorValueB = 51;
			startColorValueB = 88;
		}
		else if (valueSensor > step4 && valueSensor <= step5)
		{
			valueLimitHigh = step5;
			valueLimitLow = step4;
			endColorValueR = 115;
			startColorValueR = 136;
			endColorValueG = 40; // red to violet
			startColorValueG = 26;
			endColorValueB = 125;
			startColorValueB = 51;
		}

		result.R = (byte)(((endColorValueR - startColorValueR) * ((valueSensor - valueLimitLow) / (valueLimitHigh - valueLimitLow))) + startColorValueR);
		result.G = (byte)(((endColorValueG - startColorValueG) * ((valueSensor - valueLimitLow) / (valueLimitHigh - valueLimitLow))) + startColorValueG);
		result.B = (byte)(((endColorValueB - startColorValueB) * ((valueSensor - valueLimitLow) / (valueLimitHigh - valueLimitLow))) + startColorValueB);
	}
	else if (valueSensor > step5)
	{
		result.R = 115;
		result.G = 40; //violet
		result.B = 125;
	}
	else
	{
		result.R = 0;
		result.G = 0;
		result.B = 0;
	}

	//Gamma Correction

	if (correction == true)
	{
		result.R = pgm_read_byte(&gamma8[result.R]);
		result.G = pgm_read_byte(&gamma8[result.G]);
		result.B = pgm_read_byte(&gamma8[result.B]);
	}

	rgb565 = ((result.R & 0b11111000) << 8) | ((result.G & 0b11111100) << 3) | (result.B >> 3);
	//Debug.println(rgb565); // to get list of color if drawGradient is acitvated
	return result;
}

//LAST VERSIONS OF GRADIENT

// struct RGB colorPM(int valueSensor, int step1, int step2, int step3, int step4, int step5, bool correction)
// {
// 	struct RGB result;
// 	uint16_t rgb565;

// 	if (valueSensor == 0)
// 	{
// 		result.R = 80;
// 		result.G = 240; //blue
// 		result.B = 230;
// 	}
// 	else if (valueSensor > 0 && valueSensor <= step5)
// 	{
// 		if (valueSensor <= step1)
// 		{
// 			result.R = 80;
// 			result.G = 240; //blue
// 			result.B = 230;
// 		}
// 		else if (valueSensor > step1 && valueSensor <= step2)
// 		{
// 			result.R = 80;
// 			result.G = 204; //green
// 			result.B = 170;
// 		}
// 		else if (valueSensor > step2 && valueSensor <= step3)
// 		{
// 			result.R = 237;
// 			result.G = 230; //yellow
// 			result.B = 97;
// 		}
// 		else if (valueSensor > step3 && valueSensor <= step4)
// 		{
// 			result.R = 237;
// 			result.G = 94; //orange
// 			result.B = 88;
// 		}
// 		else if (valueSensor > step4 && valueSensor <= step5)
// 		{
// 			result.R = 136;
// 			result.G = 26; //red
// 			result.B = 51;
// 		}
// 	}
// 	else if (valueSensor > step5)
// 	{
// 		result.R = 115;
// 		result.G = 40; //violet
// 		result.B = 125;
// 	}
// 	else
// 	{
// 		result.R = 0;
// 		result.G = 0;
// 		result.B = 0;
// 	}

// 	//Gamma Correction

// 	if (correction == true)
// 	{
// 		result.R = pgm_read_byte(&gamma8[result.R]);
// 		result.G = pgm_read_byte(&gamma8[result.G]);
// 		result.B = pgm_read_byte(&gamma8[result.B]);
// 	}

// 	rgb565 = ((result.R & 0b11111000) << 8) | ((result.G & 0b11111100) << 3) | (result.B >> 3);
// 	//Debug.println(rgb565); // to get list of color if drawGradient is acitvated
// 	return result;
// }

// struct RGB interpolatePM(float valueSensor, int step1, int step2, int step3, int step4, int step5, bool correction)
// {

// 	byte endColorValueR;
// 	byte startColorValueR;
// 	byte endColorValueG;
// 	byte startColorValueG;
// 	byte endColorValueB;
// 	byte startColorValueB;

// 	int valueLimitHigh;
// 	int valueLimitLow;
// 	struct RGB result;
// 	uint16_t rgb565;

// 	if (valueSensor == 0)
// 	{

// 		result.R = 80;
// 		result.G = 240; //blue
// 		result.B = 230;
// 	}
// 	else if (valueSensor > 0 && valueSensor <= step5)
// 	{
// 		if (valueSensor <= step1)
// 		{
// 			valueLimitHigh = step1;
// 			valueLimitLow = 0;
// 			endColorValueR = 80;
// 			startColorValueR = 80; //blue to green
// 			endColorValueG = 204;
// 			startColorValueG = 240;
// 			endColorValueB = 170;
// 			startColorValueB = 230;
// 		}
// 		else if (valueSensor > step1 && valueSensor <= step2)
// 		{
// 			valueLimitHigh = step2;
// 			valueLimitLow = step1;
// 			endColorValueR = 237;
// 			startColorValueR = 80;
// 			endColorValueG = 230; //green to yellow
// 			startColorValueG = 204;
// 			endColorValueB = 97;
// 			startColorValueB = 170;
// 		}
// 		else if (valueSensor > step2 && valueSensor <= step3)
// 		{
// 			valueLimitHigh = step3;
// 			valueLimitLow = step2;
// 			endColorValueR = 237;
// 			startColorValueR = 237;
// 			endColorValueG = 94; //yellow to orange
// 			startColorValueG = 230;
// 			endColorValueB = 88;
// 			startColorValueB = 97;
// 		}
// 		else if (valueSensor > step3 && valueSensor <= step4)
// 		{

// 			valueLimitHigh = step4;
// 			valueLimitLow = step3;
// 			endColorValueR = 136;
// 			startColorValueR = 237;
// 			endColorValueG = 26; // orange to red
// 			startColorValueG = 94;
// 			endColorValueB = 51;
// 			startColorValueB = 88;
// 		}
// 		else if (valueSensor > step4 && valueSensor <= step5)
// 		{
// 			valueLimitHigh = step5;
// 			valueLimitLow = step4;
// 			endColorValueR = 115;
// 			startColorValueR = 136;
// 			endColorValueG = 40; // red to violet
// 			startColorValueG = 26;
// 			endColorValueB = 125;
// 			startColorValueB = 51;
// 		}

// 		result.R = (byte)(((endColorValueR - startColorValueR) * ((valueSensor - valueLimitLow) / (valueLimitHigh - valueLimitLow))) + startColorValueR);
// 		result.G = (byte)(((endColorValueG - startColorValueG) * ((valueSensor - valueLimitLow) / (valueLimitHigh - valueLimitLow))) + startColorValueG);
// 		result.B = (byte)(((endColorValueB - startColorValueB) * ((valueSensor - valueLimitLow) / (valueLimitHigh - valueLimitLow))) + startColorValueB);
// 	}
// 	else if (valueSensor > step5)
// 	{
// 		result.R = 115;
// 		result.G = 40; //violet
// 		result.B = 125;
// 	}
// 	else
// 	{
// 		result.R = 0;
// 		result.G = 0;
// 		result.B = 0;
// 	}

// 	//Gamma Correction

// 	if (correction == true)
// 	{
// 		result.R = pgm_read_byte(&gamma8[result.R]);
// 		result.G = pgm_read_byte(&gamma8[result.G]);
// 		result.B = pgm_read_byte(&gamma8[result.B]);
// 	}

// 	rgb565 = ((result.R & 0b11111000) << 8) | ((result.G & 0b11111100) << 3) | (result.B >> 3);
// 	//Debug.println(rgb565); // to get list of color if drawGradient is acitvated
// 	return result;
// }

// struct RGB interpolateCOV(float valueSensor, int step1, int step2, bool correction)
// {

// 	struct RGB result;
// 	uint16_t rgb565;

// 	if (valueSensor == 0)
// 	{
// 		result.R = 0;
// 		result.G = 255; // Green entre 0 et 800
// 		result.B = 0;
// 	}
// 	else if (valueSensor > 0 && valueSensor < step1)
// 	{

// 		result.R = 0;
// 		result.G = 255; // Green entre 0 et 800
// 		result.B = 0;
// 	}
// 	else if (valueSensor >= step1 && valueSensor < step2)
// 	{
// 		result.R = 255;
// 		result.G = 140; // Orange entre 800 et 1500
// 		result.B = 0;
// 	}
// 	else if (valueSensor >= step2)
// 	{
// 		result.R = 255;
// 		result.G = 0; // Rouge supérieur à 1500
// 		result.B = 0;
// 	}
// 	else
// 	{
// 		result.R = 0;
// 		result.G = 0;
// 		result.B = 0;
// 	}

// 	if (correction == true)
// 	{
// 		result.R = pgm_read_byte(&gamma8[result.R]);
// 		result.G = pgm_read_byte(&gamma8[result.G]);
// 		result.B = pgm_read_byte(&gamma8[result.B]);
// 	}

// 	rgb565 = ((result.R & 0b11111000) << 8) | ((result.G & 0b11111100) << 3) | (result.B >> 3);
// 	//Debug.println(rgb565); // to get list of color if drawGradient is acitvated
// 	return result;
// }

// struct RGB colorNO2(int valueSensor, int step1, int step2, int step3, int step4, int step5, bool correction)
// {
// 	struct RGB result;
// 	uint16_t rgb565;

// 	if (valueSensor == 0)
// 	{
// 		result.R = 80;
// 		result.G = 240; //blue
// 		result.B = 230;
// 	}
// 	else if (valueSensor > 0 && valueSensor <= step5)
// 	{
// 		if (valueSensor <= step1)
// 		{
// 			result.R = 80;
// 			result.G = 240; //blue
// 			result.B = 230;
// 		}
// 		else if (valueSensor > step1 && valueSensor <= step2)
// 		{
// 			result.R = 80;
// 			result.G = 204; //green
// 			result.B = 170;
// 		}
// 		else if (valueSensor > step2 && valueSensor <= step3)
// 		{
// 			result.R = 237;
// 			result.G = 230; //yellow
// 			result.B = 97;
// 		}
// 		else if (valueSensor > step3 && valueSensor <= step4)
// 		{
// 			result.R = 237;
// 			result.G = 94; //orange
// 			result.B = 88;
// 		}
// 		else if (valueSensor > step4 && valueSensor <= step5)
// 		{
// 			result.R = 136;
// 			result.G = 26; //red
// 			result.B = 51;
// 		}
// 	}
// 	else if (valueSensor > step5)
// 	{
// 		result.R = 115;
// 		result.G = 40; //violet
// 		result.B = 125;
// 	}
// 	else
// 	{
// 		result.R = 0;
// 		result.G = 0;
// 		result.B = 0;
// 	}

// 	//Gamma Correction

// 	if (correction == true)
// 	{
// 		result.R = pgm_read_byte(&gamma8[result.R]);
// 		result.G = pgm_read_byte(&gamma8[result.G]);
// 		result.B = pgm_read_byte(&gamma8[result.B]);
// 	}

// 	rgb565 = ((result.R & 0b11111000) << 8) | ((result.G & 0b11111100) << 3) | (result.B >> 3);
// 	//Debug.println(rgb565); // to get list of color if drawGradient is acitvated
// 	return result;
// }

// struct RGB interpolateNO2(float valueSensor, int step1, int step2, int step3, int step4, int step5, bool correction)
// {

// 	byte endColorValueR;
// 	byte startColorValueR;
// 	byte endColorValueG;
// 	byte startColorValueG;
// 	byte endColorValueB;
// 	byte startColorValueB;

// 	int valueLimitHigh;
// 	int valueLimitLow;
// 	struct RGB result;
// 	uint16_t rgb565;

// 	if (valueSensor == 0)
// 	{

// 		result.R = 80;
// 		result.G = 240; //blue
// 		result.B = 230;
// 	}
// 	else if (valueSensor > 0 && valueSensor <= step5)
// 	{
// 		if (valueSensor <= step1)
// 		{
// 			valueLimitHigh = step1;
// 			valueLimitLow = 0;
// 			endColorValueR = 80;
// 			startColorValueR = 80; //blue to green
// 			endColorValueG = 204;
// 			startColorValueG = 240;
// 			endColorValueB = 170;
// 			startColorValueB = 230;
// 		}
// 		else if (valueSensor > step1 && valueSensor <= step2)
// 		{
// 			valueLimitHigh = step2;
// 			valueLimitLow = step1;
// 			endColorValueR = 237;
// 			startColorValueR = 80;
// 			endColorValueG = 230; //green to yellow
// 			startColorValueG = 204;
// 			endColorValueB = 97;
// 			startColorValueB = 170;
// 		}
// 		else if (valueSensor > step2 && valueSensor <= step3)
// 		{
// 			valueLimitHigh = step3;
// 			valueLimitLow = step2;
// 			endColorValueR = 237;
// 			startColorValueR = 237;
// 			endColorValueG = 94; //yellow to orange
// 			startColorValueG = 230;
// 			endColorValueB = 88;
// 			startColorValueB = 97;
// 		}
// 		else if (valueSensor > step3 && valueSensor <= step4)
// 		{

// 			valueLimitHigh = step4;
// 			valueLimitLow = step3;
// 			endColorValueR = 136;
// 			startColorValueR = 237;
// 			endColorValueG = 26; // orange to red
// 			startColorValueG = 94;
// 			endColorValueB = 51;
// 			startColorValueB = 88;
// 		}
// 		else if (valueSensor > step4 && valueSensor <= step5)
// 		{
// 			valueLimitHigh = step5;
// 			valueLimitLow = step4;
// 			endColorValueR = 115;
// 			startColorValueR = 136;
// 			endColorValueG = 40; // red to violet
// 			startColorValueG = 26;
// 			endColorValueB = 125;
// 			startColorValueB = 51;
// 		}

// 		result.R = (byte)(((endColorValueR - startColorValueR) * ((valueSensor - valueLimitLow) / (valueLimitHigh - valueLimitLow))) + startColorValueR);
// 		result.G = (byte)(((endColorValueG - startColorValueG) * ((valueSensor - valueLimitLow) / (valueLimitHigh - valueLimitLow))) + startColorValueG);
// 		result.B = (byte)(((endColorValueB - startColorValueB) * ((valueSensor - valueLimitLow) / (valueLimitHigh - valueLimitLow))) + startColorValueB);
// 	}
// 	else if (valueSensor > step5)
// 	{
// 		result.R = 115;
// 		result.G = 40; //violet
// 		result.B = 125;
// 	}
// 	else
// 	{
// 		result.R = 0;
// 		result.G = 0;
// 		result.B = 0;
// 	}

// 	//Gamma Correction

// 	if (correction == true)
// 	{
// 		result.R = pgm_read_byte(&gamma8[result.R]);
// 		result.G = pgm_read_byte(&gamma8[result.G]);
// 		result.B = pgm_read_byte(&gamma8[result.B]);
// 	}

// 	rgb565 = ((result.R & 0b11111000) << 8) | ((result.G & 0b11111100) << 3) | (result.B >> 3);
// 	//Debug.println(rgb565); // to get list of color if drawGradient is acitvated
// 	return result;
// }

// struct RGB interpolateHumi(float valueSensor, int step1, int step2, bool correction) // Humi
// {

// 	struct RGB result;
// 	uint16_t rgb565;

// 	if (valueSensor == 0)
// 	{
// 		result.R = 255;
// 		result.G = 0; // red
// 		result.B = 0;
// 	}
// 	else if (valueSensor > 0 && valueSensor < step1)
// 	{
// 		result.R = 255;
// 		result.G = 0; // red
// 		result.B = 0;
// 	}
// 	else if (valueSensor >= step1 && valueSensor < step2)
// 	{
// 		result.R = 0;
// 		result.G = 255; // green
// 		result.B = 0;
// 	}
// 	else if (valueSensor > step2)
// 	{
// 		result.R = 255;
// 		result.G = 0; // red
// 		result.B = 0;
// 	}
// 	else
// 	{
// 		result.R = 0;
// 		result.G = 0;
// 		result.B = 0;
// 	}

// 	if (correction == true)
// 	{
// 		result.R = pgm_read_byte(&gamma8[result.R]);
// 		result.G = pgm_read_byte(&gamma8[result.G]);
// 		result.B = pgm_read_byte(&gamma8[result.B]);
// 	}

// 	rgb565 = ((result.R & 0b11111000) << 8) | ((result.G & 0b11111100) << 3) | (result.B >> 3);
// 	//Debug.println(rgb565); // to get list of color if drawGradient is acitvated
// 	return result;
// }

// struct RGB interpolatePress(float valueSensor, int step1, int step2, bool correction) // Humi
// {

// 	struct RGB result;
// 	uint16_t rgb565;

// 	if (valueSensor == 0)
// 	{
// 		result.R = 255;
// 		result.G = 0; // red
// 		result.B = 0;
// 	}
// 	else if (valueSensor > 0 && valueSensor < step1)
// 	{
// 		result.R = 255;
// 		result.G = 0; // red
// 		result.B = 0;
// 	}
// 	else if (valueSensor >= step1 && valueSensor < step2)
// 	{
// 		result.R = 0;
// 		result.G = 255; // green
// 		result.B = 0;
// 	}
// 	else if (valueSensor > step2)
// 	{
// 		result.R = 255;
// 		result.G = 0; // red
// 		result.B = 0;
// 	}
// 	else
// 	{
// 		result.R = 0;
// 		result.G = 0;
// 		result.B = 0;
// 	}

// 	if (correction == true)
// 	{
// 		result.R = pgm_read_byte(&gamma8[result.R]);
// 		result.G = pgm_read_byte(&gamma8[result.G]);
// 		result.B = pgm_read_byte(&gamma8[result.B]);
// 	}

// 	rgb565 = ((result.R & 0b11111000) << 8) | ((result.G & 0b11111100) << 3) | (result.B >> 3);
// 	//Debug.println(rgb565); // to get list of color if drawGradient is acitvated
// 	return result;
// }

// struct RGB interpolateTemp(float valueSensor, int step1, int step2, bool correction) // temp
// {

// 	struct RGB result;
// 	uint16_t rgb565;

// 	if (valueSensor >= -128 && valueSensor < step1)
// 	{
// 		result.R = 0;
// 		result.G = 0; // Bleu / Trop froid inférieur à 19 (step1)
// 		result.B = 255;
// 	}
// 	else if (valueSensor >= step1 && valueSensor < step2)
// 	{
// 		result.R = 0;
// 		result.G = 255; // Green ok
// 		result.B = 0;
// 	}
// 	else if (valueSensor >= step2)
// 	{
// 		result.R = 255;
// 		result.G = 0; // RED / trop chaud supérieur à 28
// 		result.B = 0;
// 	}
// 	else
// 	{
// 		result.R = 0;
// 		result.G = 0;
// 		result.B = 0;
// 	}

// 	if (correction == true)
// 	{
// 		result.R = pgm_read_byte(&gamma8[result.R]);
// 		result.G = pgm_read_byte(&gamma8[result.G]);
// 		result.B = pgm_read_byte(&gamma8[result.B]);
// 	}

// 	rgb565 = ((result.R & 0b11111000) << 8) | ((result.G & 0b11111100) << 3) | (result.B >> 3);
// 	//Debug.println(rgb565); // to get list of color if drawGradient is acitvated
// 	return result;
// }

// struct RGB interpolateSignal(int32_t valueSignal, int step1, int step2)
// {

// 	byte endColorValueR;
// 	byte startColorValueR;
// 	byte endColorValueG;
// 	byte startColorValueG;
// 	byte endColorValueB;
// 	byte startColorValueB;

// 	int valueLimitHigh;
// 	int valueLimitLow;
// 	struct RGB result;
// 	uint16_t rgb565;

// 	if (valueSignal == 0)
// 	{

// 		result.R = 255;
// 		result.G = 0; //red
// 		result.B = 0;
// 	}
// 	else if (valueSignal > 0 && valueSignal < 100)
// 	{
// 		if (valueSignal <= step1)
// 		{
// 			valueLimitHigh = step1;
// 			valueLimitLow = 0;
// 			endColorValueR = 255;
// 			startColorValueR = 255; //red to orange
// 			endColorValueG = 128;
// 			startColorValueG = 0;
// 			endColorValueB = 0;
// 			startColorValueB = 0;
// 		}
// 		else if (valueSignal > step1 && valueSignal <= step2)
// 		{
// 			valueLimitHigh = step2;
// 			valueLimitLow = step1;
// 			endColorValueR = 255;
// 			startColorValueR = 255;
// 			endColorValueG = 255; //orange to yellow
// 			startColorValueG = 128;
// 			endColorValueB = 0;
// 			startColorValueB = 0;
// 		}
// 		else if (valueSignal > step2)
// 		{
// 			valueLimitHigh = 100;
// 			valueLimitLow = step2;
// 			endColorValueR = 0;
// 			startColorValueR = 255;
// 			endColorValueG = 255; // yellow to green
// 			startColorValueG = 255;
// 			endColorValueB = 0;
// 			startColorValueB = 0;
// 		}

// 		result.R = (byte)(((endColorValueR - startColorValueR) * ((valueSignal - valueLimitLow) / (valueLimitHigh - valueLimitLow))) + startColorValueR);
// 		result.G = (byte)(((endColorValueG - startColorValueG) * ((valueSignal - valueLimitLow) / (valueLimitHigh - valueLimitLow))) + startColorValueG);
// 		result.B = (byte)(((endColorValueB - startColorValueB) * ((valueSignal - valueLimitLow) / (valueLimitHigh - valueLimitLow))) + startColorValueB);
// 	}
// 	else if (valueSignal >= 100)
// 	{
// 		result.R = 0;
// 		result.G = 255; //green
// 		result.B = 0;
// 	}
// 	else
// 	{
// 		result.R = 0;
// 		result.G = 0;
// 		result.B = 0;
// 	}

// 	return result;
// }











void drawgradient(int x, int y, float valueSensor, int step1, int step2, int step3, int step4, int step5)
{
	int gradientHeight = 7;
	int gradientWidth = 64;
	int pixelvalue[64];
	RGB pixelcolors[64];

	Debug.println("Pixel values");
	for (uint8_t i = 0; i < gradientWidth; i++)
	{
		pixelvalue[i] = (int)((i * step5) / (gradientWidth - 1));
		Debug.print(" ");
		Debug.print(pixelvalue[i]);
		if (i == 63)
		{
			Debug.printf("\n");
		}
	}

	for (uint8_t j = 0; j < gradientWidth; j++)
	{
		int value = pixelvalue[j];
		pixelcolors[j] = interpolate(value, step1, step2, step3, step4, step5, true);
	}

	for (uint8_t k = 0; k < gradientHeight; k++)
	{

		for (int l = 0; l < gradientWidth; l++)
		{
			uint16_t myPIXEL = display.color565(pixelcolors[l].R, pixelcolors[l].G, pixelcolors[l].B);
			display.drawPixel(x + l, y + k, myPIXEL);
		}
	}
}

void messager1(float valueSensor, int step1, int step2, int step3)
{

	display.setTextSize(1);

	if (valueSensor > -1 && valueSensor < step1)
	{
		display.setFont(NULL);
		display.setCursor(23, 25);
		display.print("BON");
	}
	else if (valueSensor >= step1 && valueSensor < step2)
	{
		display.setFont(NULL);
		display.setCursor(17, 25);
		display.print("MOYEN");
	}
	else if (valueSensor >= step2 && valueSensor < step3)
	{
		display.setFont(NULL);
		display.setCursor(11, 25);
		display.print("DEGRADE");
	}
	else if (valueSensor >= step3)
	{
		display.setFont(NULL);
		display.setCursor(11, 25);
		display.print("MAUVAIS");
	}
	else
	{
		display.setFont(NULL);
		display.setCursor(14, 25);
		display.print("ERREUR");
	}
}

void messager2(float valueSensor, int step1, int step2)
{

	display.setFont(NULL);
	display.setTextSize(1);

	if (valueSensor > -1 && valueSensor < step1)
	{
		display.setCursor(20, 25);
		display.print("BIEN"); // inférieur à 800ppm
	}
	else if (valueSensor >= step1 && valueSensor < step2)
	{
		display.setCursor(5, 25);
		display.print("AERER SVP"); // entre 800 et 1500
	}
	else if (valueSensor >= step2)
	{
		display.setCursor(2, 25);
		display.print("AERER VITE");
	}
	else
	{
		display.setCursor(14, 25);
		display.setTextSize(1);
		display.print("ERREUR");
	}
}

void messager3(float valueSensor, int step1, int step2) // humi
{
	display.setFont(NULL);
	display.setTextSize(1);

	if (valueSensor > -1 && valueSensor < step1)
	{
		display.setCursor(2, 25);
		display.print("TROP SEC");
	}
	else if (valueSensor >= step1 && valueSensor < step2)
	{

		display.setCursor(20, 25);
		display.print("IDEAL");
	}
	else if (valueSensor >= step2)
	{
		display.setCursor(0, 25);
		display.print("TROP HUMIDE");
	}
	else
	{
		display.setCursor(14, 25);
		display.setTextSize(1);
		display.print("ERREUR");
	}
}

void messager4(float valueSensor, int step1, int step2) // temp
{
	display.setFont(NULL);
	display.setTextSize(1);

	if (valueSensor > -128 && valueSensor < step1)
	{
		display.setCursor(2, 25);
		display.print("TROP FROID");
	}
	else if (valueSensor >= step1 && valueSensor < step2)
	{

		display.setCursor(10, 25);
		display.print("CONFORT");
	}
	else if (valueSensor >= step2)
	{
		display.setCursor(2, 25);
		display.print("TROP CHAUD");
	}
	else
	{
		display.setCursor(14, 25);
		display.setTextSize(1);
		display.print("ERREUR");
	}
}

void messager5(int value) // Indice Atmo
{
	display.setFont(NULL);
	display.setTextSize(1);

	switch (value)
	{
	case 1:
		display.setFont(NULL);
		display.setCursor(23, 25);
		display.print("BON");
		break;
	case 2:
		display.setFont(NULL);
		display.setCursor(17, 25);
		display.print("MOYEN");
		break;
	case 3:
		display.setFont(NULL);
		display.setCursor(11, 25);
		display.print("DEGRADE");
		break;
	case 4:
		display.setFont(NULL);
		display.setCursor(11, 25);
		display.print("MAUVAIS");
		break;
	case 5:
		display.setFont(&Font4x7Fixed);
		display.setCursor(0, 31);
		display.print("TRES MAUVAIS");
		break;
	case 6:
		display.setFont(&Font4x7Fixed);
		display.setCursor(0, 31);
		display.print("EXT. MAUVAIS");
		break;
	default:
		display.setFont(NULL);
		display.setCursor(14, 25);
		display.print("ERREUR");
	}
}

void drawCentreString(const String &buf, int x, int y, int offset)
{
	int16_t x1, y1;
	uint16_t w, h;
	display.getTextBounds(buf, x, y, &x1, &y1, &w, &h); //calc width of new string
	display.setCursor(((64 - offset) - w) / 2, y);		//si 1 seul chiffre => taille de 2 chiffres !!!
	display.print(buf);
}

/*****************************************************************
 * GPS coordinates                                              *
 *****************************************************************/

struct gps
{
	String latitude;
	String longitude;
};

/*****************************************************************
 * NebuleAir data                                            *
 *****************************************************************/

struct sensordata
{
	float pm1;
	float pm2_5;
	float pm10;
	float no2;
	float cov;
	float t;
	float h;
	float p;
};

struct sensordata nebuleair
{
	-1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0
};

/*****************************************************************
 * Forecast Atmosud                                              *
 *****************************************************************/
struct forecast
{
	float multi;
	float no2;
	float o3;
	float pm10;
	float pm2_5;
	float so2;
};

struct forecast atmoSud
{
	- 1.0, -1.0, -1.0, -1.0, -1.0, -1.0
};

uint8_t forecast_selector;


/*****************************************************************
 * Time                                             *
 *****************************************************************/
struct timedata
{
	uint16_t year;
	uint16_t month;
	uint16_t day;
	uint16_t hour;
	uint16_t minute;
	uint16_t second;
};

struct timedata matrix_time
{
	2000, 0, 0, 0, 0, 0
};

/*****************************************************************
 * Serial declarations                                           *
 *****************************************************************/

#define serialNPM (Serial1)
#define serialMHZ (Serial2)
EspSoftwareSerial::UART serialNO2; //Serial3

/*****************************************************************
 * BMP/BME280 declaration                                        *
 *****************************************************************/
BMX280 bmx280;

/*****************************************************************
 * MH-Z16 declaration                                        *
 *****************************************************************/
MHZ16_uart mhz16;

/*****************************************************************
 * MH-Z19 declaration                                        *
 *****************************************************************/
MHZ19 mhz19;

/*****************************************************************
 * CCS811 declaration                                        *
 *****************************************************************/
CCS811 ccs811(-1);

/*****************************************************************
 * Envea Cairsens declaration                                        *
 *****************************************************************/
CairsensUART cairsens(&serialNO2);

/*****************************************************************
 * Time                                       *
 *****************************************************************/

// time management varialbles
bool send_now = false;
unsigned long starttime;
unsigned long time_end_setup;
unsigned long time_before_config;
int prec;
unsigned long time_point_device_start_ms;
unsigned long starttime_NPM;
unsigned long starttime_MHZ16;
unsigned long starttime_MHZ19;
unsigned long starttime_CCS811;
unsigned long starttime_Cairsens;
unsigned long act_micro;
unsigned long act_milli;
unsigned long last_micro = 0;
unsigned long min_micro = 1000000000;
unsigned long max_micro = 0;

unsigned long sending_time = 0;
unsigned long last_update_attempt;
// int last_update_returncode;
int last_sendData_returncode;

bool wifi_connection_lost;
bool lora_connection_lost;


/*****************************************************************
 * dew point helper function                                     *
 *****************************************************************/
static float dew_point(const float temperature, const float humidity)
{
	float dew_temp;
	const float k2 = 17.62;
	const float k3 = 243.12;

	dew_temp = k3 * (((k2 * temperature) / (k3 + temperature)) + log(humidity / 100.0f)) / (((k2 * k3) / (k3 + temperature)) - log(humidity / 100.0f));

	return dew_temp;
}

/*****************************************************************
 * Pressure at sea level function                                     *
 *****************************************************************/
static float pressure_at_sealevel(const float temperature, const float pressure)
{
	float pressure_at_sealevel;

		// pressure_at_sealevel = pressure * pow(((temperature + 273.15f) / (temperature + 273.15f + (0.0065f * readCorrectionOffset(cfg::height_above_sealevel)))), -5.255f);
		pressure_at_sealevel = pressure * pow(((temperature + 273.15f) / (temperature + 273.15f + (0.0065f * cfg::height_above_sealevel))), -5.255f); //A VERIFIER


	return pressure_at_sealevel;
}

/*****************************************************************
 * NPM variables and enums                                       *
 *****************************************************************/

bool is_NPM_running = false;
bool nextpmconnected; //important to test nextpm and avoid endless loops

// To read NPM responses
enum
{
	NPM_REPLY_HEADER_16 = 16,
	NPM_REPLY_STATE_16 = 14,
	NPM_REPLY_BODY_16 = 13,
	NPM_REPLY_CHECKSUM_16 = 1
} NPM_waiting_for_16; // for concentration

enum
{
	NPM_REPLY_HEADER_4 = 4,
	NPM_REPLY_STATE_4 = 2,
	NPM_REPLY_CHECKSUM_4 = 1
} NPM_waiting_for_4; // for change

enum
{
	NPM_REPLY_HEADER_5 = 5,
	NPM_REPLY_STATE_5 = 3,
	NPM_REPLY_DATA_5 = 2,
	NPM_REPLY_CHECKSUM_5 = 1
} NPM_waiting_for_5; // for fan speed

enum
{
	NPM_REPLY_HEADER_6 = 6,
	NPM_REPLY_STATE_6 = 4,
	NPM_REPLY_DATA_6 = 3,
	NPM_REPLY_CHECKSUM_6 = 1
} NPM_waiting_for_6; // for version

enum
{
	NPM_REPLY_HEADER_8 = 8,
	NPM_REPLY_STATE_8 = 6,
	NPM_REPLY_BODY_8 = 5,
	NPM_REPLY_CHECKSUM_8 = 1
} NPM_waiting_for_8; // for temperature/humidity

String current_state_npm;
String current_th_npm;

/*****************************************************************
 * Data variables                                      *
 *****************************************************************/
float last_value_BMX280_T = -128.0;
float last_value_BMX280_P = -1.0;
float last_value_BME280_H = -1.0;

uint32_t npm_pm1_sum = 0;
uint32_t npm_pm10_sum = 0;
uint32_t npm_pm25_sum = 0;
uint32_t npm_pm1_sum_pcs = 0;
uint32_t npm_pm10_sum_pcs = 0;
uint32_t npm_pm25_sum_pcs = 0;
uint16_t npm_val_count = 0;

float last_value_NPM_P0 = -1.0;
float last_value_NPM_P1 = -1.0;
float last_value_NPM_P2 = -1.0;
float last_value_NPM_N1 = -1.0;
float last_value_NPM_N10 = -1.0;
float last_value_NPM_N25 = -1.0;

float last_value_MHZ16 = -1.0;
uint32_t mhz16_sum = 0;
uint16_t mhz16_val_count = 0;

float last_value_MHZ19 = -1.0;
uint32_t mhz19_sum = 0;
uint16_t mhz19_val_count = 0;

float last_value_CCS811 = -1.0;
uint32_t ccs811_sum = 0;
uint16_t ccs811_val_count = 0;

float last_value_no2 = -1.0;
uint32_t no2_sum = 0;
uint16_t no2_val_count = 0;

String esp_chipid;

String last_value_NPM_version;

unsigned long NPM_error_count;
unsigned long MHZ16_error_count;
unsigned long MHZ19_error_count;
unsigned long CCS811_error_count;
unsigned long Cairsens_error_count;

unsigned long last_page_load = millis();

unsigned long count_sends = 0;
unsigned long last_display_millis_oled = 0;
unsigned long last_display_millis_matrix = 0;
uint8_t next_display_count = 0;

#define msSince(timestamp_before) (act_milli - (timestamp_before))

const char data_first_part[] PROGMEM = "{\"software_version\": \"" SOFTWARE_VERSION_STR "\", \"sensordatavalues\":[";
const char JSON_SENSOR_DATA_VALUES[] PROGMEM = "sensordatavalues";


/*****************************************************************
 * NPM functions     *
 *****************************************************************/

static int8_t NPM_get_state()
{
	int8_t result = -1;
	NPM_waiting_for_4 = NPM_REPLY_HEADER_4;
	debug_outln_info(F("State NPM..."));
	NPM_cmd(PmSensorCmd2::State);

	unsigned long timeout = millis();

	do
	{
		debug_outln("Wait for Serial...", DEBUG_MAX_INFO);
	} while (!serialNPM.available() && millis() - timeout < 3000);

	while (serialNPM.available() >= NPM_waiting_for_4)
	{
		const uint8_t constexpr header[2] = {0x81, 0x16};
		uint8_t state[1];
		uint8_t checksum[1];
		uint8_t test[4];

		switch (NPM_waiting_for_4)
		{
		case NPM_REPLY_HEADER_4:
			if (serialNPM.find(header, sizeof(header)))
				NPM_waiting_for_4 = NPM_REPLY_STATE_4;
			break;
		case NPM_REPLY_STATE_4:
			serialNPM.readBytes(state, sizeof(state));
			NPM_state(state[0]);
			result = state[0];
			NPM_waiting_for_4 = NPM_REPLY_CHECKSUM_4;
			break;
		case NPM_REPLY_CHECKSUM_4:
			serialNPM.readBytes(checksum, sizeof(checksum));
			memcpy(test, header, sizeof(header));
			memcpy(&test[sizeof(header)], state, sizeof(state));
			memcpy(&test[sizeof(header) + sizeof(state)], checksum, sizeof(checksum));
			NPM_data_reader(test, 4);
			NPM_waiting_for_4 = NPM_REPLY_HEADER_4;
			if (NPM_checksum_valid_4(test))
			{
				debug_outln_info(F("Checksum OK..."));
			}
			break;
		}
	}
	return result;
}

static bool NPM_start_stop()
{
	bool result;
	NPM_waiting_for_4 = NPM_REPLY_HEADER_4;
	debug_outln_info(F("Switch start/stop NPM..."));
	NPM_cmd(PmSensorCmd2::Change);

	unsigned long timeout = millis();

	do
	{
		debug_outln("Wait for Serial...", DEBUG_MAX_INFO);
	} while (!serialNPM.available() && millis() - timeout < 3000);

	while (serialNPM.available() >= NPM_waiting_for_4)
	{
		const uint8_t constexpr header[2] = {0x81, 0x15};
		uint8_t state[1];
		uint8_t checksum[1];
		uint8_t test[4];

		switch (NPM_waiting_for_4)
		{
		case NPM_REPLY_HEADER_4:
			if (serialNPM.find(header, sizeof(header)))
				NPM_waiting_for_4 = NPM_REPLY_STATE_4;
			break;
		case NPM_REPLY_STATE_4:
			serialNPM.readBytes(state, sizeof(state));
			NPM_state(state[0]);

			if (bitRead(state[0], 0) == 0)
			{
				debug_outln_info(F("NPM start..."));
				result = true;
			}
			else if (bitRead(state[0], 0) == 1)
			{
				debug_outln_info(F("NPM stop..."));
				result = false;
			}
			else
			{
				result = !is_NPM_running; //DANGER BECAUSE NON INITIALISED
			}

			NPM_waiting_for_4 = NPM_REPLY_CHECKSUM_4;
			break;
		case NPM_REPLY_CHECKSUM_4:
			serialNPM.readBytes(checksum, sizeof(checksum));
			memcpy(test, header, sizeof(header));
			memcpy(&test[sizeof(header)], state, sizeof(state));
			memcpy(&test[sizeof(header) + sizeof(state)], checksum, sizeof(checksum));
			NPM_data_reader(test, 4);
			NPM_waiting_for_4 = NPM_REPLY_HEADER_4;
			if (NPM_checksum_valid_4(test))
			{
				debug_outln_info(F("Checksum OK..."));
			}
			break;
		}
	}
	return result;
}

static String NPM_version_date()
{
	delay(250);
	NPM_waiting_for_6 = NPM_REPLY_HEADER_6;
	debug_outln_info(F("Version NPM..."));
	NPM_cmd(PmSensorCmd2::Version);

	unsigned long timeout = millis();

	do
	{
		debug_outln("Wait for Serial...", DEBUG_MAX_INFO);
	} while (!serialNPM.available() && millis() - timeout < 3000);

	while (serialNPM.available() >= NPM_waiting_for_6)
	{
		const uint8_t constexpr header[2] = {0x81, 0x17};
		uint8_t state[1];
		uint8_t data[2];
		uint8_t checksum[1];
		uint8_t test[6];

		switch (NPM_waiting_for_6)
		{
		case NPM_REPLY_HEADER_6:
			if (serialNPM.find(header, sizeof(header)))
				NPM_waiting_for_6 = NPM_REPLY_STATE_6;
			break;
		case NPM_REPLY_STATE_6:
			serialNPM.readBytes(state, sizeof(state));
			NPM_state(state[0]);
			NPM_waiting_for_6 = NPM_REPLY_DATA_6;
			break;
		case NPM_REPLY_DATA_6:
			if (serialNPM.readBytes(data, sizeof(data)) == sizeof(data))
			{
				NPM_data_reader(data, 2);
				uint16_t NPMversion = word(data[0], data[1]);
				last_value_NPM_version = String(NPMversion);
				debug_outln_info(F("Next PM Firmware: "), last_value_NPM_version);
			}
			NPM_waiting_for_6 = NPM_REPLY_CHECKSUM_6;
			break;
		case NPM_REPLY_CHECKSUM_6:
			serialNPM.readBytes(checksum, sizeof(checksum));
			memcpy(test, header, sizeof(header));
			memcpy(&test[sizeof(header)], state, sizeof(state));
			memcpy(&test[sizeof(header) + sizeof(state)], data, sizeof(data));
			memcpy(&test[sizeof(header) + sizeof(state) + sizeof(data)], checksum, sizeof(checksum));
			NPM_data_reader(test, 6);
			NPM_waiting_for_6 = NPM_REPLY_HEADER_6;
			if (NPM_checksum_valid_6(test))
			{
				debug_outln_info(F("Checksum OK..."));
			}
			break;
		}
	}
	return last_value_NPM_version;
}

static void NPM_fan_speed()
{

	NPM_waiting_for_5 = NPM_REPLY_HEADER_5;
	debug_outln_info(F("Set fan speed to 50 %..."));
	NPM_cmd(PmSensorCmd2::Speed);

	unsigned long timeout = millis();

	do
	{
		debug_outln("Wait for Serial...", DEBUG_MAX_INFO);
	} while (!serialNPM.available() && millis() - timeout < 3000);

	while (serialNPM.available() >= NPM_waiting_for_5)
	{
		const uint8_t constexpr header[2] = {0x81, 0x21};
		uint8_t state[1];
		uint8_t data[1];
		uint8_t checksum[1];
		uint8_t test[5];

		switch (NPM_waiting_for_5)
		{
		case NPM_REPLY_HEADER_5:
			if (serialNPM.find(header, sizeof(header)))
				NPM_waiting_for_5 = NPM_REPLY_STATE_5;
			break;
		case NPM_REPLY_STATE_5:
			serialNPM.readBytes(state, sizeof(state));
			NPM_state(state[0]);
			NPM_waiting_for_5 = NPM_REPLY_DATA_5;
			break;
		case NPM_REPLY_DATA_5:
			if (serialNPM.readBytes(data, sizeof(data)) == sizeof(data))
			{
				NPM_data_reader(data, 1);
			}
			NPM_waiting_for_5 = NPM_REPLY_CHECKSUM_5;
			break;
		case NPM_REPLY_CHECKSUM_5:
			serialNPM.readBytes(checksum, sizeof(checksum));
			memcpy(test, header, sizeof(header));
			memcpy(&test[sizeof(header)], state, sizeof(state));
			memcpy(&test[sizeof(header) + sizeof(state)], data, sizeof(data));
			memcpy(&test[sizeof(header) + sizeof(state) + sizeof(data)], checksum, sizeof(checksum));
			NPM_data_reader(test, 5);
			NPM_waiting_for_5 = NPM_REPLY_HEADER_5;
			if (NPM_checksum_valid_5(test))
			{
				debug_outln_info(F("Checksum OK..."));
			}
			break;
		}
	}
}

static String NPM_temp_humi()
{
	uint16_t NPM_temp;
	uint16_t NPM_humi;
	NPM_waiting_for_8 = NPM_REPLY_HEADER_8;
	debug_outln_info(F("Temperature/Humidity in Next PM..."));
	NPM_cmd(PmSensorCmd2::Temphumi);

	unsigned long timeout = millis();

	do
	{
		debug_outln("Wait for Serial...", DEBUG_MAX_INFO);
	} while (!serialNPM.available() && millis() - timeout < 3000);

	while (serialNPM.available() >= NPM_waiting_for_8)
	{
		const uint8_t constexpr header[2] = {0x81, 0x14};
		uint8_t state[1];
		uint8_t data[4];
		uint8_t checksum[1];
		uint8_t test[8];

		switch (NPM_waiting_for_8)
		{
		case NPM_REPLY_HEADER_8:
			if (serialNPM.find(header, sizeof(header)))
				NPM_waiting_for_8 = NPM_REPLY_STATE_8;
			break;
		case NPM_REPLY_STATE_8:
			serialNPM.readBytes(state, sizeof(state));
			NPM_state(state[0]);
			NPM_waiting_for_8 = NPM_REPLY_BODY_8;
			break;
		case NPM_REPLY_BODY_8:
			if (serialNPM.readBytes(data, sizeof(data)) == sizeof(data))
			{
				NPM_data_reader(data, 4);
				NPM_temp = word(data[0], data[1]);
				NPM_humi = word(data[2], data[3]);
				debug_outln_verbose(F("Temperature (°C): "), String(NPM_temp / 100.0f));
				debug_outln_verbose(F("Relative humidity (%): "), String(NPM_humi / 100.0f));
			}
			NPM_waiting_for_8 = NPM_REPLY_CHECKSUM_8;
			break;
		case NPM_REPLY_CHECKSUM_16:
			serialNPM.readBytes(checksum, sizeof(checksum));
			memcpy(test, header, sizeof(header));
			memcpy(&test[sizeof(header)], state, sizeof(state));
			memcpy(&test[sizeof(header) + sizeof(state)], data, sizeof(data));
			memcpy(&test[sizeof(header) + sizeof(state) + sizeof(data)], checksum, sizeof(checksum));
			NPM_data_reader(test, 8);
			if (NPM_checksum_valid_8(test))
				debug_outln_info(F("Checksum OK..."));
			NPM_waiting_for_8 = NPM_REPLY_HEADER_8;
			break;
		}
	}
	return String(NPM_temp / 100.0f) + " / " + String(NPM_humi / 100.0f);
}

/*****************************************************************
 * read BMP280/BME280 sensor values                              *
 *****************************************************************/
static void fetchSensorBMX280()
{
	const char *const sensor_name = (bmx280.sensorID() == BME280_SENSOR_ID) ? SENSORS_BME280 : SENSORS_BMP280;
	debug_outln_verbose(FPSTR(DBG_TXT_START_READING), FPSTR(sensor_name));

	bmx280.takeForcedMeasurement();
	const auto t = bmx280.readTemperature();
	const auto p = bmx280.readPressure();
	const auto h = bmx280.readHumidity();
	if (isnan(t) || isnan(p))
	{
		last_value_BMX280_T = -128.0;
		last_value_BMX280_P = -1.0;
		last_value_BME280_H = -1.0;
		debug_outln_error(F("BMP/BME280 read failed"));
	}
	else
	{
		last_value_BMX280_T = t;
		last_value_BMX280_P = p;
		if (bmx280.sensorID() == BME280_SENSOR_ID)
		{
			last_value_BME280_H = h;
		}
	}
	debug_outln_info(FPSTR(DBG_TXT_SEP));
	debug_outln_verbose(FPSTR(DBG_TXT_END_READING), FPSTR(sensor_name));
}

/*****************************************************************
 * read MHZ16 sensor values                              *
 *****************************************************************/
static void fetchSensorMHZ16()
{
	const char *const sensor_name = SENSORS_MHZ16;
	debug_outln_verbose(FPSTR(DBG_TXT_START_READING), FPSTR(sensor_name));

	int value = mhz16.getPPM();

	if (isnan(value))
	{
		debug_outln_error(F("MHZ16 read failed"));
	}
	else
	{
		int value = mhz16.getPPM();
		mhz16_sum += value;
		mhz16_val_count++;
		debug_outln(String(mhz16_val_count), DEBUG_MAX_INFO);
	}

	if (send_now && cfg::sending_intervall_ms >= 120000)
	{
		last_value_MHZ16 = -1.0f;

		if (mhz16_val_count == 12)
		{
			last_value_MHZ16 = float(mhz16_sum / mhz16_val_count);
			debug_outln_info(FPSTR(DBG_TXT_SEP));
		}
		else
		{
			MHZ16_error_count++;
		}

		mhz16_sum = 0;
		mhz16_val_count = 0;
	}

	debug_outln_info(FPSTR(DBG_TXT_SEP));
	debug_outln_verbose(FPSTR(DBG_TXT_END_READING), FPSTR(sensor_name));
}

/*****************************************************************
 * read MHZ19 sensor values                              *
 *****************************************************************/
static void fetchSensorMHZ19()
{
	const char *const sensor_name = SENSORS_MHZ19;
	debug_outln_verbose(FPSTR(DBG_TXT_START_READING), FPSTR(sensor_name));

	int value;

	value = mhz19.getCO2();

	if (isnan(value))
	{
		debug_outln_error(F("MHZ19 read failed"));
	}
	else
	{
		mhz19_sum += value;
		mhz19_val_count++;
		debug_outln(String(mhz19_val_count), DEBUG_MAX_INFO);
	}

	if (send_now && cfg::sending_intervall_ms == 120000)
	{
		last_value_MHZ19 = -1.0f;

		if (mhz19_val_count >= 12)
		{
			last_value_MHZ19 = float(mhz19_sum / mhz19_val_count);
			debug_outln_info(FPSTR(DBG_TXT_SEP));
		}
		else
		{
			MHZ19_error_count++;
		}

		mhz19_sum = 0;
		mhz19_val_count = 0;
	}

	debug_outln_info(FPSTR(DBG_TXT_SEP));
	debug_outln_verbose(FPSTR(DBG_TXT_END_READING), FPSTR(sensor_name));
}

/*****************************************************************
 * read CCS811 sensor values                              *
 *****************************************************************/
static void fetchSensorCCS811()
{
	const char *const sensor_name = SENSORS_CCS811;
	debug_outln_verbose(FPSTR(DBG_TXT_START_READING), FPSTR(sensor_name));

	uint16_t etvoc, errstat;
	ccs811.read(NULL, &etvoc, &errstat, NULL);

	if (errstat == CCS811_ERRSTAT_OK)
	{

		ccs811_sum += etvoc;
		ccs811_val_count++;
		debug_outln(String(ccs811_val_count), DEBUG_MAX_INFO);
	}
	else if (errstat == CCS811_ERRSTAT_OK_NODATA)
	{
		Debug.println("CCS811: waiting for (new) data");
	}
	else if (errstat & CCS811_ERRSTAT_I2CFAIL)
	{
		Debug.println("CCS811: I2C error");
	}
	else
	{
		Debug.print("CCS811: errstat=");
		Debug.print("errstat,HEX");
		Debug.print("=");
		Debug.println(ccs811.errstat_str(errstat));
	}

	if (send_now && cfg::sending_intervall_ms == 120000)
	{
		last_value_CCS811 = -1.0f;

		if (ccs811_val_count >= 12)
		{
			last_value_CCS811 = float(ccs811_sum / ccs811_val_count);
			debug_outln_info(FPSTR(DBG_TXT_SEP));
		}
		else
		{
			CCS811_error_count++;
		}

		ccs811_sum = 0;
		ccs811_val_count = 0;
	}

	debug_outln_info(FPSTR(DBG_TXT_SEP));
	debug_outln_verbose(FPSTR(DBG_TXT_END_READING), FPSTR(sensor_name));
}

/*****************************************************************
 * read Tera Sensor Next PM sensor sensor values                 *
 *****************************************************************/
static void fetchSensorNPM()
{

	NPM_waiting_for_16 = NPM_REPLY_HEADER_16;

	debug_outln_info(F("Concentration NPM..."));
	NPM_cmd(PmSensorCmd2::Concentration);

	unsigned long timeout = millis();

	do
	{
		debug_outln("Wait for Serial...", DEBUG_MAX_INFO);
	} while (!serialNPM.available() && millis() - timeout < 3000);

	while (serialNPM.available() >= NPM_waiting_for_16)
	{
		const uint8_t constexpr header[2] = {0x81, 0x12};
		uint8_t state[1];
		uint8_t data[12];
		uint8_t checksum[1];
		uint8_t test[16];
		uint16_t N1_serial;
		uint16_t N25_serial;
		uint16_t N10_serial;
		uint16_t pm1_serial;
		uint16_t pm25_serial;
		uint16_t pm10_serial;

		switch (NPM_waiting_for_16)
		{
		case NPM_REPLY_HEADER_16:
			if (serialNPM.find(header, sizeof(header)))
				NPM_waiting_for_16 = NPM_REPLY_STATE_16;
			break;
		case NPM_REPLY_STATE_16:
			serialNPM.readBytes(state, sizeof(state));
			current_state_npm = NPM_state(state[0]);
			NPM_waiting_for_16 = NPM_REPLY_BODY_16;
			break;
		case NPM_REPLY_BODY_16:
			if (serialNPM.readBytes(data, sizeof(data)) == sizeof(data))
			{
				NPM_data_reader(data, 12);
				N1_serial = word(data[0], data[1]);
				N25_serial = word(data[2], data[3]);
				N10_serial = word(data[4], data[5]);

				pm1_serial = word(data[6], data[7]);
				pm25_serial = word(data[8], data[9]);
				pm10_serial = word(data[10], data[11]);

				debug_outln_info(F("Next PM Measure..."));

				debug_outln_verbose(F("PM1 (μg/m3) : "), String(pm1_serial / 10.0f));
				debug_outln_verbose(F("PM2.5 (μg/m3): "), String(pm25_serial / 10.0f));
				debug_outln_verbose(F("PM10 (μg/m3) : "), String(pm10_serial / 10.0f));

				debug_outln_verbose(F("PM1 (pcs/L) : "), String(N1_serial));
				debug_outln_verbose(F("PM2.5 (pcs/L): "), String(N25_serial));
				debug_outln_verbose(F("PM10 (pcs/L) : "), String(N10_serial));
			}
			NPM_waiting_for_16 = NPM_REPLY_CHECKSUM_16;
			break;
		case NPM_REPLY_CHECKSUM_16:
			serialNPM.readBytes(checksum, sizeof(checksum));
			memcpy(test, header, sizeof(header));
			memcpy(&test[sizeof(header)], state, sizeof(state));
			memcpy(&test[sizeof(header) + sizeof(state)], data, sizeof(data));
			memcpy(&test[sizeof(header) + sizeof(state) + sizeof(data)], checksum, sizeof(checksum));
			NPM_data_reader(test, 16);
			if (NPM_checksum_valid_16(test))
			{
				debug_outln_info(F("Checksum OK..."));

				npm_pm1_sum += pm1_serial;
				npm_pm25_sum += pm25_serial;
				npm_pm10_sum += pm10_serial;

				npm_pm1_sum_pcs += N1_serial;
				npm_pm25_sum_pcs += N25_serial;
				npm_pm10_sum_pcs += N10_serial;
				npm_val_count++;
				debug_outln(String(npm_val_count), DEBUG_MAX_INFO);
			}
			NPM_waiting_for_16 = NPM_REPLY_HEADER_16;
			break;
		}
	}

	if (send_now && cfg::sending_intervall_ms >= 120000)
	{
		last_value_NPM_P0 = -1.0f;
		last_value_NPM_P1 = -1.0f;
		last_value_NPM_P2 = -1.0f;
		last_value_NPM_N1 = -1.0f;
		last_value_NPM_N10 = -1.0f;
		last_value_NPM_N25 = -1.0f;

		if (npm_val_count == 2)
		{
			last_value_NPM_P0 = float(npm_pm1_sum) / (npm_val_count * 10.0f);
			last_value_NPM_P1 = float(npm_pm10_sum) / (npm_val_count * 10.0f);
			last_value_NPM_P2 = float(npm_pm25_sum) / (npm_val_count * 10.0f);

			// last_value_NPM_N1 = float(npm_pm1_sum_pcs) / (npm_val_count * 1000.0f);
			// last_value_NPM_N10 = float(npm_pm10_sum_pcs) / (npm_val_count * 1000.0f);
			// last_value_NPM_N25 = float(npm_pm25_sum_pcs) / (npm_val_count * 1000.0f);

			last_value_NPM_N1 = float(npm_pm1_sum_pcs) / (npm_val_count);
			last_value_NPM_N10 = float(npm_pm10_sum_pcs) / (npm_val_count);
			last_value_NPM_N25 = float(npm_pm25_sum_pcs) / (npm_val_count);


			debug_outln_info(FPSTR(DBG_TXT_SEP));
		}
		else
		{
			NPM_error_count++;
		}

		npm_pm1_sum = 0;
		npm_pm10_sum = 0;
		npm_pm25_sum = 0;

		npm_val_count = 0;

		npm_pm1_sum_pcs = 0;
		npm_pm10_sum_pcs = 0;
		npm_pm25_sum_pcs = 0;

		debug_outln_info(F("Temperature and humidity in NPM after measure..."));
		current_th_npm = NPM_temp_humi();
	}
}


/*****************************************************************
 * read Cairsens sensor values                              *
 *****************************************************************/
static void fetchSensorCairsens()
{
	const char *const sensor_name = SENSORS_ENVEANO2;
	debug_outln_verbose(FPSTR(DBG_TXT_START_READING), FPSTR(sensor_name));

	uint8_t no2_val = 0;

	if (cairsens.getNO2InstantVal(no2_val) == CairsensUART::NO_ERROR)
	{
		no2_sum += no2_val;
		no2_val_count++;
		debug_outln(String(no2_val_count), DEBUG_MAX_INFO);
	}
	else
	{
		Debug.println("Could not get Cairsens NOX value");
	}

	if (send_now)
	{
		last_value_no2 = -1.0f;

		if (no2_val_count >= 12)
		{
			last_value_no2 = CairsensUART::ppbToPpm(CairsensUART::NO2, float(no2_sum / no2_val_count));
			debug_outln_info(FPSTR(DBG_TXT_SEP));
		}
		else
		{
			Cairsens_error_count++;
		}

		no2_sum = 0;
		no2_val_count = 0;
	}

	debug_outln_info(FPSTR(DBG_TXT_SEP));
	debug_outln_verbose(FPSTR(DBG_TXT_END_READING), FPSTR(sensor_name));
}



/*****************************************************************
 * display values                                                *
 *****************************************************************/

static void display_values_matrix()
{
	float t_value = -128.0;
	float h_value = -1.0;
	float p_value = -1.0;
	String t_sensor, h_sensor, p_sensor;
	float pm01_value = -1.0;
	float pm25_value = -1.0;
	float pm10_value = -1.0;
	String pm01_sensor;
	String pm10_sensor;
	String pm25_sensor;
	float nc010_value = -1.0;
	float nc025_value = -1.0;
	float nc100_value = -1.0;

	String co2_sensor;
	String cov_sensor;
	String no2_sensor;

	float co2_value = -1.0;
	float cov_value = -1.0;
	float no2_value = -1.0;

	float nebuleair_pm10_value = -1.0;
	float nebuleair_pm25_value = -1.0;
	float nebuleair_pm01_value = -1.0;
	float nebuleair_t_value = -128.0;
	float nebuleair_h_value = -1.0;
	float nebuleair_p_value = -1.0;
	float nebuleair_cov_value = -1.0;
	float nebuleair_no2_value = -1.0;

	uint16_t year;
	uint16_t month;
	uint16_t day;
	uint16_t hour;
	uint16_t minute;
	uint16_t second;

	double lat_value = -200.0;
	double lon_value = -200.0;
	double alt_value = -1000.0;
	uint8_t screen_count = 0;
	uint8_t screens[21];
	int line_count = 0;
	//debug_outln_info(F("output values to matrix..."));

	if (cfg::npm_read)
	{
		pm01_value = last_value_NPM_P0;
		pm10_value = last_value_NPM_P1;
		pm25_value = last_value_NPM_P2;
		pm01_sensor = FPSTR(SENSORS_NPM);
		pm10_sensor = FPSTR(SENSORS_NPM);
		pm25_sensor = FPSTR(SENSORS_NPM);
		nc010_value = last_value_NPM_N1;
		nc100_value = last_value_NPM_N10;
		nc025_value = last_value_NPM_N25;
	}

	if (cfg::bmx280_read)
	{
		t_sensor = p_sensor = FPSTR(SENSORS_BMP280);
		t_value = last_value_BMX280_T;
		p_value = last_value_BMX280_P;
		if (bmx280.sensorID() == BME280_SENSOR_ID)
		{
			h_sensor = t_sensor = FPSTR(SENSORS_BME280);
			h_value = last_value_BME280_H;
		}
	}

	if (cfg::mhz16_read)
	{
		co2_value = last_value_MHZ16;
		co2_sensor = FPSTR(SENSORS_MHZ16);
	}

	if (cfg::mhz19_read)
	{
		co2_value = last_value_MHZ19;
		co2_sensor = FPSTR(SENSORS_MHZ19);
	}

	if (cfg::ccs811_read)
	{
		cov_value = last_value_CCS811;
		cov_sensor = FPSTR(SENSORS_CCS811);
	}

	if (cfg::enveano2_read)
	{
		no2_value = last_value_no2;
		cov_sensor = FPSTR(SENSORS_ENVEANO2);
	}


	if ((cfg::npm_read || cfg::bmx280_read || cfg::mhz16_read || cfg::mhz19_read || cfg::ccs811_read || cfg::enveano2_read) && cfg::display_measure)
	{
		screens[screen_count++] = 0; //Air intérieur
	}

	if (cfg::mhz16_read && cfg::display_measure)
	{
		if (cfg_screen_co2)
			screens[screen_count++] = 1;
	}
	if (cfg::mhz19_read && cfg::display_measure)
	{
		if (cfg_screen_co2)
			screens[screen_count++] = 2;
	}

	if (cfg::npm_read && cfg::display_measure)
	{
		if (cfg_screen_pm10)
			screens[screen_count++] = 3; //PM10
		if (cfg_screen_pm25)
			screens[screen_count++] = 4; //PM2.5
		if (cfg_screen_pm01)
			screens[screen_count++] = 5; //PM1
	}

	if (cfg::ccs811_read && cfg::display_measure)
	{
		if (cfg_screen_cov)
			screens[screen_count++] = 6;
	}

	if (cfg::enveano2_read && cfg::display_measure)
	{
		if (cfg_screen_enveano2)
		screens[screen_count++] = 7;
	}

	if (cfg::bmx280_read && cfg::display_measure)
	{
		if (cfg_screen_temp)
			screens[screen_count++] = 8; //T
		if (cfg_screen_humi)
			screens[screen_count++] = 9; //H
		if (cfg_screen_press)
			screens[screen_count++] = 10; //P
	}

	
	if (cfg::display_forecast || cfg::show_nebuleair )
	{
		screens[screen_count++] = 11; // Air exterieur
	}
	
	if (cfg::show_nebuleair )
	{
		screens[screen_count++] = 12; // NebuleAir
	}
	
	
	
	if (cfg::display_forecast)
	{
		screens[screen_count++] = 13; // Ecran prevision avec heure
		if (cfg_screen_atmo_index)
			screens[screen_count++] = 14; // Atmo Sud forecast Indice
		if (cfg_screen_atmo_no2)
			screens[screen_count++] = 15; // Atmo Sud forecast NO2
		if (cfg_screen_atmo_o3)
			screens[screen_count++] = 16; // Atmo Sud forecast O3
		if (cfg_screen_atmo_pm10)
			screens[screen_count++] = 17; // Atmo Sud forecast PM10
		if (cfg_screen_atmo_pm25)
			screens[screen_count++] = 18; // Atmo Sud forecast PM2.5
		if (cfg_screen_atmo_so2)
			screens[screen_count++] = 19; // Atmo Sud forecast SO2
	}



	screens[screen_count++] = 20; // Logos

	switch (screens[next_display_count % screen_count])
	{
	case 0:
		if (pm10_value != -1.0 || pm25_value != -1.0 || pm01_value != -1.0 || t_value != -128.0 || h_value != -1.0 || p_value != -1.0 || co2_value != -1.0 || cov_value != -1.0 || no2_value != -1.0)
		{
			if ((!cfg::has_wifi && !cfg::has_lora) || (cfg::has_wifi && wifi_connection_lost && !cfg::has_lora) || (cfg::has_lora && lora_connection_lost && !cfg::has_wifi))
			{
				drawImage(0, 0, 32, 64, interieur_no_connection);
			}
			if (cfg::has_wifi && !wifi_connection_lost)
			{
				drawImage(0, 0, 32, 64, interieur_wifi);
			}
			if (cfg::has_lora && (!cfg::has_wifi || (cfg::has_wifi && wifi_connection_lost)) && !lora_connection_lost)
			{
				drawImage(0, 0, 32, 64, interieur_lora);
			} //wifi prioritaire
		}
		else
		{
			display.fillScreen(myBLACK);
			display.setTextColor(myWHITE);
			display.setFont(NULL);
			display.setTextSize(1);
			display.setCursor(1, 0);
			display.print("Premi");
			display.write(138);
			display.print("res");
			display.setCursor(1, 11);
			display.print("mesures");
			display.setCursor(1, 22);
			display.print("2 min.");

			int div_entiere = (millis() - time_end_setup) / 17142;

			switch (div_entiere)
			{
			case 0:
				drawImage(47, 15, 16, 16, sablierloop1);
				break;
			case 1:
				drawImage(47, 15, 16, 16, sablierloop2);
				break;
			case 2:
				drawImage(47, 15, 16, 16, sablierloop3);
				break;
			case 3:
				drawImage(47, 15, 16, 16, sablierloop4);
				break;
			case 4:
				drawImage(47, 15, 16, 16, sablierloop5);
				break;
			case 5:
				drawImage(47, 15, 16, 16, sablierloop6);
				break;
			case 6:
				drawImage(47, 15, 16, 16, sablierloop7);
				break;
			case 7:
				drawImage(47, 15, 16, 16, sablierloop7);
				break;
			}
		}

		break;
	case 3: 
		if (pm10_value != -1.0)
		{
			display.fillScreen(myBLACK);
			display.setTextColor(myCYAN);
			display.setFont(NULL);
			display.setCursor(1, 0);
			display.setTextSize(1);
			display.print("PM10");
			display.setFont(&Font4x7Fixed);
			display.setCursor(display.getCursorX() + 2, 7);
			display.write(181);
			display.print("g/m");
			display.write(179);
			drawImage(55, 0, 7, 9, maison);
			displayColor = interpolateint(pm10_value, 15, 30, 75, gamma_correction);
			myCUSTOM = display.color565(displayColor.R, displayColor.G, displayColor.B);
			display.fillRect(50, 9, 14, 14, myCUSTOM);
			display.setFont(NULL);
			display.setTextSize(2);
			display.setTextColor(myWHITE);
			drawCentreString(String(pm10_value, 0), 0, 9, 14);
			display.setTextColor(myCUSTOM);
			messager1(pm10_value, 15, 30, 75);
		}
		else
		{
			act_milli += 5000;
		}
		break;
	case 4:
		if (pm25_value != -1.0)
		{
			display.fillScreen(myBLACK);
			display.setTextColor(myCYAN);
			display.setFont(NULL);
			display.setCursor(1, 0);
			display.setTextSize(1);
			display.print("PM2.5");
			display.setFont(&Font4x7Fixed);
			display.setCursor(display.getCursorX() + 2, 7);
			display.write(181);
			display.print("g/m");
			display.write(179);
			drawImage(55, 0, 7, 9, maison);
			displayColor = interpolateint(pm25_value, 10, 20, 50, gamma_correction);
			myCUSTOM = display.color565(displayColor.R, displayColor.G, displayColor.B);
			display.fillRect(50, 9, 14, 14, myCUSTOM);
			display.setFont(NULL);
			display.setTextSize(2);
			display.setTextColor(myWHITE);
			drawCentreString(String(pm25_value, 0), 0, 9, 14);
			display.setTextColor(myCUSTOM);
			messager1(pm25_value, 10, 20, 50);
		}
		else
		{
			act_milli += 5000;
		}
		break;
	case 5:
		if (pm01_value != -1.0)
		{
			display.fillScreen(myBLACK);
			display.setTextColor(myCYAN);
			display.setFont(NULL);
			display.setCursor(1, 0);
			display.setTextSize(1);
			display.print("PM1");
			display.setFont(&Font4x7Fixed);
			display.setCursor(display.getCursorX() + 2, 7);
			display.write(181);
			display.print("g/m");
			display.write(179);
			drawImage(55, 0, 7, 9, maison);
			displayColor = interpolateint(pm01_value, 10, 20, 50, gamma_correction);
			myCUSTOM = display.color565(displayColor.R, displayColor.G, displayColor.B);
			display.fillRect(50, 9, 14, 14, myCUSTOM);
			display.setFont(NULL);
			display.setTextSize(2);
			display.setTextColor(myWHITE);
			drawCentreString(String(pm01_value, 0), 0, 9, 14);
			display.setTextColor(myCUSTOM);
			messager1(pm01_value, 10, 20, 50);
		}
		else
		{
			act_milli += 5000;
		}
		break;
	case 1:
		if (co2_value != -1.0)
		{
			display.fillScreen(myBLACK);
			display.setTextColor(myCYAN);
			display.setFont(NULL);
			display.setCursor(1, 0);
			display.setTextSize(1);
			display.print("C0");
			display.write(250);
			display.setFont(&Font4x7Fixed);
			display.setCursor(display.getCursorX() + 2, 7);
			display.print("ppm");
			drawImage(55, 0, 7, 9, maison);
			displayColor = interpolateint2(co2_value, 800, 1500, gamma_correction);
			myCUSTOM = display.color565(displayColor.R, displayColor.G, displayColor.B);
			display.fillRect(50, 9, 14, 14, myCUSTOM);
			display.setFont(NULL);
			display.setTextSize(2);
			display.setTextColor(myWHITE);
			drawCentreString(String(co2_value, 0), 0, 9, 14);
			display.setTextColor(myCUSTOM);
			messager2(co2_value, 800, 1500);
		}
		else
		{
			act_milli += 5000;
		}
		break;
	case 2:
		if (co2_value != -1.0)
		{
			display.fillScreen(myBLACK);
			display.setTextColor(myCYAN);
			display.setFont(NULL);
			display.setCursor(1, 0);
			display.setTextSize(1);
			display.print("C0");
			display.write(250);
			display.setFont(&Font4x7Fixed);
			display.setCursor(display.getCursorX() + 2, 7);
			display.print("ppm");
			drawImage(55, 0, 7, 9, maison);
			displayColor = interpolateint2(co2_value, 800, 1500, gamma_correction);
			myCUSTOM = display.color565(displayColor.R, displayColor.G, displayColor.B);
			display.fillRect(50, 9, 14, 14, myCUSTOM);
			display.setFont(NULL);
			display.setTextSize(2);
			display.setTextColor(myWHITE);
			drawCentreString(String(co2_value, 0), 0, 9, 14);
			display.setTextColor(myCUSTOM);
			messager2(co2_value, 800, 1500);
		}
		else
		{
			act_milli += 5000;
		}
		break;
	case 6:
		if (cov_value != -1.0)
		{
			display.fillScreen(myBLACK);
			display.setTextColor(myCYAN);
			display.setFont(NULL);
			display.setCursor(1, 0);
			display.setTextSize(1);
			display.print("COV");
			display.setFont(&Font4x7Fixed);
			display.setCursor(display.getCursorX() + 2, 7);
			display.print("ppb");
			drawImage(55, 0, 7, 9, maison);
			display.setFont(NULL);
			display.setTextSize(2);
			display.setTextColor(myWHITE);
			drawCentreString(String(cov_value, 0), 0, 9, 0);
		}
		else
		{
			act_milli += 5000;
		}
		break;
		case 7:
		if (no2_value != -1.0)
		{
			display.fillScreen(myBLACK);
			display.setTextColor(myCYAN);
			display.setFont(NULL);
			display.setCursor(1, 0);
			display.setTextSize(1);
			display.print("NO2");
			display.setFont(&Font4x7Fixed);
			display.setCursor(display.getCursorX() + 2, 7);
			display.write(181);
			display.print("g/m");
			display.write(179);
			drawImage(55, 0, 7, 9, maison);
			display.setFont(NULL);
			display.setTextSize(2);
			display.setTextColor(myWHITE);
			drawCentreString(String(no2_value, 0), 0, 9, 0);

			//REVOIR COULEUR

		}
		else
		{
			act_milli += 5000;
		}
		break;


	case 8:
		if (t_value != -128.0)
		{
			display.fillScreen(myBLACK);
			display.setTextColor(myCYAN);
			display.setFont(NULL);
			display.setCursor(1, 0);
			display.setTextSize(1);
			display.print("Temp.");
			display.setFont(&Font4x7Fixed);
			display.setCursor(display.getCursorX() + 2, 7);
			display.write(176);
			display.print("C");
			drawImage(55, 0, 7, 9, maison);
			displayColor = interpolateint4(t_value, 19, 28, gamma_correction);
			myCUSTOM = display.color565(displayColor.R, displayColor.G, displayColor.B);
			display.fillRect(50, 9, 14, 14, myCUSTOM);
			display.setFont(NULL);
			display.setTextSize(2);
			display.setTextColor(myWHITE);
			drawCentreString(String(t_value, 1), 0, 9, 14);
			display.setTextColor(myCUSTOM);
			messager4(t_value, 19, 28);
		}
		else
		{
			act_milli += 5000;
		}
		break;
	case 9:
		if (h_value != -1.0)
		{
			display.fillScreen(myBLACK);
			display.setTextColor(myCYAN);
			display.setFont(NULL);
			display.setCursor(1, 0);
			display.setTextSize(1);
			display.print("Humidit");
			display.write(130);
			display.setFont(&Font4x7Fixed);
			display.setCursor(display.getCursorX() + 2, 7);
			display.write(37);
			drawImage(55, 0, 7, 9, maison);
			displayColor = interpolateint3(h_value, 40, 60, gamma_correction);
			myCUSTOM = display.color565(displayColor.R, displayColor.G, displayColor.B);
			display.fillRect(50, 9, 14, 14, myCUSTOM);
			display.setFont(NULL);
			display.setTextSize(2);
			display.setTextColor(myWHITE);
			drawCentreString(String(h_value, 0), 0, 9, 14);
			display.setTextColor(myCUSTOM);
			messager3(h_value, 40, 60);
		}
		else
		{
			act_milli += 5000;
		}
		break;
	case 10:
		if (p_value != -1.0)
		{
			display.fillScreen(myBLACK);
			display.setTextColor(myCYAN);
			display.setFont(NULL);
			display.setCursor(1, 0);
			display.setTextSize(1);
			display.print("Press.");
			display.setFont(&Font4x7Fixed);
			display.setCursor(display.getCursorX() + 2, 7);
			display.print("hPa");
			drawImage(55, 0, 7, 9, maison);
			display.setFont(NULL);
			display.setTextSize(2);
			display.setTextColor(myWHITE);
			// drawCentreString(String(pressure_at_sealevel(t_value, p_value) / 100, 0), 0, 9, 0);
		}
		else
		{
			act_milli += 5000;
		}
		break;
	case 11:
		if (nebuleair_pm10_value != -1.0 || nebuleair_pm25_value != -1.0 || nebuleair_pm01_value != -1.0 || nebuleair_t_value != -128.0 || nebuleair_h_value != -1.0 || nebuleair_p_value != -1.0 || nebuleair_cov_value != -1.0 || nebuleair_no2_value != -1.0 || atmoSud.multi != -1.0 || atmoSud.no2 != -1.0 || atmoSud.o3 != -1.0 || atmoSud.pm10 != -1.0 || atmoSud.pm2_5 != -1.0 || atmoSud.so2 != -1.0)
		{
			drawImage(0, 0, 32, 64, exterieur);

		}
		else
		{
			act_milli += 5000;
		}
		break;
	case 12:
		if (nebuleair_pm10_value != -1.0 || nebuleair_pm25_value != -1.0 || nebuleair_pm01_value != -1.0 || nebuleair_t_value != -128.0 || nebuleair_h_value != -1.0 || nebuleair_p_value != -1.0 || nebuleair_cov_value != -1.0 || nebuleair_no2_value != -1.0)
		{
			drawImage(0, 0, 32, 64, monnebuleair);  //+ AJOUTER 8 ECRANS OU ECRANS PARALLELES
		}
		else
		{
			act_milli += 5000;
		}
		break;
	case 13:
		if (atmoSud.multi != -1.0 || atmoSud.no2 != -1.0 || atmoSud.o3 != -1.0 || atmoSud.pm10 != -1.0 || atmoSud.pm2_5 != -1.0 || atmoSud.so2 != -1.0)
		{
			drawImage(0, 0, 32, 64, previsions);
		}
		else
		{
			act_milli += 5000;
		}
		break;
	case 14:
		if (atmoSud.multi != -1.0)
		{
			display.fillScreen(myBLACK);
			display.setTextColor(myWHITE);
			display.setFont(NULL);
			display.setCursor(1, 0);
			display.setTextSize(1);
			display.print("ICAIRh");
			drawImage(55, 0, 7, 9, soleil);
			displayColor = interpolateindice((int)atmoSud.multi, gamma_correction);
			myCUSTOM = display.color565(displayColor.R, displayColor.G, displayColor.B);
			display.fillRect(50, 9, 14, 14, myCUSTOM);
			display.setFont(NULL);
			display.setTextColor(myWHITE);
			display.setTextSize(2);
			drawCentreString(String(atmoSud.multi, 0), 0, 9, 14);
			display.setTextColor(myCUSTOM);
			messager5((int)atmoSud.multi);

			// //drawgradient(0, 25, atmoSud.no2, 20, 40, 50, 100, 150);
			// if(gamma_correction){drawImage(0, 28, 4, 64, gradient_20_150_gamma);}else{drawImage(0, 28, 4, 64, gradient_20_150);}
			// display.setTextSize(1);
			// display.setCursor((uint8_t)((63*atmoSud.multi)/150)-2, 25-2); //2 pixels de offset
			// display.write(31);
		}
		else
		{
			act_milli += 5000;
		}
		break;
	case 15:
		if (atmoSud.no2 != -1.0)
		{
			display.fillScreen(myBLACK);
			display.setTextColor(myWHITE);
			display.setFont(NULL);
			display.setCursor(1, 0);
			display.setTextSize(1);
			display.print("NO");
			display.write(250);
			display.setFont(&Font4x7Fixed);
			display.setCursor(display.getCursorX() + 2, 7);
			display.write(181);
			display.print("g/m");
			display.write(179);
			drawImage(55, 0, 7, 9, soleil);
			displayColor = interpolate(atmoSud.no2, 40, 90, 120, 230, 340, gamma_correction);
			myCUSTOM = display.color565(displayColor.R, displayColor.G, displayColor.B);
			display.fillRect(50, 9, 14, 14, myCUSTOM);
			display.setFont(NULL);
			display.setTextColor(myWHITE);
			display.setTextSize(2);
			drawCentreString(String(atmoSud.no2, 0), 0, 9, 14);
			//drawgradient(0, 25, atmoSud.no2, 40, 90, 120, 230, 340);
			if (gamma_correction)
			{
				drawImage(0, 28, 4, 64, gradient_40_340_gamma);
			}
			else
			{
				drawImage(0, 28, 4, 64, gradient_40_340);
			}
			display.setTextSize(1);
			display.setCursor((uint8_t)((63 * atmoSud.no2) / 340) - 2, 25 - 2); //2 pixels de offset
			display.write(31);
		}
		else
		{
			act_milli += 5000;
		}
		break;
	case 16:
		if (atmoSud.o3 != -1.0)
		{
			display.fillScreen(myBLACK);
			display.setTextColor(myWHITE);
			display.setFont(NULL);
			display.setCursor(1, 0);
			display.setTextSize(1);
			display.print("O");
			display.write(253);
			display.setFont(&Font4x7Fixed);
			display.setCursor(display.getCursorX() + 2, 7);
			display.write(181);
			display.print("g/m");
			display.write(179);
			drawImage(55, 0, 7, 9, soleil);
			displayColor = interpolate(atmoSud.o3, 50, 100, 130, 240, 380, gamma_correction);
			myCUSTOM = display.color565(displayColor.R, displayColor.G, displayColor.B);
			display.fillRect(50, 9, 14, 14, myCUSTOM);
			display.setFont(NULL);
			display.setTextColor(myWHITE);
			display.setTextSize(2);
			drawCentreString(String(atmoSud.o3, 0), 0, 9, 14);
			//drawgradient(0, 25, atmoSud.o3, 50, 100, 130, 240, 380);
			if (gamma_correction)
			{
				drawImage(0, 28, 4, 64, gradient_50_380_gamma);
			}
			else
			{
				drawImage(0, 28, 4, 64, gradient_50_380);
			}
			display.setTextSize(1);
			display.setCursor((uint8_t)((63 * atmoSud.o3) / 380) - 2, 25 - 2); //2 pixels de offset
			display.write(31);
		}
		else
		{
			act_milli += 5000;
		}
		break;
	case 17:
		if (atmoSud.pm10 != -1.0)
		{
			display.fillScreen(myBLACK);
			display.setTextColor(myWHITE);
			display.setFont(NULL);
			display.setCursor(1, 0);
			display.setTextSize(1);
			display.print("PM10");
			display.setFont(&Font4x7Fixed);
			display.setCursor(display.getCursorX() + 2, 7);
			display.write(181);
			display.print("g/m");
			display.write(179);
			drawImage(55, 0, 7, 9, soleil);
			displayColor = interpolate(atmoSud.pm10, 20, 40, 50, 100, 150, gamma_correction);
			myCUSTOM = display.color565(displayColor.R, displayColor.G, displayColor.B);
			display.fillRect(50, 9, 14, 14, myCUSTOM);
			display.setFont(NULL);
			display.setTextColor(myWHITE);
			display.setTextSize(2);
			drawCentreString(String(atmoSud.pm10, 0), 0, 9, 14);
			//drawgradient(0, 25, atmoSud.pm10, 20, 40, 50, 100, 150);
			if (gamma_correction)
			{
				drawImage(0, 28, 4, 64, gradient_20_150_gamma);
			}
			else
			{
				drawImage(0, 28, 4, 64, gradient_20_150);
			}
			display.setTextSize(1);
			display.setCursor((uint8_t)((63 * atmoSud.pm10) / 150) - 2, 25 - 2); //2 pixels de offset
			display.write(31);
		}
		else
		{
			act_milli += 5000;
		}
		break;
	case 18:
		if (atmoSud.pm2_5 != -1.0)
		{
			display.fillScreen(myBLACK);
			display.setTextColor(myWHITE);
			display.setFont(NULL);
			display.setCursor(1, 0);
			display.setTextSize(1);
			display.print("PM2.5");
			display.setFont(&Font4x7Fixed);
			display.setCursor(display.getCursorX() + 2, 7);
			display.write(181);
			display.print("g/m");
			display.write(179);
			drawImage(55, 0, 7, 9, soleil);
			displayColor = interpolate(atmoSud.pm2_5, 10, 20, 25, 50, 75, gamma_correction);
			myCUSTOM = display.color565(displayColor.R, displayColor.G, displayColor.B);
			display.fillRect(50, 9, 14, 14, myCUSTOM);
			display.setFont(NULL);
			display.setTextColor(myWHITE);
			display.setTextSize(2);
			drawCentreString(String(atmoSud.pm2_5, 0), 0, 9, 14);
			//drawgradient(0, 25, atmoSud.pm2_5, 10, 20, 25, 50, 75);
			if (gamma_correction)
			{
				drawImage(0, 28, 4, 64, gradient_10_75_gamma);
			}
			else
			{
				drawImage(0, 28, 4, 64, gradient_10_75);
			}
			display.setTextSize(1);
			display.setCursor((uint8_t)((63 * atmoSud.pm2_5) / 75) - 2, 25 - 2); //2 pixels de offset
			display.write(31);
		}
		else
		{
			act_milli += 5000;
		}
		break;
	case 19:
		if (atmoSud.so2 != -1.0)
		{
			display.fillScreen(myBLACK);
			display.setTextColor(myWHITE);
			display.setFont(NULL);
			display.setCursor(1, 0);
			display.setTextSize(1);
			display.print("SO2");
			display.setFont(&Font4x7Fixed);
			display.setCursor(display.getCursorX() + 2, 7);
			display.write(181);
			display.print("g/m");
			display.write(179);
			drawImage(55, 0, 7, 9, soleil);
			displayColor = interpolate(atmoSud.so2, 50, 100, 130, 240, 380, gamma_correction); //REVOIR LE GRADIENT SO2
			myCUSTOM = display.color565(displayColor.R, displayColor.G, displayColor.B);
			display.fillRect(50, 9, 14, 14, myCUSTOM);
			display.setFont(NULL);
			display.setTextColor(myWHITE);
			display.setTextSize(2);
			drawCentreString(String(atmoSud.so2, 0), 0, 9, 14);
			//drawgradient(0, 25, atmoSud.so2, 50, 100, 130, 240, 380);
			if (gamma_correction)
			{
				drawImage(0, 28, 4, 64, gradient_50_380_gamma);
			}
			else
			{
				drawImage(0, 28, 4, 64, gradient_50_380);
			}
			display.setTextSize(1);
			display.setCursor((uint8_t)((63 * atmoSud.so2) / 75) - 2, 25 - 2); //2 pixels de offset
			display.write(31);
		}
		else
		{
			act_milli += 5000;
		}
		break;
	case 20:
		if (has_logo && (logos[logo_index + 1] != 0 && logo_index != 5))
		{
			logo_index++;
		}
		else if (has_logo && (logos[logo_index + 1] == 0) || logo_index == 5)
		{
			logo_index = 0;
		}

		if (logos[logo_index] == cfg_logo_moduleair)
			drawImage(0, 0, 32, 64, logo_moduleair);
		if (logos[logo_index] == cfg_logo_aircarto)
			drawImage(0, 0, 32, 64, logo_aircarto);
		if (logos[logo_index] == cfg_logo_atmo)
			drawImage(0, 0, 32, 64, logo_atmo);
		if (logos[logo_index] == cfg_logo_region)
			drawImage(0, 0, 32, 64, logo_region);
		if (logos[logo_index] == cfg_logo_custom1)
			drawImage(0, 0, 32, 64, logo_custom1);
		if (logos[logo_index] == cfg_logo_custom2)
			drawImage(0, 0, 32, 64, logo_custom2);

		break;
	}

	yield();
	next_display_count++;
}

/*****************************************************************
 * Init matrix                                         *
 *****************************************************************/

static void init_matrix()
{
	timer = timerBegin(0, 80, true); //init timer once only
	display.begin(16);
	display.setDriverChip(SHIFT);  // SHIFT ou FM6124 ou FM6126A
	display.setColorOrder(RRGGBB); // ATTENTION à changer en fonction de l'écran !!!! Small Matrix (160x80mm) is RRBBGG and Big Matrix (192x96mm) is RRGGBB
	display_update_enable(true);
	display.setFont(NULL); //Default font

	for (int i = 1; i < 6; i++)
	{

		if (i == cfg_logo_moduleair)
		{
			display.fillScreen(myBLACK); //display.clearDisplay(); produces a flash
			drawImage(0, 0, 32, 64, logo_moduleair);
			logo_index++;
			logos[logo_index] = i;
			delay(5000);
		}
		if (i == cfg_logo_aircarto)
		{
			display.fillScreen(myBLACK); //display.clearDisplay(); produces a flash
			drawImage(0, 0, 32, 64, logo_aircarto);
			logo_index++;
			logos[logo_index] = i;
			delay(5000);
		}
		if (i == cfg_logo_atmo)
		{
			display.fillScreen(myBLACK); //display.clearDisplay(); produces a flash
			drawImage(0, 0, 32, 64, logo_atmo);
			logo_index++;
			logos[logo_index] = i;
			delay(5000);
		}
		if (i == cfg_logo_region)
		{
			display.fillScreen(myBLACK); //display.clearDisplay(); produces a flash
			drawImage(0, 0, 32, 64, logo_region);
			logo_index++;
			logos[logo_index] = i;
			delay(5000);
		}
		if (i == cfg_logo_custom1)
		{
			display.fillScreen(myBLACK); //display.clearDisplay(); produces a flash
			drawImage(0, 0, 32, 64, logo_custom1);
			logo_index++;
			logos[logo_index] = i;
			delay(5000);
		}
		if (i == cfg_logo_custom2)
		{
			display.fillScreen(myBLACK); //display.clearDisplay(); produces a flash
			drawImage(0, 0, 32, 64, logo_custom2);
			logo_index++;
			logos[logo_index] = i;
			delay(5000);
		}
	}

	if (logo_index != -1)
	{
		has_logo = true;
		logo_index = -1;
	}
	else
	{
		has_logo = false;
	}
	//IL FAUDRA ADAPTER LES DELAY SELON LE NOMBRE DE LOGOS!!!!!!
}

/*****************************************************************
 * Init BMP280/BME280                                            *
 *****************************************************************/
static bool initBMX280(char addr)
{
	debug_out(String(F("Trying BMx280 sensor on ")) + String(addr, HEX), DEBUG_MIN_INFO);

	if (bmx280.begin(addr))
	{
		debug_outln_info(FPSTR(DBG_TXT_FOUND));
		bmx280.setSampling(
			BMX280::MODE_FORCED,
			BMX280::SAMPLING_X1,
			BMX280::SAMPLING_X1,
			BMX280::SAMPLING_X1);
		return true;
	}
	else
	{
		debug_outln_info(FPSTR(DBG_TXT_NOT_FOUND));
		return false;
	}
}

/*****************************************************************
 * Init CCS811                                            *
 *****************************************************************/
static bool initCCS811()
{

	debug_out(String(F("Trying CCS811 sensor: ")), DEBUG_MIN_INFO);

	if (!ccs811.begin())
	{
		debug_out(String(F("CCS811 begin FAILED")), DEBUG_MIN_INFO);
		return false;
	}
	else
	{
		// Print CCS811 versions
		debug_outln_info(F("hardware version: "), ccs811.hardware_version());
		debug_outln_info(F("bootloader version: "), ccs811.bootloader_version());
		debug_outln_info(F("application version: "), ccs811.application_version());

		if (!ccs811.start(CCS811_MODE_1SEC))
		{
			debug_out(String(F("CCS811 start FAILED")), DEBUG_MIN_INFO);
			return false;
		}
		else
		{
			debug_out(String(F("CCS811 OK")), DEBUG_MIN_INFO);
			Debug.printf("\n");
			return true;
		}
	}
}

/*****************************************************************
   Functions
 *****************************************************************/

static void powerOnTestSensors()
{

	if (cfg::has_matrix)
	{
		display.fillScreen(myBLACK);
		display.setTextColor(myWHITE);
		display.setFont(NULL);
		display.setTextSize(1);
		display.setCursor(1, 0);
		display.print("Activation");
		display.setCursor(1, 11);
		display.print("des sondes");
	}

	if (cfg::npm_read)
	{
		int8_t test_state;

	if (cfg::has_matrix)
	{
		for (size_t i = 0; i < 30; ++i)
			{
				display.fillRect(i, 22, 1, 4, myWHITE);
				delay(500);
			}
	}else{delay(15000);};


		test_state = NPM_get_state();
		if (test_state == -1)
		{
			debug_outln_info(F("NPM not connected"));
			nextpmconnected = false;
		}
		else
		{
			nextpmconnected = true;
			if (test_state == 0x00)
			{
				debug_outln_info(F("NPM already started..."));
				nextpmconnected = true;
			}
			else if (test_state == 0x01)
			{
				debug_outln_info(F("Force start NPM...")); // to read the firmware version
				is_NPM_running = NPM_start_stop();
			}
			else
			{
				if (bitRead(test_state, 1) == 1)
				{
					debug_outln_info(F("Degraded state"));
				}
				else
				{
					debug_outln_info(F("Default state"));
				}
				if (bitRead(test_state, 2) == 1)
				{
					debug_outln_info(F("Not ready"));
				}
				if (bitRead(test_state, 3) == 1)
				{
					debug_outln_info(F("Heat error"));
				}
				if (bitRead(test_state, 4) == 1)
				{
					debug_outln_info(F("T/RH error"));
				}
				if (bitRead(test_state, 5) == 1)
				{
					debug_outln_info(F("Fan error"));

					// if (bitRead(test_state, 0) == 1){
					// 	debug_outln_info(F("Force start NPM..."));
					// 	is_NPM_running = NPM_start_stop();
					// 	delay(5000);
					// }
					// NPM_fan_speed();
					// delay(5000);
				}
				if (bitRead(test_state, 6) == 1)
				{
					debug_outln_info(F("Memory error"));
				}
				if (bitRead(test_state, 7) == 1)
				{
					debug_outln_info(F("Laser error"));
				}
				if (bitRead(test_state, 0) == 0)
				{
					debug_outln_info(F("NPM already started..."));
					is_NPM_running = true;
				}
				else
				{
					debug_outln_info(F("Force start NPM..."));
					is_NPM_running = NPM_start_stop();
				}
			}
		}

		if (nextpmconnected)
		{
		
			if (cfg::has_matrix)
	{

			for (size_t i = 30; i < 54; ++i)
			{
				display.fillRect(i, 22, 1, 4, myWHITE);
				delay(500);
			}
	}else{delay(15000);}

			NPM_version_date();

			if (cfg::has_matrix)
	{
			for (size_t i = 54; i < 60; ++i)
			{
				display.fillRect(i, 22, 1, 4, myWHITE);
				delay(500);
			}
	}else{delay(3000);}
			NPM_temp_humi();

				if (cfg::has_matrix)
	{
			for (size_t i = 60; i < 64; ++i)
			{
				display.fillRect(i, 22, 1, 4, myWHITE);
				delay(500);
			}

	}else{delay(2000);};
		}
	}

	if (cfg::bmx280_read)
	{
		debug_outln_info(F("Read BMxE280..."));
		if (!initBMX280(0x76) && !initBMX280(0x77))
		{
			debug_outln_error(F("Check BMx280 wiring"));
			bmx280_init_failed = true;
		}
	}

	if (cfg::ccs811_read)
	{
		debug_outln_info(F("Read CCS811..."));
		if (!initCCS811())
		{
			debug_outln_error(F("Check CCS811 wiring"));
			ccs811_init_failed = true;
		}
	}

		if (cfg::has_sdcard)
	{

		if (!SD.begin(SD_SPI_BUS_SS, SD_SPI_BUS_MOSI, SD_SPI_BUS_MISO, SD_SPI_BUS_CLK))
		{
			Debug.println("Card Mount Failed");
			return;
		}
		uint8_t cardType = SD.cardType();

		if (cardType == CARD_NONE)
		{
			Debug.println("No SD card attached");
			return;
		}

		Debug.print("SD Card Type: ");
		if (cardType == CARD_MMC)
		{
			Debug.println("MMC");
		}
		else if (cardType == CARD_SD)
		{
			Debug.println("SDSC");
		}
		else if (cardType == CARD_SDHC)
		{
			Debug.println("SDHC");
		}
		else
		{
			Debug.println("UNKNOWN");
		}

		uint64_t cardSize = SD.cardSize() / (1024 * 1024);
		Debug.printf("SD Card Size: %lluMB\n", cardSize);
		sdcard_found = true;
	}
}

/*****************************************************************
 * Check stack                                                    *
 *****************************************************************/
void *StackPtrAtStart;
void *StackPtrEnd;
UBaseType_t watermarkStart;


/*****************************************************************
 * SD card                                                  *
 *****************************************************************/

// char data_file[24];

String file_name;

static void listDir(fs::FS &fs, const char *dirname, uint8_t levels)
{
	Debug.printf("Listing directory: %s\n", dirname);

	File root = fs.open(dirname);
	if (!root)
	{
		Debug.println("Failed to open directory");
		return;
	}
	if (!root.isDirectory())
	{
		Debug.println("Not a directory");
		return;
	}

	File file = root.openNextFile();
	while (file)
	{
		if (file.isDirectory())
		{
			Debug.print("  DIR : ");
			Debug.println(file.name());
			if (levels)
			{
				listDir(fs, file.name(), levels - 1);
			}
		}
		else
		{
			Debug.print("  FILE: ");
			Debug.print(file.name());
			Debug.print("  SIZE: ");
			Debug.println(file.size());
		}
		file = root.openNextFile();
	}
}

static void createDir(fs::FS &fs, const char *path)
{
	Debug.printf("Creating Dir: %s\n", path);
	if (fs.mkdir(path))
	{
		Debug.println("Dir created");
	}
	else
	{
		Debug.println("mkdir failed");
	}
}

static void removeDir(fs::FS &fs, const char *path)
{
	Debug.printf("Removing Dir: %s\n", path);
	if (fs.rmdir(path))
	{
		Debug.println("Dir removed");
	}
	else
	{
		Debug.println("rmdir failed");
	}
}

static void readFile(fs::FS &fs, const char *path)
{
	Debug.printf("Reading file: %s\n", path);

	File file = fs.open(path);
	if (!file)
	{
		Debug.println("Failed to open file for reading");
		return;
	}

	Debug.print("Read from file: ");
	while (file.available())
	{
		Serial.write(file.read());
	}
	file.close();
}

static void writeFile(fs::FS &fs, const char *path, const char *message)
{
	Debug.printf("Writing file: %s\n", path);

	File file = fs.open(path, FILE_WRITE);
	if (!file)
	{
		Debug.println("Failed to open file for writing");
		return;
	}
	if (file.print(message))
	{
		Debug.println("File written");
	}
	else
	{
		Debug.println("Write failed");
	}
	file.close();
}

static void appendFile(fs::FS &fs, const char *path, const char *message)
{
	Debug.printf("Appending to file: %s\n", path);

	File file = fs.open(path, FILE_APPEND);
	if (!file)
	{
		Debug.println("Failed to open file for appending");
		return;
	}
	if (file.print(message))
	{
		Debug.println("Message appended");
	}
	else
	{
		Debug.println("Append failed");
	}
	file.close();
}

static void renameFile(fs::FS &fs, const char *path1, const char *path2)
{
	Debug.printf("Renaming file %s to %s\n", path1, path2);
	if (fs.rename(path1, path2))
	{
		Debug.println("File renamed");
	}
	else
	{
		Debug.println("Rename failed");
	}
}

static void deleteFile(fs::FS &fs, const char *path)
{
	Debug.printf("Deleting file: %s\n", path);
	if (fs.remove(path))
	{
		Debug.println("File deleted");
	}
	else
	{
		Debug.println("Delete failed");
	}
}

static void testFileIO(fs::FS &fs, const char *path)
{
	File file = fs.open(path);
	static uint8_t buf[512];
	size_t len = 0;
	uint32_t start = millis();
	uint32_t end = start;
	if (file)
	{
		len = file.size();
		size_t flen = len;
		start = millis();
		while (len)
		{
			size_t toRead = len;
			if (toRead > 512)
			{
				toRead = 512;
			}
			file.read(buf, toRead);
			len -= toRead;
		}
		end = millis() - start;
		Debug.printf("%u bytes read for %u ms\n", flen, end);
		file.close();
	}
	else
	{
		Debug.println("Failed to open file for reading");
	}

	file = fs.open(path, FILE_WRITE);
	if (!file)
	{
		Debug.println("Failed to open file for writing");
		return;
	}

	size_t i;
	start = millis();
	for (i = 0; i < 2048; i++)
	{
		file.write(buf, 512);
	}
	end = millis() - start;
	Debug.printf("%u bytes written for %u ms\n", 2048 * 512, end);
	file.close();
}

/*****************************************************************
 * I2C                                                  *
 *****************************************************************/

bool get_config = false;
bool get_matrix = false;
bool get_wifi = false;
bool get_wifi_test = false;
bool get_ethernet = false;
bool get_ethernet_test = false;
bool get_lora = false;
bool get_lora_test = false;

byte further = 0x00;


uint32_t i = 0;

byte config_byte[LEN_CONFIG_BYTE];
byte data_byte[LEN_DATA_BYTE];
byte matrix_byte[LEN_MATRIX_BYTE] = {0xBF, 0x80, 0x00, 0x00, 0xBF, 0x80, 0x00, 0x00, 0xBF, 0x80, 0x00, 0x00, 0xBF, 0x80, 0x00, 0x00, 0xBF, 0x80, 0x00, 0x00, 0xBF, 0x80, 0x00, 0x00, 0xBF, 0x80, 0x00, 0x00, 0xBF, 0x80, 0x00, 0x00, 0xBF, 0x80, 0x00, 0x00, 0xBF, 0x80, 0x00, 0x00, 0xBF, 0x80, 0x00, 0x00, 0xBF, 0x80, 0x00, 0x00, 0xBF, 0x80, 0x00, 0x00, 0xBF, 0x80, 0x00, 0x00, 0x07, 0xD0, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
byte wifi_byte[LEN_WIFI_BYTE];
byte lora_byte[LEN_LORA_BYTE];

void onRequestData(){
Debug.printf("Data to be sent through I2C:\n");
for (int i = 0; i < LEN_DATA_BYTE - 1; i++)
{
	Debug.printf(" %02x", data_byte[i]);
	if (i == LEN_DATA_BYTE - 2)
	{
		Debug.printf("\n");
	}
}
  Wire.write(data_byte, sizeof(data_byte));
  Debug.println("Data request!");
}

void onRequestFurther(){
  Wire.write(further);
  Debug.println("1 step further!");
  Debug.print("Send step: ");
  Debug.println(further);
}

void onReceiveMatrix(int len){
  while(Wire.available()){
	cfg::has_matrix = Wire.read();
  }
  	get_matrix = true;
	further = 0x01;

	if(cfg::has_matrix){
	Debug.println("Init Matrix");
	init_matrix();
	}else{
		Debug.println("No Matrix");
	}
}

void onReceiveWifi(int len){
	Debug.println("Receive WiFi");
  while(Wire.available()){
	cfg::has_wifi = Wire.read();
  }

	if(cfg::has_wifi && cfg::has_matrix){
		display.fillScreen(myBLACK);
		display.setTextColor(myWHITE);
		display.setFont(NULL);
		display.setCursor(1, 0);
		display.setTextSize(1);
		display.print("Activation");
		display.setCursor(1, 11);
		display.print("WiFi");
		for (int i = 0; i < 5; i++)
		{
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop1);
			delay(200);
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop2);
			delay(200);
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop3);
			delay(200);
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop4);
			delay(200);
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop5);
			delay(200);
		}
		display.fillScreen(myBLACK);
		display.setCursor(1, 0);
		display.print("Connexion");
		display.setCursor(1, 11);
		display.print("WiFi");
		for (int i = 0; i < 5; i++)
		{
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop1);
			delay(200);
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop2);
			delay(200);
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop3);
			delay(200);
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop4);
			delay(200);
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop5);
			delay(200);
		}
		display.fillScreen(myBLACK);

	}
	get_wifi = true;
	further = 0x02;
}

void onReceiveWifiTest(int len){  //REPRENDRE NUMÈRO

  while(Wire.available()){
	Wire.readBytes(wifi_byte,LEN_WIFI_BYTE);
  }
	if (cfg::has_matrix && wifi_byte[0] == 0x00)
	{
		display.fillScreen(myBLACK);
		display.setTextColor(myWHITE);
		display.setFont(NULL);
		display.setTextSize(1);
		display.setCursor(1, 0);
		display.print("Configurer");
		display.setCursor(1, 11);
		display.print("le WiFi");
		display.setCursor(1, 22);
		display.print("3 min.");

		for (int i = 0; i < 5; i++)
		{
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop1);
			delay(200);
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop2);
			delay(200);
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop3);
			delay(200);
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop4);
			delay(200);
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop5);
			delay(200);
		}
		display.fillScreen(myBLACK);
	}

	if (cfg::has_matrix && wifi_byte[0] == 0x01)
	{
		display.fillScreen(myBLACK);
			display.setTextColor(myWHITE);
			display.setFont(NULL);
			display.setTextSize(1);
			display.setCursor(1, 0);
			display.print("Connexion");
			display.setCursor(1, 11);
			display.write(130);
			display.print("tablie");
			display.setCursor(1, 22);
			display.print(String(wifi_byte[1]));
			display.print("%");

			for (int i = 0; i < 5; i++)
			{
				display.fillRect(47, 15, 16, 16, myBLACK);
				drawImage(47, 15, 16, 16, wifiloop1);
				delay(200);
				display.fillRect(47, 15, 16, 16, myBLACK);
				drawImage(47, 15, 16, 16, wifiloop2);
				delay(200);
				display.fillRect(47, 15, 16, 16, myBLACK);
				drawImage(47, 15, 16, 16, wifiloop3);
				delay(200);
				display.fillRect(47, 15, 16, 16, myBLACK);
				drawImage(47, 15, 16, 16, wifiloop4);
				delay(200);
				display.fillRect(47, 15, 16, 16, myBLACK);
				drawImage(47, 15, 16, 16, wifiloop5);
				delay(200);
			}
			display.fillScreen(myBLACK);
	}
	get_wifi_test = true;
	further = 0x03;
}


void onReceiveEthernet(int len){
  while(Wire.available()){
cfg::has_ethernet = Wire.read();
  }
if(cfg::has_ethernet && cfg::has_matrix){

		display.fillScreen(myBLACK);
		display.setTextColor(myWHITE);
		display.setFont(NULL);
		display.setCursor(1, 0);
		display.setTextSize(1);
		display.print("Activation");
		display.setCursor(1, 11);
		display.print("Ethernet");
		for (int i = 0; i < 5; i++)
		{
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop1);
			delay(200);
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop2);
			delay(200);
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop3);
			delay(200);
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop4);
			delay(200);
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop5);
			delay(200);
		}
		display.fillScreen(myBLACK);
		display.setCursor(1, 0);
		display.print("Connexion");
		display.setCursor(1, 11);
		display.print("Ethernet");
		for (int i = 0; i < 5; i++)
		{
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop1);
			delay(200);
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop2);
			delay(200);
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop3);
			delay(200);
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop4);
			delay(200);
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop5);
			delay(200);
		}
		display.fillScreen(myBLACK);
	}

	get_ethernet = true;
	further = 0x04;

}


void onReceiveEthernetTest(int len){  //REPRENDRE NUMÈRO

 byte ethernet_test;

  while(Wire.available()){
	ethernet_test = Wire.read();
  }
	if (cfg::has_matrix && ethernet_test == 0x00)
	{
		display.fillScreen(myBLACK);
		display.setTextColor(myWHITE);
		display.setFont(NULL);
		display.setTextSize(1);
		display.setCursor(1, 0);
		display.print("Aucune");
		display.setCursor(1, 11);
		display.print("connexion");
		display.setCursor(1, 22);
		display.print("Ehernet");
		delay(2000);
	}

	if (cfg::has_matrix && ethernet_test == 0x01)
	{
		display.fillScreen(myBLACK);
		display.setTextColor(myWHITE);
		display.setFont(NULL);
		display.setTextSize(1);
		display.setCursor(1, 0);
		display.print("Connexion");
		display.setCursor(1, 11);
		display.print("Ethernet");
		display.setCursor(1, 22);
		display.print("OK");
		delay(2000);
	}
	display.fillScreen(myBLACK);

	get_ethernet_test = true;
	further = 0x05;
}

void onReceiveLora(int len){
  while(Wire.available()){
	cfg::has_lora = Wire.read();
  }
	if(cfg::has_lora && cfg::has_matrix){

		display.fillScreen(myBLACK);
		display.setTextColor(myWHITE);
		display.setFont(NULL);
		display.setCursor(1, 0);
		display.setTextSize(1);
		display.print("Activation");
		display.setCursor(1, 11);
		display.print("LoRaWAN");
		for (int i = 0; i < 5; i++)
		{
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop1);
			delay(200);
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop2);
			delay(200);
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop3);
			delay(200);
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop4);
			delay(200);
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop5);
			delay(200);
		}
		display.fillScreen(myBLACK);
		display.setCursor(1, 0);
		display.print("Connexion");
		display.setCursor(1, 11);
		display.print("LoRaWAN");
		for (int i = 0; i < 5; i++)
		{
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop1);
			delay(200);
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop2);
			delay(200);
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop3);
			delay(200);
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop4);
			delay(200);
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop5);
			delay(200);
		}
		display.fillScreen(myBLACK);
	}
	get_lora = true;
	further = 0x06;
}


void onReceiveLoraTest(int len){  //REPRENDRE NUMÈRO

  while(Wire.available()){
	Wire.readBytes(lora_byte,LEN_LORA_BYTE);
  }
	if (cfg::has_matrix && lora_byte[0] == 0x00)
	{
		display.fillScreen(myBLACK);
		display.setTextColor(myWHITE);
		display.setFont(NULL);
		display.setTextSize(1);
		display.setCursor(1, 0);
		display.print("Aucune");
		display.setCursor(1, 11);
		display.print("Connexion");
		display.setCursor(1, 22);
		display.print("LoRaWAN");

		for (int i = 0; i < 5; i++)
		{
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop1);
			delay(200);
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop2);
			delay(200);
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop3);
			delay(200);
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop4);
			delay(200);
			display.fillRect(47, 15, 16, 16, myBLACK);
			drawImage(47, 15, 16, 16, wifiloop5);
			delay(200);
		}
		display.fillScreen(myBLACK);
	}

	if (cfg::has_matrix && lora_byte[0] == 0x01)
	{
		display.fillScreen(myBLACK);
			display.setTextColor(myWHITE);
			display.setFont(NULL);
			display.setTextSize(1);
			display.setCursor(1, 0);
			display.print("Connexion");
			display.setCursor(1, 11);
			display.write(130);
			display.print("LoRaWAN");
			display.setCursor(1, 22);
			display.print(String(lora_byte[1]));
			display.print("%");

			for (int i = 0; i < 5; i++)
			{
				display.fillRect(47, 15, 16, 16, myBLACK);
				drawImage(47, 15, 16, 16, wifiloop1);
				delay(200);
				display.fillRect(47, 15, 16, 16, myBLACK);
				drawImage(47, 15, 16, 16, wifiloop2);
				delay(200);
				display.fillRect(47, 15, 16, 16, myBLACK);
				drawImage(47, 15, 16, 16, wifiloop3);
				delay(200);
				display.fillRect(47, 15, 16, 16, myBLACK);
				drawImage(47, 15, 16, 16, wifiloop4);
				delay(200);
				display.fillRect(47, 15, 16, 16, myBLACK);
				drawImage(47, 15, 16, 16, wifiloop5);
				delay(200);
			}
			display.fillScreen(myBLACK);
	}
	get_lora_test = true;
	further = 0x07;
}

void onReceiveConfig(int len){
  while(Wire.available()){
	Wire.readBytes(config_byte,LEN_CONFIG_BYTE);
  }
	Debug.println(len);
	get_config = true;
	further = 0x08;
}

void onReceiveData(int len){
  while(Wire.available()){
	Wire.readBytes(matrix_byte,LEN_MATRIX_BYTE);
  }
	Debug.println(len);

union float_2_byte
{
	float temp_float;
	byte temp_byte[4];
} u_float;

if(matrix_byte[68] == 0x01)
{
	Debug.println("Restart triggered!");
	delay(500);
	ESP.restart();
}else{

u_float.temp_byte[0] = matrix_byte[0];
u_float.temp_byte[1] = matrix_byte[1];
u_float.temp_byte[2] = matrix_byte[2];
u_float.temp_byte[3] = matrix_byte[3];

atmoSud.multi = u_float.temp_float;

u_float.temp_byte[0] = matrix_byte[4];
u_float.temp_byte[1] = matrix_byte[5];
u_float.temp_byte[2] = matrix_byte[6];
u_float.temp_byte[3] = matrix_byte[7];

atmoSud.no2 = u_float.temp_float;

u_float.temp_byte[0] = matrix_byte[8];
u_float.temp_byte[1] = matrix_byte[9];
u_float.temp_byte[2] = matrix_byte[10];
u_float.temp_byte[3] = matrix_byte[11];

atmoSud.o3 = u_float.temp_float;

u_float.temp_byte[0] = matrix_byte[12];
u_float.temp_byte[1] = matrix_byte[13];
u_float.temp_byte[2] = matrix_byte[14];
u_float.temp_byte[3] = matrix_byte[15];

atmoSud.pm10 = u_float.temp_float;

u_float.temp_byte[0] = matrix_byte[16];
u_float.temp_byte[1] = matrix_byte[17];
u_float.temp_byte[2] = matrix_byte[18];
u_float.temp_byte[3] = matrix_byte[19];

atmoSud.pm2_5 = u_float.temp_float;

u_float.temp_byte[0] = matrix_byte[20];
u_float.temp_byte[1] = matrix_byte[21];
u_float.temp_byte[2] = matrix_byte[22];
u_float.temp_byte[3] = matrix_byte[23];

atmoSud.so2 = u_float.temp_float;

u_float.temp_byte[0] = matrix_byte[24];
u_float.temp_byte[1] = matrix_byte[25];
u_float.temp_byte[2] = matrix_byte[26];
u_float.temp_byte[3] = matrix_byte[27];

nebuleair.pm1 = u_float.temp_float;

u_float.temp_byte[0] = matrix_byte[28];
u_float.temp_byte[1] = matrix_byte[29];
u_float.temp_byte[2] = matrix_byte[30];
u_float.temp_byte[3] = matrix_byte[31];

nebuleair.pm2_5 = u_float.temp_float;

u_float.temp_byte[0] = matrix_byte[32];
u_float.temp_byte[1] = matrix_byte[33];
u_float.temp_byte[2] = matrix_byte[34];
u_float.temp_byte[3] = matrix_byte[35];

nebuleair.pm10 = u_float.temp_float;

u_float.temp_byte[0] = matrix_byte[36];
u_float.temp_byte[1] = matrix_byte[37];
u_float.temp_byte[2] = matrix_byte[38];
u_float.temp_byte[3] = matrix_byte[39];

nebuleair.no2 = u_float.temp_float;

u_float.temp_byte[0] = matrix_byte[40];
u_float.temp_byte[1] = matrix_byte[41];
u_float.temp_byte[2] = matrix_byte[42];
u_float.temp_byte[3] = matrix_byte[43];

nebuleair.cov = u_float.temp_float;

u_float.temp_byte[0] = matrix_byte[44];
u_float.temp_byte[1] = matrix_byte[45];
u_float.temp_byte[2] = matrix_byte[46];
u_float.temp_byte[3] = matrix_byte[47];

nebuleair.t = u_float.temp_float;

u_float.temp_byte[0] = matrix_byte[48];
u_float.temp_byte[1] = matrix_byte[49];
u_float.temp_byte[2] = matrix_byte[50];
u_float.temp_byte[3] = matrix_byte[51];

nebuleair.h = u_float.temp_float;

u_float.temp_byte[0] = matrix_byte[52];
u_float.temp_byte[1] = matrix_byte[53];
u_float.temp_byte[2] = matrix_byte[54];
u_float.temp_byte[3] = matrix_byte[55];

nebuleair.p= u_float.temp_float;

matrix_time.year = word(matrix_byte[56], matrix_byte[57]);
matrix_time.month = word(matrix_byte[58], matrix_byte[59]);
matrix_time.day = word(matrix_byte[60], matrix_byte[61]);
matrix_time.hour = word(matrix_byte[62], matrix_byte[63]);
matrix_time.minute = word(matrix_byte[64], matrix_byte[65]);
matrix_time.second = word(matrix_byte[66], matrix_byte[67]);

debug_outln_info(F("AtmoSud ICAIR: "), atmoSud.multi);
debug_outln_info(F("AtmoSud NO2: "), atmoSud.no2);
debug_outln_info(F("AtmoSud O3: "), atmoSud.o3);
debug_outln_info(F("AtmoSud PM10: "), atmoSud.pm10);
debug_outln_info(F("AtmoSud PM2.5: "), atmoSud.pm2_5);
debug_outln_info(F("AtmoSud SO2: "), atmoSud.no2);
debug_outln_info(F("NebuleAir PM1: "), nebuleair.pm1);
debug_outln_info(F("NebuleAir PM2.5: "), nebuleair.pm2_5);
debug_outln_info(F("NebuleAir PM10: "), nebuleair.pm10);
debug_outln_info(F("NebuleAir NO2: "), nebuleair.no2);
debug_outln_info(F("NebuleAir VOC: "), nebuleair.cov);
debug_outln_info(F("NebuleAir T°:"), nebuleair.t);
debug_outln_info(F("NebuleAir RH: "), nebuleair.h);
debug_outln_info(F("NebuleAir P: "), nebuleair.p);
debug_outln_info(F("Year:: "), matrix_time.year);
debug_outln_info(F("Month: "), matrix_time.month);
debug_outln_info(F("Day: "), matrix_time.day);
debug_outln_info(F("Hour: "), matrix_time.hour);
debug_outln_info(F("Minute: "), matrix_time.minute);
debug_outln_info(F("Second: "), matrix_time.second);
}
}

/*****************************************************************
 * The Setup                                                     *
 *****************************************************************/

void setup()
{
	void *SpStart = NULL;
	StackPtrAtStart = (void *)&SpStart;
	watermarkStart = uxTaskGetStackHighWaterMark(NULL);
	StackPtrEnd = StackPtrAtStart - watermarkStart;

	Debug.begin(115200); // Output to Serial at 115200 baud
	Debug.println(F("Starting ESP32 SENS"));

	Debug.printf("\r\n\r\nAddress of Stackpointer near start is:  %p \r\n", (void *)StackPtrAtStart);
	Debug.printf("End of Stack is near: %p \r\n", (void *)StackPtrEnd);
	Debug.printf("Free Stack at start of setup() is:  %d \r\n", (uint32_t)StackPtrAtStart - (uint32_t)StackPtrEnd);

	esp_chipid = String((uint16_t)(ESP.getEfuseMac() >> 32), HEX); // for esp32
	esp_chipid += String((uint32_t)ESP.getEfuseMac(), HEX);
	esp_chipid.toUpperCase();

	Debug.println(F("I2C Slave"));
	// Wire.begin((uint8_t)I2C_SLAVE_ADDR,I2C_PIN_SDA,I2C_PIN_SCL);
	Wire.begin((uint8_t)I2C_SLAVE_ADDR);

    Debug.println(F("I2C RTC")); 
	Wire1.begin(I2C_PIN_SDA_2, I2C_PIN_SCL_2);  //REVOIRLES PINS
	
	Wire.onRequest(onRequestFurther); //toujours valable

	Wire.onReceive(onReceiveMatrix);
	while(!get_matrix && further == 0x00)
	{
		Debug.println("Wait for matrix config...");
	}
	//onRequestFurther will be activated => further = false
	Wire.onReceive(onReceiveWifi);

	while(!get_wifi && further == 0x01)
	{
		Debug.println("Wait for wifi config...");
	}
	
	//onRequestFurther will be activated => further = false

	Wire.onReceive(onReceiveWifiTest);
	while(!get_wifi_test && further == 0x02)
	{
		Debug.println("Wait for wifi test...");
	}

	//onRequestFurther will be activated => further = false

	Wire.onReceive(onReceiveEthernet);
	while(!get_ethernet && further == 0x03)
	{
		Debug.println("Wait for Ethernet start...");
	}
	
	Wire.onReceive(onReceiveEthernetTest);
	while(!get_ethernet_test && further == 0x04)
	{
		Debug.println("Wait for Ethernet test...");
	}

	Wire.onReceive(onReceiveLora);
	while(!get_lora && further == 0x05)
	{
		Debug.println("Wait for LoRaWAN start...");
	}
	
	Wire.onReceive(onReceiveLoraTest);
	while(!get_lora_test && further == 0x06)
	{
		Debug.println("Wait for lora test...");
	}

	Wire.onReceive(onReceiveConfig);
	while(!get_config && further == 0x07)
	{
		Debug.println("Wait for whole config...");
	}
    
cfg::has_sdcard = config_byte[0];
cfg::has_matrix = config_byte[1];
cfg::has_wifi = config_byte[2];
cfg::has_lora = config_byte[3];
cfg::has_ethernet = config_byte[4];
cfg::npm_read = config_byte[5];
cfg::bmx280_read = config_byte[6];
cfg::mhz16_read = config_byte[7];
cfg::mhz19_read = config_byte[8];
cfg::ccs811_read = config_byte[9];
cfg::enveano2_read = config_byte[10];
cfg::display_measure = config_byte[11];
cfg::display_forecast = config_byte[12];

String timestringntp;

timestringntp += "20";
timestringntp += String(config_byte[13]);
timestringntp += "-";
if (config_byte[14] + 1 < 10){timestringntp += "0";}
timestringntp += String(config_byte[14]);
timestringntp += "-";
if (config_byte[15] < 10){timestringntp += "0";}
timestringntp += String(config_byte[15]);
timestringntp += "T";
if (config_byte[16] < 10){timestringntp += "0";}
timestringntp += String(config_byte[16]);
timestringntp += ":";
if (config_byte[17] < 10){timestringntp += "0";}
timestringntp += String(config_byte[17]);
timestringntp += ":";
if (config_byte[18] < 10){timestringntp += "0";}
timestringntp += String(config_byte[18]);
timestringntp += "Z";

cfg::utc_offset = config_byte[19];

String current_time;
current_time += "20";
current_time += String(config_byte[20]);
current_time += "-";
if (config_byte[21] < 10){current_time += "0";}
current_time += String(config_byte[21]);
current_time += "-";
if (config_byte[22] < 10){current_time += "0";}
current_time += String(config_byte[22]);
current_time += "T";
if (config_byte[23] < 10){current_time += "0";}
current_time += String(config_byte[23]);
current_time += ":";
if (config_byte[24] < 10){current_time += "0";}
current_time += String(config_byte[24]);
current_time += ":";
if (config_byte[25] < 10){current_time += "0";}
current_time += String(config_byte[25]);
current_time += "Z";

cfg::show_nebuleair = config_byte[26];
cfg::height_above_sealevel = word(config_byte[27], config_byte[28]);

debug_outln_info(F("ModuleAirV3: " SOFTWARE_VERSION_STR "/"), String(CURRENT_LANG));
debug_outln_info(F("---- Config ----"));
debug_outln_info(F("SD: "), cfg::has_sdcard);
debug_outln_info(F("Matrix: "), cfg::has_matrix);
debug_outln_info(F("Wifi: "), cfg::has_wifi);
debug_outln_info(F("LoRaWAN: "), cfg::has_lora);
debug_outln_info(F("Ehternet: "), cfg::has_ethernet);
debug_outln_info(F("NPM: "), cfg::npm_read);
debug_outln_info(F("BME280: "), cfg::bmx280_read);
debug_outln_info(F("MHZ16: "), cfg::mhz16_read);
debug_outln_info(F("MHZ19: "), cfg::mhz19_read);
debug_outln_info(F("CCS811: "), cfg::ccs811_read);
debug_outln_info(F("Envea: "), cfg::enveano2_read);
debug_outln_info(F("Display measure: "), cfg::display_measure);
debug_outln_info(F("Display forecast: "), cfg::display_forecast);
debug_outln_info(F("Time NTP: "), timestringntp);
debug_outln_info(F("UTC offset: "), cfg::utc_offset);
debug_outln_info(F("Time RTC: "), current_time);
debug_outln_info(F("Show NebuleAir: "), cfg::show_nebuleair);
debug_outln_info(F("Altitude: "), cfg::height_above_sealevel);


	if (cfg::npm_read)
	{
		serialNPM.begin(115200, SERIAL_8E1, PM_SERIAL_RX, PM_SERIAL_TX);
		Debug.println("Read Next PM... serialNPM 115200 8E1");
		serialNPM.setTimeout(400);
	}

	if (cfg::mhz16_read || cfg::mhz19_read)
	{
		//serialMHZ.begin(9600, SERIAL_8N1, CO2_SERIAL_RX, CO2_SERIAL_TX);
		Debug.println("serialMHZ 9600 8N1");
		//serialMHZ.setTimeout((4 * 12 * 1000) / 9600); //VOIR ICI LE TIMEOUT

		if (cfg::mhz16_read)
		{
			mhz16.begin(CO2_SERIAL_RX, CO2_SERIAL_TX, 2);
			//mhz16.autoCalibration(false);
		}

		if (cfg::mhz19_read)
		{
			serialMHZ.begin(9600, SERIAL_8N1, CO2_SERIAL_RX, CO2_SERIAL_TX);
			mhz19.begin(serialMHZ);
			mhz19.autoCalibration(false);
		}
	}

		if (cfg::enveano2_read)
	{
		serialNO2.begin(9600, EspSoftwareSerial::SWSERIAL_8N1, NO2_SERIAL_RX, NO2_SERIAL_TX); 
		Debug.println("Envea Cairsens NO2... serialN02 9600 8N1 SoftwareSerial");
		serialNO2.setTimeout((4 * 12 * 1000) / 9600);
	}

	debug_outln_info(F("\nChipId: "), esp_chipid);

	// if (cfg::has_matrix)
	// {
		
	// }

	powerOnTestSensors();

	delay(50);

	starttime = millis(); // store the start time
	last_update_attempt = time_point_device_start_ms = starttime;

	if (cfg::npm_read)
	{
		last_display_millis_matrix = starttime_NPM = starttime;
	}

	if (cfg::mhz16_read)
	{
		last_display_millis_matrix = starttime_MHZ16 = starttime;
	}

	if (cfg::mhz19_read)
	{
		last_display_millis_matrix = starttime_MHZ19 = starttime;
	}
	if (cfg::ccs811_read)
	{
		last_display_millis_matrix = starttime_CCS811 = starttime;
	}

	if (cfg::enveano2_read)
	{
		last_display_millis_matrix = starttime_Cairsens = starttime;
	}

	if (cfg::display_forecast)
	{
		forecast_selector = 0; 
	}


	if (cfg::has_sdcard && sdcard_found)  
		{
			listDir(SD, "/", 0);
			if(cfg::has_wifi && timestringntp != "2000-00-00T00:00:00Z")
			{
			file_name = String("/") + String(config_byte[14]) + String("_") + String(config_byte[15]) + String("_") + String(config_byte[16]) + String("_") + String(config_byte[17]) + String("_") + String(config_byte[18]) + String("_") + String(config_byte[19]) + String(".csv");
			
			writeFile(SD, file_name.c_str(), "");
			appendFile(SD, file_name.c_str(), "Date;NextPM_PM1;NextPM_PM2_5;NextPM_PM10;NextPM_NC1;NextPM_NC2_5;NextPM_NC10;CCS811_COV;Cairsens_NO2;BME280_T;BME280_H;BME280_P;Latitude;Longitude;Altitude;Type\n");
			Debug.println("Date;NextPM_PM1;NextPM_PM2_5;NextPM_PM10;NextPM_NC1;NextPM_NC2_5;NextPM_NC10;CCS811_COV;Cairsens_NO2;BME280_T;BME280_H;BME280_P;Latitude;Longitude;Altitude;Type\n");
			file_created = true;
			}

			if(!cfg::has_wifi && current_time != "2000-00-00T00:00:00Z")
			{
			file_name = String("/") + String(config_byte[21]) + String("_") + String(config_byte[22]) + String("_") + String(config_byte[23]) + String("_") + String(config_byte[24]) + String("_") + String(config_byte[25]) + String("_") + String(config_byte[26]) + String(".csv");
			
			writeFile(SD, file_name.c_str(), "");
			appendFile(SD, file_name.c_str(), "Date;NextPM_PM1;NextPM_PM2_5;NextPM_PM10;NextPM_NC1;NextPM_NC2_5;NextPM_NC10;CCS811_COV;Cairsens_NO2;BME280_T;BME280_H;BME280_P;Latitude;Longitude;Altitude;Type\n");
			Debug.println("Date;NextPM_PM1;NextPM_PM2_5;NextPM_PM10;NextPM_NC1;NextPM_NC2_5;NextPM_NC10;CCS811_COV;Cairsens_NO2;BME280_T;BME280_H;BME280_P;Latitude;Longitude;Altitude;Type\n");
			file_created = true;
			}

			if(timestringntp != "2000-00-00T00:00:00Z" && current_time == "2000-00-00T00:00:00Z")
			{
				Debug.println("Can't create file!");
			}
		}else
		{
			Debug.println("Can't create file!");
		}

	Debug.printf("End of void setup()\n");
	time_end_setup = millis();
	Wire.onRequest(onRequestData);
	Wire.onReceive(onReceiveData);
}

void loop()
{
	
	unsigned sum_send_time = 0;

	act_micro = micros();
	act_milli = millis();
	send_now = msSince(starttime) > cfg::sending_intervall_ms;

	sample_count++;

	if (last_micro != 0)
	{
		unsigned long diff_micro = act_micro - last_micro;
		UPDATE_MIN_MAX(min_micro, max_micro, diff_micro);
	}
	last_micro = act_micro;

	if (cfg::npm_read)
	{
		if ((msSince(starttime_NPM) > SAMPLETIME_NPM_MS && npm_val_count == 0) || send_now)
		{
			starttime_NPM = act_milli;
			fetchSensorNPM();
		}
	}

	if (cfg::mhz16_read)
	{
		if ((msSince(starttime_MHZ16) > SAMPLETIME_MHZ16_MS && mhz16_val_count < 11) || send_now)
		{
			starttime_MHZ16 = act_milli;
			fetchSensorMHZ16();
		}
	}

	if (cfg::mhz19_read)
	{
		if ((msSince(starttime_MHZ19) > SAMPLETIME_MHZ19_MS && mhz19_val_count < 11) || send_now)
		{
			starttime_MHZ19 = act_milli;
			fetchSensorMHZ19();
		}
	}

	if (cfg::ccs811_read && (!ccs811_init_failed))
	{
		if ((msSince(starttime_CCS811) > SAMPLETIME_CCS811_MS && ccs811_val_count < 11) || send_now)
		{
			starttime_CCS811 = act_milli;
			fetchSensorCCS811();
		}
	}

	if (cfg::enveano2_read)
	{
		if ((msSince(starttime_Cairsens) > SAMPLETIME_Cairsens_MS && no2_val_count < 11) || send_now)
		{
			starttime_Cairsens = act_milli;
			fetchSensorCairsens();
		}
	}

	if ((msSince(last_display_millis_matrix) > DISPLAY_UPDATE_INTERVAL_MS) && (cfg::has_matrix))
	{
		display_values_matrix();
		last_display_millis_matrix = act_milli;
	}


	if (send_now && cfg::sending_intervall_ms >= 120000)
	{
		void *SpActual = NULL;
		Debug.printf("Free Stack at send_now is: %d \r\n", (uint32_t)&SpActual - (uint32_t)StackPtrEnd);

	// union int16_2_byte
	// {
	// 	int16_t temp_int;
	// 	byte temp_byte[2];
	// } u1;


	union float_2_byte
	{
		float temp_float;
		byte temp_byte[4];
	} ufloat;


	ufloat.temp_float = last_value_NPM_P0;

	data_byte[0] = ufloat.temp_byte[0];
	data_byte[1] = ufloat.temp_byte[1];
	data_byte[2] = ufloat.temp_byte[2];
	data_byte[3] = ufloat.temp_byte[3];

	ufloat.temp_float = last_value_NPM_P1;

	data_byte[4] = ufloat.temp_byte[0];
	data_byte[5] = ufloat.temp_byte[1];
	data_byte[6] = ufloat.temp_byte[2];
	data_byte[7] = ufloat.temp_byte[3];

	ufloat.temp_float = last_value_NPM_P2;

	data_byte[8] = ufloat.temp_byte[0];
	data_byte[9] = ufloat.temp_byte[1];
	data_byte[10] = ufloat.temp_byte[2];
	data_byte[11] = ufloat.temp_byte[3];

	ufloat.temp_float = last_value_NPM_N1;

	data_byte[12] = ufloat.temp_byte[0];
	data_byte[13] = ufloat.temp_byte[1];
	data_byte[14] = ufloat.temp_byte[2];
	data_byte[15] = ufloat.temp_byte[3];

	ufloat.temp_float = last_value_NPM_N10;

	data_byte[16] = ufloat.temp_byte[0];
	data_byte[17] = ufloat.temp_byte[1];
	data_byte[18] = ufloat.temp_byte[2];
	data_byte[19] = ufloat.temp_byte[3];

	ufloat.temp_float = last_value_NPM_N25;

	data_byte[20] = ufloat.temp_byte[0];
	data_byte[21] = ufloat.temp_byte[1];
	data_byte[22] = ufloat.temp_byte[2];
	data_byte[23] = ufloat.temp_byte[3];


	ufloat.temp_float = last_value_MHZ16;

	data_byte[24] = ufloat.temp_byte[0];
	data_byte[25] = ufloat.temp_byte[1];
	data_byte[26] = ufloat.temp_byte[2];
	data_byte[27] = ufloat.temp_byte[3];


	ufloat.temp_float = last_value_MHZ19;

	data_byte[28] = ufloat.temp_byte[0];
	data_byte[29] = ufloat.temp_byte[1];
	data_byte[30] = ufloat.temp_byte[2];
	data_byte[31] = ufloat.temp_byte[3];

	ufloat.temp_float = last_value_CCS811;

	data_byte[32] = ufloat.temp_byte[0];
	data_byte[33] = ufloat.temp_byte[1];
	data_byte[34] = ufloat.temp_byte[2];
	data_byte[35] = ufloat.temp_byte[3];


	ufloat.temp_float = last_value_BMX280_T;

	data_byte[36] = ufloat.temp_byte[0];
	data_byte[37] = ufloat.temp_byte[1];
	data_byte[38] = ufloat.temp_byte[2];
	data_byte[39] = ufloat.temp_byte[3];


	ufloat.temp_float = last_value_BME280_H;

	data_byte[40] = ufloat.temp_byte[0];
	data_byte[41] = ufloat.temp_byte[1];
	data_byte[42] = ufloat.temp_byte[2];
	data_byte[43] = ufloat.temp_byte[3];

	ufloat.temp_float = last_value_BMX280_P;

	data_byte[44] = ufloat.temp_byte[0];
	data_byte[45] = ufloat.temp_byte[1];
	data_byte[46] = ufloat.temp_byte[2];
	data_byte[47] = ufloat.temp_byte[3];

	ufloat.temp_float = last_value_no2;

	data_byte[48] = ufloat.temp_byte[0];
	data_byte[49] = ufloat.temp_byte[1];
	data_byte[50] = ufloat.temp_byte[2];
	data_byte[51] = ufloat.temp_byte[3];

	Debug.printf("HEX values data:\n");
	for (int i = 0; i < LEN_DATA_BYTE - 1; i++)
	{
		Debug.printf(" %02x", data_byte[i]);
		if (i == LEN_DATA_BYTE - 2) 
		{
			Debug.printf("\n");
		}
	}

	Wire.slaveWrite(data_byte, LEN_DATA_BYTE);

		yield();  //UTILE???

		// Resetting for next sampling
		sample_count = 0;
		last_micro = 0;
		min_micro = 1000000000;
		max_micro = 0;
		sum_send_time = 0;

		starttime = millis(); // store the start time
		count_sends++;

		// Update Forecast selector
		if (cfg::display_forecast)
		{
			if (forecast_selector < 5)
			{
				forecast_selector++;
			}
			else
			{
				forecast_selector = 0;
			}
		}
	}

	if (sample_count % 500 == 0)
	{
		//		Serial.println(ESP.getFreeHeap(),DEC);
	}
}

const uint8_t PROGMEM gamma8[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2,
	2, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5,
	5, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10,
	10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
	17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
	25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
	37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
	51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
	69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
	90, 92, 93, 95, 96, 98, 99, 101, 102, 104, 105, 107, 109, 110, 112, 114,
	115, 117, 119, 120, 122, 124, 126, 127, 129, 131, 133, 135, 137, 138, 140, 142,
	144, 146, 148, 150, 152, 154, 156, 158, 160, 162, 164, 167, 169, 171, 173, 175,
	177, 180, 182, 184, 186, 189, 191, 193, 196, 198, 200, 203, 205, 208, 210, 213,
	215, 218, 220, 223, 225, 228, 231, 233, 236, 239, 241, 244, 247, 249, 252, 255};
