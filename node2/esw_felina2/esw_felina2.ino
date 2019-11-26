#include <JSONVar.h>
#include <Arduino_JSON.h>
#include <JSON.h>
#include <Wire.h>
#include "Adafruit_SGP30.h"
#include "MutichannelGasSensor.h"
#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#include "ThingSpeak.h"
#include "secrets2.h"

#include <Adafruit_Sensor.h>
#include "DHT.h"



uint32_t delayMS;
int count=0;


// ##################### Update the Wifi SSID, Password and IP adress of the server ##########
// WIFI params
char* WIFI_SSID = "esw-m19@iiith"; 
char* WIFI_PSWD = "e5W-eMai@3!20hOct";

String CSE_IP      = "onem2m.iiit.ac.in";
// #######################################################

int WIFI_DELAY  = 100; //ms

// oneM2M : CSE params
int   CSE_HTTP_PORT = 443;
String CSE_NAME    = "in-name";
String CSE_M2M_ORIGIN  = "admin:admin";

// oneM2M : resources' params
String DESC_CNT_NAME = "DESCRIPTOR";
String DATA_CNT_NAME = "DATA";
String CMND_CNT_NAME = "COMMAND";
int TY_AE  = 2;
int TY_CNT = 3;
int TY_CI  = 4;
int TY_SUB = 23;

// HTTP constants
int LOCAL_PORT = 9999;
char* HTTP_CREATED = "HTTP/1.1 201 Created";
char* HTTP_OK    = "HTTP/1.1 200 OK\r\n";
int REQUEST_TIME_OUT = 5000; //ms

//MISC
int LED_PIN   = D1;
int SERIAL_SPEED  = 9600;

#define DEBUG

///////////////////////////////////////////
//sensor variables
#define DHTPIN D4     // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
DHT dht(DHTPIN, DHTTYPE);
float dht_val[2];
Adafruit_SGP30 sgp;

unsigned long DHT22_API = SECRET_CH_ID_DHT22;
unsigned long SGP20 = SECRET_CH_ID_SGP20;
unsigned long MGS = SECRET_CH_ID_MGS;
const char * DHT22_WRITE_API = SECRET_WRITE_APIKEY_DHT22;
const char * SGP20_WRITE_API = SECRET_WRITE_APIKEY_SGP20;
const char * MGS_WRITE_API = SECRET_WRITE_APIKEY_MGS;

void dht_usr()
{
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  float f = dht.readTemperature(true);

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || isnan(f)) {
    ;//Serial.println(F("Failed to read from DHT sensor!"));
    ;
    return;
  }

  // Compute heat index in Fahrenheit (the default)
  float hif = dht.computeHeatIndex(f, h);
  // Compute heat index in Celsius (isFahreheit = false)
  float hic = dht.computeHeatIndex(t, h, false);
  dht_val[0] = h;
  dht_val[1] = t;

  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.print(F("%  Temperature: "));
  Serial.print(t);
  Serial.print(F("°C "));
  Serial.print(f);
  Serial.print(F("°F  Heat index: "));
  Serial.print(hic);
  Serial.print(F("°C "));
  Serial.print(hif);
  Serial.println(F("°F"));

  ThingSpeak.setField(1, h);
  ThingSpeak.setField(2, t);
  ThingSpeak.setField(3, hic);
  int x = ThingSpeak.writeFields(DHT22_API, DHT22_WRITE_API);
  if(x == 200){
    Serial.println("Channel update successful.");
  }
  else{
    Serial.println("Problem updating channel. HTTP error code " + String(x));
  }
  
}
float sgp_val[3];

