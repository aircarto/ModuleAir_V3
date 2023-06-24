#include <WString.h>
#include <pgmspace.h>

#define SOFTWARE_VERSION_STR "ModuleAirV3-V1-062023-com"
#define SOFTWARE_VERSION_STR_SHORT "V1-062023"
String SOFTWARE_VERSION(SOFTWARE_VERSION_STR);
String SOFTWARE_VERSION_SHORT(SOFTWARE_VERSION_STR_SHORT);

#include <Arduino.h>
#include <arduino_lmic.h>
#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>
#include <Wire.h>
#include "./WirePacker.h"
#include <Ethernet.h>
#include <RTClib.h>

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

//Dans Ethernet.h   ///REVOIR ICI POUR ETHERNET

// On remplace tous les SPI. par SPI_H.

//Normalement les pins sont les suivants:

// _sck = (_spi_num == VSPI) ? SCK : 14;
// _miso = (_spi_num == VSPI) ? MISO : 12;
// _mosi = (_spi_num == VSPI) ? MOSI : 13;
// _ss = (_spi_num == VSPI) ? SS : 15;

//on remplace la glcdfont.c original dans AdaFruitGFX => mod dans le dossier Fonts

/*****************************************************************
 * IMPORTANT FIN                                          *
 *****************************************************************/

#include "ca-root.h"

// includes ESP32 libraries
#define FORMAT_SPIFFS_IF_FAILED true
#include <FS.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
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

#include <WebServer.h>
#include <ESPmDNS.h>
#include <MD5Builder.h>

// includes external libraries

#define ARDUINOJSON_ENABLE_ARDUINO_STREAM 0
#define ARDUINOJSON_ENABLE_ARDUINO_PRINT 0
#define ARDUINOJSON_DECODE_UNICODE 0
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <StreamString.h>

// includes files
#include "./intl.h"
#include "./utils.h"
#include "defines.h"
#include "ext_def.h"
#include "html-content.h"

/*****************************************************************
 * CONFIGURATION                                          *
 *****************************************************************/
namespace cfg
{
	unsigned debug = DEBUG;

	unsigned time_for_wifi_config = TIME_FOR_WIFI_CONFIG;
	unsigned sending_intervall_ms = SENDING_INTERVALL_MS;

	char current_lang[3];

	// credentials for basic auth of internal web server
	bool www_basicauth_enabled = WWW_BASICAUTH_ENABLED;
	char www_username[LEN_WWW_USERNAME];
	char www_password[LEN_CFG_PASSWORD];

	// wifi credentials
	char wlanssid[LEN_WLANSSID];
	char wlanpwd[LEN_CFG_PASSWORD];

	// credentials of the sensor in access point mode
	char fs_ssid[LEN_FS_SSID] = FS_SSID;
	char fs_pwd[LEN_CFG_PASSWORD] = FS_PWD;

	// main config

	bool has_wifi = HAS_WIFI;
	bool has_lora = HAS_LORA;
	char appeui[LEN_APPEUI];
	char deveui[LEN_DEVEUI];
	char appkey[LEN_APPKEY];
	bool has_sdcard = HAS_SDCARD;
	bool has_ethernet = HAS_ETHERNET;

	// (in)active sensors
	bool sds_read = SDS_READ;
	bool npm_read = NPM_READ;
	bool bmx280_read = BMX280_READ;
	bool mhz16_read = MHZ16_READ;
	bool mhz19_read = MHZ19_READ;
	bool ccs811_read = CCS811_READ;
	bool enveano2_read = ENVEANO2_READ;

	// Location

	char latitude[LEN_GEOCOORDINATES];
	char longitude[LEN_GEOCOORDINATES];

	char height_above_sealevel[8] = "0";

	unsigned utc_offset = UTC_OFFSET;  //AJOUTER dans l'interface

	// send to "APIs"
	bool send2dusti = SEND2SENSORCOMMUNITY;
	bool send2madavi = SEND2MADAVI;
	bool send2custom = SEND2CUSTOM;
	bool send2custom2 = SEND2CUSTOM2;
	bool send2csv = SEND2CSV;

	// (in)active displays
	bool has_matrix = HAS_MATRIX;
	bool display_measure = DISPLAY_MEASURE;
	bool display_forecast = DISPLAY_FORECAST;
	bool display_wifi_info = DISPLAY_WIFI_INFO;
	bool display_lora_info = DISPLAY_LORA_INFO;
	bool display_device_info = DISPLAY_DEVICE_INFO;

	// API settings
	bool ssl_madavi = SSL_MADAVI;
	bool ssl_dusti = SSL_SENSORCOMMUNITY;

	// API AirCarto
	char host_custom[LEN_HOST_CUSTOM];
	char url_custom[LEN_URL_CUSTOM];
	bool ssl_custom = SSL_CUSTOM;
	unsigned port_custom = PORT_CUSTOM;
	char user_custom[LEN_USER_CUSTOM] = USER_CUSTOM;
	char pwd_custom[LEN_CFG_PASSWORD] = PWD_CUSTOM;

	// API AtmoSud
	char host_custom2[LEN_HOST_CUSTOM2];
	char url_custom2[LEN_URL_CUSTOM2];
	bool ssl_custom2 = SSL_CUSTOM2;
	unsigned port_custom2 = PORT_CUSTOM2;
	char user_custom2[LEN_USER_CUSTOM2] = USER_CUSTOM2;
	char pwd_custom2[LEN_CFG_PASSWORD] = PWD_CUSTOM2;

	// NebuleAir
	bool show_nebuleair = SHOW_NEBULEAIR;
	char id_nebuleair[LEN_ID] = ID_NEBULEAIR;


	bool rgpd = RGPD;

	// First load
	void initNonTrivials(const char *id)
	{
		strcpy(cfg::current_lang, CURRENT_LANG);
		strcpy_P(appeui, APPEUI);
		strcpy_P(deveui, DEVEUI);
		strcpy_P(appkey, APPKEY);
		strcpy_P(www_username, WWW_USERNAME);
		strcpy_P(www_password, WWW_PASSWORD);
		strcpy_P(wlanssid, WLANSSID);
		strcpy_P(wlanpwd, WLANPWD);
		strcpy_P(host_custom, HOST_CUSTOM);
		strcpy_P(url_custom, URL_CUSTOM);
		strcpy_P(host_custom2, HOST_CUSTOM2);
		strcpy_P(url_custom2, URL_CUSTOM2);
		strcpy_P(latitude, LATITUDE);
		strcpy_P(longitude, LONGITUDE);
		// strcpy_P(id_nebuleair, ID_NEBULEAIR);

		if (!*fs_ssid)
		{
			strcpy(fs_ssid, SSID_BASENAME);
			strcat(fs_ssid, id);
		}
	}
}

bool spiffs_matrix;

//configuration summary for LoRaWAN

bool configlorawan[8] = {false, false, false, false, false, false, false, false};

// configlorawan[0] = cfg::sds_read;
// configlorawan[1] = cfg::npm_read ;
// configlorawan[2] = cfg::bmx280_read;
// configlorawan[3] = cfg::mhz16_read;
// configlorawan[4] = cfg::mhz19_read;
// configlorawan[5] = cfg::ccs811_read;
// configlorawan[6] = cfg::display_forecast;
// configlorawan[7] = cfg::has_wifi;

static byte booltobyte(bool array[8])
{

	byte result = 0;
	for (int i = 0; i < 8; i++)
	{
		if (array[i])
		{
			result |= (byte)(1 << (7 - i));
		}
	}

	return result;
}

// define size of the config JSON
#define JSON_BUFFER_SIZE 2300
// define size of the AtmoSud Forecast API JSON
#define JSON_BUFFER_SIZE2 500
// define size of I2C JSON
#define JSON_BUFFER_SIZE3 500

LoggerConfig loggerConfigs[LoggerCount];

// test variables
long int sample_count = 0;
bool moduleair_selftest_failed = false;

WebServer server(80);

// include JSON config reader
#include "./moduleair-cfg.h"

/*****************************************************************
 * GPS coordinates                                              *
 *****************************************************************/

struct gps
{
	String latitude;
	String longitude;
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

uint8_t arrayDownlink[5];
uint8_t forecast_selector;

/*****************************************************************
 * Time                                       *
 *****************************************************************/

//RTC

RTC_DS3231 rtc;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

DateTime now; //get time
DateTime now2;
// time management varialbles
bool send_now = false;
unsigned long starttime;
unsigned long time_end_setup;
unsigned long time_before_config;
int prec;
unsigned long time_point_device_start_ms;
unsigned long starttime_SDS;
unsigned long starttime_NPM;
unsigned long starttime_MHZ16;
unsigned long starttime_MHZ19;
unsigned long starttime_CCS811;
unsigned long last_NPM;
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
 * Data variables                                      *
 *****************************************************************/
float last_value_BMX280_T = -128.0;
float last_value_BMX280_P = -1.0;
float last_value_BME280_H = -1.0;

uint32_t sds_pm10_sum = 0;
uint32_t sds_pm25_sum = 0;
uint32_t sds_val_count = 0;
uint32_t sds_pm10_max = 0;
uint32_t sds_pm10_min = 20000;
uint32_t sds_pm25_max = 0;
uint32_t sds_pm25_min = 20000;

uint32_t npm_pm1_sum = 0;
uint32_t npm_pm10_sum = 0;
uint32_t npm_pm25_sum = 0;
uint32_t npm_pm1_sum_pcs = 0;
uint32_t npm_pm10_sum_pcs = 0;
uint32_t npm_pm25_sum_pcs = 0;
uint16_t npm_val_count = 0;

float last_value_SDS_P1 = -1.0;
float last_value_SDS_P2 = -1.0;
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

String last_data_string;
int last_signal_strength;
int last_disconnect_reason;
// int last_connect_reason;

String esp_chipid;

String last_value_SDS_version;
String last_value_NPM_version;

unsigned long SDS_error_count;
unsigned long NPM_error_count;
unsigned long MHZ16_error_count;
unsigned long MHZ19_error_count;
unsigned long CCS811_error_count;
unsigned long WiFi_error_count;
unsigned long Cairsens_error_count;

unsigned long last_page_load = millis();

bool wificonfig_loop = false;
uint8_t sntp_time_set;

unsigned long count_sends = 0;
unsigned long last_display_millis_oled = 0;
unsigned long last_display_millis_matrix = 0;
uint8_t next_display_count = 0;

struct struct_wifiInfo
{
	char ssid[LEN_WLANSSID];
	uint8_t encryptionType;
	int32_t RSSI;
	int32_t channel;
};

struct struct_wifiInfo *wifiInfo;
uint8_t count_wifiInfo;

#define msSince(timestamp_before) (act_milli - (timestamp_before))

const char data_first_part[] PROGMEM = "{\"software_version\": \"" SOFTWARE_VERSION_STR "\", \"sensordatavalues\":[";
const char JSON_SENSOR_DATA_VALUES[] PROGMEM = "sensordatavalues";

static String displayGenerateFooter(unsigned int screen_count)
{
	String display_footer;
	for (unsigned int i = 0; i < screen_count; ++i)
	{
		display_footer += (i != (next_display_count % screen_count)) ? " . " : " o ";
	}
	return display_footer;
}

/*****************************************************************
 * write config to spiffs                                        *
 *****************************************************************/
static bool writeConfig()
{

	Debug.print("cfg::has_matrix: ");
	Debug.println(cfg::has_matrix);

	DynamicJsonDocument json(JSON_BUFFER_SIZE);
	debug_outln_info(F("Saving config..."));
	json["SOFTWARE_VERSION"] = SOFTWARE_VERSION;

	for (unsigned e = 0; e < sizeof(configShape) / sizeof(configShape[0]); ++e)
	{
		ConfigShapeEntry c;
		memcpy_P(&c, &configShape[e], sizeof(ConfigShapeEntry));
		switch (c.cfg_type)
		{
		case Config_Type_Bool:
			json[c.cfg_key()].set(*c.cfg_val.as_bool);
			break;
		case Config_Type_UInt:
		case Config_Type_Time:
			json[c.cfg_key()].set(*c.cfg_val.as_uint);
			break;
		case Config_Type_Password:
		case Config_Type_Hex:
		case Config_Type_String:
			json[c.cfg_key()].set(c.cfg_val.as_str);
			break;
		};
	}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

	SPIFFS.remove(F("/config.json.old"));
	SPIFFS.rename(F("/config.json"), F("/config.json.old"));

	File configFile = SPIFFS.open(F("/config.json"), "w");
	if (configFile)
	{
		serializeJsonPretty(json, Debug);
		serializeJson(json, configFile);
		configFile.close();
		debug_outln_info(F("Config written successfully."));
	}
	else
	{
		debug_outln_error(F("failed to open config file for writing"));
		return false;
	}

#pragma GCC diagnostic pop

	return true;
}

/*****************************************************************
 * read config from spiffs                                       *
 *****************************************************************/

/* backward compatibility for the times when we stored booleans as strings */
static bool boolFromJSON(const DynamicJsonDocument &json, const __FlashStringHelper *key)
{
	if (json[key].is<const char *>())
	{
		return !strcmp_P(json[key].as<const char *>(), PSTR("true"));
	}
	return json[key].as<bool>();
}

static void readConfig(bool oldconfig = false)
{
	bool rewriteConfig = false;

	String cfgName(F("/config.json"));
	if (oldconfig)
	{
		cfgName += F(".old");
	}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
	File configFile = SPIFFS.open(cfgName, "r");
	if (!configFile)
	{
		if (!oldconfig)
		{
			return readConfig(true /* oldconfig */);
		}

		debug_outln_error(F("failed to open config file."));
		return;
	}

	debug_outln_info(F("opened config file..."));
	DynamicJsonDocument json(JSON_BUFFER_SIZE);
	DeserializationError err = deserializeJson(json, configFile.readString());
	configFile.close();
#pragma GCC diagnostic pop

	if (!err)
	{
		serializeJsonPretty(json, Debug);
		debug_outln_info(F("parsed json..."));
		for (unsigned e = 0; e < sizeof(configShape) / sizeof(configShape[0]); ++e)
		{
			ConfigShapeEntry c;
			memcpy_P(&c, &configShape[e], sizeof(ConfigShapeEntry));
			if (json[c.cfg_key()].isNull())
			{
				continue;
			}
			switch (c.cfg_type)
			{
			case Config_Type_Bool:
				*(c.cfg_val.as_bool) = boolFromJSON(json, c.cfg_key());
				break;
			case Config_Type_UInt:
			case Config_Type_Time:
				*(c.cfg_val.as_uint) = json[c.cfg_key()].as<unsigned int>();
				break;
			case Config_Type_String:
			case Config_Type_Hex:
			case Config_Type_Password:
				strncpy(c.cfg_val.as_str, json[c.cfg_key()].as<const char *>(), c.cfg_len);
				c.cfg_val.as_str[c.cfg_len] = '\0';
				break;
			};
		}
		String writtenVersion(json["SOFTWARE_VERSION"].as<const char *>());
		if (writtenVersion.length() && writtenVersion[0] == 'N' && SOFTWARE_VERSION != writtenVersion)
		{
			debug_outln_info(F("Rewriting old config from: "), writtenVersion);
			// would like to do that, but this would wipe firmware.old which the two stage loader
			// might still need
			// SPIFFS.format();
			rewriteConfig = true;
		}
		if (cfg::sending_intervall_ms < READINGTIME_SDS_MS)
		{
			cfg::sending_intervall_ms = READINGTIME_SDS_MS;
		}
		if (boolFromJSON(json, F("bmp280_read")) || boolFromJSON(json, F("bme280_read")))
		{
			cfg::bmx280_read = true;
			rewriteConfig = true;
		}
	}
	else
	{
		debug_outln_error(F("failed to load json config"));

		if (!oldconfig)
		{
			return readConfig(true /* oldconfig */);
		}
	}

	if (rewriteConfig)
	{
		writeConfig();
	}
}

static void init_config()
{

	debug_outln_info(F("mounting FS..."));

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

	bool spiffs_begin_ok = SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED);

#pragma GCC diagnostic pop

	if (!spiffs_begin_ok)
	{
		debug_outln_error(F("failed to mount FS"));
		return;
	}
	readConfig();
}

/*****************************************************************
 * Prepare information for data Loggers                          *
 *****************************************************************/
