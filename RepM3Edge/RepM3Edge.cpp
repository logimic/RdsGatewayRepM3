#ifdef TRC_CHANNEL
#undef TRC_CHANNEL
#endif
#define TRC_CHANNEL 43

#include "RepM3Device.h"
#include "RepM3GwDevice.h"

#include "HexStringCoversion.h"
#include "RepM3Edge.h"
#include "RepM3EdgeApi.h"

#include "repM3.h"
#include "repM3_provider.h"

#include "Trace.h"

#include "IqrfAdaptUtils.h"
#include "EmbedOs.h"
#include "EmbedUart.h"
#include "EmbedFRC.h"
#include "EmbedRAM.h"

#include "JsonMacro.h"
#include "DeviceApi.h"

#include "rapidjson/pointer.h"

#include "lgmc__RepM3Edge.hxx"

TRC_INIT_MODULE(lgmc::RepM3Edge);

using namespace oegw;

namespace lgmc {

  ////////////////////////
  class RepM3Edge::Imp
  {
  public:
    std::string m_instanceName;
    std::mutex m_workMtx;

    // services
    oegw::IIqrfAdapt* m_iIqrfAdapt = nullptr;
    shape::IConfigurationService* m_iConfigurationService = nullptr;
    shape::ILaunchService* m_iLaunchService = nullptr;
    shape::ICommandService* m_iCommandService = nullptr;
    oegw::IWorkerThread* m_iWorkerThread = nullptr;
    oegw::IIdentityProvider* m_iIdentityProvider = nullptr;
    oegw::IMsgService* m_iMsgService = nullptr;

    //UART write/read timeout
    const int UART_WR_TOUT = 6;

    //token from workerThread returned by addToLoop
    int m_processTaskToken = 0;

    //sensor name is key
    std::map<std::string, RepM3DeviceShPtr> m_RepM3DeviceShPtrMap;
    //mutex m_RepM3EdgeDeviceShPtrMap
    std::mutex m_deviceMtx;

    //debug purpose to count detect invoking
    int m_detectSensorDevices_cnt = 0;

    // mqtt to CLD
    std::string m_appPrefixTopic;
    std::string m_appGeneralRequestTopic;
    std::string m_appGeneralResponseTopic;
    std::string m_appGeneralNotifyTopic;

    RepM3GwDevice m_gatewayDevice;

  public:
    Imp()
    {
    }

    ~Imp()
    {
    }

    // detect new devices
    // device is detected, created a stored in internal m_RepM3DeviceShPtrMap and not passed to OegwControl as we have to know sensor type at 1st
    void detectSensorDevices(const std::chrono::system_clock::time_point & timePointNow)
    {
      TRC_FUNCTION_ENTER(PAR(m_detectSensorDevices_cnt++));

      std::lock_guard<std::mutex> lck(m_deviceMtx);

      auto allNodeMap = m_iIqrfAdapt->getNodes();

      //TODO
      const int REPM3_HWPID = 0022;

      std::map<unsigned, oegw::iqrfapi::Node> nadrSensorNodeMap;
      for (auto nodeIt : allNodeMap) {
        oegw::iqrfapi::Node & node = nodeIt.second;
        if (node.m_hwpid == REPM3_HWPID) {
          nadrSensorNodeMap.insert(std::make_pair(node.m_nadr, node));
        }
      }

      //release non existing devices (unbonded)
      auto sanIt = m_RepM3DeviceShPtrMap.begin();
      while (sanIt != m_RepM3DeviceShPtrMap.end()) {
        auto found = nadrSensorNodeMap.find(sanIt->second->getNode().m_nadr);
        if (found == nadrSensorNodeMap.end()) {
          m_RepM3DeviceShPtrMap.erase(sanIt++);
        }
        else {
          ++sanIt;
        }
      }

      //check and create detected devices
      for (auto nodeIt : nadrSensorNodeMap) {

        auto & node = nodeIt.second;

        std::ostringstream os;
        os << REPM3_DEVICE_NAME << '-' << node.m_nadr;
        std::string name = os.str();

        auto foundSensor = m_RepM3DeviceShPtrMap.find(name);
        if (foundSensor == m_RepM3DeviceShPtrMap.end()) {

          RepM3DeviceShPtr iqrfSensorDevice(shape_new RepM3Device(name, node));
          TRC_INFORMATION("Creating device: " << PAR(iqrfSensorDevice->getName()));
          m_RepM3DeviceShPtrMap.insert(std::make_pair(iqrfSensorDevice->getName(), iqrfSensorDevice));
        }
      }

      TRC_FUNCTION_LEAVE("")
    }

    bool isReplyOk(const std::vector<uint8_t> & reply) const
    {
      if (reply.size() >= 4) {
        if (reply[2] == 0) {
          return true;
        }
      }
      return false;
    }

    std::vector<uint8_t>  sendWriteRead(int nadr, const std::vector<uint8_t> & writeDataVec)
    {
      const int uartTimeout = 4;

      std::string instructionType = InstructionTypesConvert::int2str(writeDataVec[2]);
      TRC_FUNCTION_ENTER(">>> " << PAR(nadr) << PAR(instructionType));

      std::vector<uint8_t> retval;

      iqrfapi::embed::uart::WriteReadPtr writeRead(shape_new iqrfapi::embed::uart::WriteRead(
        nadr, 0xffff, m_gatewayDevice.m_uartTimeout, writeDataVec));

      writeRead->setInstanceMsgId();
      writeRead->setVerbose(true);

      m_iIqrfAdapt->executeMsgTransaction(writeRead, 120000);
      retval = writeRead->getReadData();

      TRC_FUNCTION_LEAVE("<<< " << PAR(nadr) << PAR(instructionType));
      return retval;
    }

    std::vector<uint8_t>  readRam(int nadr)
    {
      TRC_FUNCTION_ENTER(">>> " << PAR(nadr));

      std::vector<uint8_t> retval;

      iqrfapi::embed::ram::ReadPtr readRamMsg(shape_new iqrfapi::embed::ram::Read(nadr, 0xFFFF, 0, 24));

      readRamMsg->setInstanceMsgId();
      readRamMsg->setVerbose(true);

      m_iIqrfAdapt->executeMsgTransaction(readRamMsg, 120000);
      retval = readRamMsg->getReadData();

      TRC_FUNCTION_LEAVE("<<< " << PAR(nadr));
      return retval;
    }

