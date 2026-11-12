#pragma once
// Transport-only fake. Production BLE protocol/managers remain in this test.
#include "SPI.h"
#include <string.h>
#define BNRG_SPI_EXTI_PIN 2
#define BNRG_SPI_CS_PIN 10
#define BNRG_SPI_RESET_PIN 9
#define BLE_STATUS_SUCCESS 0
#define CONFIG_DATA_RANDOM_ADDRESS 0x80
#define GAP_PERIPHERAL_ROLE_IDB05A1 1
#define UUID_TYPE_128 1
#define PRIMARY_SERVICE 1
#define CHAR_PROP_WRITE 8
#define CHAR_PROP_WRITE_WITHOUT_RESP 4
#define CHAR_PROP_NOTIFY 16
#define ATTR_PERMISSION_NONE 0
#define GATT_NOTIFY_ATTRIBUTE_WRITE 1
#define AD_TYPE_COMPLETE_LOCAL_NAME 9
#define AD_TYPE_128_BIT_SERV_UUID_CMPLT_LIST 7
#define ADV_IND 0
#define STATIC_RANDOM_ADDR 1
#define NO_WHITE_LIST_USE 0
#define HCI_EVENT_PKT 4
#define EVT_DISCONN_COMPLETE 5
#define EVT_LE_META_EVENT 0x3E
#define EVT_LE_CONN_COMPLETE_SIZE 18
#define EVT_LE_CONN_COMPLETE 1
#define EVT_VENDOR 0xFF
#define EVT_BLUE_GATT_ATTRIBUTE_MODIFIED 0xC01
struct hci_uart_pckt { uint8_t type, data[255]; };
struct hci_event_pckt { uint8_t evt, plen, data[255]; };
struct evt_le_meta_event { uint8_t subevent, data[254]; };
struct evt_le_connection_complete { uint8_t status; uint16_t handle; };
struct evt_blue_aci { uint16_t ecode; uint8_t data[253]; };
struct evt_gatt_attr_modified_IDB05A1 {
  uint16_t conn_handle, attr_handle; uint8_t data_length; uint16_t offset; uint8_t att_data[20];
};
static unsigned mockBleFailStage = 0, mockBleCharCount = 0;
inline int mockBleReply(unsigned stage) {
  // Synchronous STBLE commands need the IRQ to enqueue their reply.
  return !mockBleIrqAttached ? 0xFF : mockBleFailStage == stage ? 0x42 : 0;
}
inline void HCI_Init() { mockBleCharCount = 0; }
inline void BNRG_SPI_Init() { mockBleIrqAttached = true; }
inline void BlueNRG_RST() {}
inline void HCI_Process() {}
inline int aci_hal_read_config_data(int, int, uint8_t *len, uint8_t *addr) {
  *len = 6; memset(addr, 0xC1, 6); return 0;
}
inline int aci_gatt_init() { return mockBleReply(2); }
inline int aci_gap_init_IDB05A1(int, int, int, uint16_t *a, uint16_t *b, uint16_t *c) {
  *a = 1; *b = 2; *c = 3; return mockBleReply(3);
}
inline int hci_le_set_random_address(uint8_t *) { return mockBleReply(4); }
inline int aci_gatt_update_char_value(uint16_t, uint16_t, int, int, uint8_t *) { return mockBleReply(5); }
inline int aci_gatt_add_serv(int, uint8_t *, int, int, uint16_t *handle) {
  *handle = 4; return mockBleReply(6);
}
inline int aci_gatt_add_char(uint16_t, int, uint8_t *, int, int, int, int, int, int, uint16_t *handle) {
  *handle = 5 + 3 * mockBleCharCount; return mockBleReply(7 + mockBleCharCount++);
}
inline int aci_hal_set_tx_power_level(int, int) { return mockBleReply(9); }
inline int hci_le_set_scan_resp_data(int, uint8_t *) { return mockBleReply(10); }
inline int aci_gap_set_discoverable(int, int, int, int, int, int, void *, int, uint8_t *, int, int) { return mockBleReply(11); }
inline int hci_disconnect(uint16_t, int) { return mockBleReply(12); }