static void createLoggerConfigs()
{
	loggerConfigs[LoggerSensorCommunity].destport = PORT_DUSTI;
	if (cfg::send2dusti && cfg::ssl_dusti)
	{
		loggerConfigs[LoggerSensorCommunity].destport = 443;
	}

	loggerConfigs[LoggerMadavi].destport = PORT_MADAVI;
	if (cfg::send2madavi && cfg::ssl_madavi)
	{
		loggerConfigs[LoggerMadavi].destport = 443;
	}

	loggerConfigs[LoggerCustom].destport = cfg::port_custom;
	if (cfg::send2custom && cfg::ssl_custom)
	{
		loggerConfigs[LoggerCustom].destport = 443;
	}
	loggerConfigs[LoggerCustom2].destport = cfg::port_custom2;
	if (cfg::send2custom2 && cfg::ssl_custom2)
	{
		loggerConfigs[LoggerCustom2].destport = 443;
	}
}



/*****************************************************************
 * html helper functions                                         *
 *****************************************************************/
static void start_html_page(String &page_content, const String &title)
{
	last_page_load = millis();

	RESERVE_STRING(s, LARGE_STR);
	s = FPSTR(WEB_PAGE_HEADER);
	s.replace("{t}", title);
	server.setContentLength(CONTENT_LENGTH_UNKNOWN);
	server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), s);

	server.sendContent_P(WEB_PAGE_HEADER_HEAD);

	s = FPSTR(WEB_PAGE_HEADER_BODY);
	s.replace("{t}", title);
	if (title != " ")
	{
		s.replace("{n}", F("&raquo;"));
	}
	else
	{
		s.replace("{n}", emptyString);
	}
	s.replace("{id}", esp_chipid);
	// s.replace("{macid}", esp_mac_id);
	// s.replace("{mac}", WiFi.macAddress());
	page_content += s;
}

static void end_html_page(String &page_content)
{
	if (page_content.length())
	{
		server.sendContent(page_content);
	}
	server.sendContent_P(WEB_PAGE_FOOTER);
}

static void add_form_input(String &page_content, const ConfigShapeId cfgid, const __FlashStringHelper *info, const int length)
{
	RESERVE_STRING(s, MED_STR);
	s = F("<tr>"
		  "<td title='[&lt;= {l}]'>{i}:&nbsp;</td>"
		  "<td style='width:{l}em'>"
		  "<input form='main' type='{t}' name='{n}' id='{n}' placeholder='{i}' value='{v}' maxlength='{l}'/>"
		  "</td></tr>");
	String t_value;
	ConfigShapeEntry c;
	memcpy_P(&c, &configShape[cfgid], sizeof(ConfigShapeEntry));
	switch (c.cfg_type)
	{
	case Config_Type_UInt:
		t_value = String(*c.cfg_val.as_uint);
		s.replace("{t}", F("number"));
		break;
	case Config_Type_Time:
		t_value = String((*c.cfg_val.as_uint) / 1000);
		s.replace("{t}", F("number"));
		break;
	case Config_Type_Password:
		s.replace("{t}", F("text"));
		info = FPSTR(INTL_PASSWORD);
	case Config_Type_Hex:
		s.replace("{t}", F("hex"));
	default:
		t_value = c.cfg_val.as_str;
		t_value.replace("'", "&#39;");
		s.replace("{t}", F("text"));
	}
	s.replace("{i}", info);
	s.replace("{n}", String(c.cfg_key()));
	s.replace("{v}", t_value);
	s.replace("{l}", String(length));
	page_content += s;
}

static String form_checkbox(const ConfigShapeId cfgid, const String &info, const bool linebreak)
{
	RESERVE_STRING(s, MED_STR);
	s = F("<label for='{n}'>"
		  "<input form='main' type='checkbox' name='{n}' value='1' id='{n}' {c}/>"
		  "<input form='main' type='hidden' name='{n}' value='0'/>"
		  "{i}</label><br/>");
	if (*configShape[cfgid].cfg_val.as_bool)
	{
		s.replace("{c}", F(" checked='checked'"));
	}
	else
	{
		s.replace("{c}", emptyString);
	};
	s.replace("{i}", info);
	s.replace("{n}", String(configShape[cfgid].cfg_key()));
	if (!linebreak)
	{
		s.replace("<br/>", emptyString);
	}
	return s;
}

static String form_submit(const String &value)
{
	String s = F("<tr>"
				 "<td>&nbsp;</td>"
				 "<td>"
				 "<input form='main' type='submit' name='submit' value='{v}' />"
				 "</td>"
				 "</tr>");
	s.replace("{v}", value);
	return s;
}

static String form_select_lang()
{
	String s_select = F(" selected='selected'");
	String s = F("<tr>"
				 "<td>" INTL_LANGUAGE ":&nbsp;</td>"
				 "<td>"
				 "<select form='main' id='current_lang' name='current_lang'>"
				 "<option value='FR'>Français (FR)</option>"
				 "<option value='EN'>English (EN)</option>"
				 "</select>"
				 "</td>"
				 "</tr>");

	s.replace("'" + String(cfg::current_lang) + "'>", "'" + String(cfg::current_lang) + "'" + s_select + ">");
	return s;
}

static void add_warning_first_cycle(String &page_content)
{
	String s = FPSTR(INTL_TIME_TO_FIRST_MEASUREMENT);
	unsigned int time_to_first = cfg::sending_intervall_ms - msSince(starttime);
	if (time_to_first > cfg::sending_intervall_ms)
	{
		time_to_first = 0;
	}
	s.replace("{v}", String(((time_to_first + 500) / 1000)));
	page_content += s;
}

static void add_age_last_values(String &s)
{
	s += "<b>";
	unsigned int time_since_last = msSince(starttime);
	if (time_since_last > cfg::sending_intervall_ms)
	{
		time_since_last = 0;
	}
	s += String((time_since_last + 500) / 1000);
	s += FPSTR(INTL_TIME_SINCE_LAST_MEASUREMENT);
	s += FPSTR(WEB_B_BR_BR);
}

/*****************************************************************
 * Webserver request auth: prompt for BasicAuth
 *
 * -Provide BasicAuth for all page contexts except /values and images
 *****************************************************************/
static bool webserver_request_auth()
{
	if (cfg::www_basicauth_enabled && !wificonfig_loop)
	{
		debug_outln_info(F("validate request auth..."));
		if (!server.authenticate(cfg::www_username, cfg::www_password))
		{
			server.requestAuthentication(BASIC_AUTH, "Sensor Login", F("Authentication failed"));
			return false;
		}
	}
	return true;
}

static void sendHttpRedirect()
{
	server.sendHeader(F("Location"), F("http://192.168.4.1/config"));
	server.send(302, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), emptyString);
}

/*****************************************************************
 * Webserver root: show all options                              *
 *****************************************************************/
static void webserver_root()
{
	// Reactivate the interrupt to turn on the matrix if the user return to homepage

	if (WiFi.status() != WL_CONNECTED)
	{
		sendHttpRedirect();
	}
	else
	{
		if (!webserver_request_auth())
		{
			return;
		}

		RESERVE_STRING(page_content, XLARGE_STR);
		start_html_page(page_content, emptyString);
		debug_outln_info(F("ws: root ..."));

		// Enable Pagination
		page_content += FPSTR(WEB_ROOT_PAGE_CONTENT);
		page_content.replace(F("{t}"), FPSTR(INTL_CURRENT_DATA));
		page_content.replace(F("{s}"), FPSTR(INTL_DEVICE_STATUS));
		page_content.replace(F("{conf}"), FPSTR(INTL_CONFIGURATION));
		page_content.replace(F("{restart}"), FPSTR(INTL_RESTART_SENSOR));
		page_content.replace(F("{debug}"), FPSTR(INTL_DEBUG_LEVEL));
		end_html_page(page_content);
	}
}

/*****************************************************************
 * Webserver config: show config page                            *
 *****************************************************************/
