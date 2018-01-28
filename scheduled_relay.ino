#include <EEPROM.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <DS1307RTC.h>
#include <Time.h>
#include <U8glib.h>

#define CONFIG_VERSION "ls2"
#define CONFIG_START 0


//Relays pins 4 to 13
const int relaysPins[] PROGMEM = {4, 5, 6, 7};


const int RELAY_COUNT = sizeof(relaysPins) / sizeof(int);
const char CONTENT_SEPARATOR PROGMEM = '|';
const char DATA_SEPARATOR PROGMEM = ':';
const int CYCLE_COUNT_CHANGE_DISPLAY_RELAY PROGMEM = 20; // depends on the loop delay
const char *dayOfWeek[] = {"", "Dim", "Lun", "Mar", "Mer", "Jeu", "Ven", "Sam"};

int cycleCounter = 0;
int displayRelayIndex = 0;

SoftwareSerial btSerial(2, 3); //Bluetooth RX, TX
U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_NONE);

///////////////////////////////////
//----- RelaySchedule class
///////////////////////////////////
//weekdays format is a byte where each bit correspond to a weekday, 0 means disabled : 0 |X       |X     |X       |X        |X      |X     |X     |
//                                                                                     NA|Saturday|Friday|Thursday|Wednesday|Tuesday|Monday|Sunday|
class RelaySchedule
{
  public:
    int relayPin; boolean forced = false; boolean enabled; char desc[20]; boolean activated; byte weekDays; int startHour; int startMinutes; int endHour; int endMinutes;
  public:
    RelaySchedule(int relayPin, boolean enabled, byte weekDays, int startHour, int startMinutes, int endHour, int endMinutes);
    boolean isForced();
    boolean isEnabled();
    int getIntStartTime();
    int getIntEndTime();
    String getDesc();
    void setDesc(String str);
    void activate();
    void desactivate();
    boolean isActivated();
    boolean isActiveThisToday(int today);
    String toString(int index);
    String displayLine1(int index);
    String displayLine2(int index);
    boolean update( boolean forced, boolean enabled, String desc, byte weekDays, int startHour, int startMinutes, int endHour, int endMinutes);
    String  formatTimeDigits(int num);
    boolean checkIntegrity();
};

RelaySchedule::RelaySchedule(int relayPin, boolean enabled, byte weekDays, int startHour, int startMinutes, int endHour, int endMinutes): relayPin(relayPin), enabled(enabled), activated(false), weekDays(weekDays), startHour(startHour), startMinutes(startMinutes), endHour(endHour), endMinutes(endMinutes) {
  pinMode(relayPin, OUTPUT);
  strncpy(desc , "Relay desc",20);
};

boolean RelaySchedule::isForced()
{
  return forced;
}

boolean RelaySchedule::isEnabled()
{
  return enabled;
}

int RelaySchedule::getIntStartTime()
{
  return (startHour * 60) + startMinutes;
};

int RelaySchedule::getIntEndTime()
{
  return (endHour * 60) + endMinutes;
};

String RelaySchedule::getDesc()
{
  return desc;
};

void RelaySchedule::activate()
{ activated = true;
  digitalWrite(relayPin, LOW);
};

void RelaySchedule::desactivate()
{ activated = false;
  digitalWrite(relayPin, HIGH);
};

boolean RelaySchedule::isActiveThisToday(int today)
{
  return bitRead(weekDays, (today - 1));
};

boolean RelaySchedule::isActivated()
{
  return activated;
};

String RelaySchedule::toString(int index)
{
  String value = "";
  value += index;
  value += "|";
  value += forced ? "1" : "0";
  value += "|";
  value += enabled ? "1" : "0";
  value += "|";
  value += desc;
  value += "|";
  value += String(weekDays, BIN);
  value += "|";
  value += startHour;
  value += "|";
  value += startMinutes;
  value += "|";
  value += endHour;
  value += "|";
  value += endMinutes;

  return value;
};

String RelaySchedule::displayLine1(int index)
{ String value = "";
  value += "R";
  value += (index + 1);
  value += "|";
  if (!forced) {
    if (enabled) {
      value += formatTimeDigits(startHour);
      value += ":";
      value += formatTimeDigits(startMinutes);
      value += "->";
      value += formatTimeDigits(endHour);
      value += ":";
      value += formatTimeDigits(endMinutes);
      value += " ";
      value += activated ? "ON" : "OF";
    } else {
      value += "Inactif";
    }
  } else {
    value += "Mode Manuel";
  }
  return value;
};

