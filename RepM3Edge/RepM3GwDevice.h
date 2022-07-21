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

    int m_getFlagsPeriod = 6; // 120;
    int m_detectDevicesPeriod = 10; // 120;
    std::vector<int> m_sensorHwpidVect = {22};
    
    //not commanded
    int m_lastDataFromNodePeriod = -1;
  };
}
