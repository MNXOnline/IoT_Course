#include <FS.h>                   //this needs to be first, or it all crashes and burns...

//ESP8266 Native libraries
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266HTTPClient.h>

//External libraries
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson


// I2C Barometric sensor libraries
#include <Wire.h>
#include <Adafruit_BMP085.h>
#define SCL_PIN D1
#define SDA_PIN D2
Adafruit_BMP085 bmp;

unsigned long previousMillis=0;
unsigned long deltaMillis=15000;
void dataSenderCallback();



ESP8266WebServer server(80);            //Web server
WiFiManager wifiManager;                //Wifi Manager to setup WIFI SSID
ESP8266HTTPUpdateServer httpUpdater;    //For HTTP OTA Update
HTTPClient http;                        //Declare object of class HTTPClient

#define USE_CREDENTIALS



/*************** Global Variables ***************/
//FIWARE Variables
String FIWARE_device_ID = "SENSOR_ID";
String FIWARE_server = "195.235.93.235";
String FIWARE_port = "8085";
String FIWARE_apikey = "REEMPLAZAR_APIKEY";

//Wifi Variables
const char* WiFi_Network = "NOMBRE_WIFI";
const char* WiFi_Password = "PASS_WIFI";

const char* WiFi_SoftAP_Name = "FIWAREZone_IoT";
const char* WiFi_SoftAP_WiFi_Name = "FIWAREZone_IoT_Wifi";


//OTA
const char* update_path = "/webota";
const char* update_username = "admin";
const char* update_password = "admin";

//Other Variables
int transmisionStatus = 0;
char rxBuf[64];
int rxBufIndex;
char dev_hostname[30];






/*************** Setup Code ***************/
void setup() {
  //serial Port setup
  Serial.begin(9600);
  Serial.println();

  //Setup Hostname
  (String("SC-") + ESP.getChipId()).toCharArray(dev_hostname, 30);
  Serial.print("Dev-name: ");
  Serial.println(dev_hostname);

  //reset settings - for testing
  //wifiManager.resetSettings();

  wifiManager.setTimeout(5);


  // I2C Sensor Initialization
  Wire.pins(SDA_PIN, SCL_PIN);
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!bmp.begin()) {
     Serial.println("No BMP180 / BMP085");// we dont wait for this
     while (1) {}
  }
  

//Configuring WIFI
#ifdef USE_CREDENTIALS
/***************************Hardcoded credentials*******************************/
  WiFi.begin(WiFi_Network, WiFi_Password);

  Serial.println();
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("success!");
  Serial.print("IP Address is: ");
  Serial.println(WiFi.localIP());


#else
  /***************************Wifi Manager usage********************************/
  if (!wifiManager.autoConnect(WiFi_SoftAP_Name, "")) {
    Serial.println("failed to connect and hit timeout");
    
    //Start Soft AP
    WiFi.softAP(WiFi_SoftAP_Name, "");
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);
  }
  else {

    //if you get here you have connected to the WiFi
    Serial.println("Connected!");

    // Set up mDNS responder:
    if (!MDNS.begin(dev_hostname)) {
      Serial.println("Error setting up MDNS responder!");
      while (1) {
        delay(1000);
      }
    }
    Serial.println("mDNS responder started");

    // Add service to MDNS-SD
    MDNS.addService("http", "tcp", 80);

  }
#endif



  //Start OTA updater
  httpUpdater.setup(&server, update_path, update_username, update_password);

  // Start TCP (HTTP) server
  server.begin();
  Serial.println("TCP server started");

  //Bind Webserver URL functions
  server.on ( "/", handleRoot );
  server.on ( "/wrst", handleWrst );
  server.on ( "/wifi", handleWifi );
  server.on ( "/webota", handleWebota );
  server.on ( "/help", handleHelp );
  server.on ( "/postul2", handlePostUL2 );
  server.on ( "/postul2data", handlePostUL2data );
  server.on ( "/sensors", handleSensors );
}



void loop() {
  //WebServer task
  server.handleClient();


  if (millis()>previousMillis+deltaMillis){
    previousMillis=millis();
    //Execute task
    dataSenderCallback();
  }
}





int ultralightSend (String URL, String port, String Token, String ID, String Body) {
  int httpCode = 0;
  
  Serial.println("http://" + URL + ":" + port + "/iot/d?k=" + Token + "&i=" + ID + "&getCmd=1");        
  http.begin("http://" + URL + ":" + port + "/iot/d?k=" + Token + "&i=" + ID + "&getCmd=1");            //Specify request destination

  http.addHeader("Content-Type", "text/plain");       	//Specify content-type header

  httpCode = http.POST(Body);           				//Send the Body
  String payload = http.getString();                    //Get the response payload

  Serial.println("Request Done");
  Serial.print("Return code: ");
  Serial.println(httpCode);   							//Print HTTP return code
  Serial.print("Response payload: ");
  Serial.println(payload);    							//Print request response payload

  http.end();  											//Close connection
  return httpCode;
}

String dataReaderSensor(){
    String t = "T=" + String(bmp.readTemperature()) + " *C";
    String p = "P=" + String(bmp.readPressure()) + " Pa";
    String a = "A=" + String(bmp.readAltitude(101325)) + " m";// insert pressure at sea level

  Serial.println("Reading data from sensors:");
  Serial.println(t);
  Serial.println(p);
  Serial.println(a);

  String r = "t|"+ String(bmp.readTemperature()) +"#p|"+ String(bmp.readPressure()) +"#a|"+ String(bmp.readAltitude(101325));
  return r;
}

void dataSenderCallback(){
  int returnCode = ultralightSend(FIWARE_server,FIWARE_port,FIWARE_apikey,FIWARE_device_ID,dataReaderSensor());
   if (returnCode ==200){
      Serial.println("Data sent successfully to FIWARE");
    }
    else{
      Serial.println("Failure sending data to FIWARE");
    }
}