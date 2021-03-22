#include <Arduino.h>
#include <LiquidCrystal.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>

#include <HardwareSerial.h>
#include <WiFiClientSecure.h>
#include "GatewayIntents.h"
#include "WebSocketClient.h"
#include "libs/ArduinoJson.h"
#include "time.h"
#include "Passwords.h" //passwords, tokens & ids are defined here

/*
 * All defines
 */
//LED DEFINES
#define GREEN_LED 2
#define BLUE_LED 4
#define RED_LED 15
#define GREEN_CHANNEL 0
#define BLUE_CHANNEL 1
#define RED_CHANNEL 2
#define LED_MODE_COUNT 5
#define GENTLE_FLASH_DELAY_TIME 1000
#define FAST_FLASH_DELAY_TIME 250
#define RAINBOW_DELAY_TIME 10
//PUSHBUTTON DEFINES
#define PUSHBUTTON_INPUT 13
//DSCRD DEFINES
#define GUILD_COMMAND_URL "/api/v8/applications/" APPLICATION_ID "/guilds/" GUILD_ID "/commands"
#define GLOBAL_COMMAND_URL "/api/v8/applications/" APPLICATION_ID "/commands"
#define INTERACTION_URL_A "/api/v8/interactions/"
#define INTERACTION_URL_B "/callback" 
#define FOLLOWUP_ENDPOINT "/api/v8/webhooks/" APPLICATION_ID "/"
#define COMMAND_ACK "{\"type\":5}" //basic acknowledgement of interaction
#define POSITIVE_COMMAND_MESSAGE "Got it!"
////LCD DEFINES
#define RS_PIN 22
#define EN_PIN 23
#define LCD_D4 21 
#define LCD_D5 19 //looks like a 13!
#define LCD_D6 18
#define LCD_D7 5
//NOTIFICATION DEFINES
#define SYSTEM_NOTIFICATION_DISPLAY_DURATION 5000 //5 seconds
#define ARROW_ICON_STR "~"
#define NOTIFICATION_ICON_COUNT 3
#define STANDARD_NOTIFICATION_COLOR "FF0000" //red
//MODE DEFINES //0 = clock //1 = note //2 = stock //3 = timer //4 = animation 
#define DEFAULT_MODE 0
//CLOCK
#define CLOCK_UPDATE_TIME 5000 //every 5 seconds for now
#define CITY_COUNTRY "Boechout, Belgium"
#define OPEN_WEATHER_API_KEY "c408d3f777d8e4ad81fe4bfce26397e0"
#define WEATHER_UPDATE_TIME 300000 //every 5 minutes
//STOCKS
#define DEFAULT_STOCK "GME"
#define STOCKS_UPDATE_TIME 30000 //every 30 seconds
//NOTES
#define DEFAULT_NOTE "Empty Note"
//TIMER + COUNTDOWN
#define TIMER_UPDATE_TIME 500 //every half a second
#define TIMER_NOTIFICATION_COLOR "0000FF" //blue
#define COUNTDOWN_DEFAULT_TARGET 15000 //3675000 //millis for 1 hour, 1 minute and 15 seconds
//ANIMATION
#define DEFAULT_FRAME_DELAY 1000
#define DEFAULT_FRAME_ONE "++++++++++++++++++++++++++++++++"
#define DEFAULT_FRAME_TWO "--------------------------------"

/*
 * Define methods
 */
void setup_pushbutton();
void check_pushbutton();
void setup_wifi();
void setup_led();
void setRGB(int r, int g, int b);
int setHex(String hex);
void setup_lcd();
void handleCommand();
String general_https_request(String requestType, const char *hostStr, String urlStr, String extraHeaderStr, String body);
String json_https_request(String requestType, const char *hostStr, String urlStr, String extraHeaderStr, String jsonStr);
void setup_commands();
void handle_pending_notification();
int createNotification(String message, int ledMode, String ledColor, int notificationIcon, bool isSystemNotification, bool systemNotificationRequiresButtonPress);
void sendRemainingNotificationMessage();
void followUpCommand(String message);
bool checkValidHex(String hex);
void handleLedMode();
void ledFlash(String hexColor, int delayTime);
bool transitionTo(int targetR, int targetG, int targetB);
void ledRainbow(int delayTime);
void handleMode();
void handleClock();
String getTemp();
void handleStocks();
void getStock();
void handleNote();
void handleTimer();
String millisToString(unsigned long millisec);
void handleAnimation();
//LCD wrapper & helper functions
void lcdClear();
void lcdSetCursor(int col, int row);
void lcdPrint(String unformattedStr);
int lcdLength(String unformattedStr);
String lcdSubstring(String unformattedStr, int from, int to); //bit abstract but it gets a substring from the unformattedString using the from and to values you would use if the string would be already formatted.
String lcdFormat(String unformattedStr);
String intToEscapeCharacter(int digit);
int intArrayContains(int intArray[], int value);

//void generateBitmapImage();
void generateBitmapImage (byte* imageData, int width, int height, int resizeFactor, bool useGreenTable);
unsigned char* createBitmapFileHeader(int height, int stride);
unsigned char* createBitmapInfoHeader(int height, int width);
void getImgurAccessToken();

// Function to copy 'len' elements from 'src' to 'dst'
void copy(const uint8_t* src, uint8_t* dst, int len) {
    memcpy(dst, src, sizeof(src[0])*len);
}

/*
 * All fields
 */
//PUSHBUTTON FIELDS
bool pushbuttonPressHandled = true; //global signal to notify others that the button has been pressed. Must be set to false by the system using it
bool pushbuttonIsAlreadyPushed = false;

//LCD FIELDS
LiquidCrystal lcd(RS_PIN, EN_PIN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
//basically virtual DDRAM to keep track of formatted characters on screen
String virtualLCDTop    = "                                "; //32 chars per row to be sure
String virtualLCDBottom = "                                ";
int virtualCursorCol = 0;
int virtualCursorRow = 0;
int customChar = 1; //0 is used by system
byte customChars[440] = {0x0, 0x0, 0xa, 0x15,0x11,0xa, 0x4, 0x0, // ==((1)) empty heart  //440 == 8rows * 50 characters  + 5 extra characters (used for system) (40 = 8*5) 
                         0x0, 0x0, 0xa, 0x1f,0x1f,0xe, 0x4, 0x0, // ==((2)) full heart
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                         0x1, 0x3, 0x3, 0x7, 0x7, 0x10,0x1f,0x1e,//koijam pt1 ((47))
                         0x1c,0x1c,0xe, 0xe, 0xe, 0xc, 0x1c,0x18,//koijam pt2 ((48))
                         0x0, 0x0, 0xa, 0x0, 0x11,0xe, 0x0, 0x0, // ==((49)) happy smiley
                         0x0, 0x0, 0xa, 0x0, 0xe, 0x11,0x0, 0x0, // ==((50)) unhappy smiley
                         0x0E,0x1B,0x11,0x11,0x00,0x1F,0x1B,0x0E,//system characters start here --> notification bell
                         0x0E,0x1B,0x1B,0x1B,0x1B,0x1F,0x1B,0x0E,//warning icon
                         0xe, 0x1f,0x15,0x17,0x11,0x1f,0xe, 0xe, //clock/watch icon
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, //placeholder
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0  //placeholder
                         };
byte virtualCGRAM[64] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, -0x1,  //copy of the cgram  8*8 = 64 //used for comparing if a character has already been saved
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, -0x1, //-1 so it's different than seemingly already defines as a blank character
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, -0x1,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, -0x1,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, -0x1,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, -0x1,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, -0x1,
                         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, -0x1}; //TODO test extra