static void webserver_config_send_body_get(String &page_content)
{
	auto add_form_checkbox = [&page_content](const ConfigShapeId cfgid, const String &info)
	{
		page_content += form_checkbox(cfgid, info, true);
	};

	auto add_form_checkbox_sensor = [&add_form_checkbox](const ConfigShapeId cfgid, __const __FlashStringHelper *info)
	{
		add_form_checkbox(cfgid, add_sensor_type(info));
	};

	debug_outln_info(F("begin webserver_config_body_get ..."));
	page_content += F("<form id='main' method='POST' action='/config' style='width:100%;'>\n"
					  "<input form='main' class='radio' id='r1' name='group' type='radio' checked>"
					  "<input form='main' class='radio' id='r2' name='group' type='radio'>"
					  "<input form='main' class='radio' id='r3' name='group' type='radio'>"
					  "<input form='main' class='radio' id='r4' name='group' type='radio'>"
					  "<input form='main' class='radio' id='r5' name='group' type='radio'>"
					  "<input form='main' class='radio' id='r6' name='group' type='radio'>"
					  "<input form='main' class='radio' id='r7' name='group' type='radio'>"
					  "<input form='main' class='radio' id='r8' name='group' type='radio'>"
					  "<div class='tabs'>"
					  "<label class='tab' id='tab1' for='r1'>");
	page_content += FPSTR(INTL_WIFI_SETTINGS);
	page_content += F("</label>"
					  "<label class='tab' id='tab2' for='r2'>");
	page_content += FPSTR(INTL_LORA_SETTINGS);
	page_content += F("</label>"
					  "<label class='tab' id='tab3' for='r3'>");
	page_content += FPSTR(INTL_ETHERNET_SETTINGS);
	page_content += F("</label>"
					  "<label class='tab' id='tab4' for='r4'>");
	page_content += FPSTR(INTL_MORE_SETTINGS);
	page_content += F("</label>"
					  "<label class='tab' id='tab5' for='r5'>");
	page_content += FPSTR(INTL_SENSORS);
	page_content += F("</label>"
					  "<label class='tab' id='tab6' for='r6'>");
	page_content += FPSTR(INTL_APIS);
	page_content += F("</label>"
					  "<label class='tab' id='tab7' for='r7'>");
	page_content += FPSTR(INTL_DATE_TIME);
	page_content += F("</label>"
					  "<label class='tab' id='tab8' for='r8'>");
	page_content += FPSTR(INTL_RGPD);
	page_content += F("</label></div>"
					  "<div class='panels'>"
					  "<div class='panel' id='panel1'>");

	if (wificonfig_loop)
	{ // scan for wlan ssids
		page_content += F("<div id='wifilist'>" INTL_WIFI_NETWORKS "</div><br/>");
	}
	add_form_checkbox(Config_has_wifi, FPSTR(INTL_WIFI_ACTIVATION));
	page_content += FPSTR(TABLE_TAG_OPEN);
	add_form_input(page_content, Config_wlanssid, FPSTR(INTL_FS_WIFI_NAME), LEN_WLANSSID - 1);
	add_form_input(page_content, Config_wlanpwd, FPSTR(INTL_PASSWORD), LEN_CFG_PASSWORD - 1);
	page_content += FPSTR(TABLE_TAG_CLOSE_BR);
	page_content += F("<hr/>\n<br/><b>");

	page_content += FPSTR(INTL_AB_HIER_NUR_ANDERN);
	page_content += FPSTR(WEB_B_BR);
	page_content += FPSTR(BR_TAG);

	// Paginate page after ~ 1500 Bytes
	server.sendContent(page_content);
	page_content = emptyString;

	add_form_checkbox(Config_www_basicauth_enabled, FPSTR(INTL_BASICAUTH));
	page_content += FPSTR(TABLE_TAG_OPEN);
	add_form_input(page_content, Config_www_username, FPSTR(INTL_USER), LEN_WWW_USERNAME - 1);
	add_form_input(page_content, Config_www_password, FPSTR(INTL_PASSWORD), LEN_CFG_PASSWORD - 1);
	page_content += FPSTR(TABLE_TAG_CLOSE_BR);
	page_content += FPSTR(BR_TAG);

	// Paginate page after ~ 1500 Bytes
	server.sendContent(page_content);

	if (!wificonfig_loop)
	{
		page_content = FPSTR(INTL_FS_WIFI_DESCRIPTION);
		page_content += FPSTR(BR_TAG);

		page_content += FPSTR(TABLE_TAG_OPEN);
		add_form_input(page_content, Config_fs_ssid, FPSTR(INTL_FS_WIFI_NAME), LEN_FS_SSID - 1);
		add_form_input(page_content, Config_fs_pwd, FPSTR(INTL_PASSWORD), LEN_CFG_PASSWORD - 1);
		page_content += FPSTR(TABLE_TAG_CLOSE_BR);

		// Paginate page after ~ 1500 Bytes
		server.sendContent(page_content);
	}

	page_content = tmpl(FPSTR(WEB_DIV_PANEL), String(2));
	page_content += FPSTR(WEB_LF_B);
	page_content += FPSTR(INTL_LORA_EXPLANATION);
	page_content += FPSTR(WEB_B_BR_BR);
	add_form_checkbox(Config_has_lora, FPSTR(INTL_LORA_ACTIVATION));
	page_content += FPSTR(TABLE_TAG_OPEN);
	add_form_input(page_content, Config_appeui, FPSTR("APPEUI"), LEN_APPEUI - 1);
	add_form_input(page_content, Config_deveui, FPSTR("DEVEUI"), LEN_DEVEUI - 1);
	add_form_input(page_content, Config_appkey, FPSTR("APPKEY"), LEN_APPKEY - 1);
	page_content += FPSTR(TABLE_TAG_CLOSE_BR);
	server.sendContent(page_content);

	page_content = tmpl(FPSTR(WEB_DIV_PANEL), String(3));
	add_form_checkbox(Config_has_lora, FPSTR(INTL_ETHERNET_ACTIVATION));
	server.sendContent(page_content);

	page_content = tmpl(FPSTR(WEB_DIV_PANEL), String(4));
	page_content += F("<b>" INTL_LOCATION "</b>&nbsp;");
	page_content += FPSTR(TABLE_TAG_OPEN);
	add_form_input(page_content, Config_latitude, FPSTR(INTL_LATITUDE), LEN_GEOCOORDINATES - 1);
	add_form_input(page_content, Config_longitude, FPSTR(INTL_LONGITUDE), LEN_GEOCOORDINATES - 1);
	add_form_input(page_content, Config_height_above_sealevel, FPSTR(INTL_HEIGHT_ABOVE_SEALEVEL), LEN_HEIGHT_ABOVE_SEALEVEL - 1);
	page_content += FPSTR(TABLE_TAG_CLOSE_BR);

	// Paginate page after ~ 1500 Bytes

	server.sendContent(page_content);
	page_content = emptyString;

	page_content = FPSTR(WEB_BR_LF_B);
	page_content += F(INTL_DISPLAY);
	page_content += FPSTR(WEB_B_BR);
	add_form_checkbox(Config_has_matrix, FPSTR(INTL_MATRIX));
	add_form_checkbox(Config_display_measure, FPSTR(INTL_DISPLAY_MEASURES));
	add_form_checkbox(Config_display_forecast, FPSTR(INTL_DISPLAY_FORECAST));
	add_form_checkbox(Config_display_wifi_info, FPSTR(INTL_DISPLAY_WIFI_INFO));
	add_form_checkbox(Config_display_lora_info, FPSTR(INTL_DISPLAY_LORA_INFO));
	add_form_checkbox(Config_display_device_info, FPSTR(INTL_DISPLAY_DEVICE_INFO));

	page_content = FPSTR(WEB_BR_LF_B);
	page_content += F(INTL_NEBULEAIR);
	page_content += FPSTR(WEB_B_BR);
	add_form_checkbox(Config_show_nebuleair, FPSTR(INTL_SHOW_NEBULEAIR));
	page_content += FPSTR(TABLE_TAG_OPEN);
	add_form_input(page_content, Config_id_nebuleair, FPSTR(INTL_ID_NEBULEAIR), LEN_ID - 1);
	page_content += FPSTR(TABLE_TAG_CLOSE_BR);
	server.sendContent(page_content);

	page_content = FPSTR(WEB_BR_LF_B);
	page_content += F(INTL_FIRMWARE "</b>&nbsp;");

	page_content += FPSTR(TABLE_TAG_OPEN);
	page_content += form_select_lang();
	page_content += FPSTR(TABLE_TAG_CLOSE_BR);

	page_content += FPSTR(TABLE_TAG_OPEN);
	add_form_input(page_content, Config_debug, FPSTR(INTL_DEBUG_LEVEL), 1);
	add_form_input(page_content, Config_sending_intervall_ms, FPSTR(INTL_MEASUREMENT_INTERVAL), 5);
	add_form_input(page_content, Config_time_for_wifi_config, FPSTR(INTL_DURATION_ROUTER_MODE), 5);
	page_content += FPSTR(TABLE_TAG_CLOSE_BR);

	server.sendContent(page_content);


	page_content = tmpl(FPSTR(WEB_DIV_PANEL), String(5));

	page_content += FPSTR("<b>");
	page_content += FPSTR(INTL_PM_SENSORS);
	page_content += FPSTR(WEB_B_BR);
	add_form_checkbox_sensor(Config_sds_read, FPSTR(INTL_SDS011));
	add_form_checkbox_sensor(Config_npm_read, FPSTR(INTL_NPM));

	// Paginate page after ~ 1500 Bytes  //ATTENTION RYTHME PAGINATION !
	server.sendContent(page_content);
	page_content = emptyString;

	page_content += FPSTR(WEB_BR_LF_B);
	page_content += FPSTR(INTL_THP_SENSORS);
	page_content += FPSTR(WEB_B_BR);

	add_form_checkbox_sensor(Config_bmx280_read, FPSTR(INTL_BMX280));

	// // Paginate page after ~ 1500 Bytes
	// server.sendContent(page_content);
	// page_content = emptyString;

	page_content += FPSTR(WEB_BR_LF_B);
	page_content += FPSTR(INTL_CO2_SENSORS);
	page_content += FPSTR(WEB_B_BR);

	add_form_checkbox_sensor(Config_mhz16_read, FPSTR(INTL_MHZ16));
	add_form_checkbox_sensor(Config_mhz19_read, FPSTR(INTL_MHZ19));

	// // Paginate page after ~ 1500 Bytes
	server.sendContent(page_content);
	page_content = emptyString;

	page_content += FPSTR(WEB_BR_LF_B);
	page_content += FPSTR(INTL_VOC_SENSORS);
	page_content += FPSTR(WEB_B_BR);

	add_form_checkbox_sensor(Config_ccs811_read, FPSTR(INTL_CCS811));


	page_content += FPSTR(WEB_BR_LF_B);
	page_content += FPSTR(INTL_NO2_SENSORS);
	page_content += FPSTR(WEB_B_BR);

	add_form_checkbox_sensor(Config_enveano2_read, FPSTR(INTL_ENVEANO2));


	// Paginate page after ~ 1500 Bytes
	server.sendContent(page_content);
	//page_content = emptyString;

	page_content = tmpl(FPSTR(WEB_DIV_PANEL), String(6));

	// //page_content += tmpl(FPSTR(INTL_SEND_TO), F("APIs"));
	// page_content += tmpl(FPSTR(INTL_SEND_TO), F(""));

	page_content += FPSTR("<b>");
	page_content += FPSTR(INTL_SEND_TO);
	page_content += FPSTR(WEB_B_BR);

	// page_content += FPSTR(BR_TAG);
	page_content += form_checkbox(Config_send2dusti, FPSTR(WEB_SENSORCOMMUNITY), false);
	page_content += FPSTR(WEB_NBSP_NBSP_BRACE);
	page_content += form_checkbox(Config_ssl_dusti, FPSTR(WEB_HTTPS), false);
	page_content += FPSTR(WEB_BRACE_BR);
	// page_content += FPSTR("<br>");
	page_content += form_checkbox(Config_send2madavi, FPSTR(WEB_MADAVI), false);
	page_content += FPSTR(WEB_NBSP_NBSP_BRACE);
	page_content += form_checkbox(Config_ssl_madavi, FPSTR(WEB_HTTPS), false);
	page_content += FPSTR(WEB_BRACE_BR);
	// page_content += FPSTR("<br>");

	add_form_checkbox(Config_send2csv, FPSTR(WEB_CSV));
	add_form_checkbox(Config_has_sdcard, FPSTR(WEB_SD));

	server.sendContent(page_content);
	page_content = emptyString;

	page_content += FPSTR(BR_TAG);
	page_content += form_checkbox(Config_send2custom, FPSTR(INTL_SEND_TO_OWN_API), false);
	page_content += FPSTR(WEB_NBSP_NBSP_BRACE);
	page_content += form_checkbox(Config_ssl_custom, FPSTR(WEB_HTTPS), false);
	page_content += FPSTR(WEB_BRACE_BR);
	server.sendContent(page_content);
	page_content = FPSTR(TABLE_TAG_OPEN);
	add_form_input(page_content, Config_host_custom, FPSTR(INTL_SERVER), LEN_HOST_CUSTOM - 1);
	add_form_input(page_content, Config_url_custom, FPSTR(INTL_PATH), LEN_URL_CUSTOM - 1);
	add_form_input(page_content, Config_port_custom, FPSTR(INTL_PORT), MAX_PORT_DIGITS);
	add_form_input(page_content, Config_user_custom, FPSTR(INTL_USER), LEN_USER_CUSTOM - 1);
	add_form_input(page_content, Config_pwd_custom, FPSTR(INTL_PASSWORD), LEN_CFG_PASSWORD - 1);
	page_content += FPSTR(TABLE_TAG_CLOSE_BR);
	page_content += FPSTR(BR_TAG);
	page_content += form_checkbox(Config_send2custom2, FPSTR(INTL_SEND_TO_OWN_API2), false);
	page_content += FPSTR(WEB_NBSP_NBSP_BRACE);
	page_content += form_checkbox(Config_ssl_custom2, FPSTR(WEB_HTTPS), false);
	page_content += FPSTR(WEB_BRACE_BR);
	server.sendContent(page_content);
	page_content = FPSTR(TABLE_TAG_OPEN);
	add_form_input(page_content, Config_host_custom2, FPSTR(INTL_SERVER2), LEN_HOST_CUSTOM2 - 1);
	add_form_input(page_content, Config_url_custom2, FPSTR(INTL_PATH2), LEN_URL_CUSTOM2 - 1);
	add_form_input(page_content, Config_port_custom2, FPSTR(INTL_PORT2), MAX_PORT_DIGITS2);
	add_form_input(page_content, Config_user_custom2, FPSTR(INTL_USER2), LEN_USER_CUSTOM2 - 1);
	add_form_input(page_content, Config_pwd_custom2, FPSTR(INTL_PASSWORD2), LEN_CFG_PASSWORD2 - 1);
	page_content += FPSTR(TABLE_TAG_CLOSE_BR);

	server.sendContent(page_content);
	page_content = emptyString;

	page_content = tmpl(FPSTR(WEB_DIV_PANEL), String(7));
	page_content += F("<b>" INTL_RTC_TIME "</b>&nbsp;");
	page_content += F("<br/><br/>");
	now = rtc.now();
	String current_time;
	current_time += String(now.year(), DEC);
	current_time += "-";

	if (now.month() < 10)
	{
		current_time += "0";
	}
	current_time += String(now.month(), DEC);
	current_time += "-";

	if (now.day() < 10)
	{
		current_time += "0";
	}
	current_time += String(now.day(),DEC);

	current_time += "T";
	if (now.hour() < 10)
	{
		current_time += "0";
	}
	current_time += String(now.hour(), DEC);
	current_time += ":";

	if (now.minute() < 10)
	{
		current_time += "0";
	}
	current_time += String(now.minute(), DEC);
	current_time += ":";

	if (now.second() < 10)
	{
		current_time += "0";
	}
	current_time += String(now.second());
	current_time += "Z";

	page_content += current_time;	
	page_content += F("<br/><br/>");

	page_content += FPSTR(TABLE_TAG_OPEN);
page_content += F("<tr>"
          "<td title='[&lt;= 4]'>" INTL_YEAR ":&nbsp;</td>"
          "<td style='width:4em'>"
          "<input type='number' form='secondar' name='year' id='year' placeholder='Year' value='");	  
	page_content += String(now.year(), DEC);
	page_content += F("' maxlength='4'/>"
          "</td></tr>"
		  "<tr>"
          "<td title='[&lt;= 2]'>" INTL_MONTH ":&nbsp;</td>"
          "<td style='width:2em'>"
          "<input type='number' form='secondar' name='month' id='month' placeholder='Month' value='");
	page_content += String(now.month(), DEC);
	page_content += F("' maxlength='2'/>"
          "</td></tr>"
		  "<tr>"
          "<td title='[&lt;= 2]'>" INTL_DAY ":&nbsp;</td>"
          "<td style='width:2em'>"
          "<input type='number' form='secondar' name='day' id='day' placeholder='Day' value='");
	page_content += String(now.day(), DEC);
	page_content += F("' maxlength='2'/>"
          "</td></tr>"
		  "<tr>"
          "<td title='[&lt;= 2]'>" INTL_HOUR ":&nbsp;</td>"
          "<td style='width:2em'>"
          "<input type='number' form='secondar' name='hour' id='hour' placeholder='Hour' value='");
	page_content += String(now.hour(), DEC);  
	page_content += F("' maxlength='2'/>"
          "</td></tr>"
		  "<tr>"
          "<td title='[&lt;= 2]'>" INTL_MINUTE ":&nbsp;</td>"
          "<td style='width:2em'>"
          "<input type='number' form='secondar' name='minute' id='minute' placeholder='Minute' value='");
	page_content += String(now.minute(), DEC);
	page_content += F("' maxlength='2'/>"
          "</td></tr>"
		  "<tr>"
          "<td title='[&lt;= 2]'>" INTL_SECOND ":&nbsp;</td>"
          "<td style='width:2em'>"
          "<input type='number' form='secondar' name='second' id='second' placeholder='Second' value='");
	page_content += String(now.second(), DEC);
	page_content += F("' maxlength='2'/>"
          "</td></tr>");
	page_content += F("<tr><td>"
	"<input type='submit' form='secondar' class='s_red' name='submit' value='" INTL_SET_TIME "'/>"
	"</td></tr></table>");

	server.sendContent(page_content);
	page_content = emptyString;

	page_content = tmpl(FPSTR(WEB_DIV_PANEL), String(8));
		//AJOUTER TEXTE, LIEN etc.
	add_form_checkbox(Config_rgpd, FPSTR(INTL_RGPD_ACCEPT));

	// server.sendContent(page_content);
	// page_content = emptyString;

	page_content += F("</div></div>");
	page_content += form_submit(FPSTR(INTL_SAVE_AND_RESTART));
	page_content += FPSTR(BR_TAG);
	page_content += FPSTR(WEB_BR_FORM);
	page_content += F("<form id='secondar' method='POST' action='/settime'></form>");
	if (wificonfig_loop)
	{ // scan for wlan ssids
		page_content += F("<script>window.setTimeout(load_wifi_list,1000);</script>");
	}
	server.sendContent(page_content);
	page_content = emptyString;
}

static void webserver_config_send_body_post(String &page_content)
{
	String masked_pwd;

	for (unsigned e = 0; e < sizeof(configShape) / sizeof(configShape[0]); ++e)
	{
		ConfigShapeEntry c;
		memcpy_P(&c, &configShape[e], sizeof(ConfigShapeEntry));
		const String s_param(c.cfg_key());
		if (!server.hasArg(s_param))
		{
			continue;
		}
		const String server_arg(server.arg(s_param));

		switch (c.cfg_type)
		{
		case Config_Type_UInt:
			*(c.cfg_val.as_uint) = server_arg.toInt();
			break;
		case Config_Type_Time:
			*(c.cfg_val.as_uint) = server_arg.toInt() * 1000;
			break;
		case Config_Type_Bool:
			*(c.cfg_val.as_bool) = (server_arg == "1");
			break;
		case Config_Type_String:
			strncpy(c.cfg_val.as_str, server_arg.c_str(), c.cfg_len);
			c.cfg_val.as_str[c.cfg_len] = '\0';
			break;
		case Config_Type_Password:
			if (server_arg.length())
			{
				server_arg.toCharArray(c.cfg_val.as_str, LEN_CFG_PASSWORD);
			}
			break;
		case Config_Type_Hex:
			strncpy(c.cfg_val.as_str, server_arg.c_str(), c.cfg_len);
			c.cfg_val.as_str[c.cfg_len] = '\0';
			break;
		}
	}

	page_content += FPSTR(INTL_SENSOR_IS_REBOOTING);

	server.sendContent(page_content);
	page_content = emptyString;
}

static void sensor_restart()
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

	SPIFFS.end();

#pragma GCC diagnostic pop

	debug_outln_info(F("Restart."));
	delay(500);
	ESP.restart();
	// should not be reached
	while (true)
	{
		yield();
	}
}

static void webserver_config()
{
	if (WiFi.getMode() == WIFI_MODE_STA)
	{
		debug_outln_info(F("STA"));
	}

	if (WiFi.getMode() == WIFI_MODE_AP)
	{
		debug_outln_info(F("AP"));
	}

	if (!webserver_request_auth())
	{
		return;
	}

	debug_outln_info(F("ws: config page ..."));

	server.sendHeader(F("Cache-Control"), F("no-cache, no-store, must-revalidate"));
	server.sendHeader(F("Pragma"), F("no-cache"));
	server.sendHeader(F("Expires"), F("0"));
	// Enable Pagination (Chunked Transfer)
	server.setContentLength(CONTENT_LENGTH_UNKNOWN);

	RESERVE_STRING(page_content, XLARGE_STR);

	start_html_page(page_content, FPSTR(INTL_CONFIGURATION));
	if (wificonfig_loop)
	{ // scan for wlan ssids
		page_content += FPSTR(WEB_CONFIG_SCRIPT);
	}

	if (server.method() == HTTP_GET)
	{
		webserver_config_send_body_get(page_content);
	}
	else
	{
		webserver_config_send_body_post(page_content);
	}
	end_html_page(page_content);

	if (server.method() == HTTP_POST)
	{

		if (writeConfig())
		{
			sensor_restart();
		}
	}
}

/*****************************************************************
 * Webserver wifi: show available wifi networks                  *
 *****************************************************************/
static void webserver_wifi()
{
	String page_content;

	debug_outln_info(F("wifi networks found: "), String(count_wifiInfo));
	if (count_wifiInfo == 0)
	{
		page_content += FPSTR(BR_TAG);
		page_content += FPSTR(INTL_NO_NETWORKS);
		page_content += FPSTR(BR_TAG);
	}
	else
	{
		std::unique_ptr<int[]> indices(new int[count_wifiInfo]);
		debug_outln_info(F("ws: wifi ..."));
		for (unsigned i = 0; i < count_wifiInfo; ++i)
		{
			indices[i] = i;
		}
		for (unsigned i = 0; i < count_wifiInfo; i++)
		{
			for (unsigned j = i + 1; j < count_wifiInfo; j++)
			{
				if (wifiInfo[indices[j]].RSSI > wifiInfo[indices[i]].RSSI)
				{
					std::swap(indices[i], indices[j]);
				}
			}
		}
		int duplicateSsids = 0;
		for (int i = 0; i < count_wifiInfo; i++)
		{
			if (indices[i] == -1)
			{
				continue;
			}
			for (int j = i + 1; j < count_wifiInfo; j++)
			{
				if (strncmp(wifiInfo[indices[i]].ssid, wifiInfo[indices[j]].ssid, sizeof(wifiInfo[0].ssid)) == 0)
				{
					indices[j] = -1; // set dup aps to index -1
					++duplicateSsids;
				}
			}
		}

		page_content += FPSTR(INTL_NETWORKS_FOUND);
		page_content += String(count_wifiInfo - duplicateSsids);
		page_content += FPSTR(BR_TAG);
		page_content += FPSTR(BR_TAG);
		page_content += FPSTR(TABLE_TAG_OPEN);
		// if (n > 30) n=30;
		for (int i = 0; i < count_wifiInfo; ++i)
		{
			if (indices[i] == -1)
			{
				continue;
			}
			// Print SSID and RSSI for each network found
			page_content += wlan_ssid_to_table_row(wifiInfo[indices[i]].ssid, ((wifiInfo[indices[i]].encryptionType == WIFI_AUTH_OPEN) ? " " : u8"🔒"), wifiInfo[indices[i]].RSSI);
		}
		page_content += FPSTR(TABLE_TAG_CLOSE_BR);
		page_content += FPSTR(BR_TAG);
	}
	server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), page_content);
}

/*****************************************************************
 * Webserver root: show latest values                            *
 *****************************************************************/
