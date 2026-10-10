#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Preferences.h>

// Defining Pins 
#define PIN_V       34
#define PIN_I       35
#define PIN_RELAY   26
#define PIN_SET     32
#define PIN_ACCEPT  33
#define PIN_PLUS    25
#define PIN_MINUS   27

#define RELAY_ON    HIGH
#define RELAY_OFF   LOW


#define V_FULLSCALE       300.0f
#define I_FULLSCALE        20.0f
#define DEBOUNCE_MS        20
#define DOUBLE_CLICK_MS   400
#define RAND_SAFE_PERCENT  60
#define RAND_MIN_MS      2000L
#define RAND_MAX_MS      2000L
#define HYST_V            5.0f

LiquidCrystal_I2C lcd(0x27, 16, 2);
Preferences prefs;


struct Settings {
  float ov;
  float uv;
  float oc;
  int tripMs;
  int reconSec;
  bool autoSrc;
};

Settings cfg, ed;

enum Fault {
  F_NONE,
  F_OVERV,
  F_UNDERV,
  F_OVERI,
  F_SHORT,
  F_MANUAL
};

const char* faultName[] = {
  "OK", "OVER-VOLT", "UNDER-VOLT",
  "OVER-CURR", "SHORT-CKT", "MANUAL-OFF"
};


float volts = 0, amps = 0;
float autoVolt = 230;

bool relayOn = false;
bool streaming = false;

Fault activeFault = F_NONE;
Fault candidate = F_NONE;

unsigned long candSince = 0;
unsigned long safeSince = 0;
unsigned long nextPick = 0;

int page = 0;
const int LAST_PAGE = 6;

char flashMsg[17] = "";
unsigned long flashUntil = 0;

// Saving to ESP32's non-volatile 

void loadSettings() {
  prefs.begin("vprot", false);

  cfg.ov       = prefs.getFloat("ov", 250);
  cfg.uv       = prefs.getFloat("uv", 180);
  cfg.oc       = prefs.getFloat("oc", 10);
  cfg.tripMs   = prefs.getInt("td", 120);
  cfg.reconSec = prefs.getInt("recon", 5);
  cfg.autoSrc  = prefs.getBool("auto", false);
}

void saveSettings() {
  prefs.putFloat("ov", cfg.ov);
  prefs.putFloat("uv", cfg.uv);
  prefs.putFloat("oc", cfg.oc);
  prefs.putInt("td", cfg.tripMs);
  prefs.putInt("recon", cfg.reconSec);
  prefs.putBool("auto", cfg.autoSrc);
}

bool validSettings(float ov, float uv, float oc, int td, int rs) {
  return ov >= 100 && ov <= 300 &&
         ov >= uv + 20 && uv >= 50 &&
         oc >= 1 && oc <= 20 &&
         td >= 20 && td <= 1000 &&
         rs >= 0 && rs <= 60;
}

void flash(const char* message) {
  strncpy(flashMsg, message, 16);
  flashMsg[16] = '\0';
  flashUntil = millis() + 1500;
}

// Reading values 

float readAvg(int pin, float fullScale) {
  long sum = 0;

  for (int i = 0; i < 8; i++)
    sum += analogRead(pin); // reads upto 8 values and averages 

  return (sum / 8.0f) * fullScale / 4095.0f;
}

//rand voltage generation for realtime_simulation 

void pickRandom() {
  if (random(100) < RAND_SAFE_PERCENT) {
    autoVolt = random((long)cfg.uv + 10,
                      (long)cfg.ov - 10 + 1);
  } else {
    autoVolt = random(0L, 301L);
  }

  nextPick = millis() +
             random(RAND_MIN_MS, RAND_MAX_MS + 1);

  Serial.printf("[AUTO] new supply voltage = %.0f V\n", autoVolt);
}

float readVoltage() {
  static bool prevAuto = false;

  if (cfg.autoSrc) {
    if (!prevAuto || millis() >= nextPick)
      pickRandom();

    prevAuto = true;

    float v = autoVolt + random(-10L, 11L) / 10.0f;
    return v < 0 ? 0 : v;
  }

  prevAuto = false;
  return readAvg(PIN_V, V_FULLSCALE);
}

