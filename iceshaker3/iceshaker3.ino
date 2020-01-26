/*
 *  Ice Shaker WIFI Version
 *  2020-01-24: kongdej.s
 *  Access point: Ice_Shaker (no password)
 *  IP: 192.168.4.1
 *  Board: TTGO T-Display Esp32 Wifi and Bluetooth Module Development Board for Arduino 1.14 Inch Lcd
 */

#include <WiFi.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <FS.h>
#include <SPIFFS.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <StringSplitter.h>

// Access Point name and password
const char* ssid     = "Ice_Shaker";
const char* password = "";

// Defined Pins ---------------------
#define LRELAY  25   // Left Rotation Realy
#define RRELAY  26   // Rigth Rotation Relay
#define MODE    39   // Select Mode 
#define DS18PIN 27   // Temperature DS18

// OLED -------------------------------------------
#define SDA_PIN 4   // GPIO21 -> SDA
#define SCL_PIN 15  // GPIO22 -> SCL
#define OLEDRST 16  // Reset
#define SSD_ADDRESS 0x3c

// build-in OLED
#ifndef TFT_DISPOFF
#define TFT_DISPOFF 0x28
#endif
#ifndef TFT_SLPIN
#define TFT_SLPIN   0x10
#endif

#define TFT_MOSI            19
#define TFT_SCLK            18
#define TFT_CS              5
#define TFT_DC              16
#define TFT_RST             23
#define TFT_BL          4  // Display backlight control pin
TFT_eSPI tft = TFT_eSPI(320,240); // Invoke custom library

// user define LCD
//#define SSD_ADDRESS 0x78
//SSD1306  display(SSD_ADDRESS, SDA_PIN, SCL_PIN);

#define FORMAT_SPIFFS_IF_FAILED true

OneWire oneWire(DS18PIN);
DallasTemperature sensors(&oneWire);

WiFiServer server(80);

String header;
// ===== Configuration ============================
int auto_manual = 1;            // 0 = auto, 1 = manual 
char r_rotate_time[] = "100";   // rotation left time in ms
char l_rotate_time[] = "100";   // rotation right time in ms
char stop_time[]   = "500";     // stop time in ms
char no_shake[]    = "3600";    // no of shake
char settemp[] = "-7";          // defaut set point 
unsigned int r_time,l_time,s_time,n_shake; // settings
float set_temp;
unsigned int relaystatus = 0;
unsigned int cmd = 0;         // command from LIFF default stop
int countdown = 0;            // count down shake
float temperature;
//unsigned long previousMillis = 0;        // will store last time LED was updated

// ==== Functions ============================================
// SPIFFS ----------------------------------------------------
int readFile(fs::FS &fs, const char * path){
    String line;
    Serial.printf("Reading file: %s\r\n", path);

    File file = fs.open(path);
    if(!file || file.isDirectory()){
        Serial.println("- failed to open file for reading");
        return 0;
    }

    while(file.available()) {
      line = file.readStringUntil('\n');
    }
    return line.toInt();
}

void writeFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Writing file: %s\r\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("- failed to open file for writing");
        return;
    }
    if(file.println(message)){
        Serial.println("- file written");
    } else {
        Serial.println("- frite failed");
    }
}

void deleteFile(fs::FS &fs, const char * path){
    Serial.printf("Deleting file: %s\r\n", path);
    if(fs.remove(path)){
        Serial.println("- file deleted");
    } else {
        Serial.println("- delete failed");
    }
}

int getSPIFF(char* file, char* val) {
  int d = readFile(SPIFFS, file); 
  if ( d == 0) {
    Serial.print("data init = ");
    Serial.println(val);
    writeFile(SPIFFS, file, val);
    return 0;
  }
  else {
    return d;
  }
}

// Relays function ---------------------------------------
void onRelay(int n, int delaytime) {
  if (n == RRELAY) {
    if (digitalRead(LRELAY) == LOW) {
      digitalWrite(RRELAY,HIGH); 
      delay(delaytime);
      digitalWrite(RRELAY,LOW); 
    }
  }
  else if (n == LRELAY) {
    if (digitalRead(RRELAY) == LOW) {
      digitalWrite(LRELAY,HIGH);   
      delay(delaytime);
      digitalWrite(LRELAY,LOW);   
    }    
  }
}

void runMotor() {
  onRelay(LRELAY,r_time);
  if (s_time < 500) s_time = 500;
  delay(s_time);
  onRelay(RRELAY,l_time);  
  delay(s_time);
}