const byte virtualCGROM[256][8] = {{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x04,0x04,0x04,0x04,0x00,0x00,0x04,0x00},
                                  {0x0A,0x0A,0x0A,0x00,0x00,0x00,0x00,0x00},
                                  {0x0A,0x0A,0x1F,0x0A,0x1F,0x0A,0x0A,0x00},
                                  {0x04,0x0F,0x14,0x0E,0x05,0x1E,0x04,0x00},
                                  {0x18,0x19,0x02,0x04,0x08,0x13,0x03,0x00},
                                  {0x0C,0x12,0x14,0x08,0x15,0x12,0x0D,0x00},
                                  {0x0C,0x04,0x08,0x00,0x00,0x00,0x00,0x00},
                                  {0x02,0x04,0x08,0x08,0x08,0x04,0x02,0x00},
                                  {0x08,0x04,0x02,0x02,0x02,0x04,0x08,0x00},
                                  {0x00,0x04,0x15,0x0E,0x15,0x04,0x00,0x00},
                                  {0x00,0x04,0x04,0x1F,0x04,0x04,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x0C,0x04,0x08,0x00},
                                  {0x00,0x00,0x00,0x1F,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00},
                                  {0x00,0x01,0x02,0x04,0x08,0x10,0x00,0x00},
                                  {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E,0x00},
                                  {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E,0x00},
                                  {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F,0x00},
                                  {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E,0x00},
                                  {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02,0x00},
                                  {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E,0x00},
                                  {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E,0x00},
                                  {0x1F,0x11,0x01,0x02,0x04,0x04,0x04,0x00},
                                  {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E,0x00},
                                  {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C,0x00},
                                  {0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x00,0x00},
                                  {0x00,0x0C,0x0C,0x00,0x0C,0x04,0x08,0x00},
                                  {0x02,0x04,0x08,0x10,0x08,0x04,0x02,0x00},
                                  {0x00,0x00,0x1F,0x00,0x1F,0x00,0x00,0x00},
                                  {0x08,0x04,0x02,0x01,0x02,0x04,0x08,0x00},
                                  {0x0E,0x11,0x01,0x02,0x04,0x00,0x04,0x00},
                                  {0x0E,0x11,0x01,0x0D,0x15,0x15,0x0E,0x00},
                                  {0x0E,0x11,0x11,0x11,0x1F,0x11,0x11,0x00},
                                  {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E,0x00},
                                  {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E,0x00},
                                  {0x1C,0x12,0x11,0x11,0x11,0x12,0x1C,0x00},
                                  {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F,0x00},
                                  {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10,0x00},
                                  {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F,0x00},
                                  {0x11,0x11,0x11,0x1F,0x11,0x11,0x11,0x00},
                                  {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E,0x00},
                                  {0x07,0x02,0x02,0x02,0x02,0x12,0x0C,0x00},
                                  {0x11,0x12,0x14,0x18,0x14,0x12,0x11,0x00},
                                  {0x10,0x10,0x10,0x10,0x10,0x10,0x1F,0x00},
                                  {0x11,0x1B,0x15,0x15,0x11,0x11,0x11,0x00},
                                  {0x11,0x11,0x19,0x15,0x13,0x11,0x11,0x00},
                                  {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E,0x00},
                                  {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10,0x00},
                                  {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D,0x00},
                                  {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11,0x00},
                                  {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E,0x00},
                                  {0x1F,0x04,0x04,0x04,0x04,0x04,0x04,0x00},
                                  {0x11,0x11,0x11,0x11,0x11,0x11,0x0E,0x00},
                                  {0x11,0x11,0x11,0x11,0x11,0x0A,0x04,0x00},
                                  {0x11,0x11,0x11,0x15,0x15,0x15,0x0A,0x00},
                                  {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11,0x00},
                                  {0x11,0x11,0x11,0x0A,0x04,0x04,0x04,0x00}, //Y
                                  {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F,0x00},
                                  {0x0E,0x08,0x08,0x08,0x08,0x08,0x0E,0x00},
                                  {0x11,0x0A,0x1F,0x04,0x1F,0x04,0x04,0x00},
                                  {0x0E,0x02,0x02,0x02,0x02,0x02,0x0E,0x00},
                                  {0x04,0x0A,0x11,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x1F,0x00},
                                  {0x08,0x04,0x02,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x0E,0x01,0x0F,0x11,0x0F,0x00},
                                  {0x10,0x10,0x16,0x19,0x11,0x11,0x1E,0x00},
                                  {0x00,0x00,0x0E,0x10,0x10,0x11,0x0E,0x00},
                                  {0x01,0x01,0x0D,0x13,0x11,0x11,0x0F,0x00},
                                  {0x00,0x00,0x0E,0x11,0x1F,0x10,0x0E,0x00},
                                  {0x06,0x09,0x08,0x1C,0x08,0x08,0x08,0x00},
                                  {0x00,0x0F,0x11,0x11,0x0F,0x01,0x0E,0x00},
                                  {0x10,0x10,0x16,0x19,0x11,0x11,0x11,0x00},
                                  {0x04,0x00,0x0C,0x04,0x04,0x04,0x0E,0x00},
                                  {0x02,0x00,0x06,0x02,0x02,0x12,0x0C,0x00},
                                  {0x10,0x10,0x12,0x14,0x18,0x14,0x12,0x00},
                                  {0x0C,0x04,0x04,0x04,0x04,0x04,0x0E,0x00},
                                  {0x00,0x00,0x1A,0x15,0x15,0x11,0x11,0x00},
                                  {0x00,0x00,0x16,0x19,0x11,0x11,0x11,0x00},
                                  {0x00,0x00,0x0E,0x11,0x11,0x11,0x0E,0x00},
                                  {0x00,0x00,0x1E,0x11,0x1E,0x10,0x10,0x00},
                                  {0x00,0x00,0x0D,0x13,0x0F,0x01,0x01,0x00},
                                  {0x00,0x00,0x16,0x19,0x10,0x10,0x10,0x00},
                                  {0x00,0x00,0x0E,0x10,0x0E,0x01,0x1E,0x00},
                                  {0x08,0x08,0x1C,0x08,0x08,0x09,0x06,0x00},
                                  {0x00,0x00,0x11,0x11,0x11,0x13,0x0D,0x00},
                                  {0x00,0x00,0x11,0x11,0x11,0x0A,0x04,0x00},
                                  {0x00,0x00,0x11,0x15,0x15,0x15,0x0A,0x00},
                                  {0x00,0x00,0x11,0x0A,0x04,0x0A,0x11,0x00},
                                  {0x00,0x00,0x11,0x11,0x0F,0x01,0x0E,0x00},
                                  {0x00,0x00,0x1F,0x02,0x04,0x08,0x1F,0x00},
                                  {0x02,0x04,0x04,0x08,0x04,0x04,0x02,0x00},
                                  {0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x00},
                                  {0x08,0x04,0x04,0x02,0x04,0x04,0x08,0x00},
                                  {0x00,0x04,0x02,0x1F,0x02,0x04,0x00,0x00},
                                  {0x00,0x04,0x08,0x1F,0x08,0x04,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x1C,0x14,0x1C,0x00},
                                  {0x07,0x04,0x04,0x04,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x00,0x04,0x04,0x04,0x1C,0x00},
                                  {0x00,0x00,0x00,0x00,0x10,0x08,0x04,0x00},
                                  {0x00,0x00,0x00,0x0C,0x0C,0x00,0x00,0x00},
                                  {0x00,0x1F,0x01,0x1F,0x01,0x02,0x04,0x00},
                                  {0x00,0x00,0x1F,0x01,0x06,0x04,0x08,0x00},
                                  {0x00,0x00,0x02,0x04,0x0C,0x14,0x04,0x00},
                                  {0x00,0x00,0x04,0x1F,0x11,0x01,0x06,0x00},
                                  {0x00,0x00,0x1F,0x04,0x04,0x04,0x1F,0x00},
                                  {0x00,0x00,0x02,0x1F,0x06,0x0A,0x12,0x00},
                                  {0x00,0x00,0x08,0x1F,0x09,0x0A,0x08,0x00},
                                  {0x00,0x00,0x00,0x0E,0x02,0x02,0x1F,0x00},
                                  {0x00,0x00,0x1E,0x02,0x1E,0x02,0x1E,0x00},
                                  {0x00,0x00,0x00,0x15,0x15,0x01,0x06,0x00},
                                  {0x00,0x00,0x00,0x1F,0x00,0x00,0x00,0x00},
                                  {0x1F,0x01,0x05,0x06,0x04,0x04,0x08,0x00},
                                  {0x01,0x02,0x04,0x0C,0x14,0x04,0x04,0x00},
                                  {0x04,0x1F,0x11,0x11,0x01,0x02,0x04,0x00},
                                  {0x00,0x1F,0x04,0x04,0x04,0x04,0x1F,0x00},
                                  {0x02,0x1F,0x02,0x06,0x0A,0x12,0x02,0x00},
                                  {0x08,0x1F,0x09,0x09,0x09,0x09,0x12,0x00},
                                  {0x04,0x1F,0x04,0x1F,0x04,0x04,0x04,0x00},
                                  {0x00,0x0F,0x09,0x11,0x01,0x02,0x0C,0x00},
                                  {0x08,0x0F,0x12,0x02,0x02,0x02,0x04,0x00},
                                  {0x00,0x1F,0x01,0x01,0x01,0x01,0x1F,0x00},
                                  {0x0A,0x1F,0x0A,0x0A,0x02,0x04,0x08,0x00},
                                  {0x00,0x18,0x01,0x19,0x01,0x02,0x1C,0x00},
                                  {0x00,0x1F,0x01,0x02,0x04,0x0A,0x11,0x00},
                                  {0x08,0x1F,0x09,0x0A,0x08,0x08,0x07,0x00},
                                  {0x00,0x11,0x11,0x09,0x01,0x02,0x0C,0x00},
                                  {0x00,0x0F,0x09,0x15,0x03,0x02,0x0C,0x00},
                                  {0x02,0x1C,0x04,0x1F,0x04,0x04,0x08,0x00},
                                  {0x00,0x15,0x15,0x15,0x01,0x02,0x04,0x00},
                                  {0x0E,0x00,0x1F,0x04,0x04,0x04,0x08,0x00},
                                  {0x08,0x08,0x08,0x0C,0x0A,0x08,0x08,0x00},
                                  {0x04,0x04,0x1F,0x04,0x04,0x08,0x10,0x00},
                                  {0x00,0x0E,0x00,0x00,0x00,0x00,0x1F,0x00},
                                  {0x00,0x1F,0x01,0x0A,0x04,0x0A,0x10,0x00},
                                  {0x04,0x1F,0x02,0x04,0x0E,0x15,0x04,0x00},
                                  {0x02,0x02,0x02,0x02,0x02,0x04,0x08,0x00},
                                  {0x00,0x04,0x02,0x11,0x11,0x11,0x11,0x00},
                                  {0x10,0x10,0x1F,0x10,0x10,0x10,0x0F,0x00},
                                  {0x00,0x1F,0x01,0x01,0x01,0x02,0x0C,0x00},
                                  {0x00,0x08,0x14,0x02,0x01,0x01,0x00,0x00},
                                  {0x04,0x1F,0x04,0x04,0x15,0x15,0x04,0x00},
                                  {0x00,0x1F,0x01,0x01,0x0A,0x04,0x02,0x00},
                                  {0x00,0x0E,0x00,0x0E,0x00,0x0E,0x01,0x00},
                                  {0x00,0x04,0x08,0x10,0x11,0x1F,0x01,0x00},
                                  {0x00,0x01,0x01,0x0A,0x04,0x0A,0x10,0x00},
                                  {0x00,0x1F,0x08,0x1F,0x08,0x08,0x07,0x00},
                                  {0x08,0x08,0x1F,0x09,0x0A,0x08,0x08,0x00},
                                  {0x00,0x0E,0x02,0x02,0x02,0x02,0x1F,0x00},
                                  {0x00,0x1F,0x01,0x1F,0x01,0x01,0x1F,0x00},
                                  {0x0E,0x00,0x1F,0x01,0x01,0x02,0x04,0x00},
                                  {0x12,0x12,0x12,0x12,0x02,0x04,0x08,0x00},
                                  {0x00,0x04,0x14,0x14,0x15,0x15,0x16,0x00},
                                  {0x00,0x10,0x10,0x11,0x12,0x14,0x18,0x00},
                                  {0x00,0x1F,0x11,0x11,0x11,0x11,0x1F,0x00},
                                  {0x00,0x1F,0x11,0x11,0x01,0x02,0x04,0x00},
                                  {0x00,0x18,0x00,0x01,0x01,0x02,0x1C,0x00},
                                  {0x04,0x12,0x08,0x00,0x00,0x00,0x00,0x00},
                                  {0x1C,0x14,0x1C,0x00,0x00,0x00,0x00,0x00},
                                  {0x00,0x00,0x09,0x15,0x12,0x12,0x0D,0x00},
                                  {0x0A,0x00,0x0E,0x01,0x0F,0x11,0x0F,0x00},
                                  {0x00,0x00,0x0E,0x11,0x1E,0x11,0x1E,0x10},
                                  {0x00,0x00,0x0E,0x10,0x0C,0x11,0x0E,0x00},
                                  {0x00,0x00,0x11,0x11,0x11,0x13,0x1D,0x10},
                                  {0x00,0x00,0x0F,0x14,0x12,0x11,0x0E,0x00},
                                  {0x00,0x00,0x06,0x09,0x11,0x11,0x1E,0x10},
                                  {0x00,0x00,0x0F,0x11,0x11,0x11,0x0F,0x01},
                                  {0x00,0x00,0x07,0x04,0x04,0x14,0x08,0x00},
                                  {0x00,0x02,0x1A,0x02,0x00,0x00,0x00,0x00},
                                  {0x02,0x00,0x06,0x02,0x02,0x02,0x02,0x02},
                                  {0x00,0x14,0x08,0x14,0x00,0x00,0x00,0x00},
                                  {0x00,0x04,0x0E,0x14,0x15,0x0E,0x04,0x00},
                                  {0x08,0x08,0x1C,0x08,0x1C,0x08,0x0F,0x00},
                                  {0x0E,0x00,0x16,0x19,0x11,0x11,0x11,0x00},
                                  {0x0A,0x00,0x0E,0x11,0x11,0x11,0x0E,0x00},
                                  {0x00,0x00,0x16,0x19,0x11,0x11,0x1E,0x10},
                                  {0x00,0x00,0x0D,0x13,0x11,0x11,0x0F,0x01},
                                  {0x00,0x0E,0x11,0x1F,0x11,0x11,0x0E,0x00},
                                  {0x00,0x00,0x00,0x0B,0x15,0x1A,0x00,0x00},
                                  {0x00,0x00,0x0E,0x11,0x11,0x0A,0x1B,0x00},
                                  {0x0A,0x00,0x11,0x11,0x11,0x11,0x13,0x0D},
                                  {0x1F,0x10,0x08,0x04,0x08,0x10,0x1F,0x00},
                                  {0x00,0x00,0x1F,0x0A,0x0A,0x0A,0x13,0x00},
                                  {0x1F,0x00,0x11,0x0A,0x04,0x0A,0x11,0x00},
                                  {0x00,0x00,0x11,0x11,0x11,0x11,0x0F,0x01},
                                  {0x00,0x01,0x1E,0x04,0x1F,0x04,0x04,0x00},
                                  {0x00,0x00,0x1F,0x08,0x0F,0x09,0x11,0x00},
                                  {0x00,0x00,0x1F,0x15,0x1F,0x11,0x11,0x00},
                                  {0x00,0x04,0x00,0x1F,0x00,0x04,0x00,0x00},
                                  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
                                  {0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F}};

int customCharsUsed[7] = {0,0,0,0,0,0,0}; //tracks if a custom char has already been used since last clear