//Relay_control

void setRelay(bool on) {
  relayOn = on;
  digitalWrite(PIN_RELAY, on ? RELAY_ON : RELAY_OFF);
}

Fault detect() {
  if (amps > cfg.oc * 2.0f) return F_SHORT;
  if (volts > cfg.ov)       return F_OVERV;
  if (volts < cfg.uv)       return F_UNDERV;
  if (amps > cfg.oc)        return F_OVERI;

  return F_NONE;
}

bool isSafe() {
  return volts < cfg.ov - HYST_V &&
         volts > cfg.uv + HYST_V &&
         amps < cfg.oc * 0.9f;
}

void trip(Fault f) {
  setRelay(false);
  activeFault = f;
  safeSince = 0;

  Serial.printf("!! TRIP: %s (V=%.1f I=%.2f)\n",
                faultName[f], volts, amps);
}

void protectionTask() {
  unsigned long now = millis();

  if (relayOn) {
    Fault f = detect();

    if (f == F_NONE) {
      candidate = F_NONE;
      return;
    }

    if (f != candidate) {
      candidate = f;
      candSince = now;
    }

    unsigned long delayMs =
      (f == F_SHORT) ? 0UL : (unsigned long)cfg.tripMs;

    if (now - candSince >= delayMs)
      trip(f);
  } else {
    if (cfg.reconSec > 0 &&
        activeFault != F_MANUAL &&
        isSafe()) {

      if (safeSince == 0)
        safeSince = now;

      if (now - safeSince >= (unsigned long)cfg.reconSec * 1000UL) {
        activeFault = F_NONE;
        candidate = F_NONE;
        setRelay(true);

        Serial.println("Auto-reconnect: supply is safe again");
      }
    } else {
      safeSince = 0;
    }
  }
}


bool manualReset() {
  if (relayOn) return true;
  if (!isSafe()) return false;

  activeFault = F_NONE;
  candidate = F_NONE;
  setRelay(true);

  Serial.println("Manual reconnect");
  return true;
}

// Buttons 

const uint8_t btnPins[4] = {
  PIN_SET, PIN_ACCEPT, PIN_PLUS, PIN_MINUS
};

bool btnLast[4] = {HIGH, HIGH, HIGH, HIGH};
unsigned long btnTime[4] = {0, 0, 0, 0};

bool pressed(uint8_t i) {
  bool current = digitalRead(btnPins[i]);

  if (current != btnLast[i] &&
      millis() - btnTime[i] > DEBOUNCE_MS) {

    btnTime[i] = millis();
    btnLast[i] = current;

    if (current == LOW)
      return true;
  }

  return false;
}

// 0 = no click, 1 = single click, 2 = double click. Debouncing has been used (20 ms)

int acceptClick() {
  static int clicks = 0;
  static unsigned long lastClick = 0;

  unsigned long now = millis();

  if (pressed(1)) {
    clicks++;
    lastClick = now;
  }

  if (clicks >= 2) {
    clicks = 0;
    return 2;
  }

  if (clicks == 1 && now - lastClick > DOUBLE_CLICK_MS) {
    clicks = 0;
    return 1;
  }

  return 0;
}

void nextPage() {
  page++;

  if (page > LAST_PAGE)
    page = 0;

  lcd.clear();
}

// On board control -> overrite to ROM 

void adjust(int dir) {
  switch (page) {
    case 1:
      ed.ov = constrain(ed.ov + 10 * dir, 100, 300);
      break;

    case 2:
      ed.uv = constrain(ed.uv + 10 * dir, 50, 250);
      break;

    case 3:
      ed.oc = constrain(ed.oc + dir, 1, 20);
      break;

    case 4:
      ed.tripMs = constrain(ed.tripMs + 20 * dir, 20, 1000);
      break;

    case 5:
      ed.reconSec = constrain(ed.reconSec + dir, 0, 60);
      break;

    case 6:
      ed.autoSrc = !ed.autoSrc;
      break;
  }
}