// TFT display text  ------------------------------------
void displayData(float t)  {
  tft.setTextColor(TFT_BLACK,TFT_WHITE);
  tft.setTextSize(2);
  tft.drawString("- EGAT Maker Club -", tft.width() / 2, 65);   
  tft.setTextSize(3);
  tft.drawString("          ", tft.width() / 2, 100);
  tft.setTextColor(TFT_MAROON,TFT_WHITE);
  tft.drawString(String(t) + char(247) + "C", tft.width() / 2, 100);
  tft.setTextColor(TFT_BLACK,TFT_WHITE);
  tft.setTextSize(2); 
  if (auto_manual == 0) {
    if (countdown > 0) {
      tft.drawString("            ", tft.width() / 2, 135);
      tft.drawString("Shake:"+ String(countdown) ,tft.width() / 2, 135); 
    }
    else {
      tft.drawString("** Auto Mode **  ",  tft.width() / 2, 165);        
    }
  }
  else {
     tft.drawString("** Manaul Mode **", tft.width() / 2, 165);
  }
}

// WEB html ---------------------------------------------
const char MAIN_page[] PROGMEM = R"=====(

<html>

  <head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=0">
    <style>
    body, table {
    font-size: 14pt;
    font-family: "Montserrat", sans-serif;
    margin: 0pt;
    text-align: center;
    padding-top: 18pt;
  }
  
  input {
     margin:10px auto;
     max-width: 400px;
     padding: 10px 12px 10px 10px;
     font-size: 16pt;
     font-family: "Montserrat", sans-serif;
  }
  
  p {
      padding: 0pt 14pt;
  }
  
  .btn {
      padding: 2pt;
      font-size: 14pt;
      text-align: center;
      text-decoration: none;
      display: inline-block;
  }
  
  .btn-led-toggle {
      box-shadow: 0 2pt 4pt 0 rgba(0,0,0,0.2), 0 3pt 10pt 0 rgba(0,0,0,0.19);
      color: #f8f9fa; /* Light */
      background-color: #ff0000; /* Gray */
      width: 160pt;
      font-weight: bold;
      height: 40pt;
      border: 2pt solid black;
      border-radius: 8pt;
  }
  
  .btn-led-toggle:active {
      transform: translateY(4pt);
      box-shadow: 0 2pt 4pt 0 rgba(0,0,0,0.2), 0 1pt 5pt 0 rgba(0,0,0,0.19);
      background-color: #6c0000; /* Gray */
      margin-bottom: 8pt;
  }
  
  .led-on, .led-on:active {
      background-color: #16C464; /* LINE Green */
  }
  
  .btn-state {
      box-shadow: 0pt 6pt #999;
      border-radius: 4pt;
      margin-bottom: 8pt;
      width: 50pt;
      border: 1pt solid #6c757d; /* Gray */
      color: #6c757d; /* Gray */
      background-color: #f8f9fa; /* Light */
  }
  
  .btn-state:active, .pressed {
      transform: translateY(4pt);
      box-shadow: 0 2pt #666;
      color: #343a40; /* Dark */
      border: 1pt solid #343a40; /* Dark */
      background-color: #08b961;
  }
  
  .inactive {
      color: #6c757d; /* Gray */
  }
  
  .success {
      color: #16C464; /* LINE Green */
  }
  
  .error {
      color: #dc3545; /* Red */
  }
  
  .hidden {
      display: none;
  }
  
  .device-info-table {
      width: 80%;
      margin: auto;
      padding: 0;
  }
  
  .device-info-cell {
      text-align: left;
      vertical-align: middle;
      padding-bottom: 8px;
  }
  
  .device-info-key {
      padding-right: 10px;
      word-wrap: none;
  }
  
  .device-info-val {
      padding-left: 10px;
      word-wrap: break-word;
      max-width: 200px;
      font-size: 16pt;
      font-family: monospace, monospace;
  }
    </style>
  </head>

  <body>
    <!-- Title -->
    <h2>ICE SHAKER</h2>
    <hr />
    
    <div id="controls">
      <!-- Device Info Table -->
      <table class="device-info-table">

        <tr>
          <th class="device-info-cell device-info-key">Temperature</th>
          <td class="device-info-cell device-info-val" id="temperature">-</td>
        </tr>
        <tr>
          <th class="device-info-cell device-info-key">Countdown</th>
          <td class="device-info-cell device-info-val" id="countdown">-</td>
        </tr>
        <tr>
          <th class="device-info-cell device-info-key">Mode</th>
          <td class="device-info-cell device-info-val" id="mode">-</td>
        </tr>
        <tr>
          <th class="device-info-cell device-info-key">Set Point(C)</th>
          <td class="device-info-cell device-info-val"><input type="text" value="-8" id="setpoint"></td>
        </tr>
        <tr>
          <th class="device-info-cell device-info-key">Left(ms)</th>
          <td class="device-info-cell device-info-val"><input type="text" value="50" id="lefttime"></td>
        </tr>
        <tr>
          <th class="device-info-cell device-info-key">Right(ms)</th>
          <td class="device-info-cell device-info-val"><input type="text" value="50" id="righttime"></td>
        </tr>
         <tr>
          <th class="device-info-cell device-info-key">Stop(ms)</th>
          <td class="device-info-cell device-info-val"><input type="text" value="500" id="stoptime"></td>
        </tr>
        <tr>
          <th class="device-info-cell device-info-key">Shake(n)</th>
          <td class="device-info-cell device-info-val"><input type="text" value="10000" id="nshake"></td>
        </tr>


      </table>
      <hr />
      <!-- Toggle LED -->
      <p>
        <button id="btn-led-toggle" class="btn btn-led-toggle" onclick="handlerToggleLed()">START</button>
      </p>
    
      </div>

    <script> 
    window.onload = function() {
        // client.println(String(set_temp) + "," + String(l_time) + "," + String(r_time) + "," +String(s_time) + "," +String(n_shake) );  
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
          if (this.readyState == 4 && this.status == 200) {
              var vals = this.responseText.split(',');
              document.getElementById("setpoint").value = vals[0];
              document.getElementById("lefttime").value = vals[1];
              document.getElementById("righttime").value = vals[2];
              document.getElementById("stoptime").value = vals[3];
              document.getElementById("nshake").value = vals[4];
          }
        };
        
        xhttp.open("GET", "initData", true);
        xhttp.send();
    }
    
    function getData() {
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
          if (this.readyState == 4 && this.status == 200) {
              var vals = this.responseText.split(',');
              document.getElementById("temperature").innerHTML = vals[0];
              document.getElementById("countdown").innerHTML = vals[1];
              document.getElementById("mode").innerHTML = vals[2];
          }
        };
        
        xhttp.open("GET", "getData", true);
        xhttp.send();
    }
    setInterval(function() {
        getData();
    }, 2000);

    var ledState;
    function handlerToggleLed() {
        ledState = !ledState;

        uiToggleLedButton(ledState);
    }

    function uiToggleLedButton(state) {
        const el = document.getElementById("btn-led-toggle");
        el.innerText = state ? "STOP" : "START";

        if (state) {
            el.classList.add("led-on");
        } else {
            el.classList.remove("led-on");
        }

        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
          if (this.readyState == 4 && this.status == 200) {
              document.getElementById("temperature").innerHTML = this.responseText;
          }
        };

        var data  = document.getElementById("setpoint").value+","+document.getElementById("lefttime").value+","+document.getElementById("righttime").value
            data += ","+document.getElementById("stoptime").value+","+document.getElementById("nshake").value
            data += ","+state
       
        xhttp.open("GET", "setData,"+data+",", true);
        xhttp.send();
    }

    </script>
    </body>
