#define SENSOR_BASENAME "esp32-"

#define SSID_BASENAME "moduleair-"
#define HOSTNAME_BASE "moduleair-"

#define LEN_CONFIG_BYTE 29
#define LEN_DATA_BYTE 52
#define LEN_MATRIX_BYTE 69

// define debug levels
#define DEBUG_ERROR 1
#define DEBUG_WARNING 2
#define DEBUG_MIN_INFO 3
#define DEBUG_MED_INFO 4
#define DEBUG_MAX_INFO 5

/******************************************************************
 * Constants                                                      *
 ******************************************************************/
constexpr const unsigned long SAMPLETIME_MS = 30000;									// time between two measurements of the PPD42NS
constexpr const unsigned long SAMPLETIME_SDS_MS = 1000;								// time between two measurements of the SDS011, PMSx003, Honeywell PM sensor
constexpr const unsigned long WARMUPTIME_SDS_MS = 15000;								// time needed to "warm up" the sensor before we can take the first measurement
constexpr const unsigned long READINGTIME_SDS_MS = 5000;								// how long we read data from the PM sensors

constexpr const unsigned long SAMPLETIME_NPM_MS = 60000;

constexpr const unsigned long SAMPLETIME_MHZ16_MS = 10000;
constexpr const unsigned long SAMPLETIME_MHZ19_MS = 10000;
constexpr const unsigned long SAMPLETIME_CCS811_MS = 10000;
constexpr const unsigned long SAMPLETIME_Cairsens_MS = 10000;


constexpr const unsigned long SAMPLETIME_GPS_MS = 50;
constexpr const unsigned long DISPLAY_UPDATE_INTERVAL_MS = 5000;						// time between switching display to next "screen"
constexpr const unsigned long ONE_DAY_IN_MS = 24 * 60 * 60 * 1000;
constexpr const unsigned long PAUSE_BETWEEN_UPDATE_ATTEMPTS_MS = ONE_DAY_IN_MS;		// check for firmware updates once a day
constexpr const unsigned long DURATION_BEFORE_FORCED_RESTART_MS = ONE_DAY_IN_MS * 28;	// force a reboot every ~4 weeks

//GPIO Pins
// the IO pins which can be used for what depends on the following:
//   - The board which is used
//     - onboard peripherials like LCD or LoRa chips which already occupy an IO pin
//     - the ESP32 module which is used
//         - the WROVER board uses the IOs 16 and 17 to access the PSRAW
//         - on WROOM boards the IOs 16 and 17 can be freely used
//   - if JTAG debugging shall be used
//   - some IOs have constraints
//     - configuration of ESP32 module configuration options ("strapping") like operating voltage and boot medium
//     - some IOs can only be used for inputs (34, 35, 36, 39)
// see https://randomnerdtutorials.com/esp32-pinout-reference-gpios/
//     https://github.com/va3wam/TWIPi/blob/master/Eagle/doc/feather-pinout-map.pdf

#if defined(ARDUINO_ESP32_DEV) and defined(KIT_C)
#define D0 0
#define D1 1
#define D2 2
#define D3 3
#define D4 4
#define D5 5
#define D12 12
#define D13 13
#define D14 14
#define D15 15
#define D16 16
#define D17 17
#define D18 18
#define D19 19
#define D21 21
#define D22 22
#define D23 23
#define D25 25
#define D26 26
#define D27 27
#define D32 32
#define D33 33
#define D34 34
#define D35 35
#define D36 36
#define D39 39
#endif