#include "BleLink.h"
#if STUDYPET_ENABLE_BLE
#include <SPI.h>
#include <STBLE.h>
#include <stddef.h>
#include <stdio.h>

// Hardware sequence/API based on TinyCircuits' STBLE UARTPassThrough example.
// ASD2116 is BlueNRG-MS (not a Nordic BLEPeripheral device).
static BleLink *activeLink = nullptr;
static bool radioPresent() {
  // Probe before attaching STBLE's IRQ handler. A floating IRQ on an absent
  // shield must not trap the local application inside the driver's ISR loop.
  detachInterrupt(BNRG_SPI_EXTI_PIN);
  pinMode(BNRG_SPI_CS_PIN, OUTPUT); digitalWrite(BNRG_SPI_CS_PIN, HIGH);
  pinMode(BNRG_SPI_RESET_PIN, OUTPUT); pinMode(BNRG_SPI_EXTI_PIN, INPUT_PULLDOWN);
  SPI.begin();
  BlueNRG_RST();
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  for (uint8_t attempt = 0; attempt < 100; ++attempt) {
    uint8_t header[5]; digitalWrite(BNRG_SPI_CS_PIN, LOW);
    header[0] = SPI.transfer(0x0B);
    for (uint8_t i = 1; i < 5; ++i) header[i] = SPI.transfer(0);
    digitalWrite(BNRG_SPI_CS_PIN, HIGH);
    if (header[0] == 0x02) { SPI.endTransaction(); return true; }
    delay(5);
  }
  SPI.endTransaction(); return false;
}

static void uartUuid(uint8_t *uuid, uint8_t kind) {
  static const uint8_t bytes[16] = {0x9E,0xCA,0xDC,0x24,0x0E,0xE5,0xA9,0xE0,
                                  0x93,0xF3,0xA3,0xB5,0x01,0x00,0x40,0x6E};
  memcpy(uuid, bytes, 16); uuid[12] = kind;
}

extern "C" void HCI_Event_CB(void *packet) {
  if (activeLink) activeLink->event(packet);
}

void BleLink::clearConnection() {
  connected_ = subscribed_ = initialState_ = closing_ = false;
  connection_ = 0; protocol_.reset();
}

bool BleLink::begin() {
  activeLink = this; ready_ = advertise_ = false; clearConnection(); error_ = 0;
  if (!radioPresent()) return check(1, 0xFF);
  HCI_Init(); BNRG_SPI_Init();
  // Keep the rising-edge interrupt installed by BNRG_SPI_Init. STBLE's
  // synchronous hci_send_req waits on its queue WITHOUT polling the IRQ;
  // detaching it here prevents startup command replies from being received.
  BlueNRG_RST();
  uint8_t address[6], length = 0;
  bool addressValid = aci_hal_read_config_data(CONFIG_DATA_RANDOM_ADDRESS, 6, &length, address) == BLE_STATUS_SUCCESS && length == 6;
  if (addressValid) {
    bool zeros = true, ones = true;
    for (uint8_t i = 0; i < 6; ++i) {
      const uint8_t mask = i == 5 ? 0x3F : 0xFF;
      zeros &= (address[i] & mask) == 0; ones &= (address[i] & mask) == mask;
    }
    addressValid = !zeros && !ones;
  }
  if (!addressValid) {
    // Stable private address from the TinyScreen+ SAMD21 silicon identifier.
    const uint32_t a = *(volatile const uint32_t *)0x0080A00CUL;
    const uint32_t b = *(volatile const uint32_t *)0x0080A040UL;
    for (uint8_t i = 0; i < 4; ++i) address[i] = a >> (i * 8);
    address[4] = b; address[5] = b >> 8;
  }
  address[5] |= 0xC0; // Bluetooth static-random address bits
  uint16_t gap, nameChar, appearance;
  if (!check(2, aci_gatt_init()) ||
      !check(3, aci_gap_init_IDB05A1(GAP_PERIPHERAL_ROLE_IDB05A1, 0, 8, &gap, &nameChar, &appearance)) ||
      !check(4, hci_le_set_random_address(address))) return false;
  uint8_t name[] = "StudyPet";
  if (!check(5, aci_gatt_update_char_value(gap, nameChar, 0, 8, name))) return false;
  uint8_t uuid[16]; uartUuid(uuid, 1);
  // Service + write declaration/value + notify declaration/value/CCCD = 6 records.
  if (!check(6, aci_gatt_add_serv(UUID_TYPE_128, uuid, PRIMARY_SERVICE, 7, &service_))) return false;
  uartUuid(uuid, 2); // ...0002: phone -> device, supports both ATT write modes
  if (!check(7, aci_gatt_add_char(service_, UUID_TYPE_128, uuid, 20,
      CHAR_PROP_WRITE | CHAR_PROP_WRITE_WITHOUT_RESP, ATTR_PERMISSION_NONE,
      GATT_NOTIFY_ATTRIBUTE_WRITE, 16, 1, &writeChar_))) return false;
  uartUuid(uuid, 3); // ...0003: device -> phone notifications
  if (!check(8, aci_gatt_add_char(service_, UUID_TYPE_128, uuid, 20, CHAR_PROP_NOTIFY,
      ATTR_PERMISSION_NONE, GATT_NOTIFY_ATTRIBUTE_WRITE, 16, 1, &notifyChar_))) return false;
  if (!check(9, aci_hal_set_tx_power_level(1, 3))) return false;
  ready_ = true;
  if (!advertise()) { ready_ = false; return false; }
  lastTelemetry_ = lastProgress_ = millis(); nextSend_ = millis();
  return true;
}