void sgp_think()
{
  ThingSpeak.setField(1, sgp_val[0]);
  ThingSpeak.setField(2, sgp_val[1]);
  ThingSpeak.setField(3, sgp_val[2]);
  ThingSpeak.setField(4, sgp_val[3]);
  int x = ThingSpeak.writeFields(SGP20 , SGP20_WRITE_API);
  if(x == 200){
    Serial.println("Channel update successful.");
  }
  else{
    Serial.println("Problem updating channel. HTTP error code " + String(x));
  }
}
void sgp_usr()
{
    if (! sgp.IAQmeasure()) {
      Serial.println("Measurement failed");
    return;
  }
  sgp_val[0] = sgp.TVOC;
  sgp_val[1]= sgp.eCO2;
  Serial.print("TVOC "); Serial.print(sgp.TVOC); Serial.print(" ppb\t");
  Serial.print("eCO2 "); Serial.print(sgp.eCO2); Serial.println(" ppm");

  if (! sgp.IAQmeasureRaw()) {
    Serial.println("Raw Measurement failed");
    return;
  }
  sgp_val[2]= sgp.rawH2;
  sgp_val[2]= sgp.rawEthanol;
  Serial.print("Raw H2 "); Serial.print(sgp.rawH2); Serial.print(" \t");
  Serial.print("Raw Ethanol "); Serial.print(sgp.rawEthanol); Serial.println("");
 
  delay(1000);


    uint16_t TVOC_base, eCO2_base;
    if (! sgp.getIAQBaseline(&eCO2_base, &TVOC_base)) {
      Serial.println("Failed to get baseline readings");
      return;
    }
    Serial.print("****Baseline values: eCO2: 0x"); Serial.print(eCO2_base, HEX);
    Serial.print(" & TVOC: 0x"); Serial.println(TVOC_base, HEX);


  
}

float mgs_val[3];
void mgs_usr()
{
      float c;

    c = gas.measure_NH3();
    mgs_val[0]=c;
    Serial.print("The concentration of NH3 is ");
    if(c>=0) {
      Serial.print(c);
      ThingSpeak.setField(1, c);
    }
    else Serial.print("invalid");
    Serial.println(" ppm");

    c = gas.measure_CO();
    mgs_val[1] = c;
    Serial.print("The concentration of CO is ");
    if(c>=0) {
      Serial.print(c);
      ThingSpeak.setField(2, c);

    }
    else Serial.print("invalid");
    Serial.println(" ppm");

    c = gas.measure_NO2();
    mgs_val[2] = c;
    Serial.print("The concentration of NO2 is ");
    if(c>=0) {
      Serial.print(c);
      ThingSpeak.setField(3, c);
    }
    else Serial.print("invalid");
    Serial.println(" ppm");

//    c = gas.measure_C3H8();
//    Serial.print("The concentration of C3H8 is ");
//    if(c>=0) {
//      Serial.print(c);
//    }
//    else Serial.print("invalid");
//    Serial.println(" ppm");
//    
//    c = gas.measure_C4H10();
//    Serial.print("The concentration of C4H10 is ");
//    if(c>=0) {
//      Serial.print(c);
//    }
//    else Serial.print("invalid");
//    Serial.println(" ppm");
//
//    c = gas.measure_CH4();
//    Serial.print("The concentration of CH4 is ");
//    if(c>=0) {
//      Serial.print(c);
//    }
//    else Serial.print("invalid");
//    Serial.println(" ppm");
//
//    c = gas.measure_H2();
//    Serial.print("The concentration of H2 is ");
//    if(c>=0) {
//      Serial.print(c);
//    }
//    else Serial.print("invalid");
//    Serial.println(" ppm");
//
//    c = gas.measure_C2H5OH();
//    Serial.print("The concentration of C2H5OH is ");
//    if(c>=0) {
//      Serial.print(c);
//    }
//    else Serial.print("invalid");
//    Serial.println(" ppm");

    delay(1000);
    ;//Serial.println("...");
   int x = ThingSpeak.writeFields(MGS , MGS_WRITE_API);
  if(x == 200){
    Serial.println("Channel update successful.");
  }
  else{
    Serial.println("Problem updating channel. HTTP error code " + String(x));
  }
        
}
///////////////////////////////////////////



// Global variables
WiFiServer server(LOCAL_PORT);    // HTTP Server (over WiFi). Binded to listen on LOCAL_PORT contant
WiFiClient client;
String context = "";
String command = "";        // The received command



