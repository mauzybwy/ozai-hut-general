#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WiFiMulti.h> 
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include "RTClib.h"
#include <EEPROM.h>
#include "relay_box.h"

/******************************************************************************
 * Global Objects
 ******************************************************************************/
/* Wifi Objects */
ESP8266WiFiMulti wifiMulti;
ESP8266WebServer server(80);

/* RTC Objects */
RTC_DS3231 rtc;

/******************************************************************************
 * Setup
 ******************************************************************************/
void setup(void)
{
	/* Set up all relevant components */
	setupSerial();
  setupGPIO();
	setupRTC();
	setupEEPROM();
	setupDayNight();
	setupWifi();
	setupServer();	

	/* Turn on the built-in LED to indicate successful setup */
	ledOn(ESP_BUILTIN_LED);

	/* Begin the server */
  server.begin();
  Serial.println("HTTP server started");
}

/******************************************************************************
 * Loop
 ******************************************************************************/
void loop(void){
	/* Listen for HTTP requests from client */
  server.handleClient();
	
	DateTime now = rtc.now();

	/* Is it actually night-time? */
	bool is_nighttime = isNighttime();

	/*  */
	byte day_night= cfg_rd_reg(CFG__DAY_NIGHT__OFFSET);

	/* Handle day-night transitions */
	if (is_nighttime == true && day_night == DAYTIME) {
		/* Night-time begins */
		Serial.println("ITS NIGHT NOW");
		handleNighttime();
	} else if (is_nighttime == false && day_night == NIGHTTIME) {
		/* Day-time begins */
		Serial.println("ITS DAY NOW");
		handleDaytime();
	}

	delay(1000);
}

/******************************************************************************
 * Definitions
 ******************************************************************************/
void setupSerial(void)
{
	Serial.begin(115200);
  delay(10);
  Serial.println('\n');
}

void setupGPIO(void)
{
	/* Set up LEDs */
	pinMode(ESP_BUILTIN_LED, OUTPUT);
	ledOff(ESP_BUILTIN_LED);

	/* Set up outlet GPIOs */
	for (int i = 0; i < NUM_OUTLETS; i++) {
		pinMode(outlet_gpio_map[i], OUTPUT);
		outletOff(outlet_gpio_map[i]);
	}
}

void setupWifi(void)
{
	for (int i = 0; i < NUM_WIFI; i++)
	{
		wifiMulti.addAP(ssid[i], password[i]);
	}
  
  Serial.println("Connecting ...");
	for (int i = 0; wifiMulti.run() != WL_CONNECTED && i < 40; i++)
	{
    delay(250);
    Serial.print('.');
  }

	if (wifiMulti.run() == WL_CONNECTED) {		
		Serial.println('\n');
		Serial.print("Connected to ");
		Serial.println(WiFi.SSID());
		Serial.print("IP address:\t");
		Serial.println(WiFi.localIP());

		if (MDNS.begin("esp8266")) {
			Serial.println("mDNS responder started");
		} else {
			Serial.println("Error setting up MDNS responder!");
		}
	} else {
		Serial.println("Timeout connecting to Wifi, will operate on defaults.");
	}
}

void setupServer(void)
{
	/* Main Webpage */
	server.on("/", HTTP_GET, handleRoot);

	/* POST Handlers */
  server.on("/LED", HTTP_POST, handleLed);
	server.on("/RELAY/1", HTTP_POST, handleRelay1);
	server.on("/RELAY/2", HTTP_POST, handleRelay2);
	server.on("/RELAY/3", HTTP_POST, handleRelay3);
	server.on("/RELAY/4", HTTP_POST, handleRelay4);

	/* GET Handlers */
  server.on("/LED", HTTP_GET, handleLedGet);
	server.on("/TIME", HTTP_GET, handleTime);
	server.on("/RELAY/1", HTTP_GET, handleRelayGet1);
	server.on("/RELAY/2", HTTP_GET, handleRelayGet2);
	server.on("/RELAY/3", HTTP_GET, handleRelayGet3);
	server.on("/RELAY/4", HTTP_GET, handleRelayGet4);

	/* Invalid request */
  server.onNotFound(handleNotFound);
}