static void webserver_values()
{

	if (WiFi.status() != WL_CONNECTED)
	{
		sendHttpRedirect();
		return;
	}

	RESERVE_STRING(page_content, XLARGE_STR);
	start_html_page(page_content, FPSTR(INTL_CURRENT_DATA));
	const String unit_Deg("°");
	const String unit_P("hPa");
	const String unit_T("°C");
	const String unit_CO2("ppm");
	const String unit_COV("ppb");
	const String unit_NC();
	const String unit_LA(F("dB(A)"));
	float dew_point_temp;

	const int signal_quality = calcWiFiSignalQuality(last_signal_strength);
	debug_outln_info(F("ws: values ..."));
	if (!count_sends)
	{
		page_content += F("<b style='color:red'>");
		add_warning_first_cycle(page_content);
		page_content += FPSTR(WEB_B_BR_BR);
	}
	else
	{
		add_age_last_values(page_content);
	}

	auto add_table_pm_value = [&page_content](const __FlashStringHelper *sensor, const __FlashStringHelper *param, const float &value)
	{
		add_table_row_from_value(page_content, sensor, param, check_display_value(value, -1, 1, 0), F("µg/m³"));
	};

	auto add_table_nc_value = [&page_content](const __FlashStringHelper *sensor, const __FlashStringHelper *param, const float value)
	{
		add_table_row_from_value(page_content, sensor, param, check_display_value(value, -1, 1, 0), F("#/cm³"));
	};

	auto add_table_t_value = [&page_content](const __FlashStringHelper *sensor, const __FlashStringHelper *param, const float value)
	{
		add_table_row_from_value(page_content, sensor, param, check_display_value(value, -128, 1, 0), "°C");
	};

	auto add_table_h_value = [&page_content](const __FlashStringHelper *sensor, const __FlashStringHelper *param, const float value)
	{
		add_table_row_from_value(page_content, sensor, param, check_display_value(value, -1, 1, 0), "%");
	};

	auto add_table_co2_value = [&page_content](const __FlashStringHelper *sensor, const __FlashStringHelper *param, const float &value)
	{
		add_table_row_from_value(page_content, sensor, param, check_display_value(value, -1, 1, 0).substring(0, check_display_value(value, -1, 1, 0).indexOf(".")), "ppm"); //remove after .
	};

	auto add_table_voc_value = [&page_content](const __FlashStringHelper *sensor, const __FlashStringHelper *param, const float &value)
	{
		add_table_row_from_value(page_content, sensor, param, check_display_value(value, -1, 1, 0).substring(0, check_display_value(value, -1, 1, 0).indexOf(".")), "ppb"); //remove after .
	};

	auto add_table_no2_value = [&page_content](const __FlashStringHelper *sensor, const __FlashStringHelper *param, const float &value)
	{
		add_table_row_from_value(page_content, sensor, param, check_display_value(value, -1, 1, 0).substring(0, check_display_value(value, -1, 1, 0).indexOf(".")), "μg/m3");
	};

	auto add_table_value = [&page_content](const __FlashStringHelper *sensor, const __FlashStringHelper *param, const String &value, const String &unit)
	{
		add_table_row_from_value(page_content, sensor, param, value, unit);
	};

	server.sendContent(page_content);
	page_content = F("<table cellspacing='0' cellpadding='5' class='v'>\n"
					 "<thead><tr><th>" INTL_SENSOR "</th><th> " INTL_PARAMETER "</th><th>" INTL_VALUE "</th></tr></thead>");

	if (cfg::sds_read)
	{
		add_table_pm_value(FPSTR(SENSORS_SDS011), FPSTR(WEB_PM25), last_value_SDS_P2);
		add_table_pm_value(FPSTR(SENSORS_SDS011), FPSTR(WEB_PM10), last_value_SDS_P1);
		page_content += FPSTR(EMPTY_ROW);
	}
	if (cfg::npm_read)
	{
		add_table_pm_value(FPSTR(SENSORS_NPM), FPSTR(WEB_PM1), last_value_NPM_P0);
		add_table_pm_value(FPSTR(SENSORS_NPM), FPSTR(WEB_PM25), last_value_NPM_P2);
		add_table_pm_value(FPSTR(SENSORS_NPM), FPSTR(WEB_PM10), last_value_NPM_P1);
		add_table_nc_value(FPSTR(SENSORS_NPM), FPSTR(WEB_NC1k0), last_value_NPM_N1);
		add_table_nc_value(FPSTR(SENSORS_NPM), FPSTR(WEB_NC2k5), last_value_NPM_N25);
		add_table_nc_value(FPSTR(SENSORS_NPM), FPSTR(WEB_NC10), last_value_NPM_N10);
		page_content += FPSTR(EMPTY_ROW);
	}

	if (cfg::bmx280_read)
	{
		//REVOIR ICI

		// const char *const sensor_name = (bmx280.sensorID() == BME280_SENSOR_ID) ? SENSORS_BME280 : SENSORS_BMP280;
		// add_table_t_value(FPSTR(sensor_name), FPSTR(INTL_TEMPERATURE), last_value_BMX280_T);
		// add_table_value(FPSTR(sensor_name), FPSTR(INTL_PRESSURE), check_display_value(last_value_BMX280_P / 100.0f, (-1 / 100.0f), 2, 0), unit_P);
		// add_table_value(FPSTR(sensor_name), FPSTR(INTL_PRESSURE_AT_SEALEVEL), last_value_BMX280_P != -1.0f ? String(pressure_at_sealevel(last_value_BMX280_T, last_value_BMX280_P / 100.0f), 2) : "-", unit_P);
		// if (bmx280.sensorID() == BME280_SENSOR_ID)
		// {
		// 	add_table_h_value(FPSTR(sensor_name), FPSTR(INTL_HUMIDITY), last_value_BME280_H);
		// 	dew_point_temp = dew_point(last_value_BMX280_T, last_value_BME280_H);
		// 	add_table_value(FPSTR(sensor_name), FPSTR(INTL_DEW_POINT), isnan(dew_point_temp) ? "-" : String(dew_point_temp, 1), unit_T);
		// }
		// page_content += FPSTR(EMPTY_ROW);
	}

	if (cfg::mhz16_read)
	{
		const char *const sensor_name = SENSORS_MHZ16;
		add_table_co2_value(FPSTR(sensor_name), FPSTR(INTL_CO2), last_value_MHZ16);
		page_content += FPSTR(EMPTY_ROW);
	}

	if (cfg::mhz19_read)
	{
		const char *const sensor_name = SENSORS_MHZ19;
		add_table_co2_value(FPSTR(sensor_name), FPSTR(INTL_CO2), last_value_MHZ19);
		page_content += FPSTR(EMPTY_ROW);
	}

	if (cfg::ccs811_read)
	{
		const char *const sensor_name = SENSORS_CCS811;
		add_table_voc_value(FPSTR(sensor_name), FPSTR(INTL_VOC), last_value_CCS811);
		page_content += FPSTR(EMPTY_ROW);
	}

	if (cfg::enveano2_read)
	{
		const char *const sensor_name = SENSORS_ENVEANO2;
		add_table_no2_value(FPSTR(sensor_name), FPSTR(INTL_NO2), last_value_no2);
		page_content += FPSTR(EMPTY_ROW);
	}

	server.sendContent(page_content);
	page_content = emptyString;

	add_table_value(F("WiFi"), FPSTR(INTL_SIGNAL_STRENGTH), String(last_signal_strength), "dBm");
	add_table_value(F("WiFi"), FPSTR(INTL_SIGNAL_QUALITY), String(signal_quality), "%");

	page_content += FPSTR(TABLE_TAG_CLOSE_BR);
	page_content += FPSTR(BR_TAG);
	end_html_page(page_content);
}

/*****************************************************************
 * Webserver root: show device status
 *****************************************************************/
static void webserver_status()
{
	if (WiFi.status() != WL_CONNECTED)
	{
		sendHttpRedirect();
		return;
	}

	RESERVE_STRING(page_content, XLARGE_STR);
	start_html_page(page_content, FPSTR(INTL_DEVICE_STATUS));

	debug_outln_info(F("ws: status ..."));
	server.sendContent(page_content);
	page_content = F("<table cellspacing='0' cellpadding='5' class='v'>\n"
					 "<thead><tr><th> " INTL_PARAMETER "</th><th>" INTL_VALUE "</th></tr></thead>");
	String versionHtml(SOFTWARE_VERSION);
	versionHtml += F("/ST:");
	versionHtml += String(!moduleair_selftest_failed);
	versionHtml += '/';
	versionHtml.replace("/", FPSTR(BR_TAG));
	add_table_row_from_value(page_content, FPSTR(INTL_FIRMWARE), versionHtml);
	add_table_row_from_value(page_content, F("Free Memory"), String(ESP.getFreeHeap()));
	// const time_t now = time(nullptr);
	// struct tm *decomposed;
	// decomposed = gmtime(&now);
	// add_table_row_from_value(page_content, FPSTR(INTL_TIME_UTC), ctime(&now));


	struct tm timeinfo;
	String timestringntp;
	if (!getLocalTime(&timeinfo))
	{
		Debug.println("Failed to obtain time");
	}else{
			timestringntp += "20";
			timestringntp += String(timeinfo.tm_year-100);
			timestringntp += "-";
			if (timeinfo.tm_mon + 1 < 10){timestringntp += "0";}
			timestringntp += String(timeinfo.tm_mon + 1);
			timestringntp += "-";
			if (timeinfo.tm_mday < 10){timestringntp += "0";}
			timestringntp += String(timeinfo.tm_mday);
			timestringntp += "T";
			if (timeinfo.tm_hour < 10){timestringntp += "0";}
			timestringntp += String(timeinfo.tm_hour);
			timestringntp += ":";
			if (timeinfo.tm_min < 10){timestringntp += "0";}
			timestringntp += String(timeinfo.tm_min);
			timestringntp += ":";
			if (timeinfo.tm_sec < 10){timestringntp += "0";}
			timestringntp += String(timeinfo.tm_sec);
			timestringntp += "Z";
	}
	add_table_row_from_value(page_content, FPSTR(INTL_TIME_UTC), timestringntp);
	add_table_row_from_value(page_content, FPSTR(INTL_UTC_OFFSET), String(cfg::utc_offset));
	now2 = rtc.now();
	String current_time;
	
	current_time += String(now2.year(), DEC);
	current_time += "-";

	if (now2.month() < 10)
	{
		current_time += "0";
	}
	current_time += String(now2.month(), DEC);
	current_time += "-";

	if (now2.day() < 10)
	{
		current_time += "0";
	}
	current_time += String(now2.day(),DEC);

	current_time += "T";
	if (now2.hour() < 10)
	{
		current_time += "0";
	}
	current_time += String(now2.hour(), DEC);
	current_time += ":";

	if (now2.minute() < 10)
	{
		current_time += "0";
	}
	current_time += String(now2.minute(), DEC);
	current_time += ":";

	if (now2.second() < 10)
	{
		current_time += "0";
	}
	current_time += String(now2.second());
	current_time += "Z";

	add_table_row_from_value(page_content, FPSTR(INTL_TIME_RTC), current_time);
	add_table_row_from_value(page_content, F("Uptime"), delayToString(millis() - time_point_device_start_ms));
	if (cfg::sds_read)
	{
		page_content += FPSTR(EMPTY_ROW);
		add_table_row_from_value(page_content, FPSTR(SENSORS_SDS011), last_value_SDS_version);
	}
	if (cfg::npm_read)
	{
		page_content += FPSTR(EMPTY_ROW);
		add_table_row_from_value(page_content, FPSTR(SENSORS_NPM), last_value_NPM_version);
	}
	page_content += FPSTR(EMPTY_ROW);
	page_content += F("<tr><td colspan='2'><b>" INTL_ERROR "</b></td></tr>");
	String wifiStatus(WiFi_error_count);
	wifiStatus += '/';
	wifiStatus += String(last_signal_strength);
	wifiStatus += '/';
	wifiStatus += String(last_disconnect_reason);
	add_table_row_from_value(page_content, F("WiFi"), wifiStatus);

	// if (last_update_returncode != 0)
	// {
	// 	add_table_row_from_value(page_content, F("OTA Return"),
	// 							 last_update_returncode > 0 ? String(last_update_returncode) : HTTPClient::errorToString(last_update_returncode));
	// }
	for (unsigned int i = 0; i < LoggerCount; ++i)
	{
		if (loggerConfigs[i].errors)
		{
			const __FlashStringHelper *logger = loggerDescription(i);
			if (logger)
			{
				add_table_row_from_value(page_content, logger, String(loggerConfigs[i].errors));
			}
		}
	}

	if (last_sendData_returncode != 0)
	{
		add_table_row_from_value(page_content, F("Data Send Return"),
								 last_sendData_returncode > 0 ? String(last_sendData_returncode) : HTTPClient::errorToString(last_sendData_returncode));
	}
	if (cfg::sds_read)
	{
		add_table_row_from_value(page_content, FPSTR(SENSORS_SDS011), String(SDS_error_count));
	}
	if (cfg::npm_read)
	{
		add_table_row_from_value(page_content, FPSTR(SENSORS_NPM), String(NPM_error_count));
	}
	if (cfg::mhz16_read)
	{
		add_table_row_from_value(page_content, FPSTR(SENSORS_MHZ16), String(MHZ16_error_count));
	}
	if (cfg::mhz19_read)
	{
		add_table_row_from_value(page_content, FPSTR(SENSORS_MHZ19), String(MHZ19_error_count));
	}
	if (cfg::ccs811_read)
	{
		add_table_row_from_value(page_content, FPSTR(SENSORS_CCS811), String(CCS811_error_count));
	}

	if (cfg::enveano2_read)
	{
		add_table_row_from_value(page_content, FPSTR(SENSORS_ENVEANO2), String(Cairsens_error_count));
	}

	server.sendContent(page_content);
	page_content = emptyString;

	if (count_sends > 0)
	{
		page_content += FPSTR(EMPTY_ROW);
		add_table_row_from_value(page_content, F(INTL_NUMBER_OF_MEASUREMENTS), String(count_sends));
		if (sending_time > 0)
		{
			add_table_row_from_value(page_content, F(INTL_TIME_SENDING_MS), String(sending_time), "ms");
		}
	}

	page_content += FPSTR(TABLE_TAG_CLOSE_BR);
	end_html_page(page_content);
}

/*****************************************************************
 * Webserver read serial ring buffer                             *
 *****************************************************************/
static void webserver_serial()
{
	String s(Debug.popLines());

	server.send(s.length() ? 200 : 204, FPSTR(TXT_CONTENT_TYPE_TEXT_PLAIN), s);
}


/*****************************************************************
 * Webserver Set Time in RTC module                             *
 *****************************************************************/
static void webserver_settime()
{
	Debug.print("Set ddasdasdasdasd");

	if (server.hasArg("year") && server.hasArg("month") && server.hasArg("day") && server.hasArg("hour") && server.hasArg("minute") &&server.hasArg("second"))
	{
		Debug.print("Set date/time");

		rtc.adjust(DateTime(server.arg("year").toInt(), server.arg("month").toInt(), server.arg("day").toInt(), server.arg("hour").toInt(), server.arg("minute").toInt(), server.arg("second").toInt()));

	}else{
	Debug.print("Can't set date/time");
	}

	//RELOAD ALL After

	//webserver_root();

	if (WiFi.status() != WL_CONNECTED)
	{
		sendHttpRedirect();
	}
	else
	{
		if (!webserver_request_auth())
		{
			return;
		}

		RESERVE_STRING(page_content, XLARGE_STR);
		start_html_page(page_content, emptyString);
		debug_outln_info(F("ws: root ..."));

		// Enable Pagination
		page_content += FPSTR(WEB_ROOT_PAGE_CONTENT);
		page_content.replace(F("{t}"), FPSTR(INTL_CURRENT_DATA));
		page_content.replace(F("{s}"), FPSTR(INTL_DEVICE_STATUS));
		page_content.replace(F("{conf}"), FPSTR(INTL_CONFIGURATION));
		page_content.replace(F("{restart}"), FPSTR(INTL_RESTART_SENSOR));
		page_content.replace(F("{debug}"), FPSTR(INTL_DEBUG_LEVEL));
		end_html_page(page_content);
	}
}

/*****************************************************************
 * Webserver set debug level                                     *
 *****************************************************************/