void buttonsTask() {
  bool bSet = pressed(0);
  bool bPlus = pressed(2);
  bool bMinus = pressed(3);
  int ac = acceptClick();

  if (page == 0) {
    if (bSet) {
      ed = cfg;
      page = 1;
      lcd.clear();
    }

    if (ac == 1 && !relayOn)
      flash(manualReset() ? "Reconnected" : "Supply not safe");

    return;
  }

  int dir = bPlus ? 1 : (bMinus ? -1 : 0);

  if (dir)
    adjust(dir);

  if (bSet) {
    ed = cfg;
    nextPage();
  }

  if (ac == 1) {
    if (validSettings(ed.ov, ed.uv, ed.oc,
                      ed.tripMs, ed.reconSec)) {

      cfg = ed;
      saveSettings();

      Serial.println("Settings saved (buttons)");
      flash("Saved");
    } else {
      ed = cfg;
      flash("Invalid! kept old");
    }

    nextPage();
  }

  if (ac == 2) {
    ed = cfg;
    page = 0;
    lcd.clear();
    flash("Cancelled");
  }
}


// Display


void lcdLine(int row, const char* message) {
  char buf[17];

  snprintf(buf, sizeof(buf), "%-16s", message);
  lcd.setCursor(0, row);
  lcd.print(buf);
}

void settingValue(char* val, size_t size) {
  switch (page) {
    case 1:
      snprintf(val, size, "%.0f V", ed.ov);
      break;

    case 2:
      snprintf(val, size, "%.0f V", ed.uv);
      break;

    case 3:
      snprintf(val, size, "%.0f A", ed.oc);
      break;

    case 4:
      snprintf(val, size, "%d ms", ed.tripMs);
      break;

    case 5:
      if (ed.reconSec)
        snprintf(val, size, "%d s", ed.reconSec);
      else
        snprintf(val, size, "MANUAL");
      break;

    default:
      snprintf(val, size, ed.autoSrc ? "AUTO rand" : "POT");
      break;
  }
}

void displayTask() {
  char l0[32], l1[32], val[16];

  if (page == 0) {
    snprintf(l0, sizeof(l0), "V:%5.1f I:%4.1f", volts, amps);

    if (relayOn) {
      snprintf(l1, sizeof(l1), "%-15s%c",
               "LOAD ON", cfg.autoSrc ? 'A' : 'P');
    } else {
      char t[20];

      snprintf(t, sizeof(t), "OFF:%s", faultName[activeFault]);
      snprintf(l1, sizeof(l1), "%-15s%c",
               t, cfg.autoSrc ? 'A' : 'P');
    }
  } else {
    const char* title[] = {
      "", "OVER-VOLT limit", "UNDER-VOLT limit",
      "OVER-CURR limit", "Trip delay",
      "Auto-reconnect", "Supply source"
    };

    settingValue(val, sizeof(val));

    snprintf(l0, sizeof(l0), "%d/6 %s", page, title[page]);
    snprintf(l1, sizeof(l1), "%-8sA:ok R:x", val);
  }

  if (millis() < flashUntil)
    snprintf(l1, sizeof(l1), "%s", flashMsg);

  lcdLine(0, l0);
  lcdLine(1, l1);
}

// Serial Communication

void printStatus() {
  Serial.printf("STATUS: V=%.1fV I=%.2fA RELAY=%s FAULT=%s\n",
                volts, amps,
                relayOn ? "ON" : "OFF",
                faultName[activeFault]);
}

void printSettings() {
  Serial.printf(
    "SETTINGS: OV=%.0fV UV=%.0fV OC=%.1fA DELAY=%dms "
    "RECON=%ds SRC=%s\n",
    cfg.ov, cfg.uv, cfg.oc, cfg.tripMs,
    cfg.reconSec, cfg.autoSrc ? "AUTO" : "POT"
  );
}