    // regular actions - sensor frc, polling, it respect device sleeping, etc
    void sendFrc(const std::chrono::system_clock::time_point & timePointNow)
    {
      TRC_FUNCTION_ENTER("");
      //handleAllEvented();

      using namespace std::chrono;

      bool get_data_from_node = false;

      auto time = std::chrono::system_clock::to_time_t(timePointNow);
      auto tm = *std::localtime(&time);
      int minutes_since_day = tm.tm_hour * 60 + tm.tm_min;

      std::lock_guard<std::mutex> lck(m_deviceMtx);

      int act_period_data_from_node = minutes_since_day / m_gatewayDevice.m_dataFromNodePeriod;

      if (m_gatewayDevice.m_lastDataFromNodePeriod != act_period_data_from_node) {
        m_gatewayDevice.m_lastDataFromNodePeriod = act_period_data_from_node;
        get_data_from_node = true;
      }

      //broadcast
      //if (get_data_from_node) {
      //  sendWriteRead(0xFF, m_midSensorShPtrMap.begin()->second, cmdVec_DATA_FROM_NODE);
      //}

      for (auto sanIt : m_RepM3DeviceShPtrMap) {

        RepM3DeviceShPtr & san = sanIt.second;
        int nadr = san->getNode().m_nadr;

        int act_period_get_state = minutes_since_day / m_gatewayDevice.m_getStatePeriod;
        if (san->m_lastGetStatePeriod != act_period_get_state) {
          TRC_DEBUG(PAR(nadr) << PAR(san->m_getStateCnt++) << PAR(san->m_lastGetStatePeriod) << PAR(act_period_get_state));
          //TODO check wake_up time
          san->m_lastGetStatePeriod = act_period_get_state;

          try {
            san->setParamGetState(sendWriteRead(nadr, cmdVec_GET_STATE));
          }
          catch (iqrfapi::IqrfError & e) {
            CATCH_EXC_TRC_WAR(iqrfapi::IqrfError, e, "Cannot get state: " << NAME_PAR(NADR, san->getNode().m_nadr));
          }
        }

        if (get_data_from_node) {
          TRC_DEBUG(PAR(san->m_dataFromNodeCnt++) << PAR(nadr));
          //TODO check wake_up time
          //TODO read by FRC
          try {
            san->setDataFromNode(sendWriteRead(nadr, cmdVec_DATA_FROM_NODE));
            
            //send ACK
            auto res = sendWriteRead(nadr, cmdVec_ACKNOWLEDGE_TO_NODE);
            std::string resStr = encodeBinary(res.data(), (int)res.size());
            TRC_DEBUG(PAR(nadr) << "ACK: " << resStr);
            
            TRC_DEBUG(PAR(nadr) << NAME_PAR(san->getDataFromNode(), encodeBinary(san->getDataFromNode().data(), san->getDataFromNode().size())));

            auto readRamRes = readRam(nadr);
            auto readRamResStr = encodeBinary(readRamRes.data(), readRamRes.size());
            TRC_DEBUG(PAR(nadr) << PAR(readRamResStr));
          }
          catch (iqrfapi::IqrfError & e) {
            CATCH_EXC_TRC_WAR(iqrfapi::IqrfError, e, "Cannot read data: " << NAME_PAR(NADR, san->getNode().m_nadr));
          }
        }

        //while (san->isCommand()) {
        //  auto cmd = san->fetchCommand();
        //  TRC_INFORMATION("send cmd: " << PAR(nadr) << NAME_PAR(cmd, encodeBinary(cmd.data(), cmd.size())));
        //  auto reply = sendWriteRead(nadr, cmd);
        //  if (isReplyOk(reply)) {
        //    TRC_DEBUG("Reply OK")
        //  }
        //  else {
        //    TRC_INFORMATION("reply on cmd is not confirmed: "
        //      << PAR(nadr) << NAME_PAR(cmd, encodeBinary(cmd.data(), cmd.size())) << NAME_PAR(cmd, encodeBinary(reply.data(), reply.size())));
        //  }
        //}

        //std::set<int> nadrSet;
        //for (auto sanIt : m_midSensorShPtrMap) {
        //  SensorDeviceShPtr & san = sanIt.second;
        //  int nadr = san->getNode().m_nadr;
        //  nadrSet.insert(nadr);
        //}

        //getDataAndAck(nadrSet);
      }

      //TODO watch command and update commanded sensors
      if (!m_gatewayDevice.m_sleep || (m_gatewayDevice.m_sleep && get_data_from_node)) {
        commitCommands();
      }

      if (get_data_from_node) { //TODO or command
        repm3api::DeviceDataMsg msg;

        msg.setGateway(m_gatewayDevice);
        msg.setRepM3Devices(m_RepM3DeviceShPtrMap);
        msg.setTime(timePointNow);
        // set msgId to timestamp
        msg.setMsgId(std::to_string(std::chrono::duration_cast<std::chrono::seconds>(timePointNow.time_since_epoch()).count()));

        rapidjson::Document resDoc;
        msg.encodeResponse(resDoc, resDoc.GetAllocator());

        std::string notifyTopic = m_appGeneralNotifyTopic + msg.getType();
        m_iMsgService->publish(notifyTopic, resDoc, 1);

        rapidjson::StringBuffer buffer;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
        resDoc.Accept(writer);
        std::string msgStr = buffer.GetString();
        TRC_DEBUG("repm3api::DeviceDataMsg: " << std::endl << msgStr);
      }

      TRC_FUNCTION_LEAVE("");
    }

#if 0
    void getDataAndAck(std::set<int> nadrSet)
    {
      TRC_FUNCTION_ENTER("");

      //TODO read by FRC
      try {
        std::set<int> unreachableNodes = nadrSet;
        std::set<int> ackNodes;

        {
          const uint8_t pnum = 0x0C; // uart
          const uint8_t pcmd = 0x02; // ReadWrite
          uint16_t hwpid = 0xFFFF; //Hwpid
          std::vector<uint8_t> cmdVec = { 4 }; //prepend wait 40 ms when sent by selectiveBatch
          cmdVec.insert(cmdVec.end(), cmdVec_DATA_FROM_NODE.begin(), cmdVec_DATA_FROM_NODE.end());

          iqrfapi::embed::frc::AcknowledgedBroadcastBitsPtr frcMsg(shape_new iqrfapi::embed::frc::AcknowledgedBroadcastBits(nadrSet, pnum, pcmd, hwpid, cmdVec));

          std::ostringstream os;
          os << frcMsg->getMsgId() << "-extra";
          iqrfapi::embed::frc::ExtraResultPtr extraMsg(shape_new iqrfapi::embed::frc::ExtraResult());
          extraMsg->setMsgId(os.str());

          m_iIqrfAdapt->executeMsgTransaction(frcMsg, 60000);
          m_iIqrfAdapt->executeMsgTransaction(extraMsg, 3000);

          frcMsg->getFrcDataAs2bit(ackNodes, extraMsg->getFrcData());

          unreachableNodes = oegw::iqrfadapt::substractSet(nadrSet, ackNodes);

          unreachableNodes = oegw::iqrfadapt::substractSet(nadrSet, ackNodes);

          //trace unreachable
          if (unreachableNodes.size() > 0) {
            std::ostringstream os;
            for (int node : unreachableNodes) {
              os << node << ", ";
            }
            TRC_WARNING("DATA_FROM_NODE is not delivered to unreachable nodes: " << os.str())
          }
        }

        //////////////////////////////////
        {
          const uint8_t pnum = 0x05; // RAM
          const uint8_t pcmd = 0x00; // read
          uint16_t hwpid = 0xFFFF; //Hwpid
          std::vector<uint8_t> cmdVec = { 0, 4 };

          //read 1st 4-bytes
          //MemoryRead4B(const std::set<int> & selectedNodes, uint16_t address, uint8_t pnum, uint8_t pcmd, bool inc, std::vector<uint8_t> pdata = std::vector<uint8_t>())
          std::shared_ptr<iqrfapi::embed::frc::MemoryRead4B> frcMsgPtr(
            shape_new iqrfapi::embed::frc::MemoryRead4B(nadrSet, 0x4A0, pnum, pcmd, true, cmdVec));

          std::ostringstream os;
          os << frcMsgPtr->getMsgId() << "-extra";
          iqrfapi::embed::frc::ExtraResultPtr extraMsgPtr(shape_new iqrfapi::embed::frc::ExtraResult());
          extraMsgPtr->setMsgId(os.str());

          m_iIqrfAdapt->executeMsgTransaction(frcMsgPtr, 60000);
          m_iIqrfAdapt->executeMsgTransaction(extraMsgPtr, 3000);

          std::map<int, uint32_t> resultMap1;
          frcMsgPtr->getFrcDataAs(resultMap1, extraMsgPtr->getFrcData());

          //debug TRC
          std::ostringstream os2;
          for (auto & it : resultMap1) {
            uint32_t val = it.second;
            os2 << NAME_PAR(nadr, it.first) << ": " << encodeBinary((uint8_t*)&val, sizeof(val)) << std::endl;
          }
          TRC_DEBUG(os2.str());
        }
        
        //unreachableNodes = oegw::iqrfadapt::substractSet(nadrSet, ackNodes);

        ////trace unreachable
        //if (unreachableNodes.size() > 0) {
        //  std::ostringstream os;
        //  for (int node : unreachableNodes) {
        //    os << node << ", ";
        //  }
        //  TRC_WARNING("DATA_FROM_NODE is not delivered to unreachable nodes: " << os.str())
        //}

        ////MemoryRead4B(const std::set<int> & selectedNodes, uint16_t address, uint8_t pnum, uint8_t pcmd, bool inc, std::vector<uint8_t> pdata = std::vector<uint8_t>())
        //std::shared_ptr<iqrfapi::embed::frc::MemoryRead4B> MemoryRead4BPtr AckMsg(ackNodes, 0x620);

        ////trace and update acknowledged
        //if (ackNodes.size() > 0) {
        //  std::ostringstream os;
        //  for (auto nadr : ackNodes) {
        //    auto found = nadrSanMap.find(nadr);
        //    if (found != nadrSanMap.end()) {
        //      found->second->setReadHourDayCtxThing(ctx);
        //      os << nadr << ", ";
        //    }
        //    else {
        //      TRC_WARNING("Inconsistent FRC result for: " << PAR(nadr));
        //    }
        //  }
        //  TRC_INFORMATION("RECORD for " << PAR(day) << PAR(hourInterval) << " written to nodes: " << os.str())
        //}
      }
      catch (iqrfapi::IqrfError & e) {
        //CATCH_EXC_TRC_WAR(iqrfapi::IqrfError, e, "Cannot read data: " << NAME_PAR(NADR, san->getNode().m_nadr));
        CATCH_EXC_TRC_WAR(iqrfapi::IqrfError, e, "Cannot read data: ");
      }

      TRC_FUNCTION_LEAVE("");
    }
#endif