//WIFI AND DISCORD FIELDS
const uint16_t gateway_intents = 0;//GUILD_MESSAGES_INTENT | GUILD_MESSAGE_TYPING_INTENT | DIRECT_MESSAGES_INTENT; // Intent options can be found in GatewayIntents.h
WebSocketClient ws(true);
DynamicJsonDocument docLong(8192); 
DynamicJsonDocument doc(1024);
const char *host = "discord.com";
const int httpsPort = 443;
unsigned long heartbeatInterval = 0;
unsigned long lastHeartbeatAck = 0;
unsigned long lastHeartbeatSend = 0;
bool hasWsSession = false;
String websocketSessionId;
bool hasReceivedWSSequence = false;
unsigned long lastWebsocketSequence = 0;
bool wasDisconnected = false;
IPAddress local_IP(192, 168, 2, 43);// Set your Static IP address
IPAddress gateway(192, 168, 2, 1); // Set your Gateway IP address
IPAddress subnet(255, 255, 0, 0);
IPAddress primaryDNS(8, 8, 8, 8);   //optional
IPAddress secondaryDNS(8, 8, 4, 4); //optional

//DISCORD JSON COMMAND FIELDS
const String NOTIFICATION_CMD = "{\"name\":\"notification\",\"description\":\"Send a notification to Disco\",\"options\":[{\"name\":\"message\",\"description\":\"Your notification message\",\"type\":3,\"required\":true},{\"name\":\"ledmode\",\"description\":\"The led mode to be in while your notification is displayed\",\"type\":4,\"required\":true,\"choices\":[{\"name\":\"Gentle flash\",\"value\":1},{\"name\":\"Fast flash\",\"value\":2},{\"name\":\"Fixed color\",\"value\":3},{\"name\":\"Rainbow\",\"value\":0},{\"name\":\"Off\",\"value\":4}]},{\"name\":\"ledcolor\",\"description\":\"The hex color to use for the led if using a flash or fixed color ledmode\",\"type\":3,\"required\":false}]}";
const String SET_NOTE_CMD = "{\"name\":\"setnote\",\"description\":\"Set the note message\",\"options\":[{\"name\":\"message\",\"description\":\"Your note message\",\"type\":3,\"required\":true}]}";
const String SET_ANIMATION_CMD = "{\"name\":\"setanimation\",\"description\":\"Set the frames of the animation\",\"options\":[{\"name\":\"delaytime\",\"description\":\"The time in milliseconds between each frame. 1000 equals 1 second.\",\"type\":4,\"required\":true},{\"name\":\"frame1\",\"description\":\"The first frame. Each frame should be max 32 characters long.\",\"type\":3,\"required\":true},{\"name\":\"frame2\",\"description\":\"The second frame. Following frames are optional.\",\"type\":3,\"required\":true},{\"name\":\"frame3\",\"description\":\"frame 3\",\"type\":3,\"required\":false},{\"name\":\"frame4\",\"description\":\"frame 4\",\"type\":3,\"required\":false},{\"name\":\"frame5\",\"description\":\"frame 5\",\"type\":3,\"required\":false},{\"name\":\"frame6\",\"description\":\"frame 6\",\"type\":3,\"required\":false},{\"name\":\"frame7\",\"description\":\"frame 7\",\"type\":3,\"required\":false},{\"name\":\"frame8\",\"description\":\"frame 8\",\"type\":3,\"required\":false},{\"name\":\"frame9\",\"description\":\"frame 9\",\"type\":3,\"required\":false},{\"name\":\"frame10\",\"description\":\"frame 10\",\"type\":3,\"required\":false},{\"name\":\"frame11\",\"description\":\"frame 11\",\"type\":3,\"required\":false},{\"name\":\"frame12\",\"description\":\"frame 12\",\"type\":3,\"required\":false},{\"name\":\"frame13\",\"description\":\"frame 13\",\"type\":3,\"required\":false},{\"name\":\"frame14\",\"description\":\"frame 14\",\"type\":3,\"required\":false},{\"name\":\"frame15\",\"description\":\"frame 15\",\"type\":3,\"required\":false},{\"name\":\"frame16\",\"description\":\"frame 16\",\"type\":3,\"required\":false},{\"name\":\"frame17\",\"description\":\"frame 17\",\"type\":3,\"required\":false},{\"name\":\"frame18\",\"description\":\"frame 18\",\"type\":3,\"required\":false},{\"name\":\"frame19\",\"description\":\"frame 19\",\"type\":3,\"required\":false},{\"name\":\"frame20\",\"description\":\"frame 20\",\"type\":3,\"required\":false},{\"name\":\"frame21\",\"description\":\"frame 21\",\"type\":3,\"required\":false},{\"name\":\"frame22\",\"description\":\"frame 22\",\"type\":3,\"required\":false},{\"name\":\"frame23\",\"description\":\"frame 23\",\"type\":3,\"required\":false},{\"name\":\"frame24\",\"description\":\"frame 24\",\"type\":3,\"required\":false}]}";
const String SET_CUSTOMCHAR_CMD = "{\"name\":\"setcustomchar\",\"description\":\"Set a custom character\",\"options\":[{\"name\":\"slot\",\"description\":\"The slot where to save the character. Must be a value between 1 and 50.\",\"type\":4,\"required\":true},{\"name\":\"row1\",\"description\":\"The first pixel row of the character. This can be in DEC or HEX format.\",\"type\":3,\"required\":true},{\"name\":\"row2\",\"description\":\"row 2\",\"type\":3,\"required\":true},{\"name\":\"row3\",\"description\":\"row 3\",\"type\":3,\"required\":true},{\"name\":\"row4\",\"description\":\"row 4\",\"type\":3,\"required\":true},{\"name\":\"row5\",\"description\":\"row 5\",\"type\":3,\"required\":true},{\"name\":\"row6\",\"description\":\"row 6\",\"type\":3,\"required\":true},{\"name\":\"row7\",\"description\":\"row 7\",\"type\":3,\"required\":true},{\"name\":\"row8\",\"description\":\"row 8\",\"type\":3,\"required\":true},{\"name\":\"save\",\"description\":\"Save file to EEPROM? Default is false.\",\"type\":5,\"required\":false}]}";
const String SET_MODE_CMD = "{\"name\":\"setmode\",\"description\":\"Set the main mode\",\"options\":[{\"name\":\"mode\",\"description\":\"The mode to switch to\",\"type\":4,\"required\":true,\"choices\":[{\"name\":\"Clock & Temp mode\",\"value\":0},{\"name\":\"Note mode\",\"value\":1},{\"name\":\"Stocks mode\",\"value\":2},{\"name\":\"Timer & Countdown mode\",\"value\":3},{\"name\":\"Animation mode\",\"value\":4}]}]}";
const String SET_LED_MODE_CMD = "{\"name\":\"setledmode\",\"description\":\"Set the main LED mode\",\"options\":[{\"name\":\"ledmode\",\"description\":\"The main led mode\",\"type\":4,\"required\":true,\"choices\":[{\"name\":\"Gentle flash\",\"value\":1},{\"name\":\"Fast flash\",\"value\":2},{\"name\":\"Fixed color\",\"value\":3},{\"name\":\"Rainbow\",\"value\":0},{\"name\":\"Off\",\"value\":4}]},{\"name\":\"ledcolor\",\"description\":\"The hex color to use for the led if using a flash or fixed color ledmode\",\"type\":3,\"required\":false}]}";
const String SET_STOCK_CMD = "{\"name\":\"setstock\",\"description\":\"Set the stock\",\"options\":[{\"name\":\"stock\",\"description\":\"The stock to set\",\"type\":3,\"required\":true}]}";
const String SET_TIMER_CMD = "{\"name\":\"settimer\",\"description\":\"Set the timer to timer-mode\"}";
const String SET_COUNTDOWN_CMD = "{\"name\":\"setcountdown\",\"description\":\"Set the timer to countdown mode and specify target time\",\"options\":[{\"name\":\"hours\",\"description\":\"The hour of the target time\",\"type\":4,\"required\":true},{\"name\":\"minutes\",\"description\":\"The minutes of the target time\",\"type\":4,\"required\":true},{\"name\":\"seconds\",\"description\":\"The seconds of the target time\",\"type\":4,\"required\":true}]}";
const String GET_CUSTOMCHAR_CMD = "{\"name\":\"getcustomchar\",\"description\":\"Show a character stored at the given slot\",\"options\":[{\"name\":\"slot\",\"description\":\"Must be a value between 1 and 50\",\"type\":4,\"required\":true},{\"name\":\"screencolor\",\"description\":\"The screen color to use for the return image\",\"type\":4,\"required\":false,\"choices\":[{\"name\":\"Blue\",\"value\":0},{\"name\":\"Green\",\"value\":1}]}]}";
const String GET_SCREEN_CMD = "{\"name\":\"getscreen\",\"description\":\"Show what's on the screen right now\",\"options\":[{\"name\":\"screencolor\",\"description\":\"The screen color to use for the return image\",\"type\":4,\"required\":false,\"choices\":[{\"name\":\"Blue\",\"value\":0},{\"name\":\"Green\",\"value\":1}]}]}";

//NOTIFICATION FIELDS
String lastRemainingMessage = "";
String lastSeenMessage = ""; //keep track of what's shown on screen when showing system notification
String lastRemainingSystemMessage = "";
bool showingNotification = false;
bool showingSystemNotification = false;
String currentNotificationIcon = "((51))";
String currentSystemNotificationIcon ="((51))";

unsigned long timeSinceSystemNotification = 0;
int currentNotificationLedMode = 0;
int currentSystemNotificationLedMode = 0;
String currentNotificationLedColor = "";
String currentSystemNotificationLedColor = "";
bool systemNotificationRequiresButtonPressInsteadOfTime = false;

//MAIN MODE FIELDS
String currentLedColor = "FFFFFF";
int currentLedMode = 3;
int currentMode = DEFAULT_MODE;
unsigned long timeSinceLastClockUpdate = 0;
//CLOCK
const char* ntpServer = "pool.ntp.org";
const char* defaultTimeZone = "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00"; //TZ environment variable
DynamicJsonDocument generalDoc(1024);
//WEATHER
const char *weatherHost = "api.openweathermap.org";
unsigned long timeSinceWeatherUpdate = 0;
String lastTemp = "";
//STOCKS
const char *yahooHost = "query1.finance.yahoo.com";
String currentStock = DEFAULT_STOCK;
unsigned long timeSinceStockUpdate = 0;
float lastStockPrice = 0;
float previousStockClose = 0;
//NOTE
String currentNote = DEFAULT_NOTE;
//TIMER + COUNTDOWN
unsigned long timeSinceTimerUpdate = 0;
unsigned long timerStartMillis = 0;
unsigned long targetMillis = COUNTDOWN_DEFAULT_TARGET;
unsigned long millisAtStop = 0;
bool isCountingUp = false; //true = timer, false = countdown
bool timerStopped = true;
//ANIMATION
int frameCount = 2; //minimum 2
int currentFrame = 0;
int frameDelay = DEFAULT_FRAME_DELAY; 
unsigned long timeSinceLastFrameChange = 0;
String frames[24] = {DEFAULT_FRAME_ONE, DEFAULT_FRAME_TWO, "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", ""};
const String whiteSpaces = "                                "; //32 whitespaces we can substring from

//LED FIELDS
unsigned long previousLEDMillis;
unsigned long previousRainbowMillis;
bool ledFlashToggle = false;
int rainbowRed = 0;
int rainbowGreen = 0;
int rainbowBlue = 0;
int rainbowIndex = 0;