static void webserver_debug_level()
{
	if (!webserver_request_auth())
	{
		return;
	}

	RESERVE_STRING(page_content, LARGE_STR);
	start_html_page(page_content, FPSTR(INTL_DEBUG_LEVEL));

	if (server.hasArg("lvl"))
	{
		debug_outln_info(F("ws: debug level ..."));

		const int lvl = server.arg("lvl").toInt();
		if (lvl >= 0 && lvl <= 5)
		{
			cfg::debug = lvl;
			page_content += F("<h3>");
			page_content += FPSTR(INTL_DEBUG_SETTING_TO);
			page_content += ' ';

			const __FlashStringHelper *lvlText;
			switch (lvl)
			{
			case DEBUG_ERROR:
				lvlText = F(INTL_ERROR);
				break;
			case DEBUG_WARNING:
				lvlText = F(INTL_WARNING);
				break;
			case DEBUG_MIN_INFO:
				lvlText = F(INTL_MIN_INFO);
				break;
			case DEBUG_MED_INFO:
				lvlText = F(INTL_MED_INFO);
				break;
			case DEBUG_MAX_INFO:
				lvlText = F(INTL_MAX_INFO);
				break;
			default:
				lvlText = F(INTL_NONE);
			}

			page_content += lvlText;
			page_content += F(".</h3>");
		}
	}

	page_content += F("<br/><pre id='slog' class='panels'>");
	page_content += Debug.popLines();
	page_content += F("</pre>");
	page_content += F("<script>"
					  "function slog_update() {"
					  "fetch('/serial').then(r => r.text()).then((r) => {"
					  "document.getElementById('slog').innerText += r;}).catch(err => console.log(err));};"
					  "setInterval(slog_update, 3000);"
					  "</script>");
	page_content += F("<h4>");
	page_content += FPSTR(INTL_DEBUG_SETTING_TO);
	page_content += F("</h4>"
					  "<table style='width:100%;'>"
					  "<tr><td style='width:25%;'><a class='b' href='/debug?lvl=0'>" INTL_NONE "</a></td>"
					  "<td style='width:25%;'><a class='b' href='/debug?lvl=1'>" INTL_ERROR "</a></td>"
					  "<td style='width:25%;'><a class='b' href='/debug?lvl=3'>" INTL_MIN_INFO "</a></td>"
					  "<td style='width:25%;'><a class='b' href='/debug?lvl=5'>" INTL_MAX_INFO "</a></td>"
					  "</tr><tr>"
					  "</tr>"
					  "</table>");

	end_html_page(page_content);
}

/*****************************************************************
 * Webserver remove config                                       *
 *****************************************************************/
static void webserver_removeConfig()
{
	// For any work with SPIFFS or server, the interrupts must be deactivated. The matrix is turned off.

	if (!webserver_request_auth())
	{
		return;
	}

	RESERVE_STRING(page_content, LARGE_STR);
	start_html_page(page_content, FPSTR(INTL_DELETE_CONFIG));
	debug_outln_info(F("ws: removeConfig ..."));

	if (server.method() == HTTP_GET)
	{
		page_content += FPSTR(WEB_REMOVE_CONFIG_CONTENT);
	}
	else
	{

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
		// Silently remove the desaster backup
		SPIFFS.remove(F("/config.json.old"));
		if (SPIFFS.exists(F("/config.json")))
		{ // file exists
			debug_outln_info(F("removing config.json..."));
			if (SPIFFS.remove(F("/config.json")))
			{
				page_content += F("<h3>" INTL_CONFIG_DELETED ".</h3>");
			}
			else
			{
				page_content += F("<h3>" INTL_CONFIG_CAN_NOT_BE_DELETED ".</h3>");
			}
		}
		else
		{
			page_content += F("<h3>" INTL_CONFIG_NOT_FOUND ".</h3>");
		}
#pragma GCC diagnostic pop
	}
	end_html_page(page_content);
}

/*****************************************************************
 * Webserver reset NodeMCU                                       *
 *****************************************************************/
static void webserver_reset()
{
	if (!webserver_request_auth())
	{
		return;
	}

	String page_content;
	page_content.reserve(512);

	start_html_page(page_content, FPSTR(INTL_RESTART_SENSOR));
	debug_outln_info(F("ws: reset ..."));

	if (server.method() == HTTP_GET)
	{
		page_content += FPSTR(WEB_RESET_CONTENT);
	}
	else
	{
		sensor_restart();
	}
	end_html_page(page_content);
}

/*****************************************************************
 * Webserver data.json                                           *
 *****************************************************************/
static void webserver_data_json()
{
	String s1;
	unsigned long age = 0;

	debug_outln_info(F("ws: data json..."));
	if (!count_sends)
	{
		s1 = FPSTR(data_first_part);
		s1 += "]}";
		age = cfg::sending_intervall_ms - msSince(starttime);
		if (age > cfg::sending_intervall_ms)
		{
			age = 0;
		}
		age = 0 - age;
	}
	else
	{
		s1 = last_data_string;
		age = msSince(starttime);
		if (age > cfg::sending_intervall_ms)
		{
			age = 0;
		}
	}
	String s2 = F(", \"age\":\"");
	s2 += String((long)((age + 500) / 1000));
	s2 += F("\", \"sensordatavalues\"");
	s1.replace(F(", \"sensordatavalues\""), s2);
	server.send(200, FPSTR(TXT_CONTENT_TYPE_JSON), s1);
}

/*****************************************************************
 * Webserver metrics endpoint                                    *
 *****************************************************************/
static void webserver_metrics_endpoint()
{
	debug_outln_info(F("ws: /metrics"));
	RESERVE_STRING(page_content, XLARGE_STR);
	page_content = F("software_version{version=\"" SOFTWARE_VERSION_STR "\",$i} 1\nuptime_ms{$i} $u\nsending_intervall_ms{$i} $s\nnumber_of_measurements{$i} $c\n");
	String id(F("node=\"" SENSOR_BASENAME));
	//String id(F("node=\"" HOSTNAME_BASE));
	id += esp_chipid;
	id += '\"';
	page_content.replace("$i", id);
	page_content.replace("$u", String(msSince(time_point_device_start_ms)));
	page_content.replace("$s", String(cfg::sending_intervall_ms));
	page_content.replace("$c", String(count_sends));
	DynamicJsonDocument json2data(JSON_BUFFER_SIZE);
	DeserializationError err = deserializeJson(json2data, last_data_string);
	if (!err)
	{
		for (JsonObject measurement : json2data[FPSTR(JSON_SENSOR_DATA_VALUES)].as<JsonArray>())
		{
			page_content += measurement["value_type"].as<const char *>();
			page_content += '{';
			page_content += id;
			page_content += "} ";
			page_content += measurement["value"].as<const char *>();
			page_content += '\n';
		}
		page_content += F("last_sample_age_ms{");
		page_content += id;
		page_content += "} ";
		page_content += String(msSince(starttime));
		page_content += '\n';
	}
	else
	{
		debug_outln_error(FPSTR(DBG_TXT_DATA_READ_FAILED));
	}
	page_content += F("# EOF\n");
	debug_outln(page_content, DEBUG_MED_INFO);
	server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_PLAIN), page_content);
}

/*****************************************************************
 * Webserver Images                                              *
 *****************************************************************/

static void webserver_favicon()
{
	server.sendHeader(F("Cache-Control"), F("max-age=2592000, public"));

	server.send_P(200, TXT_CONTENT_TYPE_IMAGE_PNG,
				  AIRCARTO_INFO_LOGO_PNG, AIRCARTO_INFO_LOGO_PNG_SIZE);
}

/*****************************************************************
 * Webserver page not found                                      *
 *****************************************************************/
static void webserver_not_found()
{

	last_page_load = millis();
	debug_outln_info(F("ws: not found ..."));

	if (WiFi.status() != WL_CONNECTED)
	{
		if ((server.uri().indexOf(F("success.html")) != -1) || (server.uri().indexOf(F("detect.html")) != -1))
		{
			server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), FPSTR(WEB_IOS_REDIRECT));
		}
		else
		{
			sendHttpRedirect();
		}
	}
	else
	{
		server.send(404, FPSTR(TXT_CONTENT_TYPE_TEXT_PLAIN), F("Not found."));
	}
}

static void webserver_static()
{
	server.sendHeader(F("Cache-Control"), F("max-age=2592000, public"));

	if (server.arg(String('r')) == F("logo"))
	{

		server.send_P(200, TXT_CONTENT_TYPE_IMAGE_PNG,
					  AIRCARTO_INFO_LOGO_PNG, AIRCARTO_INFO_LOGO_PNG_SIZE);
	}
	else if (server.arg(String('r')) == F("css"))
	{
		server.send_P(200, TXT_CONTENT_TYPE_TEXT_CSS,
					  WEB_PAGE_STATIC_CSS, sizeof(WEB_PAGE_STATIC_CSS) - 1);
	}
	else
	{
		webserver_not_found();
	}
}

/*****************************************************************
 * Webserver setup                                               *
 *****************************************************************/
static void setup_webserver()
{
	server.on("/", webserver_root);
	server.on(F("/config"), webserver_config);
	server.on(F("/wifi"), webserver_wifi);
	server.on(F("/values"), webserver_values);
	server.on(F("/status"), webserver_status);
	server.on(F("/generate_204"), webserver_config);
	server.on(F("/fwlink"), webserver_config);
	server.on(F("/debug"), webserver_debug_level);
	server.on(F("/serial"), webserver_serial);
	server.on(F("/settime"), webserver_settime);
	server.on(F("/removeConfig"), webserver_removeConfig);
	server.on(F("/reset"), webserver_reset);
	server.on(F("/data.json"), webserver_data_json);
	server.on(F("/metrics"), webserver_metrics_endpoint);
	server.on(F("/favicon.ico"), webserver_favicon);
	server.on(F(STATIC_PREFIX), webserver_static);
	server.onNotFound(webserver_not_found);
	debug_outln_info(F("Starting Webserver... "));
	server.begin();
}

static int selectChannelForAp()
{
	std::array<int, 14> channels_rssi;
	std::fill(channels_rssi.begin(), channels_rssi.end(), -100);

	for (unsigned i = 0; i < std::min((uint8_t)14, count_wifiInfo); i++)
	{
		if (wifiInfo[i].RSSI > channels_rssi[wifiInfo[i].channel])
		{
			channels_rssi[wifiInfo[i].channel] = wifiInfo[i].RSSI;
		}
	}

	if ((channels_rssi[1] < channels_rssi[6]) && (channels_rssi[1] < channels_rssi[11]))
	{
		return 1;
	}
	else if ((channels_rssi[6] < channels_rssi[1]) && (channels_rssi[6] < channels_rssi[11]))
	{
		return 6;
	}
	else
	{
		return 11;
	}
}

/*****************************************************************
 * WifiConfig                                                    *
 *****************************************************************/

static void wifiConfig()
{
	debug_outln_info(F("Starting WiFiManager"));
	debug_outln_info(F("AP ID: "), String(cfg::fs_ssid));
	debug_outln_info(F("Password: "), String(cfg::fs_pwd));

	wificonfig_loop = true;
	WiFi.disconnect(true, true);

	debug_outln_info(F("scan for wifi networks..."));
	int8_t scanReturnCode = WiFi.scanNetworks(false /* scan async */, true /* show hidden networks */);
	if (scanReturnCode < 0)
	{
		debug_outln_error(F("WiFi scan failed. Treating as empty. "));
		count_wifiInfo = 0;
	}
	else
	{
		count_wifiInfo = (uint8_t)scanReturnCode;
	}

	delete[] wifiInfo;
	wifiInfo = new struct_wifiInfo[std::max(count_wifiInfo, (uint8_t)1)];

	for (unsigned i = 0; i < count_wifiInfo; i++)
	{
		String SSID;
		uint8_t *BSSID;

		memset(&wifiInfo[i], 0, sizeof(struct_wifiInfo));
		WiFi.getNetworkInfo(i, SSID, wifiInfo[i].encryptionType, wifiInfo[i].RSSI, BSSID, wifiInfo[i].channel);
		SSID.toCharArray(wifiInfo[i].ssid, sizeof(wifiInfo[0].ssid));
	}

	// Use 13 channels if locale is not "EN"
	wifi_country_t wifi;
	wifi.policy = WIFI_COUNTRY_POLICY_MANUAL;
	strcpy(wifi.cc, INTL_LANG);
	wifi.nchan = (INTL_LANG[0] == 'E' && INTL_LANG[1] == 'N') ? 11 : 13;
	wifi.schan = 1;

	WiFi.mode(WIFI_AP);
	const IPAddress apIP(192, 168, 4, 1);
	WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
	WiFi.softAP(cfg::fs_ssid, cfg::fs_pwd, selectChannelForAp());
	// In case we create a unique password at first start
	debug_outln_info(F("AP Password is: "), cfg::fs_pwd);

	DNSServer dnsServer;
	// Ensure we don't poison the client DNS cache
	dnsServer.setTTL(0);
	dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
	dnsServer.start(53, "*", apIP); // 53 is port for DNS server

	setup_webserver();

	// 10 minutes timeout for wifi config
	last_page_load = millis();

	while ((millis() - last_page_load) < cfg::time_for_wifi_config + 500)
	{
		dnsServer.processNextRequest();
		server.handleClient();
		yield();	
	}

	WiFi.softAPdisconnect(true);
	dnsServer.stop(); // A VOIR
	delay(100);
	// WiFi.disconnect(true, true);
	WiFi.mode(WIFI_OFF); //A tenter

	debug_outln_info(F("---- Result Webconfig ----"));
	debug_outln_info(F("WiFi: "), cfg::has_wifi);
	debug_outln_info(F("LoRa: "), cfg::has_lora);
	debug_outln_info(F("APPEUI: "), cfg::appeui);
	debug_outln_info(F("DEVEUI: "), cfg::deveui);
	debug_outln_info(F("APPKEY: "), cfg::appkey);
	debug_outln_info(F("WLANSSID: "), cfg::wlanssid);
	debug_outln_info(FPSTR(DBG_TXT_SEP));
	debug_outln_info_bool(F("SDS: "), cfg::sds_read);
	debug_outln_info_bool(F("NPM: "), cfg::npm_read);
	debug_outln_info_bool(F("BMX: "), cfg::bmx280_read);
	debug_outln_info_bool(F("MHZ16: "), cfg::mhz16_read);
	debug_outln_info_bool(F("MHZ19: "), cfg::mhz19_read);
	debug_outln_info_bool(F("CCS811: "), cfg::ccs811_read);
	debug_outln_info_bool(F("Cairsens NO2: "), cfg::enveano2_read);
	debug_outln_info(FPSTR(DBG_TXT_SEP));
	debug_outln_info_bool(F("SensorCommunity: "), cfg::send2dusti);
	debug_outln_info_bool(F("Madavi: "), cfg::send2madavi);
	debug_outln_info_bool(F("CSV: "), cfg::send2csv);
	debug_outln_info_bool(F("AirCarto: "), cfg::send2custom);
	debug_outln_info_bool(F("AtmoSud: "), cfg::send2custom2);
	debug_outln_info(FPSTR(DBG_TXT_SEP));
	debug_outln_info_bool(F("Matrix: "), cfg::has_matrix);
	debug_outln_info_bool(F("Display Measures: "), cfg::display_measure);
	debug_outln_info_bool(F("Display forecast: "), cfg::display_forecast);
	debug_outln_info(F("Debug: "), String(cfg::debug));
	wificonfig_loop = false; // VOIR ICI
}

static void waitForWifiToConnect(int maxRetries)
{
	int retryCount = 0;
	while ((WiFi.status() != WL_CONNECTED) && (retryCount < maxRetries))
	{
		delay(500);
		debug_out(".", DEBUG_MIN_INFO);
		++retryCount;
	}
}

/*****************************************************************
 * get GPS from AirCarto                                       *
 *****************************************************************/

String latitude_aircarto = "0.00000";
String longitude_aircarto = "0.00000";

gps getGPS(String id)
{
	String reponseAPI;
	StaticJsonDocument<JSON_BUFFER_SIZE2> json;
	char reponseJSON[JSON_BUFFER_SIZE2];

	gps coordinates{"0.00000", "0.00000"};

	HTTPClient http;
	http.setTimeout(20 * 1000);

	String urlAirCarto = "http://data.moduleair.fr/get_loc.php?id=";
	String serverPath = urlAirCarto + id;

	debug_outln_info(F("Call: "), serverPath);
	http.begin(serverPath.c_str());

	int httpResponseCode = http.GET();

	if (httpResponseCode > 0)
	{
		reponseAPI = http.getString();
		if (reponseAPI == "null")
		{
			return {"0.00000", "0.00000"};
		}

		debug_outln_info(F("Response: "), reponseAPI);
		strcpy(reponseJSON, reponseAPI.c_str());

		DeserializationError error = deserializeJson(json, reponseJSON);

		if (strcmp(error.c_str(), "Ok") == 0)
		{
			return {json["latitude"], json["longitude"]};
		}
		else
		{
			Debug.print(F("deserializeJson() failed: "));
			Debug.println(error.c_str());
			return {"0.00000", "0.00000"};
		}
		http.end();
	}
	else
	{
		debug_outln_info(F("Failed connecting to AirCarto with error code:"), String(httpResponseCode));
		return {"0.00000", "0.00000"};
		http.end();
	}
}