    // must be aready mutexed
    void commitCommands()
    {
      for (auto sanIt : m_RepM3DeviceShPtrMap) {

        RepM3DeviceShPtr & san = sanIt.second;
        int nadr = san->getNode().m_nadr;
        while (san->isCommand()) {
          auto cmd = san->fetchCommand();
          TRC_INFORMATION("send cmd: " << PAR(nadr) << NAME_PAR(cmd, encodeBinary(cmd.data(), cmd.size())));
          auto reply = sendWriteRead(nadr, cmd);
          if (isReplyOk(reply)) {
            TRC_DEBUG("Reply OK")
          }
          else {
            TRC_INFORMATION("reply on cmd is not confirmed: "
              << PAR(nadr) << NAME_PAR(cmd, encodeBinary(cmd.data(), cmd.size())) << NAME_PAR(cmd, encodeBinary(reply.data(), reply.size())));
          }
        }
      }
    }

    //--------------------------------------------------
    // CLD MQTT
    void handleApiMessage(const std::string& topic, const rapidjson::Document & reqMsg, const std::string & conId)
    {
      using namespace rapidjson;

      std::string msgType;
      Document rspDoc;
      auto &a = rspDoc.GetAllocator();
      ErrMsg errMsg;

      std::string responseTopic = m_appGeneralResponseTopic;

      try {

        if (reqMsg.HasParseError()) {
          //TODO parse error handling => send back an error JSON with details
          THROW_EXC_TRC_WAR(std::logic_error, "Json parse error: " << NAME_PAR(emsg, reqMsg.GetParseError()) <<
            NAME_PAR(eoffset, reqMsg.GetErrorOffset()));
        }

        const Value *v = Pointer("/mType").Get(reqMsg);
        if (!(v && v->IsString())) {
          THROW_EXC_TRC_WAR(std::logic_error, "Expected /mType");
        }
        msgType = v->GetString();
        responseTopic += msgType;

        //TODO validation

        //handling
        switch (repm3api::MTypeConvert::str2enum(msgType)) {

        case repm3api::MType::SensorCmd:
          handleSensorCmdMsg(reqMsg, rspDoc, a);
          break;
        case repm3api::MType::GatewayCmd:
          handleGatewayCmdMsg(reqMsg, rspDoc, a);
          break;
        default:
          THROW_EXC_TRC_WAR(std::logic_error, "Unsupported msg: " << PAR(msgType));
        }
      }
      catch (std::logic_error &e) {
        CATCH_EXC_TRC_WAR(std::logic_error, e, "Error: ");
        errMsg.setCode(-1);
        std::ostringstream os;
        os << "Error: " << e.what();
        errMsg.setReason(os.str());
        errMsg.encodeResponse(rspDoc, a);
        responseTopic += errMsg.getType();
      }

      m_iMsgService->publish(responseTopic, rspDoc, 1);

      TRC_FUNCTION_LEAVE("");
    }

