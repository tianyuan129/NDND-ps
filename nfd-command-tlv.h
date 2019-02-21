
#ifndef NFD_COMMAND_TLV_H
#define NFD_COMMAND_TLV_H

enum NFD_STATUS_CODE {
  OK = 200,
};

enum NFD_COMMAND_TLV_TYPE {
  CONTROL_PARAMETERS = 0x68,
  FACE_ID = 0x69,
  URI = 0x72,
  LOCAL_URI = 0x81,
  ORIGIN = 0x6f,
  COST = 0x6a,
  CAPACITY = 0x83,
  COUNT = 0x84,
  BASE_CONGESTION_MARKING_INTERVAL = 0x87,
  DEFAULT_CONGESTION_THRESHOLD = 0x88,
  MTU = 0x89,
  FLAGS = 0x6c,
  MASK = 0x70,
  STRATEGY = 0x6b,
  EXPIRATION_PERIOD = 0x6d,
  CONTROL_RESPONSE = 0x65,
  STATUS_CODE = 0x66,
  STATUS_TEXT = 0x67,
};

#endif // NFD_COMMAND_TLV_H