void printHelp() {
  Serial.println(F("Commands:"));
  Serial.println(F("STATUS | GET | RESET | RELAY OFF | STREAM ON/OFF | DEFAULTS"));
  Serial.println(F("SET OV <V> | SET UV <V> | SET OC <A> | SET DELAY <ms> | SET RECON <s>"));
  Serial.println(F("SIM AUTO (random voltage) | SIM POT (potentiometer)"));
}

void setSerialSetting(String key, float val) {
  Settings t = cfg;

  if      (key == "OV")    t.ov = val;
  else if (key == "UV")    t.uv = val;
  else if (key == "OC")    t.oc = val;
  else if (key == "DELAY") t.tripMs = (int)val;
  else if (key == "RECON") t.reconSec = (int)val;
  else {
    Serial.println("ERR: unknown setting");
    return;
  }

  if (!validSettings(t.ov, t.uv, t.oc, t.tripMs, t.reconSec)) {
    Serial.println("ERR: value out of range");
    return;
  }

  cfg = t;
  saveSettings();

  Serial.print("OK  ");
  printSettings();
}

void handleCommand(String line) {
  line.trim();
  line.toUpperCase();

  if (!line.length()) return;

  if (line == "HELP") {
    printHelp();
  } else if (line == "STATUS") {
    printStatus();
  } else if (line == "GET") {
    printSettings();
  } else if (line == "RESET") {
    Serial.println(manualReset()
      ? "OK: load connected"
      : "ERR: supply not safe yet");
  } else if (line == "RELAY OFF") {
    setRelay(false);
    activeFault = F_MANUAL;
    Serial.println("OK: relay off (manual)");
  } else if (line == "STREAM ON") {
    streaming = true;
    Serial.println("time_ms,V,I,relay");
  } else if (line == "STREAM OFF") {
    streaming = false;
    Serial.println("OK");
  } else if (line == "DEFAULTS") {
    cfg = {250, 180, 10, 120, 5, false};
    saveSettings();
    printSettings();
  } else if (line == "SIM AUTO") {
    cfg.autoSrc = true;
    saveSettings();
    Serial.println("OK: random voltage mode");
  } else if (line == "SIM POT") {
    cfg.autoSrc = false;
    saveSettings();
    Serial.println("OK: potentiometer mode");
  } else if (line.startsWith("SET ")) {
    int sp = line.indexOf(' ', 4);

    if (sp < 0) {
      Serial.println("ERR: usage SET <OV|UV|OC|DELAY|RECON> <value>");
      return;
    }

    String key = line.substring(4, sp);
    float val = line.substring(sp + 1).toFloat();

    setSerialSetting(key, val);
  } else {
    Serial.println("ERR: unknown command (type HELP)");
  }
}

void serialTask() {
  static String buf;

  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      handleCommand(buf);
      buf = "";
    } else if (buf.length() < 40) {
      buf += c;
    }
  }
}


void setup() {
  Serial.begin(115200);

  pinMode(PIN_RELAY, OUTPUT);

  for (int i = 0; i < 4; i++)
    pinMode(btnPins[i], INPUT_PULLUP);

  analogReadResolution(12);
  randomSeed(esp_random());

  loadSettings();
  ed = cfg;

  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();

  lcdLine(0, "Voltage Protector");
  lcdLine(1, "Starting...");
  delay(1000);
  lcd.clear();

  setRelay(false);

  volts = readVoltage();
  amps = readAvg(PIN_I, I_FULLSCALE);

  if (isSafe())
    setRelay(true);

  Serial.println("ESP32 Voltage Protector ready. Type HELP");
  printSettings();
}



void loop() {
  static unsigned long tSense = 0;
  static unsigned long tLcd = 0;
  static unsigned long tStream = 0;

  unsigned long now = millis();

  if (now - tSense >= 10) {
    tSense = now;

    volts = readVoltage();
    amps = readAvg(PIN_I, I_FULLSCALE);

    protectionTask();
  }

  buttonsTask();
  serialTask();

  if (now - tLcd >= 250) {
    tLcd = now;
    displayTask();
  }

  if (streaming && now - tStream >= 500) {
    tStream = now;

    Serial.printf("%lu,%.1f,%.2f,%d\n",
                  now, volts, amps, relayOn);
  }
}