    void handleSensorCmdMsg(const rapidjson::Value & reqVal, rapidjson::Value &rspVal, rapidjson::Document::AllocatorType & a)
    {
      TRC_FUNCTION_ENTER("");
      repm3api::SensorCmdMsg msg;

      // init maintained msg object to be ready for async response (copy replaces previous data)
      msg.parseRequest(reqVal);

      std::vector<std::string> sensorDeviceNames = msg.getSensorDeviceNames();


      std::lock_guard<std::mutex> lck(m_deviceMtx);

      const int commandNumPosition = 2;

      for (const std::string & name : sensorDeviceNames) {
        auto foundSensor = m_RepM3DeviceShPtrMap.find(name);
        if (foundSensor != m_RepM3DeviceShPtrMap.end()) {
          RepM3Device & san = *foundSensor->second;
          san.setCommand(msg.getCommandData()[commandNumPosition], msg.getCommandData());
        }
      }

      // now we ready => send 1st sync resp
      msg.encodeResponse(rspVal, a);


      TRC_FUNCTION_LEAVE("");
    }

    void handleGatewayCmdMsg(const rapidjson::Value & reqVal, rapidjson::Value &rspVal, rapidjson::Document::AllocatorType & a)
    {
      TRC_FUNCTION_ENTER("");
      repm3api::GatewayCmdMsg msg;

      // init maintained msg object to be ready for async response (copy replaces previous data)
      msg.parseRequest(reqVal);

      repm3api::GatewayItem & gw = msg.getGatewayItem();
      
      std::lock_guard<std::mutex> lck(m_deviceMtx);

      if (gw.getDataFromNodePeriodPtr())
        m_gatewayDevice.m_dataFromNodePeriod = *gw.getDataFromNodePeriodPtr();
      if (gw.getGetStatePeriodPtr())
        m_gatewayDevice.m_getStatePeriod = *gw.getGetStatePeriodPtr();
      if (gw.getDetectDevicesPeriodPtr())
        m_gatewayDevice.m_detectDevicesPeriod = *gw.getDetectDevicesPeriodPtr();
      if (gw.getWakeUpPtr())
        m_gatewayDevice.m_wakeUp = *gw.getWakeUpPtr();
      if (gw.getDataFromNodeLenPtr())
        m_gatewayDevice.m_dataFromNodeLen = *gw.getDataFromNodeLenPtr();
      if (gw.getSleepPtr())
        m_gatewayDevice.m_sleep = *gw.getSleepPtr();
      if (gw.getSensorHwpidVectPtr())
        m_gatewayDevice.m_sensorHwpidVect = *gw.getSensorHwpidVectPtr();

      // now we ready => send 1st sync resp
      msg.encodeResponse(rspVal, a);


      TRC_FUNCTION_LEAVE("");
    }

    std::vector<std::string> getAllRepM3EdgeDevicesNames()
    {
      std::vector<std::string> retval;
      std::lock_guard<std::mutex> lck(m_deviceMtx);
      for (auto sanIt : m_RepM3DeviceShPtrMap) {
        retval.push_back(sanIt.second->getName());
      }
      return retval;
    }

    void subscribeAppTopics()
    {
      TRC_FUNCTION_ENTER("");
      m_iMsgService->subscribe(m_appGeneralRequestTopic, 0,
        [&](const std::string& topic, const rapidjson::Document & msg, const std::string & clientId) { handleApiMessage(topic, msg, clientId); }
      );
      TRC_FUNCTION_LEAVE("")
    }

    void unsubscribeAppTopics()
    {
      m_iMsgService->unsubscribe(m_appGeneralRequestTopic);
    }

    void initApiTopics()
    {
      {
        std::ostringstream os;

        const auto & par = m_iIdentityProvider->getParams();

        if (par.m_devStage == "dev4") {
          //dev4
          os << m_iMsgService->getTopicPrefix() << '/'
            << m_iLaunchService->getAppName();
        }
        else {
          THROW_EXC_TRC_WAR(std::logic_error, "The only dev4 topic stage is supported")
        }

        m_appPrefixTopic = os.str();
      }

      m_appGeneralRequestTopic = m_appPrefixTopic + "/req/#";
      m_appGeneralResponseTopic = m_appPrefixTopic + "/rsp/";
      m_appGeneralNotifyTopic = m_appPrefixTopic + "/ntf/";
    }

    /////////////////////////////////////////////
    // Test CMDL  ///////////////////////////////
    /////////////////////////////////////////////
    class TestCommCommand : public shape::ICommand
    {
    public:
      TestCommCommand() = delete;
      TestCommCommand(RepM3Edge::Imp* imp)
        :m_imp(imp)
      {
      }

