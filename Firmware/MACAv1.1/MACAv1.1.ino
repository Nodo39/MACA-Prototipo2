#include <WiFi.h>
#include <SPI.h>
#include <SD.h>

/* SD */
const int SDchipSelect = 4;
//#define DataloggerSD  //Enable data storage in SD card

/* WiFi */
char ssid[] = "nodo 39"; //  your network SSID (name)
char pass[] = "canelita32";    // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;            // your network key Index number (needed only for WEP)

int status = WL_IDLE_STATUS;
char server[] = "www.dweet.io";
//IPAddress server(192,168,0,1);

WiFiClient client;

// Dweet parameters
#define thing_name  "MACA"

/* RTC DS3231
 
 CONNECTIONS:
 DS3231 SDA --> SDA
 DS3231 SCL --> SCL
 DS3231 VCC --> 3.3v or 5v
 DS3231 GND --> GND
 */
#if defined(ESP8266)
#include <pgmspace.h>d
#else
#include <avr/pgmspace.h>
#endif

/* for software wire use below
#include <SoftwareWire.h>  // must be included here so that Arduino library object file references work
#include <RtcDS3231.h>

SoftwareWire myWire(SDA, SCL);
RtcDS3231<SoftwareWire> Rtc(myWire);
 for software wire use above */

/* for normal hardware wire use below */
#include <Wire.h> // must be included here so that Arduino library object file references work
#include <RtcDS3231.h>
RtcDS3231<TwoWire> Rtc(Wire);
/* for normal hardware wire use above */


/* DHT Sensor
 Connect pin 1 (on the left) of the sensor to +5V
 Connect pin 2 of the sensor to whatever your DHTPIN is
 Connect pin 4 (on the right) of the sensor to GROUND
 Connect a 10K resistor from pin 2 (data) to pin 1 (power) of the sensor
*/
#include <DHT.h>

#define DHTPIN 12

//#define DHTTYPE DHT11   // DHT 11
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

DHT dht(DHTPIN, DHTTYPE);

float DHT_humidity;
float DHT_temperature;

/* Interface to Shinyei Model PPD42NS Particle Sensor
  JST Pin 1 (Black Wire) => Arduino GND
  JST Pin 3 (Red wire) => Arduino 5VDC
  JST Pin 4 (Yellow wire) => Arduino Digital Pin 8
*/

int Shinyei_P1 = 5;
int Shinyei_P2 = 3;
unsigned long duration;
unsigned long starttime;
unsigned long sampletime_ms = 15000;//sampe 30s ;
unsigned long lowpulseoccupancy = 0;
float ratio = 0;
float concentration = 0;

/*  MICS  */

int MiCS_2710 = A0;   //NO2 Sensor
int MICS_2611 = A1;   //O3  Sensor
int MICS_5521 = A2;   //CO  Sensor

int NO2_value = 0;
int O3_value  = 0;
int CO_value  = 0;

int NO2_out, O3_out, CO_out;

void setup() {
  Serial.begin(9600);

  #ifdef DataloggerSD 
  Serial.print("Initializing SD card...");

  // see if the card is present and can be initialized:
  if (!SD.begin(SDchipSelect)) {
    Serial.println("Card failed, or not present");
    // don't do anything more:
    return;
  }
  Serial.println("card initialized.");
  #endif

  WiFiSetup();
  
  RTCSetup();
  
  dht.begin();
  
  pinMode(Shinyei_P1,INPUT);
  pinMode(Shinyei_P2,INPUT);
  starttime = millis();//get the current time;
}

void loop() {
  
  String dataString = "";
  
  // Wait a few seconds between measurements.
  delay(2000);

  //-------------RTC-------------------
  if (!Rtc.IsDateTimeValid()) 
    {
        // Common Cuases:
        //    1) the battery on the device is low or even missing and the power line was disconnected
        Serial.println("RTC lost confidence in the DateTime!");
    }

    RtcDateTime now = Rtc.GetDateTime();
    printDateTime(now);
    Serial.println();

    RtcTemperature temp = Rtc.GetTemperature();
//    Serial.print("RTC Temp: ");
//    Serial.print(temp.AsFloat());
//    Serial.println("C");
  
  //-------------DHT SENSOR--------------
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  DHT_humidity = dht.readHumidity();
  // Read temperature as Celsius (the default)
  DHT_temperature = dht.readTemperature();

  // Check if any reads failed and exit early (to try again).
  if (isnan(DHT_humidity) || isnan(DHT_temperature)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }
  
  dataString += "DHT - ";
  dataString += "Humidity: ";
  dataString += String(DHT_humidity);
  dataString += " %";
  dataString += " , ";
  dataString += "Temperature: ";
  dataString += String(DHT_temperature);
  dataString += " C";
  dataString += "\n";
  
//  Serial.print("Humidity: ");
//  Serial.print(DHT_humidity);
//  Serial.print(" %\t");
//  Serial.print("Temperature: ");
//  Serial.print(DHT_temperature);
//  Serial.println(" C ");

  //------------SHINYEI--------------
  duration = pulseIn(Shinyei_P1, LOW);
  lowpulseoccupancy = lowpulseoccupancy+duration;
  if ((millis() -starttime) > sampletime_ms)//if the sampel time == 30s
  {
    ratio = lowpulseoccupancy/(sampletime_ms*10.0); // Integer percentage 0=>100
    concentration = 1.1*pow(ratio,3) -3.8*pow(ratio,2)+520*ratio+0.62; // using spec sheet curve
    dataString += "Shinyei: concentration = ";
    dataString += String(concentration);
    dataString += " pcs/0.01cf";
    dataString += "\n";
//    Serial.print("concentration = ");
//    Serial.print(concentration);
//    Serial.println(" pcs/0.01cf");
//    Serial.println("\n");
    lowpulseoccupancy = 0;
    starttime = millis();
  }
  
  //------------MiCS------------

  NO2_value = analogRead(MiCS_2710);
  O3_value  = analogRead(MICS_2611);
  CO_value  = analogRead(MICS_5521);

  NO2_out = map(NO2_value, 0, 1023, 0, 1023);
  O3_out  = map(O3_value, 0, 1023, 0, 1023);
  CO_out  = map(CO_value, 0, 1023, 0, 1023);

  dataString += "NO2: ";
  dataString += String(NO2_out);
  dataString += " []";
  dataString += "\n";
  dataString += "O3: ";
  dataString += String(O3_out);
  dataString += " []";
  dataString += "\n";
  dataString += "CO: ";
  dataString += String(CO_out);
  dataString += " []";
  dataString += "\n";
  
  //------------WiFi------------

  SendDataToServer ();
  
  // if there are incoming bytes available
  // from the server, read them and print them:
  while (client.available()) {
    char c = client.read();
    Serial.write(c);
  }
  
  // if the server's disconnected, stop the client:
  if (!client.connected()) {
    Serial.println("disconnected from server.");
    client.stop();
  }
    
  //---------DATALOGGER---------
  #ifdef DataloggerSD
    // open the file. note that only one file can be open at a time,
    // so you have to close this one before opening another.
    File dataFile = SD.open("datalog2.txt", FILE_WRITE);
  
    // if the file is available, write to it:
    if (dataFile) {
      dataFile.println(dataString);
      dataFile.close();
      // print to the serial port too:
      Serial.println(dataString);
    }
    // if the file isn't open, pop up an error:
    else {
      Serial.println("error opening datalog2.txt");
    }
  #endif
   Serial.println(dataString);
   Serial.println();
}