//BITMAP & IMGUR FIELDS
const int BITS_PER_PIXEL = 1; //only 2 colors
const int COLORS = 2;
const int FILE_HEADER_SIZE = 14;
const int INFO_HEADER_SIZE = 40;
const int COLOR_TABLE_SIZE = COLORS * 4; //4 bytes for each color
const int IMG_HEIGHT = 8; //
const int IMG_WIDTH = 8; //5; use multitudes of 8 to avoid extra calculations
unsigned char padding[3] = {0,0,0};
const char colorTableGreen[] = {
    0x73,0xB5,0x87,0,  /// color 0 BGR + reserved
    0x38,0x3E,0x2F,0   /// color 1
};
const char colorTableBlue[] = {
    0xF8,0x21,0x1E,0,
    0xEE,0xD6,0xB8,0
};

byte image[IMG_HEIGHT * (IMG_WIDTH/8)] = {B00001110, //used for character
                                          B00011011,
                                          B00011111,
                                          B00000000,
                                          B00010001,
                                          B00010001,
                                          B00011011,
                                          B00001110};
byte screenImage[17*12] = {0,0,0,0,0,0,0,0,0,0,0,0, //used for screen. Pixels are saved in bits hence it's only 12 bytes wide
                           0,0,0,0,0,0,0,0,0,0,0,0,
                           0,0,0,0,0,0,0,0,0,0,0,0,
                           0,0,0,0,0,0,0,0,0,0,0,0,
                           0,0,0,0,0,0,0,0,0,0,0,0,
                           0,0,0,0,0,0,0,0,0,0,0,0,
                           0,0,0,0,0,0,0,0,0,0,0,0,
                           0,0,0,0,0,0,0,0,0,0,0,0,
                           0,0,0,0,0,0,0,0,0,0,0,0,
                           0,0,0,0,0,0,0,0,0,0,0,0,
                           0,0,0,0,0,0,0,0,0,0,0,0,
                           0,0,0,0,0,0,0,0,0,0,0,0,
                           0,0,0,0,0,0,0,0,0,0,0,0,
                           0,0,0,0,0,0,0,0,0,0,0,0,
                           0,0,0,0,0,0,0,0,0,0,0,0,
                           0,0,0,0,0,0,0,0,0,0,0,0,
                           0,0,0,0,0,0,0,0,0,0,0,0,
                           };
String bitmap = "";
const char *imgurHost = "api.imgur.com";
String IMGUR_UPLOAD_ENDPOINT = "/3/image";
String STEFAN_ACCESS_TOKEN = ""; //retrieved with refresh token

/*
 * Wifi & Discord related Methods
 */