      void usage(std::ostringstream& ostr)
      {
        ostr <<
          std::left << std::setw(20) << "sn h" << "test comm help" << std::endl <<
          std::left << std::setw(20) << "sn l" << "list detected sensors" << std::endl <<
          std::left << std::setw(20) << "sn p <nadr> <num> <val>" << "set NADR (-1 ... all) parameter number, value" << std::endl <<
          std::left << std::setw(20) << "sn f <nadr>" << "forced flush of NADR (-1 ... all)" << std::endl <<
          std::left << std::setw(20) << "sn i <nadr>" << "identification of NADR (-1 ... all)" << std::endl <<
          std::left << std::setw(20) << "sn gw <sleep>" << "cmd gw to swith sleep on/off (1/0)" << std::endl <<
          std::endl;
      }

      std::string doCmd(const std::string& params) override
      {
        TRC_FUNCTION_ENTER("");

        std::string cmd;
        std::string subcmd;
        std::istringstream istr(params);
        std::ostringstream ostr;

        istr >> cmd >> subcmd;

        TRC_DEBUG("process: " << PAR(subcmd));

        //////////////
        if (subcmd == "h") {
          usage(ostr);
        }
        //////////////
        else if (subcmd == "l") {
          //list sensors
          try {
            auto sensList = m_imp->getAllRepM3EdgeDevicesNames();
            std::string dotstr;
            istr >> dotstr;

            for (auto & sen : sensList) {
              ostr << std::endl << sen;
            }
            ostr << std::endl;
          }
          catch (std::exception& e) {
            ostr << e.what();
          }
        }
        else if (subcmd == "p") {
          //cmd to set parameter
          try {
            int nadr, num, val;
            istr >> nadr >> num >> val;

            rapidjson::Document reqDoc;
            auto &a = reqDoc.GetAllocator();
            repm3api::SensorCmdMsg cmdMsg;
            cmdMsg.setMsgId("test-cmd");

            if (nadr < 0) {
              //get all
              cmdMsg.setSensorDeviceNames(m_imp->getAllRepM3EdgeDevicesNames());
            }
            else {
              std::ostringstream os;
              os << REPM3_DEVICE_NAME << '-' << nadr;
              std::string name = os.str();
              cmdMsg.setSensorDeviceNames({ os.str() });
            }

            std::vector<uint8_t> cmd = { 2, 5, 0, 0, 3 };
            cmd[2] = num;
            cmd[3] = val;
            cmdMsg.setCommandData(cmd);

            cmdMsg.encodeRequest(reqDoc, a);

            rapidjson::StringBuffer buffer;
            rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
            reqDoc.Accept(writer);
            std::string msgStr = buffer.GetString();

            TRC_DEBUG("reqDoc: " << std::endl << msgStr);

            m_imp->handleApiMessage("", reqDoc, "");

            //updated value
            ostr << "send cmd: " << encodeBinary(cmd.data(), cmd.size());
            ;
          }
          catch (std::exception& e) {
            ostr << e.what();
          }
        }
        else if (subcmd == "f") {
          //forced flush
          try {
            int nadr;
            istr >> nadr;

            rapidjson::Document reqDoc;
            auto &a = reqDoc.GetAllocator();
            repm3api::SensorCmdMsg cmdMsg;
            cmdMsg.setMsgId("test-flush");

            if (nadr < 0) {
              //get all
              cmdMsg.setSensorDeviceNames(m_imp->getAllRepM3EdgeDevicesNames());
            }
            else {
              std::ostringstream os;
              os << REPM3_DEVICE_NAME << '-' << nadr;
              std::string name = os.str();
              cmdMsg.setSensorDeviceNames({ os.str() });
            }

            std::vector<uint8_t> cmd = { 2, 4, 79, 3 };
            cmdMsg.setCommandData(cmd);

            cmdMsg.encodeRequest(reqDoc, a);

            rapidjson::StringBuffer buffer;
            rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
            reqDoc.Accept(writer);
            std::string msgStr = buffer.GetString();

            TRC_DEBUG("reqDoc: " << std::endl << msgStr);

            m_imp->handleApiMessage("", reqDoc, "");

            //updated value
            ostr << "send cmd: " << encodeBinary(cmd.data(), cmd.size());
            ;
          }
          catch (std::exception& e) {
            ostr << e.what();
          }
        }
        else if (subcmd == "i") {
          //forced flush
          try {
            int nadr;
            istr >> nadr;

            rapidjson::Document reqDoc;
            auto &a = reqDoc.GetAllocator();
            repm3api::SensorCmdMsg cmdMsg;
            cmdMsg.setMsgId("test-identification");

            if (nadr < 0) {
              //get all
              cmdMsg.setSensorDeviceNames(m_imp->getAllRepM3EdgeDevicesNames());
            }
            else {
              std::ostringstream os;
              os << REPM3_DEVICE_NAME << '-' << nadr;
              std::string name = os.str();
              cmdMsg.setSensorDeviceNames({ os.str() });
            }

            std::vector<uint8_t> cmd = { 2, 4, 91, 3 };
            cmdMsg.setCommandData(cmd);

            cmdMsg.encodeRequest(reqDoc, a);

            rapidjson::StringBuffer buffer;
            rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
            reqDoc.Accept(writer);
            std::string msgStr = buffer.GetString();

            TRC_DEBUG("reqDoc: " << std::endl << msgStr);

            m_imp->handleApiMessage("", reqDoc, "");

            //updated value
            ostr << "send cmd: " << encodeBinary(cmd.data(), cmd.size());
            ;
          }
          catch (std::exception& e) {
            ostr << e.what();
          }
        }
        else if (subcmd == "gw") {
          //forced flush
          try {
            int sleep;
            istr >> sleep;

            rapidjson::Document reqDoc;
            auto &a = reqDoc.GetAllocator();
            repm3api::GatewayCmdMsg cmdMsg;
            cmdMsg.setMsgId("gw-sleep");

            auto & sh = cmdMsg.getGatewayItem().getSleepPtr();
            sh = std::make_shared<bool>(sleep == 0 ? false : true);

            cmdMsg.encodeRequest(reqDoc, a);

            rapidjson::StringBuffer buffer;
            rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
            reqDoc.Accept(writer);
            std::string msgStr = buffer.GetString();

            TRC_DEBUG("reqDoc: " << std::endl << msgStr);

            m_imp->handleApiMessage("", reqDoc, "");

            //updated value
            ostr << "send cmd to sleep: " << sleep;
            ;
          }
          catch (std::exception& e) {
            ostr << e.what();
          }
        }
        else {
          ostr << "usage: " << std::endl;
          usage(ostr);
        }

        TRC_FUNCTION_LEAVE("");
        return ostr.str();
      }