</html>


)=====";

// END Function ===================================================

//==== SETUP ======================================================
void setup() {
  Serial.begin(115200); 
  // set pin mode
  pinMode(OLEDRST,OUTPUT);
  pinMode(LRELAY,OUTPUT);
  pinMode(RRELAY,OUTPUT);
  pinMode(MODE,INPUT);
 
  // set all relays to low
  digitalWrite(LRELAY,LOW);   
  digitalWrite(RRELAY,LOW);
     
  Serial.println("-- ICE Shaker v2.0 ---");

  // Wifi Access Point Mode
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  
  server.begin();
  
  // -- inti new TTGO --
  //bool formatted = SPIFFS.format();
  // -- delete all files --
  //deleteFile(SPIFFS, "/rotate.txt");
  //deleteFile(SPIFFS, "/stop.txt");
  //deleteFile(SPIFFS, "/run.txt");

  if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)){
        Serial.println("SPIFFS Mount Failed");
        return;
  }
  
  // Read init settings from spiffs
  l_time = getSPIFF("/left_rotate.txt", l_rotate_time); 
  r_time = getSPIFF("/right_rotate.txt", r_rotate_time); 
  s_time = getSPIFF("/stop.txt", stop_time); 
  n_shake = getSPIFF("/shake.txt", no_shake);
  set_temp = getSPIFF("/settemp.txt", settemp);
  Serial.println("- Initial value -");
  Serial.print("left rotation = "); Serial.println(l_time);
  Serial.print("right rotation = "); Serial.println(r_time);
  Serial.print("stop time = ");Serial.println(s_time);
  Serial.print("shake = ");Serial.println(n_shake);
  Serial.print("set temp. = ");Serial.println(set_temp);
  
  // -- TFT Init
  tft.init();
  //tft.setSwapBytes(true);
  tft.fillScreen(TFT_WHITE);
  tft.setTextColor(TFT_BLACK,TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  if (TFT_BL > 0) {                           // TFT_BL has been set in the TFT_eSPI library in the User Setup file TTGO_T_Display.h
       pinMode(TFT_BL, OUTPUT);               // Set backlight pin to output mode
      digitalWrite(TFT_BL, TFT_BACKLIGHT_ON); // Turn backlight on. TFT_BACKLIGHT_ON has been set in the TFT_eSPI library in the User Setup file TTGO_T_Display.h
  }

  sensors.begin();  // temperature sensor
}


