#pragma once
#include <cstdint>

#ifdef PLATFORM_WINDOWS
  #include "windows/mock_can.h"
#elif defined(PLATFORM_LINUX)
  // NOTE: this header already defines struct CANMessage
  #include "platform/linux/socketcan.h"
#endif

class CANReceiver {
public:
    CANReceiver();
    ~CANReceiver();

    bool init();
    void update();

private:
    void* platform_data = nullptr; // MultiCan* on Linux, MockCANData* on Windows
};