      std::string getHelp() override
      {
        return "Test comm simulation. Type h for help";
      }

      ~TestCommCommand() {}
    private:
      RepM3Edge::Imp* m_imp = nullptr;
    };

    ///////////////////////////////////////
    ///////////////////////////////////////
    class TestCommCommand2 : public shape::ICommand
    {
    public:
      TestCommCommand2() = delete;
      TestCommCommand2(RepM3Edge::Imp* imp)
        :m_imp(imp)
      {
      }

      void usage(std::ostringstream& ostr)
      {
        ostr <<
          std::left << std::setw(20) << "mc h" << "test comm help" << std::endl <<
          //std::left << std::setw(20) << "dm v n <+|-|value>" << "get/set value n" << std::endl <<
          std::left << std::setw(20) << "mc ver" << "get version" << std::endl <<
          std::left << std::setw(20) << "mc gt" << "get time" << std::endl <<
          std::left << std::setw(20) << "mc st <sec> <sc>" << "set time, sec: rel in secnds to actual, sc: security" << std::endl <<
          std::left << std::setw(20) << "mc pst <sec>" << "preset time, sec: rel in secnds to actual" << std::endl <<
          std::left << std::setw(20) << "mc cpst <sc>" << "change to preset time, , sc: security" << std::endl <<
          std::endl;
      }

      std::string doCmd(const std::string& params) override
      {
        TRC_FUNCTION_ENTER("");

        std::string cmd;
        std::string subcmd;
        std::istringstream istr(params);
        std::ostringstream ostr;

        istr >> cmd >> subcmd;

        TRC_DEBUG("process: " << PAR(subcmd));

        //////////////
        if (subcmd == "h") {
          usage(ostr);
        }
        //////////////
        else if (subcmd == "ver") {
          try {
            std::string dotstr;
            istr >> dotstr;

            auto ver = m_imp->getVersion();

            //updated value
            ostr << "version: "
              << ver
              ;
          }
          catch (std::exception& e) {
            ostr << e.what();
          }
        }
        else if (subcmd == "gt") {
          try {
            std::string dotstr;
            istr >> dotstr;

            auto tp = m_imp->getTime();

            //updated value
            ostr << "get time: "
              << encodeTimestamp(tp)
              ;
          }
          catch (std::exception& e) {
            ostr << e.what();
          }
        }
        else if (subcmd == "st") {
          try {
            int shift;
            istr >> shift;

            int sec;
            istr >> sec;

            auto tp = std::chrono::system_clock::now();
            tp += std::chrono::seconds(shift);

            int ret = m_imp->setTime(tp, sec != 0 ? true : false);

            //updated value
            ostr << "requested time: " << encodeTimestamp(tp) << std::endl;
            ostr << "status: " << ret;

            ;
          }
          catch (std::exception& e) {
            ostr << e.what();
          }
        }
        else if (subcmd == "pst") {
          try {
            int shift;
            istr >> shift;

            auto tp = std::chrono::system_clock::now();
            tp += std::chrono::seconds(shift);

            int ret = m_imp->presetTime(tp);

            //updated value
            ostr << "requested time: " << encodeTimestamp(tp) << std::endl;
            ostr << "status: " << ret;
            if (ret > 0 && ret < 7) {
              ostr << " => ";
              switch (ret) {
              case 0: ostr << "Sun"; break;
              case 1: ostr << "Mon"; break;
              case 2: ostr << "Tue"; break;
              case 3: ostr << "Wen"; break;
              case 4: ostr << "Thu"; break;
              case 5: ostr << "Fri"; break;
              case 6: ostr << "Sat"; break;
              default: ostr << "ERR";
              }
            }
            ;
          }
          catch (std::exception& e) {
            ostr << e.what();
          }
        }
        else if (subcmd == "cpst") {
          try {

            int sec;
            istr >> sec;

            int ret = m_imp->changeRtcToPresetTime(sec != 0 ? true : false);

            //updated value
            ostr << "status: " << ret;
            ;
          }
          catch (std::exception& e) {
            ostr << e.what();
          }
        }
        else {
          ostr << "usage: " << std::endl;
          usage(ostr);
        }

        TRC_FUNCTION_LEAVE("");
        return ostr.str();
      }

      std::string getHelp() override
      {
        return "Test comm simulation. Type h for help";
      }

      ~TestCommCommand2() {}
    private:
      RepM3Edge::Imp* m_imp = nullptr;
    };

    std::string getVersion()
    {
      TRC_FUNCTION_ENTER("");

      std::lock_guard<std::mutex> lck(m_deviceMtx);

      GetVersion cmd;
      std::vector<uint8_t> request;
      std::vector<uint8_t> response;

      cmd.serialize(request);

      iqrfapi::embed::uart::WriteReadPtr writeRead(shape_new iqrfapi::embed::uart::WriteRead(
        //nadr, 0xffff, san->getReadTimeout(), writeDataVec));
        1, 0xffff, UART_WR_TOUT, request));

      TRC_DEBUG(">>>>>>>>>>>>>>>>> GetVersion: " << NAME_PAR(request, encodeBinary(request.data(), request.size())));
      std::cout << NAME_PAR(request, encodeBinary(request.data(), request.size())) << std::endl;

      writeRead->setInstanceMsgId();
      writeRead->setVerbose(true);

      m_iIqrfAdapt->executeMsgTransaction(writeRead, 120000);
      response = writeRead->getReadData();

      TRC_DEBUG(">>>>>>>>>>>>>>>>> GetVersion: " << NAME_PAR(response, encodeBinary(response.data(), response.size())));
      std::cout << NAME_PAR(response, encodeBinary(response.data(), response.size())) << std::endl;

      cmd.deserialize(response);

      GetVersion::Version ver = cmd.getVersion();

      TRC_FUNCTION_LEAVE(NAME_PAR(version, ver.getAsString()));
      return ver.getAsString();
    }