void setupRTC(void)
{
	if (! rtc.begin()) {
    Serial.println("ERROR: Couldn't find RTC");
  }

	/* rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); */
	/* rtc.adjust(DateTime(2069, 12, 25, 7, 59, 50)); */
	
	if (rtc.lostPower()) {
    Serial.println("RTC lost power, lets set the time!");

		/* Set a dummy time, expected to be overwritten remotely */
    /* rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); */
    rtc.adjust(DateTime(2069, 12, 25, 12, 34, 56));
  }
}

void setupEEPROM()
{
	byte val;

	/* Start the EEPROM with 512 bytes */
	EEPROM.begin(512);

	/* If the magic number is incorrect, re-write defaults */
	val = cfg_rd_reg(CFG__MAGIC_NUM__OFFSET);
	if (val != MAGIC_NUM) {
		Serial.println("EEPROM Magic Number mismatch. Re-write with defaults.");

		cfg_wr_reg(CFG__MAGIC_NUM__OFFSET, MAGIC_NUM);
		cfg_wr_reg(CFG__DAY_NIGHT__OFFSET, NIGHTTIME);
		cfg_wr_reg(CFG__DAY_START_HR__OFFSET, 8);
		cfg_wr_reg(CFG__DAY_START_MN__OFFSET, 0);
		cfg_wr_reg(CFG__NIGHT_START_HR__OFFSET, 21);
		cfg_wr_reg(CFG__NIGHT_START_MN__OFFSET, 30);
	}
}

void setupDayNight()
{
	/* Is it actually night-time? */
	DateTime now = rtc.now();
	
	bool is_nighttime = isNighttime();

	/* Configure initial Day/Night */
	if (is_nighttime == true) {
		Serial.println("Starting up in NIGHTTIME");
		handleNighttime();
	} else {
		Serial.println("Starting up in DAYTIME");
		handleDaytime();
	}
}

void handleRoot(void)
{
	Serial.println("Handling ROOT request...");
	
	String root_html = "<html>"
		"<div>"
		    "<form action=\"/LED\" method=\"POST\">"
            "<input type=\"submit\" value=\"Toggle LED\">"
		    "</form>"
		    " LED " + ledStatus(ESP_BUILTIN_LED) +
		"</div>"
		"<p>" + timeStr() + "</p>"
		"</html>";
	
	
  server.send(200, "text/html", root_html);
}

void handleTime(void)
{
	Serial.println("Handling TIME GET request...");
	server.send(200, "text/html", timeStr());
}

void handleLed(void)
{
	Serial.println("Handling LED POST request...");

	ledToggle(ESP_BUILTIN_LED);
  server.sendHeader("Location","/");
  server.send(303);
}

void handleLedGet(void)
{
	Serial.println("Handling LED GET request...");
	server.send(200, "text/html", ledStatus(ESP_BUILTIN_LED));
}

void handleRelay(int relay_num)
{
	Serial.print("Handling RELAY POST request for relay number ");
	Serial.println(relay_num);

	outletToggle(outlet_gpio_map[relay_num]);
	server.sendHeader("Location","/");
  server.send(303);
}

void handleRelayGet(int relay_num)
{
	Serial.print("Handling RELAY GET request for relay number ");
	Serial.println(relay_num);

	server.send(200, "text/html", outletStatus(outlet_gpio_map[relay_num]));
}

void handleRelay1(void)
{
	handleRelay(0);
}

void handleRelay2(void)
{
	handleRelay(1);
}

void handleRelay3(void)
{
	handleRelay(2);
}

void handleRelay4(void)
{
	handleRelay(3);
}

void handleRelayGet1(void)
{
	handleRelayGet(0);
}

void handleRelayGet2(void)
{
	handleRelayGet(1);
}

void handleRelayGet3(void)
{
	handleRelayGet(2);
}

void handleRelayGet4(void)
{
	handleRelayGet(3);
}

void handleNotFound(void)
{
	Serial.println("Handling INVALID request...");
	
  server.send(404, "text/plain", "404: Not found");
}

