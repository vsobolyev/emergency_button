/**
 *  THIS SOFTWARE IS FREE TO MODIFY AND REDISTRIBUTE.
 *  TOO MANY PEOPLE CONTRIBUTED TO THE CREATION OF ALL
 *  THE LIBRARIES USED. THE EFFORT IS APPRECIATED.
 * 
 *  ALL THE LIBRARIES ARE INCLUDED HERE.
 *  THIS WAY IT IS EASIER TO CONTROL THE ORDER THEY'RE
 *  LOADED IN.
 *  
 *  VARIABLES ARE STORED HERE. THIS WAY IT WILL BE IMPOSSIBLE
 *  TO MESS UP ACCESSING THEM
 */
//#include "lwip/tcp_impl.h"        // TELEGRAM avoids memory leak.
#include <UniversalTelegramBot.h> // TELEGRAM
#include "FS.h"                   // ESP8266 STORAGE MEMORY MANAGEMENT
#include <ArduinoJson.h>          // ESP8266 json implementation
#include <ESP8266Ping.h>          // ESP8266 ping library
#include <ESP8266WiFi.h>          // WIFI    https://github.com/esp8266/Arduino
#include <DNSServer.h>            // WIFI
#include <ESP8266WebServer.h>     // WIFI
#include <WiFiManager.h>          // WIFI    https://github.com/tzapu/WiFiManager
#include <WiFiUDP.h>              // WIFI    WOL
#include <WakeOnLan.h>            // WIFI    WOL
#include <SPI.h>                  // NRF24   SPI
#include "RF24.h"                 // NRF24   LIBRARY
#include "nRF24L01.h"             // NRF24   LIBRARY HEADERS
#include <IFTTTMaker.h>           // IFTTT   SEND EMAIL ALERTS
#include <WiFiClientSecure.h>     // IFTTT   USED BY IFTTT AND TELEGRAM
#include <ArduinoOTA.h>           // OTA, must be included here. otherwise won't work.

/**
 * WEB SERVER
 */
std::unique_ptr<ESP8266WebServer> server; //pointer to server object
String header;      //header for web page
String header0;     //header for web page
String header1;     //header for web page
String error_404;   //error message for web page
String login;       //login website for web page
String wrong_user;  //wrong user for web page

/**
 * WIFI MUST DEFINE VARIABLES
 */
WiFiManager wifiManager;                              // WIFI MANAGER POINTER TO THE OBJECT
WiFiUDP UDP;                                          // WOL UDP object
String computer_ip;                                   // WOL ip of the pc you want to wake up
IPAddress _ip;                                        // WOL
String default_ip = "192.168.1.1";                    // ip address in case provided one doesn't work
byte wol_mac[6];                                      // WOL Gigabyte miniPC 
String ssid;                                          // your network SSID (name)
String password;                                      // your network key
String wifiApName;                                    // SSID for the access point
String wifiApPassword;                                // password for the access point

/**
 * COMMON VARIABLES
 */
 String settings;               
#define blueLed D1              // LED pin
#define redLed D3               // LED pin
#define button A0               // Button pin. All the buttons are connected to A0. Buttons are destinguished by different resistor value
bool ALERT = false;             // FLAG TO MAKE SURE ALERT WILL BE RAISED IF A SUDDEN REBOOT WILL HAPPEN
int btn;                        // analog button value
bool ledState = false;          // led state for toggle
bool DEBUG = true;              // enable or disable debug messages
bool SETTINGS_MODE = false;     // controls what mode device initializes in.
const int bufferSize = 2300;    // size of the buffer for reading/writing SPIFFS files 

/**
 * OTA 
 */
const char* host = "OTA-TBOT";

/**
 * NRF24 variables
 */
#define CE_PIN D2             // NRF CE PIN
#define CSN_PIN D8            // NRF CSN PIN
RF24 radio(CE_PIN, CSN_PIN);  // NRF object 
byte pipeNo;                  // pipe Number the message was received from
byte gotByte;                 // value recieved
byte gotBytePrev = 0;         // previous value received. needed to keep track of repeated messages
const byte address[][7] = {"server" , "button"};    // NRF communication roles

/**
 * TELEGRAM VARIABLES
 */
WiFiClientSecure client;                   //client used by ifttt and telegram
std::unique_ptr<UniversalTelegramBot> bot; // pointer to the bot
uint8_t numNewMessages;                    // number of incomming messages
const uint8_t numContacts = 5;             //number of telegram contacts stored
String EMPTY = "empty";                    //value used when null, deleted, etc...
String telegram_contacts[numContacts];     //telegram contact container.
String telegram_group;                     //telegram group id
String telegram_bot_token;                 //telegram bot token
int Bot_mtbs = 1000;                       //mean time between scan messages
long Bot_lasttime;                         //last time messages' scan has been done

/**
 * IFTTT VARIABLES
 */
String ifttt_key;            // Get it from this page https://ifttt.com/services/maker_webhooks/settings
String ifttt_event_name;     // Name of your event name, set when you are creating the applet

void setup() {
  pinMode(redLed, OUTPUT);
  pinMode(blueLed, OUTPUT);
  digitalWrite(redLed,HIGH);
  digitalWrite(blueLed,HIGH);
  wifiManager.setDebugOutput(false);
  
  setupFileSystem();   // initialize FS and load data from internal memory.
  if(DEBUG){
    wifiManager.setDebugOutput(true);
  }  
  setupWifi();         // initialize wifi
  if(!SETTINGS_MODE){
    //modify watchdog interval to 8seconds
    ESP.wdtDisable();
    ESP.wdtEnable(WDTO_8S);
    
    setupNRF();       // initialize nrf module
    setupTelegram();  // initialize telegram
  }else{
    setupWebServer(); //initialize configuration web server
    setupOTA();       //initialize OTA 
    //SETTINGS_MODE must be turned off after first start.
    SETTINGS_MODE = false;
    saveConfig();
    SETTINGS_MODE = true;
  }
  digitalWrite(redLed,LOW);
  digitalWrite(blueLed,LOW);
}

int n = 0; //helps to order function execution
void loop() {
  // it will help to avoid exception 29 problem.
  if(!SETTINGS_MODE){//normal, non settings mode.   
    n++; //increment n
    ESP.wdtFeed(); // feed watchdog manually
    tcpCleanup();  //THIS IS SUPER IMPORTANT. SOLVES MEMORY LEAK
    if(n == 1 || n == 3){
      readButtons(); //read buttons
    }else if(n == 2){
      listenNRF();   //listen to emergency button
    }else if(n == 4){
      listenTelegram(); //telegram messages
    }else{
      n = 0;
    }
  }else{
    // we're in settings mode
    otaHandle();
    handleWebServer();
  }
  
}