void setup_wifi() 
{
    delay(10);
    
    // Configures static IP address
    /*if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
      Serial.println("STA Failed to configure");
    }*/
    
    // We start by connecting to a WiFi network
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(WIFI_SSID);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        createNotification("Connecting to  WIFI...", 1, "null", 1, true, false);
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

String json_https_request(String requestType, const char *hostStr, String urlStr, String extraHeaderStr, String jsonStr)
{
    WiFiClientSecure httpsClient;
    int r=0; //retry counter
    while((!httpsClient.connect(hostStr, httpsPort)) && (r < 30))
    {
         delay(100);
         Serial.print(".");
         r++;
    }
    if(r==30)
    {
        Serial.println("Connection failed");
    }
    httpsClient.print(requestType + " " + urlStr + " HTTP/1.1\r\n" +
                "Host: " + hostStr + "\r\n" +
                extraHeaderStr +
                "Accept: application/json" + "\r\n" +
                "Content-Type: application/json" + "\r\n" +
                "Content-Length: " + jsonStr.length() + "\r\n" +
                "Connection: close\r\n\r\n" +
                jsonStr + "\r\n");
    Serial.println("request sent");
    while (httpsClient.connected())
    {
        String line = httpsClient.readStringUntil('\n');
        if (line == "\r")
        {
            Serial.println("headers received");
            break;
        }
    }
    Serial.println("reply was:");
    Serial.println("==========");
    String line;
    while(httpsClient.available())
    {
        line = httpsClient.readStringUntil('\n');  //Read Line by Line
        Serial.println(line); //Print response
    }
    Serial.println("==========");
    Serial.println("closing connection");
    return line;
}

String general_https_request(String requestType, const char *hostStr, String urlStr, String extraHeaderStr, String body) //TODO test
{
    WiFiClientSecure httpsClient;
    int r=0; //retry counter
    while((!httpsClient.connect(hostStr, httpsPort)) && (r < 30))
    {
         delay(100);
         Serial.print(".");
         r++;
    }
    if(r==30)
    {
        Serial.println("Connection failed");
    }
    httpsClient.print(requestType + " " + urlStr + " HTTP/1.1\r\n" +
                "Host: " + hostStr + "\r\n" +
                extraHeaderStr +
                "Content-Length: " + body.length() + "\r\n" +
                "Connection: close\r\n\r\n" + 
                body + "\r\n");
    Serial.println("request sent");
    while (httpsClient.connected())
    {
        String line = httpsClient.readStringUntil('\n');
        if (line == "\r")
        {
            Serial.println("headers received");
            break;
        }
    }
    Serial.println("reply was:");
    Serial.println("==========");
    String line;
    String output = "";
    while(httpsClient.available())
    {
        line = httpsClient.readStringUntil('\n');  //Read Line by Line
        Serial.println(line); //Print response
        output += line;
    }
    Serial.println("==========");
    Serial.println("closing connection");
    return output;
}


//UPDATE OR PATCH DISCORD COMMANDS
void setup_commands()
{
    //GUILD COMMANDS
    //remove existing guild command
    //String ENDPOINT = String(GUILD_COMMAND_URL) + "/817479309312393278"; //command_id
    //json_https_request("DELETE", host, ENDPOINT, String("Authorization: Bot " + String(BOT_TOKEN) + "\r\n"), "{}");
    
    //add or update command
    //json_https_request("POST", host, GUILD_COMMAND_URL, String("Authorization: Bot " + String(BOT_TOKEN) + "\r\n"), GET_CUSTOMCHAR_CMD);
    //json_https_request("POST", host, GUILD_COMMAND_URL, String("Authorization: Bot " + String(BOT_TOKEN) + "\r\n"), GET_SCREEN_CMD);
  
    //GLOBAL COMMANDS
    //remove exisiting command
    //String ENDPOINT = String(GLOBAL_COMMAND_URL) + "/817128662922035221"; //command_id
    //json_https_request("DELETE", host, ENDPOINT, String("Authorization: Bot " +String(BOT_TOKEN) + "\r\n"), "{}");
  
    //add new or update command
    //json_https_request("POST", host, GLOBAL_COMMAND_URL, String("Authorization: Bot " + String(BOT_TOKEN) + "\r\n"), RGB_COMMAND);
}

//PROCESSES INTERACTION COMMANDS
void handleCommand()
{
    bool commandValid = false;
    String commandName = docLong["d"]["data"]["name"];
    Serial.print("Commandname:");
    Serial.println(commandName);
    if(commandName == "notification")
    {
        Serial.println("Notification command detected");
        int ledMode = docLong["d"]["data"]["options"][1]["value"];
        String ledColor = docLong["d"]["data"]["options"][2]["value"]; //may be null as its optional
        String message = docLong["d"]["data"]["options"][0]["value"];

        int result = 0;
        if (message[0] == '1') //system notification that requires button press //TODO temp for debugging
        {
            result = createNotification(message, ledMode, ledColor, 1, true, true);
        }
        else if (message[0] == '2') //system notification that goes away by time //TODO temp for debugging
        {
            result = createNotification(message, ledMode, ledColor, 1, true, false);
        }
        else
        {
            result = createNotification(message, ledMode, ledColor, 0, false, false);
        }
        
        if (result > 0)
        {
            commandValid = true;
        }
        else
        {
            //the only error it can be in this case is hex value
            followUpCommand("Error: There was a problem with your HEX value. Command was not executed.");
        }
    }
    else if(commandName == "setnote")
    {
        Serial.println("Setnote command detected");
        commandValid = true;
        currentNote = docLong["d"]["data"]["options"][0]["value"].as<String>(); //TODO test
        if (!showingNotification && !showingSystemNotification)
        {
            lcdClear();
        }
    }
    else if (commandName == "setanimation")
    {
        Serial.println("SetAnimation command detected");
        int delayTimeTemp = docLong["d"]["data"]["options"][0]["value"];
        if (delayTimeTemp < 0)
        {
            followUpCommand("Error: delaytime must be a positive number!");
            return;
        }
        frameDelay = delayTimeTemp;

        frameCount =  docLong["d"]["data"]["options"].size() - 1; //-1 as first option is delay
        
        Serial.print("frame count:");
        Serial.println(frameCount);

        for (int i = 0; i < frameCount; i++) //TODO add check to make sure every frame is 32 characters long!!
        {
            frames[i] = ""; //clear frame
            String singleFrame = docLong["d"]["data"]["options"][i + 1]["value"];
            int formattedLength = lcdLength(singleFrame);
            if (formattedLength > 32)
            {
                singleFrame = lcdSubstring(singleFrame,0, 32);
            }
            else if(formattedLength < 32)
            {
                singleFrame += whiteSpaces.substring(0, 32 - formattedLength); //make the string 32 frame
            }
            frames[i] = singleFrame;
        }
        commandValid = true;
    }
    else if(commandName == "setcustomchar")
    {
        int slotNr = docLong["d"]["data"]["options"][0]["value"];
        if (slotNr < 1 || slotNr > 50)
        {
            followUpCommand("Error: slot must be a value between 1 and 50!");
            return;
        }

        String row1 = docLong["d"]["data"]["options"][1]["value"];
        String row2 = docLong["d"]["data"]["options"][2]["value"];
        String row3 = docLong["d"]["data"]["options"][3]["value"];
        String row4 = docLong["d"]["data"]["options"][4]["value"];
        String row5 = docLong["d"]["data"]["options"][5]["value"];
        String row6 = docLong["d"]["data"]["options"][6]["value"];
        String row7 = docLong["d"]["data"]["options"][7]["value"];
        String row8 = docLong["d"]["data"]["options"][8]["value"];

        int row1Data, row2Data, row3Data, row4Data, row5Data, row6Data, row7Data, row8Data;
        row1Data = constrain((int)strtol(row1.c_str(), NULL, 0), 0, 255); //converts to integer, trying to detect the base
        row2Data = constrain((int)strtol(row2.c_str(), NULL, 0), 0, 255);
        row3Data = constrain((int)strtol(row3.c_str(), NULL, 0), 0, 255);
        row4Data = constrain((int)strtol(row4.c_str(), NULL, 0), 0, 255);
        row5Data = constrain((int)strtol(row5.c_str(), NULL, 0), 0, 255);
        row6Data = constrain((int)strtol(row6.c_str(), NULL, 0), 0, 255);
        row7Data = constrain((int)strtol(row7.c_str(), NULL, 0), 0, 255);
        row8Data = constrain((int)strtol(row8.c_str(), NULL, 0), 0, 255);
          
        bool saveToEeprom = false;
        if(docLong["d"]["data"]["options"].size() == 10)
        {
            saveToEeprom = docLong["d"]["data"]["options"][9]["value"];
        }
        Serial.print("save to eeprom:");
        Serial.println(saveToEeprom);

        //store the value in the customChars array
        customChars[(slotNr - 1)*8] = row1Data;
        customChars[(slotNr - 1)*8 + 1] = row2Data;
        customChars[(slotNr - 1)*8 + 2] = row3Data;
        customChars[(slotNr - 1)*8 + 3] = row4Data;
        customChars[(slotNr - 1)*8 + 4] = row5Data;
        customChars[(slotNr - 1)*8 + 5] = row6Data;
        customChars[(slotNr - 1)*8 + 6] = row7Data;
        customChars[(slotNr - 1)*8 + 7] = row8Data;
               
        commandValid = true;
    }
    else if (commandName == "setmode")
    {
        Serial.println("SetMode command detected");
        currentMode = docLong["d"]["data"]["options"][0]["value"];
        lcdClear();
        commandValid = true;
    }
    else if(commandName == "setledmode")
    {
        Serial.println("SetLEDMode command detected");
        int ledMode = docLong["d"]["data"]["options"][0]["value"];
        String ledColor = docLong["d"]["data"]["options"][1]["value"]; //may be null as its optional

        if(ledColor != "null")
        {
            if (!checkValidHex(ledColor))
            {
                followUpCommand("Error: There was a problem with your HEX value. Command was not executed.");
                return;
            }
            else
            {
                currentLedColor = ledColor;
            }
        }
        currentLedMode = ledMode;
        
        commandValid = true;
    }
    else if(commandName == "setstock")
    {
        String stock = docLong["d"]["data"]["options"][0]["value"];
        if (stock == "")
        {
            followUpCommand("Error: No Stock given!");
            return;
        }
        currentStock = stock;
        getStock();
        lcdClear();
        commandValid = true;
    }
    else if(commandName == "settimer")
    {
        isCountingUp = true;
        timerStopped = true;
        millisAtStop = timerStartMillis; //Reset
        commandValid = true;
    }
    else if(commandName == "setcountdown")
    {
        int hours = docLong["d"]["data"]["options"][0]["value"];
        int minutes = docLong["d"]["data"]["options"][1]["value"];
        int seconds = docLong["d"]["data"]["options"][2]["value"];

        if (hours < 0 || minutes < 0 || seconds < 0)
        {
            followUpCommand("Error: values aren't allowed below zero!");
            return;
        }
        if(hours > 99 || minutes > 59 || seconds > 59)
        {            
            followUpCommand("Error: values are too low!");
            return;
        }
        isCountingUp = false;
        targetMillis = hours * 3600000 + minutes * 60000 + seconds * 1000;
        timerStopped = true;
        millisAtStop = timerStartMillis; //Reset
        commandValid = true;
    }
    else if(commandName == "getcustomchar")
    {
        int slotNr = docLong["d"]["data"]["options"][0]["value"];
        if (slotNr < 1 || slotNr > 50)
        {
            followUpCommand("Error: slot must be a value between 1 and 50!");
            return;
        }

        bool useGreenTable = false;
        
        int color = docLong["d"]["data"]["options"][1]["value"];
        if(color == 1)
        {
            useGreenTable = true;
        } 

        //reverse chosen character
        for (int i = 0; i < 8; i++)
        {
            image[7-i] = customChars[8*(slotNr-1) + i];
        }
        //generate bitmap file
        generateBitmapImage(image, 8, 8, 4, useGreenTable); //we use a mutliple of 8 as we're working with pure bits. Resize factor = 4

        //create imgur image
        String responseStr = general_https_request("POST", imgurHost, IMGUR_UPLOAD_ENDPOINT, String("Authorization: Bearer " + STEFAN_ACCESS_TOKEN + "\r\n"), bitmap); //WORKS yeah!
        deserializeJson(generalDoc, responseStr);
        String link = generalDoc["data"]["link"];
        Serial.print("link:");
        Serial.println(link);

        if (link == "null")
        {
            followUpCommand("Error: invalid result from Imgur. Try again?");
            return;
        }

        //send a message with image attached
        String interactionToken = docLong["d"]["token"];
        String url = FOLLOWUP_ENDPOINT + interactionToken;
        json_https_request("POST", host, url, "", "{\"content\": \"Here you go!\", \"embeds\":[{\"image\":{\"url\": \"" + link + "\"}}]}"); 
        
        commandValid = false; //we leave it false as we already handled response.
    }
    else if (commandName == "getscreen")
    {
        Serial.println("getScreen command called");
        //generate pixeldata
        String screenData = virtualLCDTop;
        byte address = 0;
        byte character[8] = {0,0,0,0,0,0,0,0};
        bool isCGRAMChar = false;
        char textChar  = ' ';
        int xCoordinate = 95; //start right
        int yCoordinate = 16;
        for (int lcdRow = 1; lcdRow >= 0; lcdRow--)
        {
            for (int lcdCol = 15; lcdCol >= 0; lcdCol--) //counting from right to left
            {
                //get characterData
                textChar = screenData[lcdCol];
                isCGRAMChar = false;
                //handle custom chars
                switch (textChar)
                {
                    case '\x08':
                        isCGRAMChar = true;
                        address = 0;
                        break;
                    case '\1':
                        isCGRAMChar = true;
                        address = 1;
                        break;
                    case '\2':
                        isCGRAMChar = true;
                        address = 2;
                        break;
                    case '\3':
                        isCGRAMChar = true;
                        address = 3;
                        break;
                    case '\4':
                        isCGRAMChar = true;
                        address = 4;
                        break;
                    case '\5':
                        isCGRAMChar = true;
                        address = 5;
                        break;
                    case '\6':
                        isCGRAMChar = true;
                        address = 6;
                        break;
                    case '\7':
                        isCGRAMChar = true;
                        address = 7;
                        break;
                }
                if (!isCGRAMChar)
                {
                    address = address = (byte)textChar; //ascii is address
                    for (int i = 0; i < 8; i++)
                    {
                        character[i] = virtualCGROM[address][i];
                    }
                }
                else
                {
                    for (int i = 0; i < 8; i++)
                    {
                        character[i] = virtualCGRAM[address*8 + i];
                    }
                }

                //actual setting of pixeldata
                for (int h = 0; h < 8; h++) //each character is 8 pixels high
                {
                    for (int w = 0; w < 5; w++) //each character is 5 pixels wide + 1 border
                    {
                        bitWrite(screenImage[(yCoordinate-h) * 12 + (xCoordinate-w)/8], (7-(xCoordinate-w)%8), bitRead(character[h],w));  
                        //bitWrite(screenImage[(23-(yCoordinate+h)) * 12 + (xCoordinate+w)/8], (xCoordinate+w)%8, bitRead(character[h],w));  
                    }
                }
                //add one pixel space after each pixel
                xCoordinate -= 6; //5 (character width) + 1
            }
            //add one pixel space after first row:
            yCoordinate -= 9; //8 (character height) + 1space
            xCoordinate = 95; //Start over
            //switch to 2nd row
            screenData = virtualLCDBottom;
        }
        bool useGreenTable = false;

        int color = docLong["d"]["data"]["options"][0]["value"];
        if(color == 1)
        {
            useGreenTable = true;
        }
        
        //generate bitmap file
        generateBitmapImage(screenImage, 96, 17, 2, useGreenTable); //we use a mutliple of 8 as we're working with pure bits. Resize factor = 1
  
        //create imgur image
        String responseStr = general_https_request("POST", imgurHost, IMGUR_UPLOAD_ENDPOINT, String("Authorization: Bearer " + STEFAN_ACCESS_TOKEN + "\r\n"), bitmap); //WORKS yeah!
        deserializeJson(generalDoc, responseStr);
        String link = generalDoc["data"]["link"];
        
        if (link == "null")
        {
            Serial.println("Link was null");
            followUpCommand("Error: invalid result from Imgur. Try again?");
            return;
        }

        //send a message with image attached
        String interactionToken = docLong["d"]["token"];
        String url = FOLLOWUP_ENDPOINT + interactionToken;
        json_https_request("POST", host, url, "", "{\"content\": \"Here you go!\", \"embeds\":[{\"image\":{\"url\": \"" + link + "\"}}]}"); 
        
        commandValid = false; //we leave it false as we already handled response.
    }
    else if (commandName == "null")
    {
        followUpCommand("Error: command was 'null'. Perhaps the message is too long?");
        return;
    }

    //Send a followup ack (with message) if command is valid.
    if (commandValid)
    {
        followUpCommand(POSITIVE_COMMAND_MESSAGE);  
    }
}

void followUpCommand(String message)
{
    String interactionToken = docLong["d"]["token"];
    String url = FOLLOWUP_ENDPOINT + interactionToken;
    json_https_request("POST", host, url, "", "{\"content\":\"" + message + "\"}");
}

//NOTIFICATION
int createNotification(String message, int ledMode, String ledColor, int notificationIcon, bool isSystemNotification, bool systemNotificationRequiresButtonPress) //returns -1 if ledColor invalid, -2 if ledMode is invalid, -3 if notificationIcon is invalid. >0 if valid.
{
    if(notificationIcon < 0 || notificationIcon > NOTIFICATION_ICON_COUNT - 1)
    {
        return -3;
    }
  
    if(ledColor != "null")
    {
        if (!checkValidHex(ledColor))
        {
            return -1;
        }
    }

    if(ledMode < 0 || ledMode > LED_MODE_COUNT -1)
    {
        return -2;
    }

    if (isSystemNotification)
    {
        systemNotificationRequiresButtonPressInsteadOfTime = systemNotificationRequiresButtonPress;
    }

    //set ledmode
    if (isSystemNotification)
    {
        currentSystemNotificationLedMode = ledMode;
        if (ledColor == "null")
        {
            currentSystemNotificationLedColor = STANDARD_NOTIFICATION_COLOR; //if no color specified we use standard color
        }
        else
        {
            currentSystemNotificationLedColor = ledColor;
        }
    }
    else
    {
        currentNotificationLedMode = ledMode;
        if (ledColor == "null")
        {
            currentNotificationLedColor = STANDARD_NOTIFICATION_COLOR;
        }
        else
        {
            currentNotificationLedColor = ledColor;
        }
    }

    //set notification Icon
    if (isSystemNotification)
    {
        switch(notificationIcon)
        {
            case 0:
              currentSystemNotificationIcon = "((51))"; //bell
              break;
            case 1:
              currentSystemNotificationIcon = "((52))"; //warning
              break;
            case 2:
              currentSystemNotificationIcon = "((53))"; //clock/watch
              break;
        }      
    }
    else
    {
        switch(notificationIcon)
        {
            case 0:
              currentNotificationIcon = "((51))"; //bell
              break;
            case 1:
              currentNotificationIcon = "((52))"; //warning
              break;
            case 2:
              currentNotificationIcon = "((53))"; //clock/watch
              break;
        }
    }

    //set message
    message.replace("(())", ""); //handle empty characters

    bool alreadyShowingSystemNotification = showingSystemNotification;

    if (isSystemNotification)
    {
        lastRemainingSystemMessage = message; //Store system message
        showingSystemNotification = true; //set a global signal to show system notification is showing.
        timeSinceSystemNotification = millis();
    }
    else
    {
        lastRemainingMessage = message; //Store message
        showingNotification = true; //set a global signal to show notification is not dismissed.
    }
    if (alreadyShowingSystemNotification && !isSystemNotification)
    {
        lastSeenMessage = message;
        //we wait with showing regular notifications if there's still an unhandled system notification.
    }
    else
    {
        sendRemainingNotificationMessage(); //handles showing notification part
    }
    pushbuttonPressHandled = true; //we wait for a fresh pusbutton push
    return 1;
}

void sendRemainingNotificationMessage()
{     
    //re-assign first char to make sure it's correct
    lcdClear();

    if (showingSystemNotification)
    {
        if (lcdLength(lastRemainingSystemMessage) > 15) //is at least 2 rows long
        {
            //first row (starts at 2 character)
            String temp = lcdSubstring(lastRemainingSystemMessage, 0, 15);
            lcdPrint(currentSystemNotificationIcon + temp);
            //second row
            int remainingSystemMessageLength = lcdLength(lastRemainingSystemMessage);//lastRemainingSystemMessage.length();
            temp = lcdSubstring(lastRemainingSystemMessage,15, min(remainingSystemMessageLength, 30));
            lcdSetCursor(0,1);
            lcdPrint(temp);
            lastRemainingSystemMessage = lcdSubstring(lastRemainingSystemMessage,15, lcdLength(lastRemainingSystemMessage));
        }
        else
        {
            lcdPrint(currentSystemNotificationIcon + lastRemainingSystemMessage);
        }
           
        if (lcdLength(lastRemainingSystemMessage) > 15) //more to come
        {
            lcdSetCursor(15,1);
            lcdPrint(ARROW_ICON_STR); //show next page indicator
            lastRemainingSystemMessage = lcdSubstring(lastRemainingSystemMessage,15, lcdLength(lastRemainingSystemMessage));
        }
        else
        {
            lastRemainingSystemMessage = "";
        }
    }
    else if(showingNotification)
    {
        lastSeenMessage = lastRemainingMessage; //Track what's being shown in case a system message pops up
        if (lcdLength(lastRemainingMessage) > 15) //is at least 2 rows long
        {
            //first row (starts at 2 character)
            String temp = lcdSubstring(lastRemainingMessage,0,15);
            lcdPrint(currentNotificationIcon + temp);
            //second row
            int remainingMessageLength = lcdLength(lastRemainingMessage);//lastRemainingMessage.length();
            temp = lcdSubstring(lastRemainingMessage,15, min(remainingMessageLength, 30));
            lcdSetCursor(0,1);
            lcdPrint(temp);
            lastRemainingMessage = lcdSubstring(lastRemainingMessage,15, lcdLength(lastRemainingMessage));
        }
        else
        {
            lcdPrint(currentNotificationIcon + lastRemainingMessage);
        }
           
        if (lcdLength(lastRemainingMessage) > 15) //more to come
        {
            lcdSetCursor(15,1);
            lcdPrint(ARROW_ICON_STR); //show next page indicator
            lastRemainingMessage = lcdSubstring(lastRemainingMessage,15, lcdLength(lastRemainingMessage));
        }
        else
        {
            lastRemainingMessage = "";
        }
    }
}

void handle_pending_notification()
{
    if (showingSystemNotification) //system notifcation are prioritised
    {
        if ((!systemNotificationRequiresButtonPressInsteadOfTime && (millis() - timeSinceSystemNotification >= SYSTEM_NOTIFICATION_DISPLAY_DURATION)) //if set time has passed
          || (systemNotificationRequiresButtonPressInsteadOfTime && !pushbuttonPressHandled)) //if button has been pressed
        {
            //dismiss notification or go to next page
            if (lastRemainingSystemMessage != "") //more to show
            {
                timeSinceSystemNotification = millis();
                sendRemainingNotificationMessage(); //show next pages
            }
            else
            {
                //notification may be dismissed and go back to previous mode
                showingSystemNotification = false;
                lcdClear(); 
                if (showingNotification)
                {
                    //show last seen screen
                    lastRemainingMessage = lastSeenMessage;
                    sendRemainingNotificationMessage();
                    pushbuttonPressHandled = true;
                }
                //go back to previous mode
            }
            pushbuttonPressHandled = true;            
        }
    }
    else if (showingNotification)
    {
        if (!pushbuttonPressHandled)//button has been pressed/
        {
            //dismiss notification or go to next page
            if (lastRemainingMessage != "") //more to show
            {
                sendRemainingNotificationMessage(); //show next pages
            }
            else //dismissed
            {
                //notification may be dismissed and go back to previous mode
                showingNotification = false;
                lcdClear();  
                //TODO go back to previous mode
            }       
            pushbuttonPressHandled = true;
        }
    }
}

/*
 * MODES
 */
void handleMode()
{
    if (currentMode != 3 && (showingNotification || showingSystemNotification)) //don't handle modes when showing notification, except in the case of timer
    {
        return;
    }

    switch(currentMode)
    {
        case 0: //Clock
            handleClock();
            break;
        case 1: //Note
            handleNote();
            break;
        case 2: //Stocks
            handleStocks();
            break;
        case 3: //Timer + Countdown
            handleTimer();
            break;
        case 4: //Animation mode
            handleAnimation();
            break;
    }   
}

//CLOCK
void handleClock() //TODO also show if less than a minute but notification cleared the screen!
{
    if (millis() - timeSinceLastClockUpdate >= CLOCK_UPDATE_TIME)
    {
        timeSinceLastClockUpdate = millis();

        struct tm timeinfo;
        if(!getLocalTime(&timeinfo)){
          Serial.println("Failed to obtain time");
          return;
        }
        char dateBuff[16];
        char timeBuff[16];
        strftime(dateBuff, sizeof(dateBuff), "%b %d          ", &timeinfo); //date
        strftime(timeBuff, sizeof(timeBuff), "%H:%M", &timeinfo); //time
        
        if(millis() - timeSinceWeatherUpdate >= WEATHER_UPDATE_TIME) //get weather data only every 5 minutes
        {
            timeSinceWeatherUpdate = millis();
            lastTemp = getTemp();
        }
        char degreesSymbol = char(223);
        String temperature = lastTemp + degreesSymbol + "C";
        lcdSetCursor(0,0);
        lcdPrint(dateBuff);
        lcdSetCursor(16 - temperature.length(),0);
        lcdPrint(temperature);
        lcdSetCursor(0,1);
        lcdPrint(timeBuff);
    }
}

//WEATHER
String getTemp()
{
    String weatherStr = json_https_request("GET", weatherHost, "/data/2.5/weather?q=" CITY_COUNTRY "&APPID=" OPEN_WEATHER_API_KEY "&units=metric", "", "{}");
    deserializeJson(generalDoc, weatherStr);
    float temp = generalDoc["main"]["temp"];
    return String(temp, 2);
}

//STOCKS
void handleStocks()
{   
    if(millis() - timeSinceStockUpdate >= STOCKS_UPDATE_TIME) //get stock data every X seconds
    {
        timeSinceStockUpdate = millis();
        getStock();
        lcdClear();
    }
    String stockStr = String(lastStockPrice);
    //String diff = String(lastStockPrice - previousStockClose);
    String sign = "+";
    if (lastStockPrice - previousStockClose < 0)
    {
        sign = ""; //min is automatic
    }
    String percentage = sign + String(((lastStockPrice - previousStockClose)/previousStockClose)*100) + "%";
    lcdSetCursor(0,0);
    lcdPrint(currentStock + ":");
    lcdSetCursor(16 - stockStr.length(), 0);
    lcdPrint(String(lastStockPrice));
    lcdSetCursor(16 - percentage.length(),1);
    lcdPrint(percentage);
}

void getStock()
{
    String stocksStr = json_https_request("GET", yahooHost, "/v8/finance/chart/" + currentStock + "?interval=1mo&range=min", "", "{}");
    deserializeJson(generalDoc, stocksStr);
    lastStockPrice = generalDoc["chart"]["result"][0]["meta"]["regularMarketPrice"];
    previousStockClose = generalDoc["chart"]["result"][0]["meta"]["chartPreviousClose"];
}

//NOTE
void handleNote()
{
    String noteTop = currentNote;
    String noteBottom = "";
    
    if (lcdLength(currentNote) > 16)
    {
        noteTop = lcdSubstring(currentNote, 0, 16);
        
        if (lcdLength(currentNote) >=32)
        {
            noteBottom = lcdSubstring(currentNote, 16, 32);
        }
        else
        {
            noteBottom = lcdSubstring(currentNote,16, lcdLength(currentNote));
        }
    }
    
    customChar = 1; //Set this so it doesn't keep counting up and resetting cgram
    lcdSetCursor(0,0);
    lcdPrint(noteTop);
    lcdSetCursor(0,1);
    lcdPrint(noteBottom);
}

//TIMER
void handleTimer()
{
    if(millis() - timeSinceTimerUpdate >= TIMER_UPDATE_TIME) //only update every half a second or so
    {
        timeSinceTimerUpdate = millis();
        if (!timerStopped && !pushbuttonPressHandled) //stop
        {
            timerStopped = true;
            millisAtStop = millis();
        }
        else if (timerStopped && !pushbuttonPressHandled)//reset
        {
            timerStopped = false;
            timerStartMillis = millis();
        }

        if(!showingNotification && !showingSystemNotification) //if timer runs in background
        {
            lcdSetCursor(4,0);
        }

        if (isCountingUp) //timer -> button stops and resets time!
        {
            if(!showingNotification && !showingSystemNotification) //if timer runs in background
            {
                if(timerStopped)
                {
                    lcdPrint(millisToString(millisAtStop - timerStartMillis));
                }
                else
                {
                    lcdPrint(millisToString(millis() - timerStartMillis));
                }
            }
        }
        else //countdown -> button stops and resets
        {
            if(!showingNotification && !showingSystemNotification) //if timer runs in background
            {
                if(timerStopped)
                {
                    lcdPrint(millisToString(targetMillis - (millisAtStop - timerStartMillis)));
                }
                else
                {
                    lcdPrint(millisToString(targetMillis - (millis() - timerStartMillis)));
                }
            }
      
            if (!timerStopped)
            {
                if (millis() - timerStartMillis >= targetMillis)
                {
                    timerStopped = true; //stop timer
                    millisAtStop = timerStartMillis; //reset
                    createNotification("Timer ended!", 2, TIMER_NOTIFICATION_COLOR, 2, true, true); //Creates a fast flashing notification
                }
            }

        }
        pushbuttonPressHandled = true;
    }
}

String millisToString(unsigned long millisec)
{
    int hours = millisec/3600000; //3 600 000
    millisec = millisec - (hours * 3600000);
    int minutes = millisec/60000; //60 000
    millisec = millisec - (minutes * 60000);
    int seconds = millisec/1000; //1 000

    String hrStr = String(hours);
    String minStr = String(minutes);
    String secStr = String(seconds);

    if (hours < 10)
    {
        hrStr = "0" +hrStr;
    }
    if (minutes < 10)
    {
        minStr = "0" + minStr;
    }
    if (seconds < 10)
    {
        secStr = "0" + secStr;
    }
    
    return hrStr + ":" + minStr + ":" + secStr;
}

//ANIMATION
void handleAnimation()
{
    if (millis() - timeSinceLastFrameChange >= frameDelay)
    {
        timeSinceLastFrameChange = millis();
        lcdClear();
        lcdPrint(lcdSubstring(frames[currentFrame], 0, 16)); //print top part of frame 
        //lcdPrint(lcdSubstring(frames, currentFrame * 32, (currentFrame * 32) + 16)); //print top part of frame
        lcdSetCursor(0,1);
        lcdPrint(lcdSubstring(frames[currentFrame], 16, 32)); //print bottom part of frame 
        //lcdPrint(lcdSubstring(frames,(currentFrame * 32) + 16, (currentFrame * 32) + 32)); //print bottom part of frame
        
        //lcd.clear();
        //lcd.print(frames.substring(currentFrame * 32, (currentFrame * 32) + 16)); //print top part of frame
        //lcd.setCursor(0,1);
        //lcd.print(frames.substring((currentFrame * 32) + 16, (currentFrame * 32) + 32)); //print bottom part of frame
        currentFrame = (currentFrame+1)%frameCount;
    }
}

int mod(int x, int m)
{
    return ((x%m)+m)%m;
}

//BITMAP Code based from: https://stackoverflow.com/questions/2654480/writing-bmp-image-in-pure-c-c-without-other-libraries
void generateBitmapImage (byte imageData[], int width, int height, int resizeFactor, bool useGreenTable) //width and height must be a multiple of 8 as we're using bits! (2 colors only)
{
    int widthInBytes = (width * resizeFactor) / 8;
    int paddingSize = (4-(widthInBytes)%4)%4;
    int stride = widthInBytes + paddingSize;
    unsigned char* fileHeader = createBitmapFileHeader(height * resizeFactor, stride);

    //clear bitmap
    bitmap = "";
    
    for (int i = 0; i < FILE_HEADER_SIZE; i++)
    {
        bitmap += (char)fileHeader[i];
    }

    unsigned char* infoHeader = createBitmapInfoHeader(height* resizeFactor, width * resizeFactor);
    for (int i = 0; i < INFO_HEADER_SIZE; i++)
    {
        bitmap += (char)infoHeader[i];
    }

    for (int i = 0; i < COLOR_TABLE_SIZE; i++)
    {
        if (useGreenTable)
        {
            bitmap += colorTableGreen[i];
        }
        else
        {
            bitmap += colorTableBlue[i];
        }
    }

    int widthInBITS = widthInBytes*8;

    for (int h = 0; h < height * resizeFactor; h++)
    {
        char temp = 0x0;
        for (int w = 0; w < widthInBITS; w++) //16
        {
            temp = bitWrite(temp, 7 - w%8, bitRead(imageData[(h/resizeFactor) * (width/8) + (w/8)/resizeFactor],mod(7 - w/resizeFactor,8))); 
            if ((w+1)%8==0)
            {
                bitmap += temp;
                temp = 0x0;
            }
        }
        for (int p = 0; p < paddingSize; p++)
        {
            bitmap += (char)padding[p];
        }
    }
}

unsigned char* createBitmapFileHeader (int height, int stride)
{
    int fileSize = FILE_HEADER_SIZE + INFO_HEADER_SIZE + COLOR_TABLE_SIZE + (stride * height);
    static unsigned char fileHeader[] = {
        0,0,     /// signature
        0,0,0,0, /// image file size in bytes
        0,0,0,0, /// reserved
        0,0,0,0, /// start of pixel array 
    };

    fileHeader[ 0] = (unsigned char)('B');
    fileHeader[ 1] = (unsigned char)('M');
    fileHeader[ 2] = (unsigned char)(fileSize      );
    fileHeader[ 3] = (unsigned char)(fileSize >>  8);
    fileHeader[ 4] = (unsigned char)(fileSize >> 16);
    fileHeader[ 5] = (unsigned char)(fileSize >> 24);
    fileHeader[10] = (unsigned char)(FILE_HEADER_SIZE + INFO_HEADER_SIZE + COLOR_TABLE_SIZE); //if using color palette add it here

    return fileHeader;
}

unsigned char* createBitmapInfoHeader (int height, int width)
{
    static unsigned char infoHeader[] = {
        0,0,0,0, /// header size
        0,0,0,0, /// image width
        0,0,0,0, /// image height
        0,0,     /// number of color planes
        0,0,     /// bits per pixel
        0,0,0,0, /// compression
        0,0,0,0, /// image size
        0,0,0,0, /// horizontal resolution
        0,0,0,0, /// vertical resolution
        0,0,0,0, /// colors in color table
        0,0,0,0, /// important color count
    };

    infoHeader[ 0] = (unsigned char)(INFO_HEADER_SIZE);
    infoHeader[ 4] = (unsigned char)(width      );
    infoHeader[ 5] = (unsigned char)(width >>  8);
    infoHeader[ 6] = (unsigned char)(width >> 16);
    infoHeader[ 7] = (unsigned char)(width >> 24);
    infoHeader[ 8] = (unsigned char)(height      );
    infoHeader[ 9] = (unsigned char)(height >>  8);
    infoHeader[10] = (unsigned char)(height >> 16);
    infoHeader[11] = (unsigned char)(height >> 24);
    infoHeader[12] = (unsigned char)(1);
    infoHeader[14] = (unsigned char)(BITS_PER_PIXEL);
    infoHeader[32] = (unsigned char)(COLORS); //2 colors

    return infoHeader;
}

//IMGUR
void getImgurAccessToken()
{
    String imgurStr = general_https_request("POST", imgurHost, "/oauth2/token", "Content-Type: application/x-www-form-urlencoded\r\n", "grant_type=refresh_token&refresh_token=" IMGUR_STEFAN_REFRESH_TOKEN "&client_id=" IMGUR_CLIENT_ID "&client_secret=" IMGUR_CLIENT_SECRET);
    deserializeJson(generalDoc, imgurStr.substring(imgurStr.indexOf('{'), imgurStr.lastIndexOf('}')+1));
    STEFAN_ACCESS_TOKEN = generalDoc["access_token"].as<String>();
}


/*
 * RGB LED Methods
 */
void setup_led()
{  
    ledcAttachPin(RED_LED, RED_CHANNEL); //assign a led to red channel
    ledcSetup(RED_CHANNEL, 4000, 8); //initialise red channel
    ledcAttachPin(GREEN_LED, GREEN_CHANNEL); //assign a led to green channel
    ledcSetup(GREEN_CHANNEL, 4000, 8); //initialise green channel
    ledcAttachPin(BLUE_LED, BLUE_CHANNEL); //assign a led to blue channel
    ledcSetup(BLUE_CHANNEL, 4000, 8); //initialise blue channel
    
    //set leds to 0 brightness
    setRGB(0,0,0);

    //temp
    previousLEDMillis = millis();
}

void setRGB(int r, int g, int b)
{
    ledcWrite(RED_CHANNEL, r);
    ledcWrite(GREEN_CHANNEL, g);
    ledcWrite(BLUE_CHANNEL, b);
}

//Returns -1 if invalid.
int setHex(String hex) //e.g. CCFFFF
{
    int r, g, b;
    int result = sscanf(hex.c_str(), "%02x%02x%02x", &r, &g, &b);
    if(result != -1)
    {
        if (r<0 || r>255)
        {
            result = -1;
        }
        if (g<0 || g>255)
        {
            result = -1;
        }
        if (b<0 || b>255)
        {
            result = -1;
        }
        if (result != -1)
        {
            setRGB(r,g,b);
        }
    }
    return result;
}

bool checkValidHex(String hex)
{
    bool isValid = true;
    int r, g, b;
    if(sscanf(hex.c_str(), "%02x%02x%02x", &r, &g, &b) != -1)
    {
        if (r<0 || r>255 || g<0 || g>255 || b<0 || b>255)
        {
            isValid = false;
        }
    }
    else
    {
        isValid = false;
    }
    return isValid;
}

void handleLedMode()
{
    int ledMode = currentLedMode;
    String ledColor = currentLedColor;
    if (showingSystemNotification)
    {
        ledMode = currentSystemNotificationLedMode;
        ledColor = currentSystemNotificationLedColor;
    }
    else if (showingNotification)
    {
        ledMode = currentNotificationLedMode;
        ledColor = currentNotificationLedColor;
    }

    switch(ledMode)
    {
        case 0: //rainbow
            ledRainbow(RAINBOW_DELAY_TIME);
            break;
        case 1: //gentle flash
            ledFlash(ledColor, GENTLE_FLASH_DELAY_TIME);
            break;
        case 2: //fast flash
            ledFlash(ledColor, FAST_FLASH_DELAY_TIME);
            break;
        case 3: //fixed color
            setHex(ledColor);
            break;
        case 4: //off
            setHex("000000"); //off
            break;
    }
}

void ledFlash(String hexColor, int delayTime)
{
    if (millis() - previousLEDMillis >= delayTime)
    {
        previousLEDMillis = millis();
        ledFlashToggle = !ledFlashToggle;

        if (ledFlashToggle)
        {
            setHex(hexColor); //on
        }
        else
        {
            setHex("000000"); //off
        }
    }
}

void ledRainbow(int delayTime)
{
    int targetR;
    int targetG;
    int targetB;
    switch(rainbowIndex)
    {
        case 6:
            rainbowIndex = 0;
        case 0: //red
            targetR = 255;
            targetG = 0;
            targetB = 0;
            break;
        case 1: //green
            targetR = 0;
            targetG = 255;
            targetB = 0;
            break;
        case 2: //blue
            targetR = 0;
            targetG = 0;
            targetB = 255;
            break;
        case 3: //yellow
            targetR = 255;
            targetG = 255;
            targetB = 0;
            break;
        case 4: //purple
            targetR = 80;
            targetG = 0;
            targetB = 80;
            break;
        case 5: //aqua
            targetR = 0;
            targetG = 255;
            targetB = 255;
            break;
    }
    if (millis() - previousRainbowMillis >= delayTime)
    {
        previousRainbowMillis = millis();
        if (transitionTo(targetR,targetG,targetB))
        {
            rainbowIndex++;
        }
    }
}

bool transitionTo(int targetR, int targetG, int targetB) //Transitions to a given color one step at a time. Returns true if already at transition position.
{
    if (rainbowRed != targetR || rainbowGreen != targetG || rainbowBlue != targetB)
    {
        if ( rainbowRed < targetR ) rainbowRed += 1;
        if ( rainbowRed > targetR ) rainbowRed -= 1;
    
        if ( rainbowGreen < targetG ) rainbowGreen += 1;
        if ( rainbowGreen > targetG ) rainbowGreen -= 1;
    
        if ( rainbowBlue < targetB ) rainbowBlue += 1;
        if ( rainbowBlue > targetB ) rainbowBlue -= 1; 
        setRGB(rainbowRed, rainbowGreen, rainbowBlue);
        return false;
    }
    else
    {
        return true;
    }
}

/*
 * Pushbutton methods
 */
void setup_pushbutton()
{
    pinMode(PUSHBUTTON_INPUT, INPUT);
}

void check_pushbutton()
{
    if(digitalRead(PUSHBUTTON_INPUT) == HIGH)
    {
        if(!pushbuttonIsAlreadyPushed)
        {
            pushbuttonIsAlreadyPushed = true;
            pushbuttonPressHandled = false; //set a global signal that button has been pushed
            Serial.println("PUSHBUTTON PRESS");
            delay(150); //prevent accidental button bounce
        }
    }
    else
    {
        pushbuttonIsAlreadyPushed = false;
    }
}

/*
 * LCD methods
 */
void setup_lcd()
{
    lcd.begin(16,2);
    lcdClear();
}
//LCD WRAPPER COMMANDS 
void lcdClear()
{
    virtualCursorCol = 0;
    virtualCursorRow = 0;
    virtualLCDTop    = "                                "; //32 chars per row to be sure
    virtualLCDBottom = "                                ";

    customCharsUsed[0] = 0;
    customCharsUsed[1] = 0;
    customCharsUsed[2] = 0;
    customCharsUsed[3] = 0;
    customCharsUsed[4] = 0;
    customCharsUsed[5] = 0;
    customCharsUsed[6] = 0;
    customChar = 1; //reset curstom character count
    lcd.clear();
}

void lcdSetCursor(int col, int row)
{
    virtualCursorCol = col;
    virtualCursorRow = row;
    lcd.setCursor(col, row);
}

void lcdPrint(String unformattedStr)
{
    String formattedString = lcdFormat(unformattedStr);
    
    //write to virtualLCD
    if (virtualCursorRow == 0) //on top
    {
        for(int i=0; i<formattedString.length(); i++)
        {
            virtualLCDTop[virtualCursorCol] = formattedString[i];
            virtualCursorCol++;
        }
    }
    else //Bottom
    {
        for(int i=0; i<formattedString.length(); i++)
        {
            virtualLCDBottom[virtualCursorCol] = formattedString[i];
            virtualCursorCol++;
        }
    }
    lcd.print(formattedString);
}

String lcdFormat(String unformattedStr) 
{
    String formattedString = unformattedStr;
    formattedString.replace("(())", ""); //empty

    int indexCustomChar = formattedString.indexOf("((");
    int indexClose = formattedString.indexOf("))"); //always need an ending
    int digit1 = 0;
    int digit2 = 0;
    int number = 0;
    int previousIndex = -2;
    while(indexCustomChar != - 1 && indexClose != -1)
    {
        number = -1;
        if(indexCustomChar + 2 < formattedString.length())
        {
            if(isDigit(formattedString[indexCustomChar +2]))
            {
                digit1 = formattedString[indexCustomChar +2]-48; //-48 convert char number to number
                number = digit1;
            }
            if(indexCustomChar + 3 < formattedString.length()) //check if 2 digit nr
            { 
                if(isDigit(formattedString[indexCustomChar +3]))
                {
                    digit2 = formattedString[indexCustomChar +3]-48;
                    number = digit1*10 + digit2;
                }
            }
        }
        if(number != -1)
        {
            String lookFor = "((" + String(number) + "))";    
                
            int indexInArray = intArrayContains(customCharsUsed, number); //check if this character has already been linked to a slot since last clear
            if (indexInArray == -1) //if not already saved
            {
                int previousCustomChar = customChar;
                if (number > 50) //using sytem icons so save them in 0 slot
                {
                    customChar = 0;
                }
                String customCharStr = intToEscapeCharacter(customChar);
                
                formattedString.replace(lookFor, customCharStr); 
                //make sure number isn't out of bounds!
                number = constrain(number,1, 55);
                byte characterData[] = {customChars[(number - 1)*8],
                                        customChars[((number - 1)*8) + 1],
                                        customChars[((number - 1)*8) + 2],
                                        customChars[((number - 1)*8) + 3],
                                        customChars[((number - 1)*8) + 4],
                                        customChars[((number - 1)*8) + 5],
                                        customChars[((number - 1)*8) + 6],
                                        customChars[((number - 1)*8) + 7]};
                //check if character already created at this slot to avoid clearing the screen again and again for nothing
                if (virtualCGRAM[customChar * 8] == characterData[0] &&
                    virtualCGRAM[(customChar * 8) + 1] == characterData[1] &&
                    virtualCGRAM[(customChar * 8) + 2] == characterData[2] &&
                    virtualCGRAM[(customChar * 8) + 3] == characterData[3] &&
                    virtualCGRAM[(customChar * 8) + 4] == characterData[4] &&
                    virtualCGRAM[(customChar * 8) + 5] == characterData[5] &&
                    virtualCGRAM[(customChar * 8) + 6] == characterData[6] &&
                    virtualCGRAM[(customChar * 8) + 7] == characterData[7])
                {
                    //char already made!
                }
                else
                {
                    //put data in virtualCGRAM
                    virtualCGRAM[customChar * 8] = characterData[0];
                    virtualCGRAM[(customChar * 8) + 1] = characterData[1];
                    virtualCGRAM[(customChar * 8) + 2] = characterData[2];
                    virtualCGRAM[(customChar * 8) + 3] = characterData[3];
                    virtualCGRAM[(customChar * 8) + 4] = characterData[4];
                    virtualCGRAM[(customChar * 8) + 5] = characterData[5];
                    virtualCGRAM[(customChar * 8) + 6] = characterData[6];
                    virtualCGRAM[(customChar * 8) + 7] = characterData[7];
                    lcd.createChar(customChar, characterData); 
                    lcd.clear(); //clearing is required after setting char.
                    //reshowing what was cleared
                    lcd.print(virtualLCDTop);
                    lcd.setCursor(0,1);
                    lcd.print(virtualLCDBottom);
                    lcd.setCursor(virtualCursorCol, virtualCursorRow); //make sure the cursor stays where it should be
                }
                customChar = previousCustomChar;
                customCharsUsed[customChar-1] = number; //mark this character as stored since clear
                if (number <= 50)
                {
                    customChar = min(customChar+1, 7); //count like 1 2 3 4 5 6 7 7 7 7 7...
                }
            }
            else
            {
                String customCharStr = intToEscapeCharacter(indexInArray+1);
                formattedString.replace(lookFor, customCharStr); 
            }
        }
        previousIndex = indexCustomChar;
        indexCustomChar = formattedString.indexOf("((");
        indexClose = formattedString.indexOf("))");
        if (previousIndex == indexCustomChar) //if looping detected because of a human error like ((2) <-- forgot bracket
        {
            return "Bad input!";
        }
    }
    return formattedString;
}

int lcdLength(String unformattedStr) //TODO
{
    unformattedStr.replace("(())", ""); //empty

    int lengthStr = 0;
    for (int i=0;i<unformattedStr.length();i++)
    {
        lengthStr++;
        if(unformattedStr[i] == '(')
        {
            if(i+1 < unformattedStr.length())
            {
                if(unformattedStr[i+1] == '(')
                {
                    int indexOfClose = unformattedStr.indexOf("))", i);
                    if (indexOfClose == -1)
                    {
                        Serial.println("Error: Can't find closing characters");
                        return unformattedStr.length(); //returns full unformatted length.
                    }
                    else
                    {
                        i = indexOfClose+1;
                    }
                }
            }
        }
    }   
    return lengthStr;
}

//bit abstract but it gets a substring from the unformattedString using the from and to values you would use if the string would be already formatted.
String lcdSubstring(String unformattedStr, int from, int to) 
{
    unformattedStr.replace("(())", ""); //empty
    //split into "characters"
    String fakeChars[unformattedStr.length()]; //should be more than enough
    int fakeCharsIndex = 0;
    for (int i=0;i<unformattedStr.length();i++)
    {
        if(unformattedStr[i] != '(')
        {
            fakeChars[fakeCharsIndex] = unformattedStr[i];
        }
        else
        {
            if(i+1 < unformattedStr.length())
            {
                if(unformattedStr[i+1] == '(')
                {
                    String fakeChar = unformattedStr.substring(i, unformattedStr.indexOf("))", i)+2);
                    if (fakeChar.length() > 6)
                    {
                        return "Bad input!!";
                    }
                    else
                    {
                        fakeChars[fakeCharsIndex] = fakeChar;
                        i += fakeChar.length()-1;
                    }
                }
                else
                {
                    fakeChars[fakeCharsIndex] = unformattedStr[i];
                }
            }
            else
            {
                fakeChars[fakeCharsIndex] = unformattedStr[i];
            }
        }
        fakeCharsIndex++;
    }
    
    String unformattedSubstring =""; 
    for (int i = 0; i < (to-from);i++)
    {
        unformattedSubstring += fakeChars[i + from]; 
    }
    
    return unformattedSubstring;
}

String intToEscapeCharacter(int digit) //used for setting number to value for custom character //must be a value from 0 to 7
{
    String escapedStr = "";
    switch(digit)
    {
        case 0:
            escapedStr = "\x08"; //foldback adress 8 == 0 https://forum.arduino.cc/index.php?topic=419812.0
            break;
        case 1:
            escapedStr = "\1";
            break;
        case 2:
            escapedStr = "\2";
            break;
        case 3:
            escapedStr = "\3";
            break;
        case 4:
            escapedStr = "\4";
            break;
        case 5:
            escapedStr = "\5";
            break;
        case 6:
            escapedStr = "\6";
            break;
        case 7:
            escapedStr = "\7";
            break;
    }
    return escapedStr;           
}

/*
 * Helper functions
 */
//returns -1 if not contains, else returns index.
int intArrayContains(int intArray[], int value)
{
    int index = -1;
    for (int i=0;i<sizeof(intArray);i++)
    {
        if (intArray[i] == value)
        {
            index = i;
            break;
        }
    }
    return index;
}

/*
 * Arduino methods
 */
void setup()
{
    Serial.begin(115200);
    setup_pushbutton();
    setup_led();
    setup_lcd();
    setup_wifi();
    setup_commands(); //Only call this when you need to update commands
    configTzTime(defaultTimeZone, ntpServer);
    lastTemp = getTemp();
    getStock();
    getImgurAccessToken();
}

void loop()
{
    check_pushbutton();
    handle_pending_notification();
    handleLedMode();
    handleMode();

    if (!ws.isConnected())
    {
        Serial.println("connecting");
        // It technically should fetch url from discord.com/api/gateway
        ws.connect("gateway.discord.gg", "/?v=8&encoding=json", 443);
        createNotification("Connecting...", 1, "null", 1, true, false);
        wasDisconnected = true;
    }
    else
    {
        if (wasDisconnected)
        {
            wasDisconnected = false;
            createNotification("Connected!", 1, "null", 1, true, false);
        }
        unsigned long now = millis();
        if(heartbeatInterval > 0)
        {
            if(now > lastHeartbeatSend + heartbeatInterval)
            {
                if(hasReceivedWSSequence)
                {
                    Serial.println("Send:: {\"op\":1,\"d\":" + String(lastWebsocketSequence, 10) + "}");
                    ws.send("{\"op\":1,\"d\":" + String(lastWebsocketSequence, 10) + "}");
                }
                else
                {
                    Serial.println("Send:: {\"op\":1,\"d\":null}");
                    ws.send("{\"op\":1,\"d\":null}");
                }
                lastHeartbeatSend = now;
            }
            if(lastHeartbeatAck > lastHeartbeatSend + (heartbeatInterval / 2))
            {
                Serial.println("Heartbeat ack timeout");
                ws.disconnect();
                heartbeatInterval = 0;
            }
        }

        String msg;
        if (ws.getMessage(msg))
        {
            deserializeJson(doc, msg); //we use a smaller doc(1024) to make it go fast
            if(doc["t"] == "INTERACTION_CREATE") //This has to happen ASAP so Discord doesn't dismiss the command.
            {
                json_https_request("POST", host,INTERACTION_URL_A + doc["d"]["id"].as<String>() + "/" + doc["d"]["token"].as<String>() + INTERACTION_URL_B, "", COMMAND_ACK); //initial basic ack
            }
            Serial.println(msg);
            
            if(doc["op"] == 0) // Message
            {
                if(doc["t"] == "INTERACTION_CREATE") 
                {
                    deserializeJson(docLong, msg);
                    handleCommand(); //handle further processing of the command
                }
                
                if(doc.containsKey("s"))
                {
                    lastWebsocketSequence = doc["s"];
                    hasReceivedWSSequence = true;
                }
                if(doc["t"] == "READY")
                {
                    websocketSessionId = doc["d"]["session_id"].as<String>();
                    hasWsSession = true;
                }
                
                /*
                if(doc["t"] == "MESSAGE_CREATE")
                {
                    Serial.println("Message:");
                    String extract = doc["d"]["content"];
                    Serial.println(extract);
                    lcd.setCursor(0, 1);
                    lcd.print(extract);
                }*/
            }
            else if(doc["op"] == 9) // Connection invalid
            {
                ws.disconnect();
                hasWsSession = false;
                heartbeatInterval = 0;
            }
            else if(doc["op"] == 11) // Heartbeat ACK
            {
                lastHeartbeatAck = now;
            }
            else if(doc["op"] == 10) // Start
            {
                heartbeatInterval = doc["d"]["heartbeat_interval"];

                if(hasWsSession)
                {
                    Serial.println("Send:: {\"op\":6,\"d\":{\"token\":\"" + String(BOT_TOKEN) + "\",\"session_id\":\"" + websocketSessionId + "\",\"seq\":\"" + String(lastWebsocketSequence, 10) + "\"}}");
                    ws.send("{\"op\":6,\"d\":{\"token\":\"" + String(BOT_TOKEN) + "\",\"session_id\":\"" + websocketSessionId + "\",\"seq\":\"" + String(lastWebsocketSequence, 10) + "\"}}");
                }
                else
                {
                    Serial.println("Send:: {\"op\":2,\"d\":{\"token\":\"" + String(BOT_TOKEN) + "\",\"intents\":" + gateway_intents + ",\"properties\":{\"$os\":\"linux\",\"$browser\":\"ESP8266\",\"$device\":\"ESP8266\"},\"compress\":false,\"large_threshold\":250}}");
                    ws.send("{\"op\":2,\"d\":{\"token\":\"" + String(BOT_TOKEN) + "\",\"intents\":" + gateway_intents + ",\"properties\":{\"$os\":\"linux\",\"$browser\":\"ESP8266\",\"$device\":\"ESP8266\"},\"compress\":false,\"large_threshold\":250}}");
                }

                lastHeartbeatSend = now;
                lastHeartbeatAck = now;
            }
        }
    }
}