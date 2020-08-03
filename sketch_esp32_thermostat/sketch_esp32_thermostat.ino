


/*
   Garage theromstat
   Sketch to read temperatures with an ESP32 board and a DHTxx sensor connected to pin 26, and to control a Relay connected to Pin 27
*/

#include <Time.h>
#include <TimeLib.h>
#include <WiFi.h>

#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include <DHTesp.h>
#include "esp_system.h"
#include "soc/rtc.h"
#include "rom/uart.h"


/****** Start configuration section ******/

const char* ssid     = "XXXXXXX"; //Replace with your network SSID
const char* password = "XXXXXXXX"; //Replace with your network password
const String nodeName = "Cave"; //Replace with the name for this Thermostat

/****** End configuration section ******/

DHTesp::DHT_MODEL_t DHT_TYPE = DHTesp::DHT22;





const String sketchVersion = "3";
const String nodeType = "TH&Relay";
unsigned long lastReceivedRelayCommandMilis;
const unsigned int TIMEOUT_REICEIVED_COMMANDS_MILIS = 1000 * 60 * 2; //Two minutes
String relayError = "";
String thError = "";
WiFiServer server(80);
WiFiClient client;
String auto_ip = ""; // place to store IP address
const int RELAY_PIN = 27;
const int ONBOARD_LED_PIN = 2;
const int DHT_PIN = 26;
DHTesp dht;
int Tset = 72.0; //Use a number higher than 40 to avoid being on edge of screen fix for #2
double Td = 0.5;

int hys=0;

int control =0;
time_t currentTime = 0,startTime=0;

      double humidity = dht.getHumidity();
      double celsiusTemp = dht.getTemperature();
      double fahrenheitTemp = -10000;
      int currentStatus = digitalRead(RELAY_PIN);

hw_timer_t *timer = NULL;

void IRAM_ATTR resetModule(){
    //ets_printf("reboot\n"); //comment out to avoid issues fix #1
    //esp_restart_noos();
    esp_restart();
}


void setup() {
  
  Serial.begin(115200);
  slowDownCpu();
  pinMode(ONBOARD_LED_PIN, OUTPUT);

  pinMode(RELAY_PIN, OUTPUT);
  
  digitalWrite(RELAY_PIN, HIGH); //shut off relay
  dht.setup(DHT_PIN, DHT_TYPE);

  setUpWifi();

  lastReceivedRelayCommandMilis = millis();


    // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  // ArduinoOTA.setHostname("myesp32");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();
  

}

void slowDownCpu() {
  rtc_clk_cpu_freq_set(RTC_CPU_FREQ_160M);
  uart_tx_wait_idle(0);
  int clockspeed = rtc_clk_cpu_freq_get();

  char* clockSpeeds[5] = {"XTAL", "80Mhz", "160Mhz", "240Mhz", "2Mhz"};
  Serial.print("Setting CPU freq to: ");
  Serial.println(clockSpeeds[clockspeed]);
  Serial.println("");
}

void setUpWifi() {
  //WiFi.config(ip,dns,gateway,subnet);


  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);


  // attempt to connect to Wifi network:
  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    int counter = 0;
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.print(".");
      counter++;
      if (counter > 10) {
        break;
      }
    }
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());
  auto_ip = WiFi.localIP().toString(); // store local IP address
  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("Signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
  

  server.begin();

  Serial.println(" Server started");

//Create a timer for watchdog timer  
timer = timerBegin(0, 80, true); //timer 0, div 80
    timerAttachInterrupt(timer, &resetModule, true);
    timerAlarmWrite(timer, 15000000, false); //set time in us to 15 seconds fixes #1
    timerAlarmEnable(timer); //enable interrupt

}



double celsius2Fahrenheit(double celsius)
{
  return celsius * 9 / 5 + 32;
}