bool BleLink::check(uint8_t stage, int status) {
  if (status == BLE_STATUS_SUCCESS) return true;
  ready_ = false; error_ = static_cast<uint8_t>(status);
  snprintf(errorText_, sizeof errorText_, "E%02u:%02X", stage, error_);
  return false;
}

bool BleLink::advertise() {
  // BlueNRG adds both flags and TX-power AD fields automatically. Put the
  // name in scan response so the 128-bit service UUID fits within 31 bytes.
  uint8_t scanResponse[] = {9, AD_TYPE_COMPLETE_LOCAL_NAME,'S','t','u','d','y','P','e','t'};
  uint8_t services[17]; services[0] = AD_TYPE_128_BIT_SERV_UUID_CMPLT_LIST;
  uartUuid(services + 1, 1);
  if (!check(10, hci_le_set_scan_resp_data(sizeof(scanResponse), scanResponse))) return false;
  return check(11, aci_gap_set_discoverable(ADV_IND, 80, 160, STATIC_RANDOM_ADDR, NO_WHITE_LIST_USE,
                                         0, nullptr, sizeof(services), services, 0, 0));
}

const char *BleLink::statusText() const {
  if (!ready_) return errorText_;
  if (closing_) return "closing";
  if (connected_) return subscribed_ ? "connected" : "subscribe";
  return "ready";
}

void BleLink::poll(FeatureManager &features, Pet &pet) {
  if (!ready_) return;
  HCI_Process();
  const unsigned long now = millis();
  if (advertise_) {
    advertise_ = false;
    if (!advertise()) { ready_ = false; return; }
  }
  if (!connected_ || !subscribed_ || closing_) return;
  protocol_.tick(features, pet, now);
  if (initialState_ && protocol_.idle()) {
    protocol_.telemetry(features, pet, true); initialState_ = false; lastTelemetry_ = now;
  } else if ((unsigned long)(now - lastTelemetry_) >= 5000UL && protocol_.idle()) {
    protocol_.telemetry(features, pet, false); lastTelemetry_ = now;
  }
  if (!protocol_.pending()) { lastProgress_ = now; return; }
  if ((unsigned long)(now - lastProgress_) >= 15000UL) {
    // A stalled partial JSON response cannot be spliced into a later session.
    if (!check(12, hci_disconnect(connection_, 0x13))) { clearConnection(); }
    else { protocol_.reset(); subscribed_ = false; closing_ = true; }
    // Resume advertising only after the controller's disconnect-complete event.
    return;
  }
  if ((long)(now - nextSend_) < 0) return;
  nextSend_ = now + 10;
  uint8_t bytes[20]; const uint8_t count = protocol_.chunk(bytes);
  if (count && aci_gatt_update_char_value(service_, notifyChar_, 0, count, bytes) == BLE_STATUS_SUCCESS) {
    protocol_.sent(count); lastProgress_ = now;
  }
}

void BleLink::event(void *packet) {
  const hci_uart_pckt *uart = static_cast<hci_uart_pckt *>(packet);
  if (uart->type != HCI_EVENT_PKT) return;
  const hci_event_pckt *event = reinterpret_cast<const hci_event_pckt *>(uart->data);
  if (event->evt == EVT_DISCONN_COMPLETE) { clearConnection(); advertise_ = true; return; }
  if (event->evt == EVT_LE_META_EVENT && event->plen >= EVT_LE_CONN_COMPLETE_SIZE + 1) {
    const evt_le_meta_event *meta = reinterpret_cast<const evt_le_meta_event *>(event->data);
    if (meta->subevent == EVT_LE_CONN_COMPLETE) {
      const evt_le_connection_complete *cc = reinterpret_cast<const evt_le_connection_complete *>(meta->data);
      if (cc->status == 0) {
        clearConnection(); connected_ = true; connection_ = cc->handle;
        lastProgress_ = lastTelemetry_ = millis();
      }
    }
    return;
  }
  if (event->evt != EVT_VENDOR || event->plen < 2) return;
  const evt_blue_aci *blue = reinterpret_cast<const evt_blue_aci *>(event->data);
  if (blue->ecode != EVT_BLUE_GATT_ATTRIBUTE_MODIFIED || event->plen < 2 + offsetof(evt_gatt_attr_modified_IDB05A1, att_data)) return;
  const evt_gatt_attr_modified_IDB05A1 *attribute = reinterpret_cast<const evt_gatt_attr_modified_IDB05A1 *>(blue->data);
  if (attribute->conn_handle != connection_ || attribute->offset != 0 ||
      event->plen < 2 + offsetof(evt_gatt_attr_modified_IDB05A1, att_data) + attribute->data_length) return;
  if (attribute->attr_handle == notifyChar_ + 2 && attribute->data_length == 2) {
    const bool enabled = (attribute->att_data[0] & 1) != 0;
    if (enabled != subscribed_) {
      protocol_.reset(); subscribed_ = enabled; initialState_ = enabled;
      lastProgress_ = millis(); nextSend_ = millis();
    }
  } else if (connected_ && subscribed_ && attribute->attr_handle == writeChar_ + 1 && attribute->data_length <= 20) {
    protocol_.receive(attribute->att_data, attribute->data_length, millis());
  }
}
#else
bool BleLink::begin() { return false; }
void BleLink::poll(FeatureManager &, Pet &) {}
const char *BleLink::statusText() const { return "off"; }
void BleLink::event(void *) {}
void BleLink::clearConnection() {}
bool BleLink::advertise() { return false; }
#endif
