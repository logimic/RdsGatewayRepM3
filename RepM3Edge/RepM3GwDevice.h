#pragma once

#include <vector>
#include <vector>

namespace lgmc {
  class RepM3GwDevice
  {
  public:
    RepM3GwDevice()
    {}

    virtual ~RepM3GwDevice()
    {}

    int m_dataFromNodePeriod = 6; // 120;
    int m_getStatePeriod = 10; // 1440;
    int m_detectDevicesPeriod = 10; // 120;
    int m_wakeUp = 10;
    int m_dataFromNodeLen = 8;
    bool m_sleep = false;
    std::vector<int> m_sensorHwpidVect = {4109};
    
    //not commanded
    int m_uartTimeout = 4;
    int m_lastDataFromNodePeriod = -1;
  };
}