String RelaySchedule::displayLine2(int index)
{ String value = "";
  if (!forced) {
    if (enabled) {
      for (int i = 2 ; i <= 7; i++) {
        if (isActiveThisToday(i)) {
          switch (i) {
            case 2:
              value += "L ";
              break;
            case 3:
              value += "Ma ";
              break;
            case 4:
              value += "Me ";
              break;
            case 5:
              value += "J ";
              break;
            case 6:
              value += "V ";
              break;
            case 7:
              value += "S ";
              break;
          }
        }
      }
      if (isActiveThisToday(1))
        value += "D ";
    }
  } else {
    value += activated ? "ON" : "OFF";
  }
  return value;
};

boolean RelaySchedule::update(boolean fo, boolean ena, String d, byte wd, int sh, int sm, int eh, int em)
{
  enabled = ena;
  char __st[20];
    d.toCharArray(desc, sizeof(desc));
  weekDays = wd;
  startHour = sh;
  startMinutes = sm;
  endHour = eh;
  endMinutes = em;
  if (!forced || forced != fo) // save configuration only if not forced
    updateConfig();

  forced = fo;

  return true;
};

String RelaySchedule::formatTimeDigits(int num)
{
  if ((num / 10) > 0) {
    return String(num, DEC);
  } else {
    return "0" + String(num, DEC);
  }
}

boolean RelaySchedule::checkIntegrity()
{
  return startHour != -1 && startMinutes != -1 && endHour != -1 && endMinutes != -1;
}

struct StoreStruct {
  char version[4];
  RelaySchedule relaySchedules[RELAY_COUNT];
} storage = {
  CONFIG_VERSION,
  { RelaySchedule(4, false, B01111111, 10, 00, 11, 00),
    RelaySchedule(5, false, B01111111, 10, 00, 11, 00),
    RelaySchedule(6, false, B01111111, 10, 00, 11, 00),
    RelaySchedule(7, false, B01111111, 10, 00, 11, 00),
  }
};





///////////////////////////////////
//----- Time functions
///////////////////////////////////
void schedulerService() {

  tmElements_t tm;
  RTC.read(tm);
  int wd = weekday();
  int hour = tm.Hour;
  int minute = tm.Minute;

  int nowIntTime = (hour * 60) + minute;
  for (int i = 0; i < RELAY_COUNT; i++) {
    if ((storage.relaySchedules[i].isForced() && storage.relaySchedules[i].isEnabled())
        || (!storage.relaySchedules[i].isForced() && storage.relaySchedules[i].isEnabled() // if is enabled
            && storage.relaySchedules[i].isActiveThisToday(wd) // if is active today
            && (nowIntTime >= storage.relaySchedules[i].getIntStartTime() && nowIntTime < storage.relaySchedules[i].getIntEndTime()))) {
      if (!storage.relaySchedules[i].isActivated()) {
        storage.relaySchedules[i].activate();
      }
    } else {
      if (storage.relaySchedules[i].isActivated()) {
        storage.relaySchedules[i].desactivate();
      }
    }
  }
}

String getDateTime() {
  tmElements_t tm;
  RTC.read(tm);
  int wd = weekday();
  int month = tm.Month;
  int day = tm.Day;
  int hour = tm.Hour;
  int minute = tm.Minute;
  int second = tm.Second;
  String nowString = String(dayOfWeek[wd])
                     + " "
                     + (day < 10 ? "0" + String(day) : String(day))
                     + "/"
                     + (month < 10 ? "0" + String(month) : String(month))
                     + " "
                     + (hour < 10 ? "0" + String(hour) : String(hour))
                     + ":"
                     + (minute < 10 ? "0" + String(minute) : String(minute))
                     + ":"
                     + (second < 10 ? "0" + String(second) : String(second)) ;
  return nowString;
}

String  formatTimeDigits(int num)
{
  if ((num / 10) > 0) {
    return String(num, DEC);
  } else {
    return "0" + String(num, DEC);
  }
}

///////////////////////////////////
//----- BT, Data parsing functions
///////////////////////////////////
// Parsing packet according to current mode
void listenForBluetoothRequest() {
  String received = "";
  while (btSerial.available()) {
    char chars = (char) btSerial.read();
    received += chars;
  }  // End of if(BTSerial.available())
  if (received.length() > 0) {
    processBluetoothRequest(received); // Parsing received Data
  }
} // End of receiveBluetoothData()