    std::chrono::system_clock::time_point getTime()
    {
      TRC_FUNCTION_ENTER("");

      std::lock_guard<std::mutex> lck(m_deviceMtx);

      GetTimeAndDate getTimeAndDate;
      std::vector<uint8_t> request;
      std::vector<uint8_t> response;

      getTimeAndDate.serialize(request);

      iqrfapi::embed::uart::WriteReadPtr writeRead(shape_new iqrfapi::embed::uart::WriteRead(
        //nadr, 0xffff, san->getReadTimeout(), writeDataVec));
        2, 0xffff, UART_WR_TOUT, request));

      TRC_DEBUG(">>>>>>>>>>>>>>>>> GetTimeEndDate: " << NAME_PAR(request, encodeBinary(request.data(), request.size())));
      std::cout << NAME_PAR(request, encodeBinary(request.data(), request.size())) << std::endl;

      writeRead->setInstanceMsgId();
      writeRead->setVerbose(true);

      m_iIqrfAdapt->executeMsgTransaction(writeRead, 120000);
      response = writeRead->getReadData();

      TRC_DEBUG(">>>>>>>>>>>>>>>>> GetTimeEndDate: " << NAME_PAR(response, encodeBinary(response.data(), response.size())));
      std::cout << NAME_PAR(response, encodeBinary(response.data(), response.size())) << std::endl;

      getTimeAndDate.deserialize(response);

      auto tp = getTimeAndDate.getTime();

      TRC_FUNCTION_LEAVE(NAME_PAR(tp, encodeTimestamp(tp)));
      return tp;
    }

    int setTime(const std::chrono::system_clock::time_point& tp, bool security)
    {
      TRC_FUNCTION_ENTER(NAME_PAR(tp, encodeTimestamp(tp)));

      std::lock_guard<std::mutex> lck(m_deviceMtx);

      SetTimeAndDate setTimeAndDate;
      std::vector<uint8_t> request;
      std::vector<uint8_t> response;

      setTimeAndDate.setTime(tp);

      setTimeAndDate.serialize(request, security);

      iqrfapi::embed::uart::WriteReadPtr writeRead(shape_new iqrfapi::embed::uart::WriteRead(
        //nadr, 0xffff, san->getReadTimeout(), writeDataVec));
        2, 0xffff, UART_WR_TOUT, request));

      TRC_DEBUG(">>>>>>>>>>>>>>>>> SetTimeEndDate: " << NAME_PAR(request, encodeBinary(request.data(), request.size())));
      std::cout << NAME_PAR(request, encodeBinary(request.data(), request.size())) << std::endl;

      writeRead->setInstanceMsgId();
      writeRead->setVerbose(true);

      m_iIqrfAdapt->executeMsgTransaction(writeRead, 120000);
      response = writeRead->getReadData();

      TRC_DEBUG(">>>>>>>>>>>>>>>>> SetTimeEndDate: " << NAME_PAR(response, encodeBinary(response.data(), response.size())));
      std::cout << NAME_PAR(response, encodeBinary(response.data(), response.size())) << std::endl;

      setTimeAndDate.deserialize(response);

      TRC_FUNCTION_LEAVE(NAME_PAR(status, setTimeAndDate.getData().status));
      return setTimeAndDate.getData().status;
    }

    int presetTime(const std::chrono::system_clock::time_point& tp)
    {
      TRC_FUNCTION_ENTER(NAME_PAR(tp, encodeTimestamp(tp)));

      std::lock_guard<std::mutex> lck(m_deviceMtx);

      //SetTimeAndDate setTimeAndDate;
      PresetTimeAndDate presetTimeAndDate;
      std::vector<uint8_t> request;
      std::vector<uint8_t> response;

      presetTimeAndDate.setTime(tp);

      presetTimeAndDate.serialize(request);

      iqrfapi::embed::uart::WriteReadPtr writeRead(shape_new iqrfapi::embed::uart::WriteRead(
        //nadr, 0xffff, san->getReadTimeout(), writeDataVec));
        1, 0xffff, UART_WR_TOUT, request));

      TRC_DEBUG(">>>>>>>>>>>>>>>>> PresetTimeEndDate: " << NAME_PAR(request, encodeBinary(request.data(), request.size())));
      std::cout << NAME_PAR(request, encodeBinary(request.data(), request.size())) << std::endl;

      writeRead->setInstanceMsgId();
      writeRead->setVerbose(true);

      m_iIqrfAdapt->executeMsgTransaction(writeRead, 120000);
      response = writeRead->getReadData();

      TRC_DEBUG(">>>>>>>>>>>>>>>>> PresetTimeEndDate: " << NAME_PAR(response, encodeBinary(response.data(), response.size())));
      std::cout << NAME_PAR(response, encodeBinary(response.data(), response.size())) << std::endl;

      presetTimeAndDate.deserialize(response);

      TRC_FUNCTION_LEAVE(NAME_PAR(status, presetTimeAndDate.getData().status));
      return presetTimeAndDate.getData().status;
    }

    int changeRtcToPresetTime(bool security)
    {
      TRC_FUNCTION_ENTER("");

      std::lock_guard<std::mutex> lck(m_deviceMtx);

      ChangeRtcToPreset cmd;
      std::vector<uint8_t> request;
      std::vector<uint8_t> response;

      cmd.serialize(request, security);

      iqrfapi::embed::uart::WriteReadPtr writeRead(shape_new iqrfapi::embed::uart::WriteRead(
        //nadr, 0xffff, san->getReadTimeout(), writeDataVec));
        1, 0xffff, UART_WR_TOUT, request));

      TRC_DEBUG(">>>>>>>>>>>>>>>>> ChangeRtcToPreset: " << NAME_PAR(request, encodeBinary(request.data(), request.size())));
      std::cout << NAME_PAR(request, encodeBinary(request.data(), request.size())) << std::endl;

      writeRead->setInstanceMsgId();
      writeRead->setVerbose(true);

      m_iIqrfAdapt->executeMsgTransaction(writeRead, 120000);
      response = writeRead->getReadData();

      TRC_DEBUG(">>>>>>>>>>>>>>>>> ChangeRtcToPreset: " << NAME_PAR(response, encodeBinary(response.data(), response.size())));
      std::cout << NAME_PAR(response, encodeBinary(response.data(), response.size())) << std::endl;

      cmd.deserialize(response);

      TRC_FUNCTION_LEAVE(NAME_PAR(status, cmd.getData().status));
      return cmd.getData().status;
    }