// Method for creating an HTTP POST with preconfigured oneM2M headers
// param : url  --> the url path of the targted oneM2M resource on the remote CSE
// param : ty --> content-type being sent over this POST request (2 for ae, 3 for cnt, etc.)
// param : rep  --> the representaton of the resource in JSON format
String doPOST(String url, int ty, String rep) {

  String postRequest = String() + "POST " + url + " HTTP/1.1\r\n" +
                       "Host: " + CSE_IP + ":" + CSE_HTTP_PORT + "\r\n" +
                       "X-M2M-Origin: " + CSE_M2M_ORIGIN + "\r\n" +
                       "Content-Type: application/json;ty=" + ty + "\r\n" +
                       "Content-Length: " + rep.length() + "\r\n"
                       "Connection: close\r\n\n" +
                       rep;

  // Connect to the CSE address

  Serial.println("connecting to " + CSE_IP + ":" + CSE_HTTP_PORT + " ...");

  // Get a client
  WiFiClient client;
  if (!client.connect(CSE_IP, CSE_HTTP_PORT)) {
    Serial.println("Connection failed !");
    return "error";
  }

  // if connection succeeds, we show the request to be send
#ifdef DEBUG
  Serial.println(postRequest);
#endif

  // Send the HTTP POST request
  client.print(postRequest);

  // Manage a timeout
  unsigned long startTime = millis();
  while (client.available() == 0) {
    if (millis() - startTime > REQUEST_TIME_OUT) {
      Serial.println("Client Timeout");
      client.stop();
      return "error";
    }
  }

  // If success, Read the HTTP response
  String result = "";
  if (client.available()) {
    result = client.readStringUntil('\r');
    //    Serial.println(result);
  }
  while (client.available()) {
    String line = client.readStringUntil('\r');
    Serial.print(line);
  }
  Serial.println();
  Serial.println("closing connection...");
  return result;
}

// Method for creating an ApplicationEntity(AE) resource on the remote CSE (this is done by sending a POST request)
// param : ae --> the AE name (should be unique under the remote CSE)
String createAE(String ae) {
  String aeRepresentation =
    "{\"m2m:ae\": {"
    "\"rn\":\"" + ae + "\","
    "\"api\":\"org.demo." + ae + "\","
    "\"rr\":\"true\","
    "\"poa\":[\"http://" + WiFi.localIP().toString() + ":" + LOCAL_PORT + "/" + ae + "\"]"
    "}}";
#ifdef DEBUG
  Serial.println(aeRepresentation);
#endif
  return doPOST("/" + CSE_NAME, TY_AE, aeRepresentation);
}

// Method for creating an Container(CNT) resource on the remote CSE under a specific AE (this is done by sending a POST request)
// param : ae --> the targeted AE name (should be unique under the remote CSE)
// param : cnt  --> the CNT name to be created under this AE (should be unique under this AE)
String createCNT(String ae, String cnt) {
  String cntRepresentation =
    "{\"m2m:cnt\": {"
    "\"rn\":\"" + cnt + "\","
    "\"min\":\"" + -1 + "\""
    "}}";
  return doPOST("/" + CSE_NAME + "/" + ae, TY_CNT, cntRepresentation);
}

// Method for creating an ContentInstance(CI) resource on the remote CSE under a specific CNT (this is done by sending a POST request)
// param : ae --> the targted AE name (should be unique under the remote CSE)
// param : cnt  --> the targeted CNT name (should be unique under this AE)
// param : sciContent --> the CI content (not the name, we don't give a name for ContentInstances)
String createCI(String ae, String cnt, String ciContent) {
  String ciRepresentation =
    "{\"m2m:cin\": {"
    "\"con\":\"" + ciContent + "\""
    "}}";
  return doPOST("/" + CSE_NAME + "/" + ae + "/" + cnt, TY_CI, ciRepresentation);
}


// Method for creating an Subscription (SUB) resource on the remote CSE (this is done by sending a POST request)
// param : ae --> The AE name under which the SUB will be created .(should be unique under the remote CSE)
//          The SUB resource will be created under the COMMAND container more precisely.
String createSUB(String ae) {
  String subRepresentation =
    "{\"m2m:sub\": {"
    "\"rn\":\"SUB_" + ae + "\","
    "\"nu\":[\"" + CSE_NAME + "/" + ae  + "\"], "
    "\"nct\":1"
    "}}";
  return doPOST("/" + CSE_NAME + "/" + ae + "/" + CMND_CNT_NAME, TY_SUB, subRepresentation);
}