void WiFiSetup()
{
  // check for the presence of the shield:
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present");
    // don't continue:
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  //Serial.println(fv);
  if (fv != "1.1.0") {
    Serial.println("Please upgrade the firmware");
  }

  // attempt to connect to Wifi network:
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    delay(10000);
  }
  Serial.println("Connected to wifi");
  printWifiStatus();

  Serial.println("\nStarting connection to server...");
  // if you get a connection, report back via serial:
  if (client.connect(server, 80)) {
    Serial.println("connected to server");
    
    // Make a HTTP request:
    client.print("GET /dweet/for//MACA1?");
    client.print("Temperature=");
    client.print(DHT_temperature);
    client.print("C");
    client.print("&Humidity=");
    client.print(DHT_humidity);
    client.print("%");
    client.print("&NO2=");
    client.print(NO2_out);
    client.print("&O3=");
    client.print(O3_out);
    client.print("&CO=");
    client.print(CO_out);
    client.println("");
    
    client.println("Host: dweet.io");
    client.println("Connection: close");
    client.println();
  }
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

void SendDataToServer ()
{
   if (client.connected()) {
    Serial.print(F("Sending request... "));
    
    client.print(F("GET /dweet/for/"));
    client.print(thing_name);
    client.print(F("?temperature="));
    client.print(DHT_temperature);
    client.print(F("&humidity="));
    client.print(DHT_humidity);
    client.println(F(" HTTP/1.1"));
    
    client.println(F("Host: dweet.io"));
    client.println(F("Connection: close"));
    client.println(F(""));
    
    Serial.println(F("done."));
  } else {
    Serial.println(F("Connection failed"));    
    return;
  }
}

#define countof(a) (sizeof(a) / sizeof(a[0]))

void RTCSetup()
{
  Serial.print("compiled: ");
  Serial.print(__DATE__);
  Serial.println(__TIME__);

  //--------RTC SETUP ------------
  Rtc.Begin();

  // if you are using ESP-01 then uncomment the line below to reset the pins to
  // the available pins for SDA, SCL
  // Wire.begin(0, 2); // due to limited pins, use pin 0 and 2 for SDA, SCL

  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  printDateTime(compiled);
  Serial.println();

  if (!Rtc.IsDateTimeValid()) 
  {
      // Common Cuases:
      //    1) first time you ran and the device wasn't running yet
      //    2) the battery on the device is low or even missing

      Serial.println("RTC lost confidence in the DateTime!");

      // following line sets the RTC to the date & time this sketch was compiled
      // it will also reset the valid flag internally unless the Rtc device is
      // having an issue

      Rtc.SetDateTime(compiled);
  }

  if (!Rtc.GetIsRunning())
  {
      Serial.println("RTC was not actively running, starting now");
      Rtc.SetIsRunning(true);
  }

  RtcDateTime now = Rtc.GetDateTime();
  if (now < compiled) 
  {
      Serial.println("RTC is older than compile time!  (Updating DateTime)");
      Rtc.SetDateTime(compiled);
  }
  else if (now > compiled) 
  {
      Serial.println("RTC is newer than compile time. (this is expected)");
  }
  else if (now == compiled) 
  {
      Serial.println("RTC is the same as compile time! (not expected but all is fine)");
  }

  // never assume the Rtc was last configured by you, so
  // just clear them to your needed state
  Rtc.Enable32kHzPin(false);
  Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeNone); 
}

void printDateTime(const RtcDateTime& dt)
{
    char datestring[20];

    snprintf_P(datestring, 
            countof(datestring),
            PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
            dt.Month(),
            dt.Day(),
            dt.Year(),
            dt.Hour(),
            dt.Minute(),
            dt.Second() );
    Serial.print(datestring);
}