// https://worldtimeapi.org/api/ip

uint8_t getOffset()
{
	String reponseAPI;
	StaticJsonDocument<JSON_BUFFER_SIZE2> json;
	char reponseJSON[JSON_BUFFER_SIZE2];

	HTTPClient http;
	http.setTimeout(20 * 1000);

	String urloffset = "https://worldtimeapi.org/api/ip";

	debug_outln_info(F("Call: "), urloffset);
	http.begin(urloffset.c_str());

	int httpResponseCode = http.GET();

	if (httpResponseCode > 0)
	{
		reponseAPI = http.getString();
		debug_outln_info(F("Response: "), reponseAPI);
		strcpy(reponseJSON, reponseAPI.c_str());
		DeserializationError error = deserializeJson(json, reponseJSON);
		if (strcmp(error.c_str(), "Ok") == 0)
		{
			return ((int)(json["dst_offset"]) + (int)(json["raw_offset"]))/3600;
		}
		else
		{
			Debug.print(F("deserializeJson() failed: "));
			Debug.println(error.c_str());
			return 0;
		}
		http.end();
	}
	else
	{
		debug_outln_info(F("Failed connecting to worldtimeapi with error code:"), String(httpResponseCode));
		return 0;
		http.end();
	}

}

/*****************************************************************
 * WiFi auto connecting script                                   *
 *****************************************************************/

static WiFiEventId_t disconnectEventHandler;
static WiFiEventId_t connectEventHandler;
static WiFiEventId_t STAstartEventHandler;
static WiFiEventId_t STAstopEventHandler;

static void connectWifi()
{
	disconnectEventHandler = WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info)
										  {
											  if (!wifi_connection_lost)
											  {
												  Debug.println("Event disconnect");
												  wifi_connection_lost = true;
											  }
										  },
										  WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

	connectEventHandler = WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info)
									   {
										   if (wifi_connection_lost)
										   {
											   Debug.println("Event connect");
											   wifi_connection_lost = false;
										   }
									   },
									   WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);

	STAstartEventHandler = WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info)
										{ Debug.println("STA start"); },
										WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_START);

	STAstopEventHandler = WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info)
									   { Debug.println("STA stop"); },
									   WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_STOP);

	if (WiFi.getAutoConnect())
	{
		WiFi.setAutoConnect(false);
	}

	WiFi.setAutoReconnect(false);

	// Use 13 channels for connect to known AP
	wifi_country_t wifi;
	wifi.policy = WIFI_COUNTRY_POLICY_MANUAL;
	strcpy(wifi.cc, INTL_LANG);
	wifi.nchan = 13;
	wifi.schan = 1;

	WiFi.mode(WIFI_STA);
	WiFi.setHostname(cfg::fs_ssid);
	WiFi.begin(cfg::wlanssid, cfg::wlanpwd); // Start WiFI

	debug_outln_info(FPSTR(DBG_TXT_CONNECTING_TO), cfg::wlanssid);

	waitForWifiToConnect(20); //diminur ici ???  //40
	debug_outln_info(emptyString);

	//if (WiFi.status() != WL_CONNECTED) //Waitforwifistatus ?
	if (WiFi.waitForConnectResult(10000) != WL_CONNECTED) //Waitforwifistatus ?
	{
		Debug.println("Force change WiFi config");
		wifi_connection_lost = true;
		cfg::has_wifi = false;
		// strcpy_P(cfg::wlanssid, "TYPE SSID");
		// strcpy_P(cfg::wlanpwd, "TYPE PWD");

		wifiConfig();
	}
	else
	{
		wifi_connection_lost = false;
		Debug.println("Get coordinates..."); //only once!
		gps coordinates = getGPS(esp_chipid);
		latitude_aircarto = coordinates.latitude;
		longitude_aircarto = coordinates.longitude;

		Debug.println(coordinates.latitude);
		Debug.println(coordinates.longitude);
		if (coordinates.latitude != "0.00000" && coordinates.latitude != "0.00000")
		{
			strcpy_P(cfg::latitude, latitude_aircarto.c_str()); //replace the values in the firmware but not in the SPIFFS
			strcpy_P(cfg::longitude, longitude_aircarto.c_str());
		}

		cfg::utc_offset = getOffset();
		Debug.println(cfg::utc_offset); //replace the values in the firmware but not in the SPIFFS
	}

	debug_outln_info(F("WiFi connected, IP is: "), WiFi.localIP().toString());
	last_signal_strength = WiFi.RSSI(); //RSSI ICI!!!!

	if (MDNS.begin(cfg::fs_ssid))
	{
		MDNS.addService("http", "tcp", 80);
		MDNS.addServiceTxt("http", "tcp", "PATH", "/config");
	}

}

static WiFiClient *getNewLoggerWiFiClient(const LoggerEntry logger)
{
	WiFiClient *_client;
	_client = new WiFiClient;
	return _client;
}

static WiFiClientSecure *getNewLoggerWiFiClientSecure(const LoggerEntry logger)
{
	WiFiClientSecure *_client;
	_client = new WiFiClientSecure;
	return _client;
}

/*****************************************************************
 * send data to rest api                                         *
 *****************************************************************/
static unsigned long sendData(const LoggerEntry logger, const String &data, const int pin, const char *host, const char *url, bool ssl)
{
	unsigned long start_send = millis();
	const __FlashStringHelper *contentType;
	int result = 0;

	String s_Host(FPSTR(host));
	String s_url(FPSTR(url));

	switch (logger)
	{
	case LoggerSensorCommunity:
		Debug.print("LoggerSensorCommunity https: ");
		Debug.println(ssl);
		contentType = FPSTR(TXT_CONTENT_TYPE_JSON);
		break;
	case LoggerMadavi:
		Debug.print("LoggerMadavi https: ");
		Debug.println(ssl);
		contentType = FPSTR(TXT_CONTENT_TYPE_JSON);
		break;
	case LoggerCustom:
		Debug.print("LoggerAirCarto https: ");
		Debug.println(ssl);
		contentType = FPSTR(TXT_CONTENT_TYPE_JSON);
		break;
	case LoggerCustom2:
		Debug.print("LoggerAtmoSud https: ");
		Debug.println(ssl);
		contentType = FPSTR(TXT_CONTENT_TYPE_JSON);
		break;
	default:
		contentType = FPSTR(TXT_CONTENT_TYPE_JSON);
		break;
	}

	if (!ssl)
	{
		std::unique_ptr<WiFiClient> client(getNewLoggerWiFiClient(logger));

		HTTPClient http;
		http.setTimeout(20 * 1000);
		http.setUserAgent(SOFTWARE_VERSION + '/' + esp_chipid);
		http.setReuse(false);
		bool send_success = false;

		if (logger == LoggerCustom && (*cfg::user_custom || *cfg::pwd_custom))
		{
			http.setAuthorization(cfg::user_custom, cfg::pwd_custom);
		}

		if (logger == LoggerCustom2 && (*cfg::user_custom2 || *cfg::pwd_custom2))
		{
			http.setAuthorization(cfg::user_custom2, cfg::pwd_custom2);
		}

		if (http.begin(*client, s_Host, loggerConfigs[logger].destport, s_url, !!loggerConfigs[logger].session))
		{
			http.addHeader(F("Content-Type"), contentType);
			http.addHeader(F("X-Sensor"), String(F(SENSOR_BASENAME)) + esp_chipid);
			// http.addHeader(F("X-MAC-ID"), String(F(SENSOR_BASENAME)) + esp_mac_id);
			if (pin)
			{
				http.addHeader(F("X-PIN"), String(pin));
			}

			result = http.POST(data);

			if (result >= HTTP_CODE_OK && result <= HTTP_CODE_ALREADY_REPORTED)
			{
				debug_outln_info(F("Succeeded http - "), s_Host);
				send_success = true;
			}
			else if (result >= HTTP_CODE_BAD_REQUEST)
			{
				debug_outln_info(F("Request http failed with error: "), String(result));
				debug_outln_info(F("Details:"), http.getString());
			}
			http.end();
		}
		else
		{
			debug_outln_info(F("Failed connecting to "), s_Host);
		}
		if (!send_success && result != 0)
		{
			loggerConfigs[logger].errors++;
			last_sendData_returncode = result;
		}

		return millis() - start_send;
	}
	else
	{
		std::unique_ptr<WiFiClientSecure> clientSecure(getNewLoggerWiFiClientSecure(logger));

		switch (logger)
		{
		case LoggerSensorCommunity:
			clientSecure->setCACert(dst_root_ca_x3);
			break;
		case LoggerMadavi:
			clientSecure->setCACert(dst_root_ca_x3);
			break;
		case LoggerCustom:
			clientSecure->setCACert(ca_aircarto);
			break;
		case LoggerCustom2:
			clientSecure->setCACert(ca_atmo);
			break;
		}

		HTTPClient https;
		https.setTimeout(20 * 1000);
		https.setUserAgent(SOFTWARE_VERSION + '/' + esp_chipid);
		https.setReuse(false);
		bool send_success = false;
		if (logger == LoggerCustom && (*cfg::user_custom || *cfg::pwd_custom))
		{
			https.setAuthorization(cfg::user_custom, cfg::pwd_custom);
		}
		if (logger == LoggerCustom2 && (*cfg::user_custom2 || *cfg::pwd_custom2))
		{
			https.setAuthorization(cfg::user_custom2, cfg::pwd_custom2);
		}

		if (https.begin(*clientSecure, s_Host, loggerConfigs[logger].destport, s_url, !!loggerConfigs[logger].session))
		{
			https.addHeader(F("Content-Type"), contentType);
			https.addHeader(F("X-Sensor"), String(F(SENSOR_BASENAME)) + esp_chipid);
			// https.addHeader(F("X-MAC-ID"), String(F(SENSOR_BASENAME)) + esp_mac_id);
			if (pin)
			{
				https.addHeader(F("X-PIN"), String(pin));
			}

			result = https.POST(data);

			if (result >= HTTP_CODE_OK && result <= HTTP_CODE_ALREADY_REPORTED)
			{
				debug_outln_info(F("Succeeded https - "), s_Host);
				send_success = true;
			}
			else if (result >= HTTP_CODE_BAD_REQUEST)
			{
				debug_outln_info(F("Request https failed with error: "), String(result));
				debug_outln_info(F("Details:"), https.getString());
			}
			https.end();
		}
		else
		{
			debug_outln_info(F("Failed connecting to "), s_Host);
		}
		if (!send_success && result != 0)
		{
			loggerConfigs[logger].errors++;
			last_sendData_returncode = result;
		}

		return millis() - start_send;
	}
}

/*****************************************************************
 * send single sensor data to sensor.community api                *
 *****************************************************************/
static unsigned long sendSensorCommunity(const String &data, const int pin, const __FlashStringHelper *sensorname, const char *replace_str)
{
	unsigned long sum_send_time = 0;

	if (cfg::send2dusti && data.length())
	{
		RESERVE_STRING(data_sensorcommunity, LARGE_STR);
		data_sensorcommunity = FPSTR(data_first_part);

		debug_outln_info(F("## Sending to sensor.community - "), sensorname);
		data_sensorcommunity += data;
		data_sensorcommunity.remove(data_sensorcommunity.length() - 1);
		data_sensorcommunity.replace(replace_str, emptyString);
		data_sensorcommunity += "]}";
		Debug.println(data_sensorcommunity);
		sum_send_time = sendData(LoggerSensorCommunity, data_sensorcommunity, pin, HOST_SENSORCOMMUNITY, URL_SENSORCOMMUNITY, cfg::ssl_dusti);
	}

	return sum_send_time;
}

/*****************************************************************
 * send data as csv to serial out                                *
 *****************************************************************/
static void send_csv(const String &data)
{
	DynamicJsonDocument json2data(JSON_BUFFER_SIZE);
	DeserializationError err = deserializeJson(json2data, data);
	debug_outln_info(F("CSV Output: "), data);
	if (!err)
	{
		String headline = F("Timestamp_ms;");
		String valueline(act_milli);
		valueline += ';';
		for (JsonObject measurement : json2data[FPSTR(JSON_SENSOR_DATA_VALUES)].as<JsonArray>())
		{
			headline += measurement["value_type"].as<const char *>();
			headline += ';';
			valueline += measurement["value"].as<const char *>();
			valueline += ';';
		}
		static bool first_csv_line = true;
		if (first_csv_line)
		{
			if (headline.length() > 0)
			{
				headline.remove(headline.length() - 1);
			}
			Debug.println(headline);
			first_csv_line = false;
		}
		if (valueline.length() > 0)
		{
			valueline.remove(valueline.length() - 1);
		}
		Debug.println(valueline);
	}
	else
	{
		debug_outln_error(FPSTR(DBG_TXT_DATA_READ_FAILED));
	}
}

/*****************************************************************
 * get data from LoRaWAN downlink payload                                        *
 *****************************************************************/

static void getDataLora(uint8_t array[5])
{

	union
	{
		float f;
		byte b[4];
	} u;

	u.b[0] = array[1];
	u.b[1] = array[2];
	u.b[2] = array[3];
	u.b[3] = array[4];

	switch (array[0])
	{
	case 0:
		atmoSud.multi = u.f;
		Debug.println("Index from LoRaWAN:");
		break;
	case 1:
		atmoSud.no2 = u.f;
		Debug.println("NO2 from LoRaWAN:");
		break;
	case 2:
		atmoSud.o3 = u.f;
		Debug.println("O3 from LoRaWAN:");
		break;
	case 3:
		atmoSud.pm10 = u.f;
		Debug.println("PM10 from LoRaWAN:");
		break;
	case 4:
		atmoSud.pm2_5 = u.f;
		Debug.println("PM2_5 from LoRaWAN:");
		break;
	case 5:
		atmoSud.so2 = u.f;
		Debug.println("SO2 from LoRaWAN:");
		break;
	}
	Debug.println(u.f, 2);
}

/*****************************************************************
 * get data from AtmoSud api                                         *
 *****************************************************************/

float getDataAtmoSud(unsigned int type)
{
	String sensor_type = "";
	struct tm timeinfo;

	if (!getLocalTime(&timeinfo))
	{
		Debug.println("Failed to obtain time");
	}

	//timeinfo.tm_mday += 1; // J+1 Change the day in AtmoSud forecast API

	char date[21];
	strftime(date, 21, "%FT%TZ", &timeinfo);

	Debug.println(String(date));

	switch (type)
	{
	case 0:
		sensor_type = "icairh";
		break;
	case 1:
		sensor_type = "no2";
		break;
	case 2:
		sensor_type = "o3";
		break;
	case 3:
		sensor_type = "pm10";
		break;
	case 4:
		sensor_type = "pm2.5";
		break;
	case 5:
		sensor_type = "so2";
		break;
	}

	String reponseAPI;
	StaticJsonDocument<JSON_BUFFER_SIZE2> json;
	char reponseJSON[JSON_BUFFER_SIZE2];

	HTTPClient http;
	http.setTimeout(20 * 1000);

	//Call: https://api.atmosud.org/prevision/cartes/horaires/point?x=null&y=null&datetime_echeance=2023-05-09T&with_list=false&polluant=icairh

	String urlAtmo1 = "https://api.atmosud.org/prevision/cartes/horaires/point?x=";
	String urlAtmo2 = "&y=";
	String urlAtmo3 = "&datetime_echeance=";
	String urlAtmo4 = "&with_list=false&polluant=";

	String serverPath = urlAtmo1 + String(cfg::longitude) + urlAtmo2 + String(cfg::latitude) + urlAtmo3 + String(date) + urlAtmo4 + sensor_type;

	debug_outln_info(F("Call: "), serverPath);

	http.begin(serverPath.c_str());

	int httpResponseCode = http.GET();

	if (httpResponseCode > 0)
	{
		reponseAPI = http.getString();
		debug_outln_info(F("Response: "), reponseAPI);
		strcpy(reponseJSON, reponseAPI.c_str());

		DeserializationError error = deserializeJson(json, reponseJSON);

		serializeJsonPretty(json, Debug);

		if (strcmp(error.c_str(), "Ok") == 0)
		{
			debug_outln_info(F("Type: "), sensor_type);

			if (json["variables"][0]["variable"] == "icairh")
			{
				Debug.println((float)json["variables"][0]["horaires"][0]["indicateur"]);
				return (float)json["variables"][0]["horaires"][0]["indicateur"];
			}
			else
			{
				Debug.println((float)json["variables"][0]["horaires"][0]["concentration"]);
				return (float)json["variables"][0]["horaires"][0]["concentration"];
			}
		}
		else
		{
			Debug.print(F("deserializeJson() failed: "));
			Debug.println(error.c_str());
			return -1.0;
		}
		http.end();
	}
	else
	{
		debug_outln_info(F("Failed connecting to Atmo Sud API with error code:"), String(httpResponseCode));
		return -1.0;
		http.end();
	}
}


