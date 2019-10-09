#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>
#include <NewPing.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "soc/soc.h" //disable brownour problems
#include "soc/rtc_cntl_reg.h"  //disable brownour problems
#include <SoftwareSerial.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include "RTClib.h"
#include <MD_YX5300.h>
//define command from app

//command music
#define NEXT_SONG 'a'
#define PLAY_SONG 'b'
#define PAUSE_SONG 'c'
#define PREV_SONG 'd'
//command lamp_mode
#define MANUAL_MODE 'e'
#define AUTO_MODE 'f'
#define WHITE_MODE 'g'
#define YELLOW_MODE 'h'
#define WHITE_YEL_MODE 'i'
#define OFF_MODE 'k'
RTC_DS1307 rtc;
//intit light sensor
Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);
sensor_t light_sensor;
//init rtc ds1307
RTC_DS1307 rtc;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
//init mp3 player
// Connections for serial interface to the YX5300 module
const uint8_t ARDUINO_RX = 4;    // connect to TX of MP3 Player module
const uint8_t ARDUINO_TX = 5;    // connect to RX of MP3 Player module
MD_YX5300 mp3(ARDUINO_RX, ARDUINO_TX);
//init srf05
#define TRIGGER_PIN  12  // Arduino pin tied to trigger pin on the ultrasonic sensor.
#define ECHO_PIN     11  // Arduino pin tied to echo pin on the ultrasonic sensor.
#define MAX_DISTANCE 200 // Maximum distance we want to ping for (in centimeters). Maximum sensor distance is rated at 400-500cm.
NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE); // NewPing setup of pins and maximum distance.
// init ble
BLEServer *pServer = NULL;
BLECharacteristic * pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t txValue = 0;
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
//wifi config
const char* host = "esp32";
const char* ssid = "FLEX";
const char* password = "helloflex";
IPAddress local_IP(192, 168, 1, 184);
// Set your Gateway IP address
IPAddress gateway(192, 168, 1, 1);

IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8); // optional
IPAddress secondaryDNS(8, 8, 4, 4); // optional
WebServer server(80);
const char* serverIndex = 
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
   "<input type='file' name='update'>"
        "<input type='submit' value='Update'>"
    "</form>"
 "<div id='prg'>progress: 0%</div>"
 "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')" 
 "},"
 "error: function (a, b, c) {"
 "}"
 "});"
 "});"
 "</script>";
 /*
 * Login page
 */
const char* loginIndex = 
 "<form name='loginForm'>"
    "<table width='20%' bgcolor='A09F9F' align='center'>"
        "<tr>"
            "<td colspan=2>"
                "<center><font size=4><b>ESP32 Login Page</b></font></center>"
                "<br>"
            "</td>"
            "<br>"
            "<br>"
        "</tr>"
        "<td>Username:</td>"
        "<td><input type='text' size=25 name='userid'><br></td>"
        "</tr>"
        "<br>"
        "<br>"
        "<tr>"
            "<td>Password:</td>"
            "<td><input type='Password' size=25 name='pwd'><br></td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
            "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
        "</tr>"
    "</table>"
"</form>"
"<script>"
    "function check(form)"
    "{"
    "if(form.userid.value=='admin' && form.pwd.value=='admin123')"
    "{"
    "window.open('/serverIndex')"
    "}"
    "else"
    "{"
    " alert('Error Password or Username')/*displays error message*/"
    "}"
    "}"
"</script>";
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {
        Serial.println("*********");
        Serial.print("Received Value: ");
        for (int i = 0; i < rxValue.length(); i++)
          Serial.print(rxValue[i]);
      }
      //music 
      if(rxValue.find(NEXT_SONG)!=-1)
      {
        Serial.println("next song");
        mp3.playNext();
      }
      else if(rxValue.find(PLAY_SONG)!=-1)
      {
        Serial.println("play song");
        mp3.playStart();
      }
      else if(rxValue.find(PAUSE_SONG)!=-1)
      {
        Serial.println("pause song");
        mp3.playPause();
      }
      else if(rxValue.find(PREV_SONG)!=-1)
      {
        Serial.println("prev song");
        mp3.playPrev();
      }
      //lamp mode
      else if(rxValue.find(MANUAL_MODE)!=-1)
      {
        Serial.println("manual lamp mode");
        manual_mode();
      }else if(rxValue.find(AUTO_MODE)!=-1)
      {
        Serial.println("auto mode");
        auto_mode();
      }else if(rxValue.find(WHITE_MODE)!=-1)
      {
        Serial.println("white mode");
        white_mode();
      }else if(rxValue.find(YELLOW_MODE)!=-1)
      {
        Serial.println("yellow mode");
        yellow_mode();
      }else if(rxValue.find(WHITE_YEL_MODE)!=-1)
      {
        Serial.println("white yellow song");
        white_yel_mode();
      }else if(rxValue.find(OFF_MODE)!=-1)
      {
        Serial.println("off");
        off();
      }
    }
};

void setup() {
  // init debug teminal 
  Serial.begin(115200);
  //init ble
  ble_init();
  //init rtc ds1307
  Wire.begin(22,21);
  if (! rtc.begin()) {
   Serial.println("Couldn't find RTC");
   while (1);
  }
  if (! rtc.isrunning()) 
  {
    Serial.println("RTC is NOT running!");
    // following line sets the RTC to the date & time this sketch was compiled
     rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  //init light sensor
  tsl.getSensor(&light_sensor);
  Serial.println("------------------------------------");
  Serial.print  ("Sensor:       "); Serial.println(light_sensor.name);
  Serial.print  ("Driver Ver:   "); Serial.println(light_sensor.version);
  tsl.enableAutoRange(true);
  tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);
  //init mp3 music module
  mp3.begin();
  mp3.setSynchronous(true);
  mp3.setCallback(cbResponse);
}

void loop() {
  if (deviceConnected) 
  {
    int distance=0,light_val=0;
    //read light lux
    sensors_event_t event;
    tsl.getEvent(&event);
    /* Display the results (light is measured in lux) */
    if (event.light)
    {
      light_val=event.light;
      Serial.print(light_val); Serial.println(" lux");
    }
    else
    {
      Serial.println("Sensor overload");
    }
    //read distance from srf05
    distance=sonar.ping_cm();
    Serial.print(distance); // Send ping, get distance in cm and print result (0 = outside set distance range)
    Serial.println("cm");
    //send data sensor via ble
    char sensor_data[10];
    sprintf(sensor_data, "%d,%d", distance, light_val);
    pCharacteristic->setValue(sensor_data);
    pCharacteristic->notify(); // Envia o valor para o aplicativo!
    Serial.print("*** Send data: ");
    Serial.print(sensor_data);
    Serial.println(" ***");
  } 
  delay(500);
}
void ble_init()
{
  // Create the BLE Device
  BLEDevice::init("Smart Lamp");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pTxCharacteristic = pService->createCharacteristic(
                    CHARACTERISTIC_UUID_TX,
                    BLECharacteristic::PROPERTY_NOTIFY
                  );
                      
  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic * pRxCharacteristic = pService->createCharacteristic(
                       CHARACTERISTIC_UUID_RX,
                      BLECharacteristic::PROPERTY_WRITE
                    );

  pRxCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");
}
