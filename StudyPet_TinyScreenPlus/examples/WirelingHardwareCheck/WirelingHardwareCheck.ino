// Standalone diagnostic. Upload separately from StudyPet, then restore
// StudyPet after testing. Requires TinyScreen and Wireling libraries.
#include <Wire.h>
#include <Wireling.h>
#include <TinyScreen.h>

TinyScreen display(TinyScreenPlus);
const uint8_t portPins[4] = {A0, A1, A2, A3};
uint8_t buzzerPort = 0, lastButtons = 0;
uint8_t deviceAddress[4] = {0, 0, 0, 0};
bool adapterFound = false;

bool responds(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

void scanPorts() {
  adapterFound = responds(0x70);
  SerialUSB.println("Scanning Wireling ports (I2C ACK is not device identification):");
  for (uint8_t port = 0; port < 4; ++port) {
    Wireling.selectPort(port);
    deviceAddress[port] = 0;
    SerialUSB.print("Port "); SerialUSB.print(port); SerialUSB.print(": ");
    for (uint8_t address = 1; address < 0x78; ++address) {
      if (address >= 0x70) continue; // exclude adapter multiplexers
      if (!responds(address)) continue;
      SerialUSB.print("0x"); SerialUSB.print(address, HEX); SerialUSB.print(" ");
      if (address == 0x29 || address == 0x68 || address == 0x69) deviceAddress[port] = address;
    }
    SerialUSB.println();
  }
}

void line(uint8_t y, const char *text) {
  display.setCursor(1, y); display.print(text);
}

void drawStatus() {
  char text[24], address[4][3];
  for (uint8_t i = 0; i < 4; ++i) {
    if (deviceAddress[i]) snprintf(address[i], 3, "%02X", deviceAddress[i]);
    else strcpy(address[i], "--");
  }
  display.clearScreen();
  display.fontColor(TS_8b_White, TS_8b_Black);
  line(0, adapterFound ? "WIRELING CHECK" : "MUX NOT FOUND");
  snprintf(text, sizeof text, "Buzzer: A%u", buzzerPort); line(12, text);
  snprintf(text, sizeof text, "0:%s  1:%s", address[0], address[1]); line(24, text);
  snprintf(text, sizeof text, "2:%s  3:%s", address[2], address[3]); line(34, text);
  line(44, "BL beep  TR port");
  line(54, "TL scan BR stop");
}

void setup() {
  SerialUSB.begin(115200);
  display.begin(); display.setBitDepth(TSBitDepth8); display.setBrightness(10);
  display.setFont(thinPixel7_10ptFontInfo);
  Wireling.begin();
  delay(250); // let connected modules power up
  scanPorts(); drawStatus();
}

void loop() {
  const uint8_t buttons = display.getButtons();
  const uint8_t pressed = buttons & ~lastButtons;
  lastButtons = buttons;
  if (pressed & TSButtonLowerLeft) tone(portPins[buzzerPort], 2700, 500);
  if (pressed & TSButtonUpperRight) {
    noTone(portPins[buzzerPort]);
    buzzerPort = (buzzerPort + 1) % 4;
    drawStatus();
  }
  if (pressed & TSButtonUpperLeft) { scanPorts(); drawStatus(); }
  if (pressed & TSButtonLowerRight) noTone(portPins[buzzerPort]);
  delay(25); // debounce; diagnostic only
}