/*****************************************************************
   I2C COM
 *****************************************************************/


String datai2c;


/*****************************************************************
   Functions
 *****************************************************************/


static void logEnabledAPIs()
{
	debug_outln_info(F("Send to :"));
	if (cfg::send2dusti)
	{
		debug_outln_info(F("sensor.community"));
	}

	if (cfg::send2madavi)
	{
		debug_outln_info(F("Madavi.de"));
	}

	if (cfg::send2csv)
	{
		debug_outln_info(F("Serial as CSV"));
	}

	if (cfg::send2custom)
	{
		debug_outln_info(F("AirCarto API"));
	}
	if (cfg::send2custom2)
	{
		debug_outln_info(F("Atmosud API"));
	}
}

static void setupNetworkTime()
{
	// server name ptrs must be persisted after the call to configTime because internally
	// the pointers are stored see implementation of lwip sntp_setservername()
	static char ntpServer1[18], ntpServer2[18];
	strcpy_P(ntpServer1, NTP_SERVER_1);
	strcpy_P(ntpServer2, NTP_SERVER_2);
	configTime(0, 0, ntpServer1, ntpServer2);
}

static unsigned long sendDataToOptionalApis(const String &data)
{
	unsigned long sum_send_time = 0;

	Debug.println(data);

	if (cfg::send2madavi)
	{
		debug_outln_info(FPSTR(DBG_TXT_SENDING_TO), F("madavi.de: "));
		sum_send_time += sendData(LoggerMadavi, data, 0, HOST_MADAVI, URL_MADAVI, cfg::ssl_madavi);
	}

	if (cfg::send2custom)
	{
		String data_to_send = data;
		data_to_send.remove(0, 1);
		String data_4_custom(F("{\"moduleairid\": \""));
		data_4_custom += esp_chipid;
		data_4_custom += "\", ";
		data_4_custom += data_to_send;
		debug_outln_info(FPSTR(DBG_TXT_SENDING_TO), F("aircarto api: "));
		sum_send_time += sendData(LoggerCustom, data_4_custom, 0, cfg::host_custom, cfg::url_custom, cfg::ssl_custom);
	}

	if (cfg::send2custom2)
	{
		String data_to_send = data;
		data_to_send.remove(0, 1);
		String data_4_custom(F("{\"moduleairid\": \""));
		data_4_custom += esp_chipid;
		data_4_custom += "\", ";
		data_4_custom += data_to_send;
		debug_outln_info(FPSTR(DBG_TXT_SENDING_TO), F("atmosud api: "));
		sum_send_time += sendData(LoggerCustom2, data_4_custom, 0, cfg::host_custom2, cfg::url_custom2, cfg::ssl_custom2);
	}

	if (cfg::send2csv)
	{
		debug_outln_info(F("## Sending as csv: "));
		send_csv(data);
	}

	return sum_send_time;
}

/*****************************************************************
 * Helium/TTN LoRaWAN                  *
 *****************************************************************/

static u1_t PROGMEM appeui_hex[8] = {};
static u1_t PROGMEM deveui_hex[8] = {};
static u1_t PROGMEM appkey_hex[16] = {};

void os_getArtEui(u1_t *buf) { memcpy_P(buf, appeui_hex, 8); }

void os_getDevEui(u1_t *buf) { memcpy_P(buf, deveui_hex, 8); }

void os_getDevKey(u1_t *buf) { memcpy_P(buf, appkey_hex, 16); }

//Initialiser avec les valeurs -1.0,-128.0 = valeurs par défaut qui doivent être filtrées

//uint8_t datalora[31] = {0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff};

// uint8_t datalora[37] = {0x00, 0xff, 0xff, 0xff, 0xff,0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff};
// 			//			conf       sds		 sds         npm 		 npm		npm		    npm			npm			npm			co2			co2			 cov     temp  humi	   press

uint8_t datalora[38] = {0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff};
//		                conf|   sds	    |	 sds    |    npm   | 	 npm   | 	npm	   |   npm	   |	npm	   |	npm	   |	co2	   |	 co2   |	cov    |    temp  | humi|   press   |       lat             |       lon             | sel

//Peut-être changer l'indianess pour temp = inverser

// 0x00, config
// 0xff, 0xff, sds -1
// 0xff, 0xff, sds -1
// 0xff, 0xff, npm -1
// 0xff, 0xff, npm -1
// 0xff, 0xff, npm -1
// 0xff, 0xff, npm_nc -1
// 0xff, 0xff, npm_nc -1
// 0xff, 0xff, npm_nc -1
// 0xff, 0xff, co2 -1
// 0xff, 0xff, co2 -1
// 0xff, 0xff, cov -1
// 0x80, temp -128
// 0xff, rh -1
// 0xff, 0xff, p -1
// 0x00, 0x00, 0x00, 0x00, lat 0.0 float
// 0x00, 0x00, 0x00, 0x00, lon 0.0 float
// 0xff sel -1

const unsigned TX_INTERVAL = (cfg::sending_intervall_ms) / 1000;

static osjob_t sendjob;

const lmic_pinmap lmic_pins = {
	.nss = D5, //AUTRE  //D5 origine
	.rxtx = LMIC_UNUSED_PIN,
	.rst = D0, //14 origine ou d12
	.dio = {D26, D35, D34},
};

void ToByteArray()
{
	String appeui_str = cfg::appeui;
	String deveui_str = cfg::deveui;
	String appkey_str = cfg::appkey;
	//  Debug.println(appeui_str);
	//  Debug.println(deveui_str);
	//  Debug.println(appkey_str);

	int j = 1;
	int k = 1;
	int l = 0;

	for (unsigned int i = 0; i < appeui_str.length(); i += 2)
	{
		String byteString = appeui_str.substring(i, i + 2);
		// Debug.println(byteString);
		byte byte = (char)strtol(byteString.c_str(), NULL, 16);
		// Debug.println(byte,HEX);
		appeui_hex[(appeui_str.length() / 2) - j] = byte; // reverse
		j += 1;
	}

	for (unsigned int i = 0; i < deveui_str.length(); i += 2)
	{
		String byteString = deveui_str.substring(i, i + 2);
		//  Debug.println(byteString);
		byte byte = (char)strtol(byteString.c_str(), NULL, 16);
		//  Debug.println(byte, HEX);
		deveui_hex[(deveui_str.length() / 2) - k] = byte; // reverse
		k += 1;
	}

	for (unsigned int i = 0; i < appkey_str.length(); i += 2)
	{
		String byteString = appkey_str.substring(i, i + 2);
		//  Debug.println(byteString);
		byte byte = (char)strtol(byteString.c_str(), NULL, 16);
		//  Debug.println(byte, HEX);
		// appkey_hex[(appkey_str.length() / 2) - 1 - l] = byte; // reverse
		appkey_hex[l] = byte; // not reverse
		l += 1;
	}
}

void printHex2(unsigned v)
{
	v &= 0xff;
	if (v < 16)
		Debug.print('0');
	Debug.print(v, HEX);
}

void do_send(osjob_t *j)
{
	// Check if there is not a current TX/RX job running
	if (LMIC.opmode & OP_TXRXPEND)
	{
		Debug.println(F("OP_TXRXPEND, not sending"));
		//Should appear sometimes because reloop while sending programmed
	}
	else
	{
		Debug.print("Size of Data:");
		Debug.println(sizeof(datalora));

		LMIC_setTxData2(1, datalora, sizeof(datalora) - 1, 1);  //1 ici pour ack

		Debug.println(F("Packet queued"));
	}
	// Next TX is scheduled after TX_COMPLETE event.
}

void onEvent(ev_t ev)
{
	Debug.print(os_getTime());
	Debug.print(": ");
	switch (ev)
	{
	case EV_SCAN_TIMEOUT:
		Debug.println(F("EV_SCAN_TIMEOUT"));
		//lora_connection_lost = true; //lora_connection_lost = true;
		break;
	case EV_BEACON_FOUND:
		Debug.println(F("EV_BEACON_FOUND"));
		break;
	case EV_BEACON_MISSED:
		Debug.println(F("EV_BEACON_MISSED"));
		break;
	case EV_BEACON_TRACKED:
		Debug.println(F("EV_BEACON_TRACKED"));
		break;
	case EV_JOINING:
		Debug.println(F("EV_JOINING"));
		break;
	case EV_JOINED:
		Debug.println(F("EV_JOINED"));
		{
			u4_t netid = 0;
			devaddr_t devaddr = 0;
			u1_t nwkKey[16];
			u1_t artKey[16];
			LMIC_getSessionKeys(&netid, &devaddr, nwkKey, artKey);
			Debug.print("netid: ");
			Debug.println(netid, DEC);
			Debug.print("devaddr: ");
			Debug.println(devaddr, HEX);
			Debug.print("AppSKey: ");
			for (size_t i = 0; i < sizeof(artKey); ++i)
			{
				if (i != 0)
					Debug.print("-");
				printHex2(artKey[i]);
			}
			Debug.println("");
			Debug.print("NwkSKey: ");
			for (size_t i = 0; i < sizeof(nwkKey); ++i)
			{
				if (i != 0)
					Debug.print("-");
				printHex2(nwkKey[i]);
			}
			Debug.println();
		}
		// Disable link check validation (automatically enabled
		// during join, but because slow data rates change max TX
		// size, we don't use it in this example.
		// LMIC_setLinkCheckMode(0);

		lora_connection_lost = false;
		break;
	/*
		|| This event is defined but not used in the code. No
		|| point in wasting codespace on it.
		||
		|| case EV_RFU1:
		||     Debug.println(F("EV_RFU1"));
		||     break;
		*/
	case EV_JOIN_FAILED:
		Debug.println(F("EV_JOIN_FAILED")); //lora_connection_lost = true;
		//lora_connection_lost = true;
		break;
	case EV_REJOIN_FAILED:
		Debug.println(F("EV_REJOIN_FAILED")); //lora_connection_lost = true;
		//lora_connection_lost = true;
		break;
	case EV_TXCOMPLETE:
		Debug.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
		if (LMIC.txrxFlags & TXRX_ACK)
		{
			Debug.println(F("Received ack"));
			lora_connection_lost = false;
		}
		else
		{
			Debug.println(F("Ack not received"));
			lora_connection_lost = true;
		}

		if (LMIC.dataLen)
		{
			Debug.print(F("Received "));
			Debug.print(LMIC.dataLen);
			Debug.println(F(" bytes of payload"));

			if (cfg::display_forecast)
			{
				Debug.println(F("Downlink payload:"));
				for (int i = 0; i < LMIC.dataLen; i++)
				{
					Debug.print(" ");
					Debug.print(LMIC.frame[LMIC.dataBeg + i], HEX);
					arrayDownlink[i] = LMIC.frame[LMIC.dataBeg + i];
					if (i == 4)
					{
						Debug.printf("\n");
						getDataLora(arrayDownlink);
					}
				}
			}
		}

		lora_connection_lost = false;
		// Schedule next transmission
		os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(TX_INTERVAL), do_send);
		Debug.println(F("Next transmission scheduled"));
		//maybe boolean here to prevent problem if wifi transmission starts...
		break;
	case EV_LOST_TSYNC:
		Debug.println(F("EV_LOST_TSYNC"));
		break;
	case EV_RESET:
		Debug.println(F("EV_RESET"));
		break;
	case EV_RXCOMPLETE:
		// data received in ping slot
		Debug.println(F("EV_RXCOMPLETE"));
		break;
	case EV_LINK_DEAD:
		Debug.println(F("EV_LINK_DEAD"));
		break;
	case EV_LINK_ALIVE:
		Debug.println(F("EV_LINK_ALIVE"));
		break;
	/*
		|| This event is defined but not used in the code. No
		|| point in wasting codespace on it.
		||
		|| case EV_SCAN_FOUND:
		||    Debug.println(F("EV_SCAN_FOUND"));
		||    break;
		*/
	case EV_TXSTART:
		Debug.println(F("EV_TXSTART"));
		break;
	case EV_TXCANCELED:
		Debug.println(F("EV_TXCANCELED"));
		break;
	case EV_RXSTART:
		/* do not print anything -- it wrecks timing */
		break;
	case EV_JOIN_TXCOMPLETE:
		Debug.println(F("EV_JOIN_TXCOMPLETE: no JoinAccept"));
		lora_connection_lost = true;
		break;

	default:
		Debug.print(F("Unknown event: "));
		Debug.println((unsigned)ev);
		break;
	}
}

static void prepareTxFrame()
{

	//Take care of the endianess in the byte array!

	// 00 00 00 c3
	// C3 00 00 00 = -128.0 in Little Endian

	// 00 00 80 bf
	// bf 80 00 00 = -1.0 in Little Endian

	union int16_2_byte
	{
		int16_t temp_int;
		byte temp_byte[2];
	} u1;

	union uint16_2_byte
	{
		uint16_t temp_uint;
		byte temp_byte[2];
	} u2;

	union float_2_byte
	{
		float temp_float;
		byte temp_byte[4];
	} u3;

	//Take care of the signed/unsigned and endianess

	//Inverser ordre pour les int16_t !

	//datalora[0] is already defined and is 1 byte

	if (wifi_connection_lost && cfg::has_wifi)
	{
		configlorawan[7] = false;
		datalora[0] = booltobyte(configlorawan); //wifi perdu et lora connecté
	}

	if (!wifi_connection_lost && cfg::has_wifi)
	{
		configlorawan[7] = true;
		datalora[0] = booltobyte(configlorawan); //wifi OK et lora connecté => priorité wifi
	}

	//x10 to get 1 decimal for PM

	if (last_value_SDS_P1 != -1.0)
		u1.temp_int = (int16_t)round(last_value_SDS_P1 * 10);
	else
		u1.temp_int = (int16_t)round(last_value_SDS_P1);

	datalora[1] = u1.temp_byte[1];
	datalora[2] = u1.temp_byte[0];

	if (last_value_SDS_P2 != -1.0)
		u1.temp_int = (int16_t)round(last_value_SDS_P2 * 10);
	else
		u1.temp_int = (int16_t)round(last_value_SDS_P2);

	datalora[3] = u1.temp_byte[1];
	datalora[4] = u1.temp_byte[0];

	if (last_value_NPM_P0 != -1.0)
		u1.temp_int = (int16_t)round(last_value_NPM_P0 * 10);
	else
		u1.temp_int = (int16_t)round(last_value_NPM_P0);

	datalora[5] = u1.temp_byte[1];
	datalora[6] = u1.temp_byte[0];

	if (last_value_NPM_P1 != -1.0)
		u1.temp_int = (int16_t)round(last_value_NPM_P1 * 10);
	else
		u1.temp_int = (int16_t)round(last_value_NPM_P1);

	datalora[7] = u1.temp_byte[1];
	datalora[8] = u1.temp_byte[0];

	if (last_value_NPM_P2 != -1.0)
		u1.temp_int = (int16_t)round(last_value_NPM_P2 * 10);
	else
		u1.temp_int = (int16_t)round(last_value_NPM_P2);

	datalora[9] = u1.temp_byte[1];
	datalora[10] = u1.temp_byte[0];

	if (last_value_NPM_N1 != -1.0)
		u1.temp_int = (int16_t)round(last_value_NPM_N1 * 1000);
	else
		u1.temp_int = (int16_t)round(last_value_NPM_N1);

	datalora[11] = u1.temp_byte[1];
	datalora[12] = u1.temp_byte[0];

	if (last_value_NPM_N10 != -1.0)
		u1.temp_int = (int16_t)round(last_value_NPM_N10 * 1000);
	else
		u1.temp_int = (int16_t)round(last_value_NPM_N10);

	datalora[13] = u1.temp_byte[1];
	datalora[14] = u1.temp_byte[0];

	if (last_value_NPM_N25 != -1.0)
		u1.temp_int = (int16_t)round(last_value_NPM_N25 * 1000);
	else
		u1.temp_int = (int16_t)round(last_value_NPM_N25);

	datalora[15] = u1.temp_byte[1];
	datalora[16] = u1.temp_byte[0];

	u1.temp_int = (int16_t)round(last_value_MHZ16);

	datalora[17] = u1.temp_byte[1];
	datalora[18] = u1.temp_byte[0];

	u1.temp_int = (int16_t)round(last_value_MHZ19);

	datalora[19] = u1.temp_byte[1];
	datalora[20] = u1.temp_byte[0];

	u1.temp_int = (int16_t)round(last_value_CCS811);

	datalora[21] = u1.temp_byte[1];
	datalora[22] = u1.temp_byte[0];

	if (last_value_BMX280_T != -128.0)
		u1.temp_int = (int16_t)round(last_value_BMX280_T * 10);
	else
		u1.temp_int = (int16_t)round(last_value_BMX280_T);

	datalora[23] = u1.temp_byte[1];
	datalora[24] = u1.temp_byte[0];

	//datalora[23] = (int8_t)round(last_value_BMX280_T);

	datalora[25] = (int8_t)round(last_value_BME280_H);

	u1.temp_int = (int16_t)round(last_value_BMX280_P);

	datalora[26] = u1.temp_byte[1];
	datalora[27] = u1.temp_byte[0];

	u3.temp_float = atof(cfg::latitude);

	datalora[28] = u3.temp_byte[0];
	datalora[29] = u3.temp_byte[1];
	datalora[30] = u3.temp_byte[2];
	datalora[31] = u3.temp_byte[3];

	u3.temp_float = atof(cfg::longitude);

	datalora[32] = u3.temp_byte[0];
	datalora[33] = u3.temp_byte[1];
	datalora[34] = u3.temp_byte[2];
	datalora[35] = u3.temp_byte[3];

	datalora[36] = forecast_selector;

	Debug.printf("HEX values:\n");
	for (int i = 0; i < 37; i++)
	{
		Debug.printf(" %02x", datalora[i]);
		if (i == 36)
		{
			Debug.printf("\n");
		}
	}
}

