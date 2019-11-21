#ifndef _RELAY_BOX__H_
#define _RELAY_BOX__H_

/* Wifi Config */
#define NUM_WIFI (1)
const char* ssid[NUM_WIFI] = {
    /* "TheSaltySpitoon", */
    "TheSaltySpitoon_2GEXT"
};

const char* password[NUM_WIFI] = {
    /* "AbandonAllHopeYeWhoEnterHere" */
    "AbandonAllHopeYeWhoEnterHere",	
};

/* LED Config */
enum relay_box_led {
    ESP_BUILTIN_LED = 2,
};

/* Outlet Config */
#define NUM_OUTLETS (4)
bool nighttime_outlets[NUM_OUTLETS] = { true,  true,  false, false };
bool daytime_outlets[NUM_OUTLETS]   = { false, false, true,  true };
int outlet_gpio_map[NUM_OUTLETS] = { 14, 12, 13, 16 }; /* D5, D6, D7, D0 */

/* Stringified days of the week */
char daysOfTheWeek[7][12] = { "Sunday", "Monday", "Tuesday", "Wednesday",
			      "Thursday", "Friday", "Saturday" };
/* CFG Memory Map */
#define CFG__EEPROM_START__ADDR     (0x0)
#define CFG__MAGIC_NUM__OFFSET      (0x0)
#define CFG__DAY_NIGHT__OFFSET      (0x1)
#define CFG__DAY_START_HR__OFFSET   (0x2)
#define CFG__DAY_START_MN__OFFSET   (0x3)
#define CFG__NIGHT_START_HR__OFFSET (0x4)
#define CFG__NIGHT_START_MN__OFFSET (0x5)

/* Register value defines */
#define MAGIC_NUM (0x75) /* Can change this to force an EEPROM write at setup */
#define DAYTIME   (0x1)
#define NIGHTTIME (0x2)

#endif /* _RELAY_BOX__H_ */