// Method to register a module (i.e. sensor or actuator) on a remote oneM2M CSE
void registerModule(String module, bool isActuator, String intialDescription, String initialData) {
  if (WiFi.status() == WL_CONNECTED) {
    String result;
    // 1. Create the ApplicationEntity (AE) for this sensor
    result = createAE(module);
    if (result == HTTP_CREATED) {
#ifdef DEBUG
      Serial.println("AE " + module + " created  !");
#endif

      // 2. Create a first container (CNT) to store the description(s) of the sensor
      result = createCNT(module, DESC_CNT_NAME);
      if (result == HTTP_CREATED) {
#ifdef DEBUG
        Serial.println("CNT " + module + "/" + DESC_CNT_NAME + " created  !");
#endif


        // Create a first description under this container in the form of a ContentInstance (CI)
        result = createCI(module, DESC_CNT_NAME, intialDescription);
        if (result == HTTP_CREATED) {
#ifdef DEBUG
          Serial.println("CI " + module + "/" + DESC_CNT_NAME + "/{initial_description} created !");
#endif
        }
      }

      // 3. Create a second container (CNT) to store the data  of the sensor
      result = createCNT(module, DATA_CNT_NAME);
      if (result == HTTP_CREATED) {
#ifdef DEBUG
        Serial.println("CNT " + module + "/" + DATA_CNT_NAME + " created !");
#endif

        // Create a first data value under this container in the form of a ContentInstance (CI)
        result = createCI(module, DATA_CNT_NAME, initialData);
        if (result == HTTP_CREATED) {
#ifdef DEBUG
          Serial.println("CI " + module + "/" + DATA_CNT_NAME + "/{initial_aata} created !");
#endif
        }
      }

      // 3. if the module is an actuator, create a third container (CNT) to store the received commands
      if (isActuator) {
        result = createCNT(module, CMND_CNT_NAME);
        if (result == HTTP_CREATED) {
#ifdef DEBUG
          Serial.println("CNT " + module + "/" + CMND_CNT_NAME + " created !");
#endif

          // subscribe to any ne command put in this container
          result = createSUB(module);
          if (result == HTTP_CREATED) {
#ifdef DEBUG
            Serial.println("SUB " + module + "/" + CMND_CNT_NAME + "/SUB_" + module + " created !");
#endif
          }
        }
      }
    }
  }
}


void init_WiFi() {
  Serial.println("Connecting to  " + String(WIFI_SSID) + " ...");
  WiFi.persistent(false);
  WiFi.begin(WIFI_SSID, WIFI_PSWD);

  // wait until the device is connected to the wifi network
  while (WiFi.status() != WL_CONNECTED) {
    delay(WIFI_DELAY);
    Serial.print(".");
  }

  // Connected, show the obtained ip address
  Serial.println("WiFi Connected ==> IP Address = " + WiFi.localIP().toString());
}

void re_WiFi() {

  // wait until the device is connected to the wifi network
  while (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(WIFI_SSID, WIFI_PSWD);
    delay(5000);
    Serial.print(".");
  }
  return;

  // Connected, show the obtained ip address
  Serial.println("WiFi Connected ==> IP Address = " + WiFi.localIP().toString());
}

void init_HTTPServer() {
  server.begin();
  Serial.println("Local HTTP Server started !");
}

void task_HTTPServer() {
  // Check if a client is connected
  client = server.available();
  if (!client)
    return;

  // Wait until the client sends some data
  Serial.println("New client connected. Receiving request... ");
  while (!client.available()) {
#ifdef DEBUG_MODE
    Serial.print(".");
#endif
    delay(5);
  }

  // Read the request
  String request = client.readString();
  Serial.println(request);
  client.flush(); 



  int start, end;
  // identify the right module (sensor or actuator) that received the notification
  // the URL used is ip:port/ae
  start = request.indexOf("/");
  end = request.indexOf("HTTP") - 1;
  context = request.substring(start + 1, end);
#ifdef DEBUG
  Serial.println(String() + start + " , " + end + " -> " + context + ".");
#endif


  // ingore verification messages
  if (request.indexOf("vrq") > 0) {
    client.flush();
    return;
  }


  //Parse the request and identify the requested command from the device
  //Request should be like "[operation_name]"
  start = request.indexOf("[");
  end = request.indexOf("]"); // first occurence of
  command = request.substring(start + 1, end);
#ifdef DEBUG
  Serial.println(String() + start + " , " + end + " -> " + command + ".");
#endif

  client.flush();
}