bool lorachip;
bool loratest(int lora_dio0)
{
	pinMode(lora_dio0, INPUT_PULLUP);
	delay(200);
	if (!digitalRead(lora_dio0))
	{ // low => LoRa chip detected
		return true;
	}
	return false;
}

/*****************************************************************
 * Check stack                                                    *
 *****************************************************************/
void *StackPtrAtStart;
void *StackPtrEnd;
UBaseType_t watermarkStart;

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
	Debug.println(F("Starting ESP32 COM"));

	Debug.printf("\r\n\r\nAddress of Stackpointer near start is:  %p \r\n", (void *)StackPtrAtStart);
	Debug.printf("End of Stack is near: %p \r\n", (void *)StackPtrEnd);
	Debug.printf("Free Stack at start of setup() is:  %d \r\n", (uint32_t)StackPtrAtStart - (uint32_t)StackPtrEnd);

	esp_chipid = String((uint16_t)(ESP.getEfuseMac() >> 32), HEX); // for esp32
	esp_chipid += String((uint32_t)ESP.getEfuseMac(), HEX);
	esp_chipid.toUpperCase();
	cfg::initNonTrivials(esp_chipid.c_str());
	WiFi.persistent(false);

	debug_outln_info(F("ModuleAirV3: " SOFTWARE_VERSION_STR "/"), String(CURRENT_LANG));

	init_config();

	// spiffs_matrix = cfg::has_matrix; //save the spiffs state on start

	// Debug.println("spiffs_matrix: ");
	// Debug.println(spiffs_matrix);

	Wire.begin(I2C_PIN_SDA, I2C_PIN_SCL);
	Wire1.begin(I2C_PIN_SDA_2, I2C_PIN_SCL_2); // For RTC

	lorachip = loratest(D26); // test if the LoRa module is connected when LoRaWAN option checked, otherwise freeze...
	Debug.print("Lora chip connected:");
	Debug.println(lorachip);

	debug_outln_info(F("\nChipId: "), esp_chipid);

	if (cfg::has_wifi)
	{
		setupNetworkTime();
		connectWifi();
		setup_webserver();
	}
	else
	{
		wifiConfig();
	}

	createLoggerConfigs();
	logEnabledAPIs();
	delay(50);

	starttime = millis(); // store the start time
	last_update_attempt = time_point_device_start_ms = starttime;


	if (cfg::has_lora && lorachip)
	{

		ToByteArray();

		Debug.printf("APPEUI:\n");
		for (int i = 0; i < 8; i++)
		{
			Debug.printf(" %02x", appeui_hex[i]);
			if (i == 7)
			{
				Debug.printf("\n");
			}
		}

		Debug.printf("DEVEUI:\n");
		for (int i = 0; i < 8; i++)
		{
			Debug.printf(" %02x", deveui_hex[i]);
			if (i == 7)
			{
				Debug.printf("\n");
			}
		}

		Debug.printf("APPKEY:\n");
		for (int i = 0; i < 16; i++)
		{
			Debug.printf(" %02x", appkey_hex[i]);
			if (i == 15)
			{
				Debug.printf("\n");
			}
		}
		// LMIC init
		os_init();

		// Reset the MAC state. Session and pending data transfers will be discarded.
		LMIC_reset();

		// Start job (sending automatically starts OTAA too)
		do_send(&sendjob); // values are -1, -128 etc. they can be easily filtered
	}

	if (cfg::display_forecast)
	{
		forecast_selector = 0; //initialisation after first LoRaWAN payload
	}

	// Prepare the configuration summary for the following messages (the first is 00000000)

	//VOIR SI CONFIG SI_USER 2 bytes

	//Ajouter ethernet + SD

	configlorawan[0] = cfg::sds_read;
	configlorawan[1] = cfg::npm_read;
	configlorawan[2] = cfg::bmx280_read;
	configlorawan[3] = cfg::mhz16_read;
	configlorawan[4] = cfg::mhz19_read;
	configlorawan[5] = cfg::ccs811_read;
	configlorawan[6] = cfg::display_forecast;
	configlorawan[7] = cfg::has_wifi; //si connection manquée => false

	//IL va falloir ajouter un byte pour RGPD?

	Debug.print("Configuration:");
	Debug.println(booltobyte(configlorawan));
	datalora[0] = booltobyte(configlorawan);

	Debug.printf("End of void setup()\n");
	time_end_setup = millis();


	if (! rtc.begin()) { //ATTENTION FORCER Wire1 dans la lib!!!
	Debug.println("Couldn't find RTC");
	}

	String cfgName(F("/config.json"));
	File configFile = SPIFFS.open(cfgName, "r");
	
	debug_outln_info(F("opened config file for i2c..."));
	datai2c = configFile.readString();
	Debug.println(datai2c);
	configFile.close();

        WirePacker packer;
        packer.write(datai2c.c_str());
        packer.end();

        // now transmit the packed data
        Wire.beginTransmission(I2C_SLAVE_ADDR);
        while (packer.available()) {    // write every packet byte
            Wire.write(packer.read());
        }
        Wire.endTransmission();         // stop transmitting

}

void loop()
{


//now = rtc.now();
// Debug.print(now.year(), DEC);
// Debug.print("-");
// Debug.print(now.month(), DEC);
// Debug.print("-");
// Debug.print(now.day(), DEC);
// Debug.print("-");
// Debug.print(daysOfTheWeek[now.dayOfTheWeek()]);
// Debug.print("/");
// Debug.print(now.hour(), DEC);
// Debug.print(":");
// Debug.print(now.minute(), DEC);
// Debug.print(":");
// Debug.println(now.second(), DEC);


	unsigned sum_send_time = 0;

	act_micro = micros();
	act_milli = millis();
	send_now = msSince(starttime) > cfg::sending_intervall_ms;

	// Wait at least 30s for each NTP server to sync

	if (cfg::has_wifi && !wifi_connection_lost)
	{
		if (!sntp_time_set && send_now && msSince(time_point_device_start_ms) < 1000 * 2 * 30 + 5000)
		{
			debug_outln_info(F("NTP sync not finished yet, skipping send"));
			send_now = false;
			starttime = act_milli;
		}
	}

	sample_count++;

	if (last_micro != 0)
	{
		unsigned long diff_micro = act_micro - last_micro;
		UPDATE_MIN_MAX(min_micro, max_micro, diff_micro);
	}
	last_micro = act_micro;

	//if (cfg::has_wifi && WiFi.waitForConnectResult(10000) == WL_CONNECTED)
	//if (cfg::has_wifi && !wifi_connection_lost)
	if (cfg::has_wifi && WiFi.status() == WL_CONNECTED)
	{
		server.handleClient();
		yield();
	}

	if (send_now && cfg::sending_intervall_ms >= 120000)
	{

		void *SpActual = NULL;
		Debug.printf("Free Stack at send_now is: %d \r\n", (uint32_t)&SpActual - (uint32_t)StackPtrEnd);

		if (cfg::has_wifi && !wifi_connection_lost)
		{
			last_signal_strength = WiFi.RSSI();
		}


		//send config to esp32 slave then receive




		RESERVE_STRING(data, LARGE_STR);
		data = FPSTR(data_first_part);
		RESERVE_STRING(result, MED_STR);

		//GET FROM I2C

		// if (cfg::sds_read)
		// {
		// 	data += result_SDS;
		// 	if (cfg::has_wifi && !wifi_connection_lost)
		// 	{
		// 		sum_send_time += sendSensorCommunity(result_SDS, SDS_API_PIN, FPSTR(SENSORS_SDS011), "SDS_");
		// 	}
		// }
		// if (cfg::npm_read)
		// {
		// 	data += result_NPM;
		// 	if (cfg::has_wifi && !wifi_connection_lost)
		// 	{
		// 		sum_send_time += sendSensorCommunity(result_NPM, NPM_API_PIN, FPSTR(SENSORS_NPM), "NPM_");
		// 	}
		// }

		// if (cfg::bmx280_read && (!bmx280_init_failed))
		// {
		// 	fetchSensorBMX280(result);
		// 	data += result;
		// 	if (bmx280.sensorID() == BME280_SENSOR_ID)
		// 	{
		// 		if (cfg::has_wifi && !wifi_connection_lost)
		// 		{
		// 			sum_send_time += sendSensorCommunity(result, BME280_API_PIN, FPSTR(SENSORS_BME280), "BME280_");
		// 		}
		// 	}
		// 	else
		// 	{
		// 		if (cfg::has_wifi && !wifi_connection_lost)
		// 		{
		// 			sum_send_time += sendSensorCommunity(result, BMP280_API_PIN, FPSTR(SENSORS_BMP280), "BMP280_");
		// 		}
		// 	}
		// 	result = emptyString;
		// }

		// //These values are not sent because not configured in the SC API:

		// if (cfg::mhz16_read)
		// {
		// 	data += result_MHZ16;
		// }

		// if (cfg::mhz19_read)
		// {
		// 	data += result_MHZ19;
		// }

		// if (cfg::ccs811_read && (!ccs811_init_failed))
		// {
		// 	data += result_CCS811;
		// }

		add_Value2Json(data, F("samples"), String(sample_count));
		add_Value2Json(data, F("min_micro"), String(min_micro));
		add_Value2Json(data, F("max_micro"), String(max_micro));
		add_Value2Json(data, F("interval"), String(cfg::sending_intervall_ms));
		add_Value2Json(data, F("signal"), String(last_signal_strength));
		add_Value2Json(data, F("latitude"), String(cfg::latitude));
		add_Value2Json(data, F("longitude"), String(cfg::longitude));

		if ((unsigned)(data.lastIndexOf(',') + 1) == data.length())
		{
			data.remove(data.length() - 1);
		}
		data += "]}";

		yield();

		if (cfg::has_wifi && !wifi_connection_lost)
		{
			sum_send_time += sendDataToOptionalApis(data);

			//json example for WiFi transmission

			//{"software_version" : "ModuleAirV2-V1-122021", "sensordatavalues" :
			//[ {"value_type" : "NPM_P0", "value" : "1.84"},
			//{"value_type" : "NPM_P1", "value" : "2.80"},
			//{"value_type" : "NPM_P2", "value" : "2.06"},
			//{"value_type" : "NPM_N1", "value" : "27.25"},
			//{"value_type" : "NPM_N10", "value" : "27.75"},
			//{"value_type" : "NPM_N25", "value" : "27.50"},
			//{"value_type" : "BME280_temperature", "value" : "20.84"},
			//{"value_type" : "BME280_pressure", "value" : "99220.03"},
			//{"value_type" : "BME280_humidity", "value" : "61.66"},
			//{"value_type" : "samples", "value" : "138555"},
			//{"value_type" : "min_micro", "value" : "933"},
			//{"value_type" : "max_micro", "value" : "351024"},
			//{"value_type" : "interval", "value" : "145000"},
			//{"value_type" : "signal", "value" : "-71"}
			//{"value_type" : "latitude", "value" : "43.2964"},
			//{"value_type" : "longitude", "value" : "5.36978"}
			// ]}

			// https://en.wikipedia.org/wiki/Moving_average#Cumulative_moving_average
			sending_time = (3 * sending_time + sum_send_time) / 4;

			if (sum_send_time > 0)
			{
				debug_outln_info(F("Time for Sending (ms): "), String(sending_time));
			}

			//RECONNECT ETAIT ICI
		}

		if ((WiFi.status() != WL_CONNECTED || sending_time > 30000 || wifi_connection_lost) && cfg::has_wifi)
		{
			debug_outln_info(F("Connection lost, reconnecting "));
			WiFi_error_count++;
			WiFi.reconnect();
			waitForWifiToConnect(20);

			if (wifi_connection_lost && WiFi.waitForConnectResult(10000) != WL_CONNECTED)
			{
				Debug.println("Reconnect failed after WiFi.reconnect()");

				WiFi.disconnect(true, true);
				WiFi.mode(WIFI_STA);
				WiFi.setHostname(cfg::fs_ssid);
				WiFi.begin(cfg::wlanssid, cfg::wlanpwd); // Start WiFI again
			}
			debug_outln_info(emptyString);
		}

		// only do a restart after finishing sending (Wifi). Befor Lora to avoid conflicts with the LMIC
		if (msSince(time_point_device_start_ms) > DURATION_BEFORE_FORCED_RESTART_MS)
		{
			sensor_restart();
		}

		// Resetting for next sampling
		last_data_string = std::move(data);
		sample_count = 0;
		last_micro = 0;
		min_micro = 1000000000;
		max_micro = 0;
		sum_send_time = 0;

		if (cfg::display_forecast && cfg::has_wifi && !wifi_connection_lost) //the reception through LoRaWAN downlink is automatically done
		{
			switch (forecast_selector)
			{
			case 0:
				atmoSud.multi = getDataAtmoSud(forecast_selector);
				break;
			case 1:
				atmoSud.no2 = getDataAtmoSud(forecast_selector);
				break;
			case 2:
				atmoSud.o3 = getDataAtmoSud(forecast_selector);
				break;
			case 3:
				atmoSud.pm10 = getDataAtmoSud(forecast_selector);
				break;
			case 4:
				atmoSud.pm2_5 = getDataAtmoSud(forecast_selector);
				break;
			case 5:
				atmoSud.so2 = getDataAtmoSud(forecast_selector);
				break;
			}
		}
		else
		{
			switch (forecast_selector)
			{
			case 0:
				atmoSud.multi = -1.0;
				break;
			case 1:
				atmoSud.no2 = -1.0;
				break;
			case 2:
				atmoSud.o3 = -1.0;
				break;
			case 3:
				atmoSud.pm10 = -1.0;
				break;
			case 4:
				atmoSud.pm2_5 = -1.0;
				break;
			case 5:
				atmoSud.so2 = -1.0;
				break;
			}
		}

		if (cfg::has_lora && lorachip)
		{
			prepareTxFrame();
			// do_send(&sendjob);

			//os_run_loop_once here ?
			//boolean in EV_TX_COMPLETE to allow WiFi after?
		}

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

	if (cfg::has_lora && lorachip)
	{
		os_runloop_once();
		//place in the send now ? Let here to let Lora lib control itself
	}
}

