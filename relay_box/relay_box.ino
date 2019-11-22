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
/* WiFiServer server(6969); */
ESP8266WiFiMulti wifiMulti;
ESP8266WebServer server(6969);

/* RTC Objects */
RTC_DS3231 rtc;

/* config static IP */
IPAddress ip(192, 168, 1, 69);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

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
		nighttime();
	} else if (is_nighttime == false && day_night == NIGHTTIME) {
		/* Day-time begins */
		Serial.println("ITS DAY NOW");
		daytime();
	}

	/* delay(1000); */
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
		outletOff(i);
	}
}

void setupWifi(void)
{
	WiFi.begin(ssid[0], password[0]);
	WiFi.config(ip, gateway, subnet); /* Set static IP */
  
  Serial.print("Connecting ...");
	for (int i = 0; WiFi.status() != WL_CONNECTED && i < 40; i++)
	{
    delay(250);
    Serial.print('.');
  }

	if (WiFi.status() == WL_CONNECTED) {		
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

void setupWifiMulti(void)
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
	server.on("/RELAY", HTTP_POST, handleRelay);
	server.on("/DATETIME", HTTP_POST, handleDateTime);
	server.on("/DAYTIME", HTTP_POST, handleDaytime);
	server.on("/NIGHTTIME", HTTP_POST, handleNighttime);

	server.on("/POSTY", HTTP_POST, handlePosty);
	

	/* GET Handlers */
  server.on("/LED", HTTP_GET, handleLedGet);
	server.on("/RELAY", HTTP_GET, handleRelayGet);
		server.on("/DATETIME", HTTP_GET, handleDateTimeGet);
	server.on("/DAYTIME", HTTP_GET, handleDaytimeGet);
	server.on("/NIGHTTIME", HTTP_GET, handleNighttimeGet);

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
		
		for (int i = 0; i < NUM_OUTLETS; i++) {
			cfg_wr_reg(CFG__DAY_RELAYS__OFFSET + i, daytime_outlets[i]);
			cfg_wr_reg(CFG__NIGHT_RELAYS__OFFSET + i, nighttime_outlets[i]);
		}
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
		nighttime();
	} else {
		Serial.println("Starting up in DAYTIME");
		daytime();
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

void handleDateTime(void)
{
	Serial.println("Handling DATETIME POST request...");

	DateTime now = rtc.now();
	uint16_t year = now.year();
	uint8_t month = now.month();
	uint8_t day = now.day();
	uint8_t hour = now.hour();
	uint8_t minute = now.minute();
	uint8_t second = now.second();

	if (server.hasArg("year")) {
		year = (uint16_t) atoiStr(server.arg("year"));
	}

	if (server.hasArg("month")) {
		month = (uint8_t) atoiStr(server.arg("month"));
	}

	if (server.hasArg("day")) {
		day = (uint8_t) atoiStr(server.arg("day"));
	}

	if (server.hasArg("hour")) {
		hour = (uint8_t) atoiStr(server.arg("hour"));
	}

	if (server.hasArg("minute")) {
		minute = (uint8_t) atoiStr(server.arg("minute"));
	}

	if (server.hasArg("second")) {
		second = (uint8_t) atoiStr(server.arg("second"));
	}

	Serial.println(year);
	Serial.println(month);
	Serial.println(day);
	Serial.println(hour);
	Serial.println(minute);
	Serial.println(second);
	/* rtc.adjust(DateTime(2069, 12, 25, 12, 34, 56)); */
	rtc.adjust(DateTime(year, month, day, hour, minute, second));
	
	server.send(303);
}

void handleDateTimeGet(void)
{
	Serial.println("Handling DATETIME GET request...");
	server.send(200, "text/plain", timeStr());
}

void handleLed(void)
{
	Serial.println("Handling LED POST request...");

	ledToggle(ESP_BUILTIN_LED);
  server.sendHeader("Location","/");
  server.send(303);
}

void handlePosty(void)
{
	String key, val;
	for (int i = 0; i < server.args();i++){
		key = server.argName(i);
		val = server.arg(i);
		key.toLowerCase();
		val.toLowerCase();

		/* LED */
		if (key == "led") {
			if (val == "on") {
				ledOn(ESP_BUILTIN_LED);
			} else if (val == "off") {
				ledOff(ESP_BUILTIN_LED);
			}
		}
	}
	
  server.send(303);
}

void handleLedGet(void)
{
	Serial.println("Handling LED GET request...");
	server.send(200, "text/plain", ledStatus(ESP_BUILTIN_LED));
}

void handleRelay()
{
	String key, val;
	for (int i = 0; i < server.args();i++){
		key = server.argName(i);
		val = server.arg(i);
		key.toLowerCase();
		val.toLowerCase();

		/* Parse out relay number */
		int strlen = key.length() + 1;
		char char_array[strlen];
		key.toCharArray(char_array, strlen);
		int relay_num = atoi((const char*)char_array);

		if (relay_num > 0) {
			Serial.print("Handling RELAY POST request for relay number ");
			Serial.println(relay_num);

			/* Relays are zero-indexed */
			relay_num--;
			
			/* Switch that relay as desired */
			if (val == "on") {
				outletOn(relay_num);
			} else if (val == "off") {
				outletOff(relay_num);
			}
		}
	}
	
  server.send(303);
}

int atoiStr(String str) {
	int strlen = str.length() + 1;
	char char_array[strlen];
	str.toCharArray(char_array, strlen);

	return atoi((const char*)char_array);
}

void handleDaytime()
{
	String key, val;
	for (int i = 0; i < server.args();i++){
		key = server.argName(i);
		val = server.arg(i);
		key.toLowerCase();
		val.toLowerCase();

		if (key == "time_hr") {
			int time_hr = atoiStr(val);
			cfg_wr_reg(CFG__DAY_START_HR__OFFSET, time_hr);
		} else if (key == "time_mn") {
			int time_mn = atoiStr(val);
			cfg_wr_reg(CFG__DAY_START_MN__OFFSET, time_mn);
		} else {
			/* Parse out relay number */
			int strlen = key.length() + 1;
			char char_array[strlen];
			key.toCharArray(char_array, strlen);
			int relay_num = atoi((const char*)char_array);

			if (relay_num > 0) {
				Serial.print("Handling DAYTIME POST request for relay number ");
				Serial.println(relay_num);

				/* Relays are zero-indexed */
				relay_num--;
			
				/* Update that relay value in daytime array */				
				int relay_eeprom = CFG__DAY_RELAYS__OFFSET + relay_num;
				cfg_wr_reg(relay_eeprom, val.equals("on"));
			}
		}
	}
	
  server.send(303);
}

void handleNighttime()
{
	String key, val;
	for (int i = 0; i < server.args();i++){
		key = server.argName(i);
		val = server.arg(i);
		key.toLowerCase();
		val.toLowerCase();

		if (key == "time_hr") {
			int time_hr = atoiStr(val);
			cfg_wr_reg(CFG__NIGHT_START_HR__OFFSET, time_hr);
		} else if (key == "time_mn") {
			int time_mn = atoiStr(val);
			cfg_wr_reg(CFG__NIGHT_START_MN__OFFSET, time_mn);
		} else {
			/* Parse out relay number */
			int strlen = key.length() + 1;
			char char_array[strlen];
			key.toCharArray(char_array, strlen);
			int relay_num = atoi((const char*)char_array);

			if (relay_num > 0) {
				Serial.print("Handling NIGHTTIME POST request for relay number ");
				Serial.println(relay_num);

				/* Relays are zero-indexed */
				relay_num--;
			
				/* Update that relay value in nighttime array */
				int relay_eeprom = CFG__NIGHT_RELAYS__OFFSET + relay_num;
				cfg_wr_reg(relay_eeprom, val.equals("on"));
			}
		}
	}
	
  server.send(303);
}

void handleRelayGet()
{
	Serial.print("Handling RELAY GET request");

	String relay_status = "{"
		"'1':\"" + outletStatus(0) + "\","
		"'2':\"" + outletStatus(1) + "\","
		"'3':\"" + outletStatus(2) + "\","
		"'4':\"" + outletStatus(3) + "\","
		"}";

	server.send(200, "text/plain", relay_status);
}

void handleDaytimeGet()
{
	Serial.print("Handling RELAY GET request");

	String resp = "{";
	
	for (int i=0; i < NUM_OUTLETS; i++) {
		int relay_eeprom = CFG__DAY_RELAYS__OFFSET + i;
		byte relay_on = cfg_rd_reg(relay_eeprom);
		
		String relay_status = relay_on ? "ON" : "OFF";
		resp += "'" + String(i) + "':\"" + relay_status + "\",";
	}

	byte time_hr = cfg_rd_reg(CFG__DAY_START_HR__OFFSET);
	byte time_mn = cfg_rd_reg(CFG__DAY_START_MN__OFFSET);
	resp += "'time_hr':\"" + String(time_hr) + "\"";
	resp += "'time_mn':\"" + String(time_mn) + "\"";
	
	resp += "}";

	server.send(200, "text/plain", resp);
}

void handleNighttimeGet()
{
	Serial.print("Handling RELAY GET request");

	String resp = "{";
	
	for (int i=0; i < NUM_OUTLETS; i++) {
		int relay_eeprom = CFG__NIGHT_RELAYS__OFFSET + i;
		byte relay_on = cfg_rd_reg(relay_eeprom);
		
		String relay_status = relay_on ? "ON" : "OFF";
		resp += "'" + String(i) + "':\"" + relay_status + "\",";		
	}

	byte time_hr = cfg_rd_reg(CFG__NIGHT_START_HR__OFFSET);
	byte time_mn = cfg_rd_reg(CFG__NIGHT_START_MN__OFFSET);
	resp += "'time_hr':\"" + String(time_hr) + "\"";
	resp += "'time_mn':\"" + String(time_mn) + "\"";
	
	resp += "}";

	server.send(200, "text/plain", resp);
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

void outletOn(int relay_num)
{
	digitalWrite(outlet_gpio_map[relay_num], HIGH);
}

void outletOff(int relay_num)
{
	digitalWrite(outlet_gpio_map[relay_num], LOW);
}

void outletToggle(int relay_num)
{
	int gpio_num = outlet_gpio_map[relay_num];
	digitalWrite(gpio_num, !digitalRead(gpio_num));	
}

String outletStatus(int relay_num) {
	int gpio_num = outlet_gpio_map[relay_num];
	return digitalRead(gpio_num) ? "ON" : "OFF";
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

void daytime()
{
	cfg_wr_reg(CFG__DAY_NIGHT__OFFSET, DAYTIME);
	
	for (int i = 0; i < NUM_OUTLETS; i++) {
		int relay_eeprom = CFG__DAY_RELAYS__OFFSET + i;
		byte relay_on = cfg_rd_reg(relay_eeprom);
		
		if (relay_on) {
			Serial.print("Turning ON outlet #");
			Serial.println(i);
			outletOn(i);
		} else {
			Serial.print("Turning OFF outlet #");
			Serial.println(i);
			outletOff(i);
		}
	}

	Serial.println("poop but daytime");
	Serial.flush();
}

void nighttime()
{
	cfg_wr_reg(CFG__DAY_NIGHT__OFFSET, NIGHTTIME);
	
	for (int i = 0; i < NUM_OUTLETS; i++) {
		int relay_eeprom = CFG__NIGHT_RELAYS__OFFSET + i;
		byte relay_on = cfg_rd_reg(relay_eeprom);
		
		if (relay_on) {
			Serial.print("Turning ON outlet #");
			Serial.println(i);
			outletOn(i);
		} else {
			Serial.print("Turning OFF outlet #");
			Serial.println(i);
			outletOff(i);
		}
	}

	Serial.flush();
}