// ######### START OF EXAMPLE ######### //
void init_luminosity() {
  String initialDescription = "Name = LuminositySensor\t"
                              "Unit = Lux\t"
                              "Location = Home\t";
  String initialData = "0";
  registerModule("LuminositySensor", false, initialDescription, initialData);
}

void task_luminosity() {
  int sensorValue;
  int sensorPin = A0;
  sensorValue = analogRead(sensorPin);
  //sensorValue = random(10, 20);
#ifdef DEBUG
  Serial.println("luminosity value = " + sensorValue);
#endif

  String ciContent = String(sensorValue);

  createCI("LuminositySensor", DATA_CNT_NAME, ciContent);
}


void init_led() {
  String initialDescription = "Name = LedActuator\t"
                              "Location = Home\t";
  String initialData = "off";
  registerModule("LedActuator", true, initialDescription, initialData);
}
void task_led() {

}

void command_led(String cmd) {
  //Serial.print(cmd);
  if (cmd == "switchOn") {
#ifdef DEBUG
    Serial.println("Switching on the LED ...");
#endif
    digitalWrite(LED_PIN, LOW);
  }
  else if (cmd == "switchOff") {
#ifdef DEBUG
    Serial.println("Switching off the LED ...");
#endif
    digitalWrite(LED_PIN, HIGH);
  }
}
// ######### END OF EXAMPLE ######### //

/////////////////////////////////////////////////////////////////////////////////

// ######################################################## //
// ######### USE THIS SPACE TO DECLARE VARIABLES  ######### //
// ########################################################  //

float temp, hum;

// ########################################################  //

void setup() {
  // intialize the serial liaison
  
  Serial.begin(SERIAL_SPEED);
  Serial.println(WiFi.macAddress());

  // Connect to WiFi network
  init_WiFi();

  // Start HTTP server
  init_HTTPServer();

  // ######### USE THIS SPACE FOR YOUR SETUP CODE ######### //

  dht.begin();
  sgp.begin();
  gas.begin(0x04);//the default I2C address of the slave is 0x04
  gas.powerOn();

  sgp_val[1]=0;
  ThingSpeak.begin(client);
  count =0;
  // ###################################################### //

}

// Main loop of the µController
void loop() {
  // ############################################################### //
  // ######### USE THIS SPACE FOR YOUR SENSOR POLING CODE  ######### //
  // ###############################################################  //
  re_WiFi();
  dht_usr();
  sgp_usr();
  sgp_think();
  delay(1);
  mgs_usr();

  // ######################################################### //




  // ################################################################### //
  // ######### USE THIS SPACE FOR YOUR SENDING DATA TO SERVER  ######### //
  // ################################################################### //

  /*
      CreateCI(AE_NAME,CONTAINER_NAME,SENSOR_VALUE)

      CreateCI is what posts your sensor data into the container.

      In the below example:
            AE_NAME: Team8_Automated_Driving (As stated in the resource tree)
            CONTAINER_NAME : node_1 ( or as stated in the resource tree)
            SENSOR_VALUE : string of comma separated all sensor values (Eg: 27,25 is temp,hum)
  */


  // Check if the data instance was created.
  delay(15000); // DO NOT CHANGE THIS VALUE
  if(count % 40 == 0){
    count=0;
      // Storing as a string in a single containers
      String sensor_value_string;
      sensor_value_string = String(dht_val[0]) + String(",") + String(dht_val[1])+\
                             String(",") + String(sgp_val[0]) + String(",") + String(sgp_val[1])\
                             + String(",") + String(sgp_val[2]) + String(",") + String(mgs_val[0])\
                             + String(",") + String(mgs_val[1]) + String(",") + String(mgs_val[2]);
      createCI("Team26_Indoor-air_pollution-3_Washrooms", "node_2", sensor_value_string);
  }
  count++;

 
  // ################################################################### //
}