    /////////////////////////////////////////////
    /////////////////////////////////////////////
    /////////////////////////////////////////////

    void activate(const shape::Properties *props)
    {
      TRC_FUNCTION_ENTER("");
      TRC_INFORMATION(std::endl <<
        "******************************" << std::endl <<
        "RepM3Edge instance activate" << std::endl <<
        "******************************"
      );

      modify(props);

      //MQTT to CLD
      initApiTopics();
      subscribeAppTopics();

      m_processTaskToken = m_iWorkerThread->addToLoop(IWorkerThread::AddFuncPar(10, 600,
        [&](const std::chrono::system_clock::time_point & now)
      {
        detectSensorDevices(now);
      }
      ));

      m_processTaskToken = m_iWorkerThread->addToLoop(IWorkerThread::AddFuncPar(11, 5,
        [&](const std::chrono::system_clock::time_point & now)
      {
        sendFrc(now);
      }
      ));

      TRC_FUNCTION_LEAVE("")
    }

    void deactivate()
    {
      TRC_FUNCTION_ENTER("");

      //MQTT to CLD
      unsubscribeAppTopics();

      TRC_INFORMATION(std::endl <<
        "******************************" << std::endl <<
        "RepM3Edge instance deactivate" << std::endl <<
        "******************************"
      );
      TRC_FUNCTION_LEAVE("")
    }

    void modify(const shape::Properties *props)
    {
      using namespace rapidjson;

      const Document& doc = props->getAsJson();

      {
        const Value* v = Pointer("/instance").Get(doc);
        if (v && v->IsString()) {
          m_instanceName = v->GetString();
        }
      }
    }

    void attachInterface(oegw::IIqrfAdapt* iface)
    {
      m_iIqrfAdapt = iface;
    }

    void detachInterface(oegw::IIqrfAdapt* iface)
    {
      if (m_iIqrfAdapt == iface) {
        m_iIqrfAdapt = nullptr;
      }
    }

    void attachInterface(oegw::IWorkerThread* iface)
    {
      m_iWorkerThread = iface;
    }

    void detachInterface(oegw::IWorkerThread* iface)
    {
      if (m_iWorkerThread == iface) {
        m_iWorkerThread = nullptr;
      }
    }

    void attachInterface(oegw::IIdentityProvider* iface)
    {
      m_iIdentityProvider = iface;
    }

    void detachInterface(oegw::IIdentityProvider* iface)
    {
      if (m_iIdentityProvider == iface) {
        m_iIdentityProvider = nullptr;
      }
    }

    void attachInterface(IMsgService* iface)
    {
      m_iMsgService = iface;
    }

    void detachInterface(IMsgService* iface)
    {
      if (m_iMsgService == iface) {
        m_iMsgService = nullptr;
      }
    }

    void attachInterface(shape::IConfigurationService* iface)
    {
      m_iConfigurationService = iface;
    }

    void detachInterface(shape::IConfigurationService* iface)
    {
      if (m_iConfigurationService == iface) {
        m_iConfigurationService = nullptr;
      }
    }

    void attachInterface(shape::ILaunchService* iface)
    {
      m_iLaunchService = iface;
    }

    void detachInterface(shape::ILaunchService* iface)
    {
      if (m_iLaunchService == iface) {
        m_iLaunchService = nullptr;
      }
    }

    void attachInterface(shape::ICommandService* iface)
    {
      m_iCommandService = iface;
      m_iCommandService->addCommand("sn", std::shared_ptr<shape::ICommand>(shape_new TestCommCommand(this)));
    }

    void detachInterface(shape::ICommandService* iface)
    {
      if (m_iCommandService == iface) {
        m_iCommandService->removeCommand("sn");
        m_iCommandService = nullptr;
      }
    }
  };

  ///////////////////////
  RepM3Edge::RepM3Edge()
  {
    m_imp = shape_new Imp();
  }

  RepM3Edge::~RepM3Edge()
  {
    delete m_imp;
  }

  void RepM3Edge::activate(const shape::Properties *props)
  {
    m_imp->activate(props);
  }

  void RepM3Edge::deactivate()
  {
    m_imp->deactivate();
  }

  void RepM3Edge::modify(const shape::Properties *props)
  {
  }

  void RepM3Edge::attachInterface(oegw::IIqrfAdapt* iface)
  {
    m_imp->attachInterface(iface);
  }

  void RepM3Edge::detachInterface(oegw::IIqrfAdapt* iface)
  {
    m_imp->detachInterface(iface);
  }

  void RepM3Edge::attachInterface(oegw::IWorkerThread* iface)
  {
    m_imp->attachInterface(iface);
  }

  void RepM3Edge::detachInterface(oegw::IWorkerThread* iface)
  {
    m_imp->detachInterface(iface);
  }


  void RepM3Edge::attachInterface(oegw::IIdentityProvider* iface)
  {
    m_imp->attachInterface(iface);
  }

  void RepM3Edge::detachInterface(oegw::IIdentityProvider* iface)
  {
    m_imp->detachInterface(iface);
  }

  void RepM3Edge::attachInterface(oegw::IMsgService* iface)
  {
    m_imp->attachInterface(iface);
  }

  void RepM3Edge::detachInterface(oegw::IMsgService* iface)
  {
    m_imp->detachInterface(iface);
  }

  void RepM3Edge::attachInterface(shape::IConfigurationService* iface)
  {
    m_imp->attachInterface(iface);
  }

  void RepM3Edge::detachInterface(shape::IConfigurationService* iface)
  {
    m_imp->detachInterface(iface);
  }

  void RepM3Edge::attachInterface(shape::ILaunchService* iface)
  {
    m_imp->attachInterface(iface);
  }

  void RepM3Edge::detachInterface(shape::ILaunchService* iface)
  {
    m_imp->detachInterface(iface);
  }

  void RepM3Edge::attachInterface(shape::ICommandService* iface)
  {
    m_imp->attachInterface(iface);
  }

  void RepM3Edge::detachInterface(shape::ICommandService* iface)
  {
    m_imp->detachInterface(iface);
  }

  void RepM3Edge::attachInterface(shape::ITraceService* iface)
  {
    shape::Tracer::get().addTracerService(iface);
  }

  void RepM3Edge::detachInterface(shape::ITraceService* iface)
  {
    shape::Tracer::get().removeTracerService(iface);
  }

}
