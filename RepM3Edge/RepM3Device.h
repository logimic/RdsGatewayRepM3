#pragma once

#include "IqrfJsonApiMsg.h"
#include "HexStringCoversion.h"
#include "Trace.h"
#include <memory>

namespace lgmc {

  static const std::string REPM3_DEVICE_NAME("RepM3Device");

  enum class InstructionTypes {
    UNKNOWN = 0x00,
    STX = 0x02,
    ETX = 0x03,
    GET_STATE = 35,
    GET_ID = 95,
    IDENTIFICATION = 91,
    DATA_FROM_NODE = 68,
    ACKNOWLEDGE_TO_NODE = 97,
    SENSITIVITY = 69,
    FLUSHING_TIME = 84,
    USE_TIME = 85,
    SANITARY_FLUSHING_LENGTH = 105,
    SANITARY_FLUSHING_INTERVAL = 73,
    TURN_ON_OFF = 78,
    FORCED_FLUSHING_LENGTH = 111,
    FORCED_FLUSH = 79,
    FACTORY_SETTING = 65,
    FLUSH_BEFORE_AFTER = 70,
    MODE = 77,
    REMOTE_CONTROL = 82,
    REPLENISH = 114,
    REVERSE_RUN = 117,
    INVALID_INSTRUCTION = 67,
  };
  class InstructionTypesConvertTable
  {
  public:
    static const std::vector<std::pair<InstructionTypes, std::string>>& table()
    {
      static std::vector <std::pair<InstructionTypes, std::string>> table = {
        { InstructionTypes::UNKNOWN, "UNKNOWN" },
        { InstructionTypes::STX, "STX" },
        { InstructionTypes::ETX, "ETX" },
        { InstructionTypes::GET_STATE, "GET_STATE" },
        { InstructionTypes::GET_ID, "GET_ID" },
        { InstructionTypes::IDENTIFICATION, "IDENTIFICATION" },
        { InstructionTypes::DATA_FROM_NODE, "DATA_FROM_NODE" },
        { InstructionTypes::ACKNOWLEDGE_TO_NODE, "ACKNOWLEDGE_TO_NODE" },
        { InstructionTypes::SENSITIVITY, "SENSITIVITY" },
        { InstructionTypes::USE_TIME, "USE_TIME" },
        { InstructionTypes::TURN_ON_OFF, "TURN_ON_OFF" },
        { InstructionTypes::SANITARY_FLUSHING_LENGTH, "SANITARY_FLUSHING_LENGTH" },
        { InstructionTypes::SANITARY_FLUSHING_INTERVAL, "SANITARY_FLUSHING_INTERVAL" },
        { InstructionTypes::FORCED_FLUSHING_LENGTH, "FORCED_FLUSHING_LENGTH" },
        { InstructionTypes::FORCED_FLUSH, "FORCED_FLUSH" },
        { InstructionTypes::FLUSH_BEFORE_AFTER, "FLUSH_BEFORE_AFTER" },
        { InstructionTypes::FACTORY_SETTING, "FACTORY_SETTING" },
        { InstructionTypes::MODE, "MODE" },
        { InstructionTypes::REMOTE_CONTROL, "REMOTE_CONTROL" },
        { InstructionTypes::REPLENISH, "REPLENISH" },
        { InstructionTypes::REVERSE_RUN, "REVERSE_RUN" },
        { InstructionTypes::INVALID_INSTRUCTION, "INVALID_INSTRUCTION" },
      };
      return table;
    }
    static InstructionTypes defaultEnum()
    {
      return InstructionTypes::UNKNOWN;
    }
    static const std::string& defaultStr()
    {
      static std::string u("UNKNOWN");
      return u;
    }
  };
  typedef shape::EnumStringConvertor<InstructionTypes, InstructionTypesConvertTable> InstructionTypesConvert;

  ////////////////////////////////////
  const std::vector<uint8_t> cmdVec_DATA_FROM_NODE = {
    (uint8_t)InstructionTypes::STX
    , 4 //len
    , (uint8_t)InstructionTypes::DATA_FROM_NODE
    , (uint8_t)InstructionTypes::ETX
  };

  const std::vector<uint8_t> cmdVec_ACKNOWLEDGE_TO_NODE = {
    (uint8_t)InstructionTypes::STX
    , 4 //len
    , (uint8_t)InstructionTypes::ACKNOWLEDGE_TO_NODE
    , (uint8_t)InstructionTypes::ETX
  };

  const std::vector<uint8_t> cmdVec_GET_STATE = {
    (uint8_t)InstructionTypes::STX
    , 4 //len
    , (uint8_t)InstructionTypes::GET_STATE
    , (uint8_t)InstructionTypes::ETX
  };

  class RepM3Device
  {
  public:
    RepM3Device(const std::string& name, oegw::iqrfapi::Node node)
      : m_name(name)
      , m_node(node)
    {}

    virtual ~RepM3Device()
    {}

    void setActive()
    {
      m_isActive = true;
    }

    bool isActive() const
    {
      return m_isActive;
    }

    const std::string & getName() const
    {
      return m_name;
    }

    const oegw::iqrfapi::Node & getNode()
    {
      return m_node;
    }

    const std::vector<uint8_t> & getParamGetState() const
    {
      return m_paramGetState;
    }

    void setParamGetState(const std::vector<uint8_t> & paramGetState)
    {
      m_paramGetState = paramGetState;
    }

    const std::vector<uint8_t> & getDataFromNode() const
    {
      return m_dataFromNode;
    }

    void setDataFromNode(const std::vector<uint8_t> & dataFromNode)
    {
      m_dataFromNode = dataFromNode;
    }

    int m_lastGetStatePeriod = -1;
    int m_getStateCnt = 0; //for debug
    int m_dataFromNodeCnt = 0;

    void setCommand(uint8_t cmdNum, const std::vector<uint8_t> & cmd)
    {
      m_cmdMap[cmdNum] = cmd;
    }

    bool isCommand() { return m_cmdMap.size() > 0; }

    std::vector<uint8_t> fetchCommand()
    {
      std::vector<uint8_t> retval;
      if (isCommand()) {
        retval = m_cmdMap.begin()->second;
        m_cmdMap.erase(m_cmdMap.begin());
      }
      return retval;
    }

  private:
    std::string m_name;

    // read data from device by DATA_FROM_NODE
    std::vector<uint8_t> m_dataFromNode;

    // read param from device by GET_STATE
    std::vector<uint8_t> m_paramGetState;

    // buffered commands
    // cmd num is key
    typedef std::map<uint8_t, std::vector<uint8_t>> CmdMap;
    CmdMap m_cmdMap;

    bool m_isActive = false;
    
    // related IQRF node
    oegw::iqrfapi::Node m_node;

    std::chrono::system_clock::time_point m_lastUpdate;
  };
  typedef std::shared_ptr<RepM3Device> RepM3DeviceShPtr;
}
