#ifndef STUDYPET_BLE_LINK_H
#define STUDYPET_BLE_LINK_H
#include "StudyPetConfig.h"
#ifndef STUDYPET_ENABLE_BLE
#define STUDYPET_ENABLE_BLE 1
#endif
#include "BleProtocol.h"

class BleLink {
public:
  bool begin();
  void poll(FeatureManager &features, Pet &pet);
  const char *statusText() const;
  uint8_t takeChanges() { return protocol_.takeChanges(); }
  void event(void *packet);
private:
  BleProtocol protocol_;
  uint16_t service_ = 0, writeChar_ = 0, notifyChar_ = 0, connection_ = 0;
  bool ready_ = false, connected_ = false, subscribed_ = false, advertise_ = false, initialState_ = false, closing_ = false;
  unsigned long lastTelemetry_ = 0, lastProgress_ = 0, nextSend_ = 0;
  uint8_t error_ = 0;
  char errorText_[8] = "E00:00";
  bool check(uint8_t stage, int status);
  bool advertise();
  void clearConnection();
};
#endif