void ledOn(int gpio_num)
{
	digitalWrite(gpio_num, LOW);
}

void ledOff(int gpio_num)
{
	digitalWrite(gpio_num, HIGH);
}

void ledToggle(int gpio_num)
{
	digitalWrite(gpio_num, !digitalRead(gpio_num));
}

String ledStatus(int gpio_num)
{
	return digitalRead(gpio_num) ? "OFF" : "ON";
}

void outletOn(int gpio_num)
{
	digitalWrite(gpio_num, HIGH);
}

void outletOff(int gpio_num)
{
	digitalWrite(gpio_num, LOW);
}

void outletToggle(int gpio_num)
{
	digitalWrite(gpio_num, !digitalRead(gpio_num));	
}

String outletStatus(int gpio_num) {
	return digitalRead(gpio_num) ? "OFF" : "ON";
}

String timeStr()
{
	DateTime now = rtc.now();
	
	String year = String(now.year(), DEC);
	String month = String(now.month(), DEC);
	String day = String(now.day(), DEC);
	String day_of_week = daysOfTheWeek[now.dayOfTheWeek()];
	String hour = String(now.hour(), DEC);
	String minute = String(now.minute(), DEC);
	String second = String(now.second(), DEC);
	String temp = String(rtc.getTemperature(), DEC);

	String out_str = String(year + "-" + month + "-" + day +
													" (" + day_of_week + ") " +
													hour + ":" + minute + ":" + second +
													" - Temperature: " + temp + " C\n");
	
	return out_str;
}

bool isNighttime()
{
	/* Read day/night configuration from memory */
	byte night_hour = cfg_rd_reg(CFG__NIGHT_START_HR__OFFSET);
	byte night_minute = cfg_rd_reg(CFG__NIGHT_START_MN__OFFSET);
	byte day_hour = cfg_rd_reg(CFG__DAY_START_HR__OFFSET);
	byte day_minute = cfg_rd_reg(CFG__DAY_START_MN__OFFSET);

	/* Read current time from RTC */
	DateTime now = rtc.now();

	/* Calculate the time of day in minutes */
	int night_start = night_hour * 60 + night_minute;
	int day_start = day_hour * 60 + day_minute;
	int current_time = now.hour() * 60 + now.minute();
	int noon = 12 * 60;

	/* Determine whether it is indeed night-time */
	bool is_nighttime = ((current_time >= noon && current_time >= night_start) ||
											 (current_time < noon && current_time < day_start));

	return is_nighttime;
}

void cfg_wr_reg(int reg_offset, byte val)
{
	/* Start EEPROM write */
	EEPROM.write(CFG__EEPROM_START__ADDR + reg_offset, val);

	/* Commit the write to memory */
	if (EEPROM.commit()) {
		Serial.println("EEPROM successfully committed");
	} else {
		Serial.println("ERROR! EEPROM commit failed");
	}
}

byte cfg_rd_reg(int reg_offset)
{
	byte val;

	/* Read val from requested register */
	val = EEPROM.read(CFG__EEPROM_START__ADDR + reg_offset);

	return val;
}

void handleDaytime()
{
	cfg_wr_reg(CFG__DAY_NIGHT__OFFSET, DAYTIME);
	
	for (int i; i < NUM_OUTLETS; i++) {
		if (daytime_outlets[i]) {
			Serial.print("Turning ON outlet #");
			Serial.println(i);
			outletOn(outlet_gpio_map[i]);
		} else {
			Serial.print("Turning OFF outlet #");
			Serial.println(i);
			outletOff(outlet_gpio_map[i]);
		}
	}
	
	Serial.flush();
}

void handleNighttime()
{
	cfg_wr_reg(CFG__DAY_NIGHT__OFFSET, NIGHTTIME);
	
	for (int i; i < NUM_OUTLETS; i++) {
		if (nighttime_outlets[i]) {
			Serial.print("Turning ON outlet #");
			Serial.println(i);
			outletOn(outlet_gpio_map[i]);
		} else {
			Serial.print("Turning OFF outlet #");
			Serial.println(i);
			outletOff(outlet_gpio_map[i]);
		}
	}

	Serial.flush();
}