void sendDataToBT(String message) {
  btSerial.println(message);
}

// Data format : [COMMAND]:[CONTENT] ex : UPDATE:1|0|1|01111111|2|27|2|28 , STATUS, SET_TIME:2015|4|7|0|14|15, PING, RELAYS_INFO:0
void processBluetoothRequest(String data) {
  //RelaySchedule(1, getValueAt(data, 1), getValueAt(data, 2) == "1", "aaaaa", "bbbb")
  String cmd = getCommand(data);
  if (cmd == "RI") {
    sendDataToBT(sendRelayInfo(getCotent(data)));
  } else if (cmd == "UR") {
    updateRelays(getCotent(data));
    sendDataToBT(sendOkResponse("UR"));
  } else if (cmd == "RC") {
    sendDataToBT(sendArduinoRelayCount());
  } else if (cmd == "ST") {
    setArduinoTime(getCotent(data));
    sendDataToBT(sendOkResponse("ST"));
  } else {
    sendDataToBT("UNKNOWN_CMD:" + data);
  }
}

String getValueAt(String content, int index) {
  return getValueAt(content, index, CONTENT_SEPARATOR);
}

String getCommand(String data) {
  return getValueAt(data, 0, DATA_SEPARATOR);
}

String getCotent(String data) {
  return getValueAt(data, 1, DATA_SEPARATOR);
}