//====== LOOP ========================================================
void loop(){
  auto_manual = digitalRead(MODE); // select mode
  sensors.requestTemperatures();  // read temperature
  temperature = sensors.getTempCByIndex(0);
  displayData(temperature);
  
  if (auto_manual == 1) { // Manual Mode
    runMotor(); 
  }
  else {                  // Auto Mode
    if (cmd == 1) {       // if Start command
      if (countdown == 0 ) {
        countdown =  n_shake;
      }
      else {             // stop
        if (temperature > set_temp) {
          runMotor();  
          countdown--;
          if (countdown == 0) { // stop when countdown to zero
            cmd = 0;
          }
        }
      }
    }
    else {
      cmd = 0;
      countdown = 0;
      digitalWrite(LRELAY,LOW);
      digitalWrite(RRELAY,LOW);
    }
  }
  
  // Send status 
  relaystatus =  (digitalRead(LRELAY) == HIGH || digitalRead(RRELAY) == HIGH) ? 1:0;
  //char bufs[100];
  //sprintf(bufs,"%0.2f,%d,%d,%d,%d,%d,%d,%d,%0.2f", temperature, countdown, relaystatus, auto_manual, r_time, l_time, s_time, n_shake,set_temp);  
  //Serial.printf("%0.2f,%d,%d,%d,%d,%d,%d,%d,%0.2f\r\n", temperature, countdown, relaystatus, auto_manual, r_time, l_time, s_time, n_shake,set_temp);  

  // Listen for incoming clients  
  WiFiClient client = server.available();  
  if (client) {                             // If a new client connects,
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;                        // collect header until '\n'
        if (c == '\n') {                    // if the byte is a newline character
          if (currentLine.length() == 0) {  // if \n\n end connection
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            
            // send current data to clients
            if (header.indexOf("GET /getData") >= 0) {
              String modes = auto_manual == 1 ? "Manual":"Auto";
              client.println(String(temperature)+","+String(countdown)+","+ modes);
            }
            
            // send initial data to clients
            else if (header.indexOf("GET /initData") >= 0) {
              client.println(String(set_temp) + "," + String(l_time) + "," + String(r_time) + "," +String(s_time) + "," +String(n_shake) );             
            }
            
            // get data from client, change new setting and save to spiffs
            else if (header.indexOf("GET /setData") >= 0) {
              StringSplitter *splitter = new StringSplitter(header, ',', 6); 
              String setpoint = splitter->getItemAtIndex(1);
              String lefttime = splitter->getItemAtIndex(2);
              String righttime = splitter->getItemAtIndex(3);
              String leftstr = splitter->getItemAtIndex(4);
              StringSplitter *splitter2 = new StringSplitter(leftstr, ',', 6); 
              String stoptime = splitter2->getItemAtIndex(0);
              String countdowns = splitter2->getItemAtIndex(1);
              String command = splitter2->getItemAtIndex(2);

              l_time = lefttime.toInt();
              r_time = righttime.toInt();
              s_time = stoptime.toInt();
              n_shake = countdowns.toInt();
              set_temp = setpoint.toInt();
              cmd = (command == "true") ? 1:0;
              
              // write to spiffs
              char buf[100];
              // left rotate
              lefttime.toCharArray(buf,lefttime.length() + 1);
              writeFile(SPIFFS, "/left_rotate.txt", buf);
              Serial.print("left = ");Serial.println(buf);
              
              // right rotate
              righttime.toCharArray(buf,righttime.length() + 1);
              writeFile(SPIFFS, "/right_rotate.txt", buf);
              Serial.print("right = ");Serial.println(buf);
          
              // stop time
              stoptime.toCharArray(buf,stoptime.length() + 1);
              writeFile(SPIFFS, "/stop.txt", buf);
              Serial.print("stop = ");Serial.println(buf);
          
              // no shake
              countdowns.toCharArray(buf,countdowns.length() + 1);
              writeFile(SPIFFS, "/shake.txt", buf);
              Serial.print("shake = ");Serial.println(buf);
          
              // set temperature
              setpoint.toCharArray(buf,setpoint.length() + 1);
              writeFile(SPIFFS, "/settemp.txt", buf);
              Serial.print("settemp = ");Serial.println(buf);
              
              client.println("Acknowlege...");
            }

            // show client Webpage
            else {
              String html = MAIN_page;
              client.println(html);
            }

            client.println();
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
          
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }

    
    header = "";   // Clear the header variable
    client.stop(); // Close the connection
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}