void loop() {

timerWrite(timer, 0); //reset timer (feed watchdog)

ArduinoOTA.handle();

  if (WiFi.status() != WL_CONNECTED) {
    setUpWifi();
  } else {
    // listen for incoming clients
    client = server.available();
    if (client.connected()) {
      digitalWrite(ONBOARD_LED_PIN, HIGH);
      client.setTimeout(5);  //set timeout to 5 second - must be done after connecting fix #1
      String req = client.readStringUntil('\r');  //read first line
      Serial.println("Received request: " + req);
      
      String response = String();
      if (req.startsWith("GET /ajax_inputs")){
      
      // send a standard http response header for json
      
      json_HTTP_header(response);
      response += "{\n";
      response += "\"Type\":\"" + nodeType + "\",\n";
      response += "\"Name\":\"" + nodeName + "\",\n";
   
      String ff = String(fahrenheitTemp);
      ff.trim();
      response += "\"Temp\":\"" + ff + "\",\n";

      String hh = String(humidity);
      hh.trim();
      response += "\"RelH\":\"" + hh + "\",\n";

      String jj = String(Tset);
      jj.trim();
      
      response += "\"Tset\":\"" + jj + "\",\n";
      
      response += "\"Heater\": \"" + String(currentStatus) + "\",\n";
      response += "\"Control\": \"" + String(control) + "\",\n"; 
      response += "\"StartTime\": \"";
      //time_and_date_string(response, startTime);
      unix_time_string(response,startTime);
      response += "\",\n";
      response += "\"CurrentTime\": \"";
      //time_and_date_string(response, now());
      unix_time_string(response,now());
      
      response += "\",\n";
      
      //response += "\"Mac\":\"" + WiFi.macAddress() + "\",\n";
            
      long rssi = WiFi.RSSI();
      response += "\"Wifi_ssi\": \"" + String(rssi) + "\"\n";
      response += "}\n";
      Serial.println("GET: Resp sent:" + response);
      client.println(response);
          
      }else if (req.startsWith("OPTIONS")) {
       response += "HTTP/1.1 200 OK\n";
       response += "Access-Control-Allow-Origin: *\n";

        response += "Access-Control-Allow-Methods: POST, GET, OPTIONS, DELETE\n";
        response += "Access-Control-Allow-Headers: access-control-allow-origin,Content-Type\n";
        response += "Access-Control-Max-Age: 100\n";
        response += "\n"; //This carry return is very important. Without it the response will not be sent.
        Serial.println("OPTIONS: Resp sent:" + response);   
        client.println(response);    
        //String msg = client.readString();
        //Serial.println(msg);

      }else if (req.startsWith("POST")) {
        
      
      
      Serial.print("POST: Remaining message:");
      int ct=0;
      while (ct<4 && client.available()){  //check for \r\n\r\n designating a blank line
        char c=client.read();
        Serial.print(c);
        if(c=='\r' || c=='\n')
          ct=ct+1;
        else
          ct=0;
      }

      //Crudely parse the JSON.  {Tset: 70, Control: 1}
      //parsing ignores the non numerics and stops after a non numeric character
      Serial.println("parsing 1...");
      Tset=client.parseInt();
      Serial.println("parsing 2...");
      control=client.parseInt();
      Serial.println("parsing complete");


      
      
     
      
      response += "HTTP/1.1 200 OK\n";
      response += "Access-Control-Allow-Origin: *\n";
      response += "\n"; //This carry return is very important. Without it the response will not be sent.
      Serial.println("POST: Resp sent:" + response);
      client.println(response);
        
      
      } else //standard HTML Page
      {
        
      response += "HTTP/1.1 200 OK\n";
      response += "Content-Type: text/html\n";
      response += "Access-Control-Allow-Origin: *\n";
      response += "\n"; //This carry return is very important. Without it the response will not be sent.

     //Below is the web page HTML that is served by the ESP32 server.
     //the index.html is converted to a string via http://tomeko.net/online_tools/cpp_text_escape.php?lang=en
      response += "<!doctype html>\n<html lang = \"en\">\n  <head>\n\t\t<meta http-equiv=\"content-type\" content=\"text/html\" charset=\"utf-8\">\n\t\t<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, shrink-to-fit=no\">\n    \n\n    \n\t\t<link rel=\"stylesheet\" href=\"https://stackpath.bootstrapcdn.com/bootstrap/4.3.1/css/bootstrap.min.css\" integrity=\"sha384-ggOyR0iXCbMQv3Xipma34MD+dH/1fQ784/j6cY/iJTQUOhcWr7x9JvoRxT2MZw1T\" crossorigin=\"anonymous\">\n\n\t\t<link rel=\"stylesheet\" href=\"https://use.fontawesome.com/releases/v5.7.2/css/all.css\" integrity=\"sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr\" crossorigin=\"anonymous\">  \n    \n\t\t<script src=\"https://code.jquery.com/jquery-3.3.1.slim.min.js\" integrity=\"sha384-q8i/X+965DzO0rT7abK41JStQIAqVgRVzpbzo5smXKp4YfRvH+8abtTE1Pi6jizo\" crossorigin=\"anonymous\"></script>\n\t\t<script src=\"https://cdnjs.cloudflare.com/ajax/libs/popper.js/1.14.7/umd/popper.min.js\" integrity=\"sha384-UO2eT0CpHqdSJQ6hJty5KVphtPhzWj9WO1clHTMGa3JDZwrnQq4sF86dIHNDz0W1\" crossorigin=\"anonymous\"></script>\n\t\t<script src=\"https://stackpath.bootstrapcdn.com/bootstrap/4.3.1/js/bootstrap.min.js\" integrity=\"sha384-JjSmVgyd0p3pXB1rRibZUAYoIIy6OrQ6VrjIEaFf/nJGzIxFDsf4x0xIM+B07jRM\" crossorigin=\"anonymous\"></script>\n\t\t<script src=\"https://cdn.jsdelivr.net/npm/knockout@3.3.0/build/output/knockout-latest.debug.min.js\"></script>\n\n\t\t<style>\t\n\t\t\t/*Make slider thumb black in all browsers*/\n\t\t\tinput[type=range]::-webkit-slider-thumb {background: #000000;}\n\t\t\tinput[type=range]::-moz-range-thumb {background: #000000;}\n\t\t\tinput[type=range]::-ms-thumb {background: #000000;}\n\t\n\t\t</style>\n  </head>\n  \n  \n  <body>\n  \n\t<nav class=\"navbar navbar-l bg-white\">\t\t\t\n\t\t\t<span class=\"navbar-text\">\n\t\t\t\t<h4 class=\"text-black\"><bdi data-bind=\"text:Name\"></bdi> Thermostat</h4>\n\t\t\t</span>\n\t\t\n\t\t <div class=\"custom-control custom-switch\">\n\t\t\t<input type=\"checkbox\" data-bind=\"checked: ControlUI, click: save\" class=\"custom-control-input\" checkedValue=\"1\" id=\"switch1\" name=\"example\">\n\t\t\t<label class=\"custom-control-label\" for=\"switch1\">Control</label>\n\t\t</div>\n\t</nav>\n\t\t\n\t<div data-bind=\"visible: refresh()\"> </div>\t\n\t\n\t<div class=\"container-fluid\">\n\t\t<div class=\"row\">\n\t\t\t<div class=\"col m-2 bg-primary text-white\">\n\t\t\t\t<h5 class=\"text-left\" > Temperature </h5>\n\t\t\t\t<h4 class=\"text-right\"><bdi data-bind=\"text: Math.round(Temp()*10)/10\"></bdi></h4>\n\t\t\t</div>\n  \n\t\t\t<div class=\"col m-2 bg-primary text-white\">\n\t\t\t\t<h5 class=\"text-left\">Humidity</h5>\n\t\t\t\t<h4 class=\"text-right\"><bdi data-bind=\"text: Math.round(RelH())\"></bdi> % </h4>\n\t\t\t</div>\n\t\t</div>\n\t\n\t\t<div class=\"row\" data-bind=\"visible: ControlUI\">\n\t\t\t<div class=\"col  m-2 bg-primary text-white\">\n\t\t\t\t<h4> Set point </h4>\t\t\t\t\n\t\t\t\t<span style=\"display:inline-block\">40</span> \n\t\t\t\t<span style=\"float: right;\">100</span>\n\t\t\t\t\n\t\t\t\t<input type=\"range\" class=\"custom-range\" data-bind=\" value: Tset, valueUpdate: 'input'\" min=\"40\" max=\"100\" >\n\t\t\t\t\n\t\t\t\t \n\t\t\t\t<span style=\"float: right;\">\n\t\t\t\t\t<h4 class=\"text-right\"><bdi data-bind=\"text: Math.round(Tset()*10)/10\"></bdi>\n\t\t\t\t\t<i class=\"fas fa-burn\" data-bind=\"visible: Heater()&gt;0 ? 1 : 0 \" ></i> </h4>\n\t\t\t\t</span>\t\t\t\t\t\t\t\t\t\n\t\t\t</div>\n\t\t</div>\n\t\t\t\n\t\t<button type=\"button\" class=\"btn btn-primary btn-sm\" data-bind=\"click: refresh\">   <i class=\"fas fa-redo-alt\"></i> </button> \n\t\n\t</div>  \n\t  \n    <footer class=\"container-fluid\">\n\t\t<h6 class=\"text-right text-muted small\">Start Time: <bdi data-bind=\"text: StartDate().toUTCString()\"></bdi></h6>\n\t\t<h6 class=\"text-right text-muted small\">Current Time: <bdi data-bind=\"text: CurrentDate().toUTCString()\"></bdi></h6>\n\t\t<h6 class=\"text-right text-muted small\">WiFi dbm= <bdi data-bind=\"text: Wifi_ssi\"></bdi></h6>\n    </footer>\n\t\n    <script>\n     \n\n\t \n    function status(response) {\n\t\tif (response.status >= 200 && response.status < 300) {\n\t\treturn Promise.resolve(response)\n\t\t} else {\n\t\treturn Promise.reject(new Error(response.statusText))\n\t\t}\n\t};\n\n   \n    // This is a simple *viewmodel*\n\tfunction AppViewModel() {\n    //data\n\tvar self = this;\n  \tself.Name = ko.observable(\"\");\n    self.Temp = ko.observable(\"\");\n    self.RelH = ko.observable(\"\");\n    self.Tset = ko.observable(\"\");\n  \tself.Heater = ko.observable(\"\");   \n    self.CurrentTime = ko.observable(\"\");\n    self.StartTime = ko.observable(\"\");\n\tself.ControlUI = ko.observable(\"\");\n\tself.Wifi_ssi = ko.observable(\"\");\n\t\n\tself.TsetDelayed = ko.pureComputed(self.Tset)\n        .extend({ rateLimit: { method: \"notifyWhenChangesStop\", timeout: 400 } });\n\t\n\tself.RefreshDelayed =ko.pureComputed(self.TsetDelayed).extend({ rateLimit: { method: \"notifyWhenChangesStop\", timeout: 400 } });\n\t\t\n\tself.Control = ko.computed({\n\tread: function(){return this.ControlUI() ? 1 : 0},\n\twrite: function(value){\n\tvar r=(value>0);\n\tthis.ControlUI(r);\n\t\n\t},\n\towner: this\n\t});\n\t\n\t\n\t\n\t\t\n\tself.StartDate = ko.computed(function() {\n\t\tvar event = new Date(self.StartTime()*1000);   \n\t\treturn event;\n    });\n  \n\tself.CurrentDate = ko.computed(function() {\n\t\tvar event = new Date(self.CurrentTime()*1000);   \n\t\treturn event;\n    });\n \n  \n  \n\tself.refresh =function(){\n\t\tfetch(\"http://192.168.0.222/ajax_inputs\").then (status)\n\t\t.then(function(response) {\n\t\treturn response.json();\n\t\t}).then(function(myJson) {\n\t\tconsole.log('Get request succeeded with resp ',myJson);\n\t\tself.Name(myJson.Name); \n\t\tself.Temp(myJson.Temp);\n\t\tself.RelH(myJson.RelH);\n\t\tself.Tset(myJson.Tset);\n\t\tself.Heater(myJson.Heater);\n\t\tself.Control(myJson.Control);\n\t\tself.CurrentTime(myJson.CurrentTime);\n\t\tself.StartTime(myJson.StartTime);\n\t\tself.Wifi_ssi(myJson.Wifi_ssi);\n\t\t}).catch(function(error){\n\t\tconsole.log('Request Failed',error);\n\t\t})\n\t};\n\n\tself.save = function(){\n\t\tvar plainJs = ko.toJS(self); \n\t\tconsole.log('Testing, Testing',plainJs.Tset);  \n\t\tfetch('http://192.168.0.222', {\n\t\tmethod: 'POST',\n\t\theaders: {\n\t\t'Accept': 'application/json',\n\t\t'Access-Control-Allow-Origin': '*',\n\t\t'Content-Type': 'application/json'\n\t\t},\n\t\tcredentials: 'same-origin',\n\t\tbody: JSON.stringify({\"Tset\":plainJs.TsetDelayed, \"Control\":plainJs.Control})\n\t\t}).then (status).catch(function(error){\n\t\tconsole.log('Request Failed',error);  \n\t\t});\n\treturn true};  \n  \n    \n\t\n  \n\tself.TsetDelayed.subscribe(function(newValue) {self.save()});\n\tself.RefreshDelayed.subscribe(function(newValue) {self.refresh()});\n  \n};\n    \n// Activates knockout.js\nko.applyBindings(new AppViewModel());    \n   \n</script>\n</body>\n</html>\n";
      response += "\n";  
         
      //The next line replaces all occurences of my hardcoded IP address in the index.html string with your router assigned IP.
      //If you hardcode your own  fixed IP address, in the string, you don't need this line, but leaving it shouldn't hurt   
      response.replace("192.168.0.222", auto_ip); 
         
      Serial.println("HTML Page: Resp sent:" + response);
      client.println(response);
      }

      humidity = dht.getHumidity();
      celsiusTemp = dht.getTemperature();
      fahrenheitTemp = -10000;
     currentStatus = 1-digitalRead(RELAY_PIN);


      if (dht.getStatus() != 0) {
        thError = String(dht.getStatusString());
        Serial.println("DHT Error status: " + thError);
        humidity = -10000;
        celsiusTemp = -10000;
      } else {
        thError = "";
        fahrenheitTemp = celsius2Fahrenheit(celsiusTemp);
        Serial.println("DHT OK");
      }

     
      
      
      delay(100);
      client.stop();
      digitalWrite(ONBOARD_LED_PIN, LOW);
      String tad = String();
      time_and_date_string(tad, now());
      Serial.println("Time: " + tad);
      Serial.println("Client_Stop _________________________________________");
    }

    
 
      
  }



 if(startTime<24*60*60) //Check if start time is less than 24 hrs, meaning no internet time fix #1
      {
        setTime(webUnixTime());
        setSyncProvider(webUnixTime);
       // adjustTime(-6*SECS_PER_HOUR);  //set to central time
        startTime=now();
        String tad = String();
        time_and_date_string(tad, startTime);
        Serial.println("Server time sync: " + tad);
      }

      humidity = dht.getHumidity();
      celsiusTemp = dht.getTemperature();
      fahrenheitTemp = 10000;
     


      if (dht.getStatus() != 0) {
        thError = String(dht.getStatusString());
        Serial.println("DHT Error status: " + thError);
        humidity = -10000;
        celsiusTemp = -10000;
      } else {
        thError = "";
        fahrenheitTemp = celsius2Fahrenheit(celsiusTemp);
        //Serial.println("DHT OK");
      }
     
        if (control==1)
      {
        if( fahrenheitTemp<(float(Tset)-Td))  //add some hysteresis
          {
            digitalWrite(RELAY_PIN, LOW); 
            currentStatus=1;
            
          }
        if ( fahrenheitTemp>(float(Tset)+Td))
        {
            digitalWrite(RELAY_PIN, HIGH); 
            currentStatus=0;          
        }
      }else
        {
            digitalWrite(RELAY_PIN, HIGH);
        }
}