String getValueAt(String content, int index, char separator) {
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = content.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (content.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? content.substring(strIndex[0], strIndex[1]) : "";
}

// TODO : need to be improved. convert binary string to byte
byte stringToByte(String st) {
  byte b = B0;
  char __st[st.length() + 1];
  st.toCharArray(__st, sizeof(__st));
  for (int i = 0; i < st.length(); i++) {
    if (__st[i] == '1')
      bitWrite(b, (7 - i), 1);
  }
  return b;
}

// CONTENT format : [INDEX int]|[FORCED 0|1]|[ENABLED 0|1]|[RELAY_DESCRIPTION]|[WEEKDAYS byte]|[STARTHOUR int]|[STARTMINUTE int]|[ENDHOUR int]|[ENDMINUTE int]  ex : 1|0|1|01111111|2|27|2|28
void updateRelays(String content) {
  int index = getValueAt(content, 0).toInt();
  storage.relaySchedules[index].update(getValueAt(content, 1) == "1", getValueAt(content, 2) == "1" , getValueAt(content, 3), stringToByte(getValueAt(content, 4)), getValueAt(content, 5).toInt(), getValueAt(content, 6).toInt(), getValueAt(content, 7).toInt(), getValueAt(content, 8).toInt());
}
// CONTENT format : [YEAR]|[MONTH]|[DAY]|[HOUR]|[MINUTE]|[SECOND]  ex : 2015|4|7|0|14|15
void setArduinoTime(String content) {
  //rtc.adjust(DateTime(getValueAt(content, 0).toInt(), getValueAt(content, 1).toInt(), getValueAt(content, 2).toInt(), getValueAt(content, 3).toInt(), getValueAt(content, 4).toInt(), getValueAt(content, 5).toInt()));
  setTime(getValueAt(content, 3).toInt(),
          getValueAt(content, 4).toInt(),
          getValueAt(content, 5).toInt(),
          getValueAt(content, 2).toInt(),
          getValueAt(content, 1).toInt(),
          getValueAt(content, 0).toInt());
  RTC.set(now());
}

// formatting JSON responses
String sendRelayInfo(String content) {
  int index = getValueAt(content, 0).toInt();
  String value = "";
  value += "RI:";
  value += "{\"ri\":\"";
  value += storage.relaySchedules[index].toString(index);
  value += "\"}";
  return value;
}

String sendArduinoRelayCount() {
  String value = "";
  value += "RC:";
  value += "{\"rc\":";
  value += RELAY_COUNT;
  value += "}";
  return value;
}

String sendOkResponse(String request) {
  String value = "";
  value += request + ":";
  value += "{\"rs\":\"OK\"}";
  return value;
}

///////////////////////////////////
//----- Dislay functions
///////////////////////////////////

/*String getRelaysStatus() {
  String result = "";
  for (int i = 0; i < RELAY_COUNT; i++) {
    if (storage.relaySchedules[i].isActivated())
      result += "1";
    else
      result += "0";

    result += "|";
  }
  return result;
}*/

void showInDisplay(char __st[20]) {
  u8g.firstPage();
  do {
    u8g.setFont(u8g_font_7x14);
    u8g.drawStr( 10, 40, __st);
  } while ( u8g.nextPage() );
}

void updateDisplay() {

  if (cycleCounter >= CYCLE_COUNT_CHANGE_DISPLAY_RELAY) {
    cycleCounter = 0;
    if (displayRelayIndex == (RELAY_COUNT - 1))
      displayRelayIndex = 0;
    else
      displayRelayIndex++;
  }

  // picture loop
  u8g.firstPage();
  do {
    // graphic commands to redraw the complete screen should be placed here
    u8g.setFont(u8g_font_7x14);

    char __st[20];

    //draw dateTime
    String str = getDateTime();

    str.toCharArray(__st, sizeof(__st));
    u8g.drawStr( 0, 10, __st);

    str = storage.relaySchedules[displayRelayIndex].displayLine1(displayRelayIndex);
    str.toCharArray(__st, sizeof(__st));
    u8g.drawStr( 0, 27, __st);

    str = storage.relaySchedules[displayRelayIndex].displayLine2(displayRelayIndex);
    str.toCharArray(__st, sizeof(__st));
    u8g.drawStr( 0, 44, __st);

    str = "FREE MEM : " + String(freeRam(), DEC) + "B";
    str.toCharArray(__st, 16);
    u8g.drawStr( 0, 64,  __st);

    /*str = storage.relaySchedules[displayRelayIndex].getDesc();
    str.toCharArray(__st, str.length());
    u8g.drawStr( 0, 64,  __st);*/

  } while ( u8g.nextPage() );
}

int freeRam () {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

///////////////////////////////////
//----- EEPROM function
///////////////////////////////////

bool loadConfig() {
  // To make sure there are settings
  if (EEPROM.read(CONFIG_START + 0) == CONFIG_VERSION[0] &&
      EEPROM.read(CONFIG_START + 1) == CONFIG_VERSION[1] &&
      EEPROM.read(CONFIG_START + 2) == CONFIG_VERSION[2]) {
    for (unsigned int t = 0; t < sizeof(storage); t++)
      *((char*)&storage + t) = EEPROM.read(CONFIG_START + t);
    return true;
  } else {
    return false;
  }
}

void updateConfig() {
  //Serial.println("writing to eeprom");
  int count = 0;
  for (unsigned int t = 0; t < sizeof(storage); t++) {
    if (EEPROM.read(CONFIG_START + t) != *((char*)&storage + t)) {
      EEPROM.write(CONFIG_START + t, *((char*)&storage + t));
      count++;
    }
  }
}

///////////////////////////////////
//----- setup function
///////////////////////////////////
void setup () {
  //init relays pins
  for (int i = 0; i < RELAY_COUNT; i++) {
    digitalWrite(relaysPins[i], HIGH);
  }
  //Serial.begin(9600);
  btSerial.begin(9600);
  Wire.begin();
  u8g.begin();

  //Sync time with RTC
  setSyncProvider(RTC.get);

  // assign default color value
  if ( u8g.getMode() == U8G_MODE_R3G3B2 ) {
    u8g.setColorIndex(255);     // white
  }
  else if ( u8g.getMode() == U8G_MODE_GRAY2BIT ) {
    u8g.setColorIndex(3);         // max intensity
  }
  else if ( u8g.getMode() == U8G_MODE_BW ) {
    u8g.setColorIndex(1);         // pixel on
  }
  else if ( u8g.getMode() == U8G_MODE_HICOLOR ) {
    u8g.setHiColorByRGB(255, 255, 255);
  }


  // Loadinf configuration from eeprom
  showInDisplay("Check saved conf ...");
  delay(1000);
  if (loadConfig()) {
    showInDisplay("config loading OK");
  } else {
    showInDisplay("No conf, init...");
    updateConfig();
  }
  delay(1000);

}

///////////////////////////////////
//----- loop function
///////////////////////////////////
void loop () {
  listenForBluetoothRequest();
  schedulerService();
  updateDisplay();
  cycleCounter++;
  delay(100);
}
