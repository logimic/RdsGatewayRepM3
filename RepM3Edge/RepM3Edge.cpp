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
#include "EmbedUart.h"
#include "EmbedFRC.h"

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
      const int REPM3_HWPID = 0x22;

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
      //std::string instructionType = InstructionTypesConvert::int2str(writeDataVec[2]);
      //TRC_FUNCTION_ENTER(">>> " << PAR(nadr) << PAR(instructionType));
      TRC_FUNCTION_ENTER(">>> " << PAR(nadr));

      std::vector<uint8_t> retval;

      iqrfapi::embed::uart::WriteReadPtr writeRead(shape_new iqrfapi::embed::uart::WriteRead(
        nadr, 0xffff, UART_WR_TOUT, writeDataVec));

      writeRead->setInstanceMsgId();
      writeRead->setVerbose(true);

      m_iIqrfAdapt->executeMsgTransaction(writeRead, 120000);
      retval = writeRead->getReadData();

      //TRC_FUNCTION_LEAVE("<<< " << PAR(nadr) << PAR(instructionType));
      TRC_FUNCTION_LEAVE("<<< " << PAR(nadr));
      return retval;
    }

    // regular actions - sensor frc, polling, it respect device sleeping, etc
    void sendFrc(const std::chrono::system_clock::time_point & timePointNow)
    {
      //TRC_FUNCTION_ENTER("");
      //send FRC getFlags

      //TRC_FUNCTION_LEAVE("");
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
          //san.setCommand(msg.getCommandData()[commandNumPosition], msg.getCommandData());
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

      //if (gw.getDataFromNodePeriodPtr())
      //  m_gatewayDevice.m_dataFromNodePeriod = *gw.getDataFromNodePeriodPtr();
      if (gw.getGetStatePeriodPtr())
        m_gatewayDevice.m_getFlagsPeriod = *gw.getGetStatePeriodPtr();
      if (gw.getDetectDevicesPeriodPtr())
        m_gatewayDevice.m_detectDevicesPeriod = *gw.getDetectDevicesPeriodPtr();
      //if (gw.getWakeUpPtr())
      //  m_gatewayDevice.m_wakeUp = *gw.getWakeUpPtr();
      //if (gw.getDataFromNodeLenPtr())
      //  m_gatewayDevice.m_dataFromNodeLen = *gw.getDataFromNodeLenPtr();
      //if (gw.getSleepPtr())
      //  m_gatewayDevice.m_sleep = *gw.getSleepPtr();
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

        if (par.m_devStage == "dev4" || par.m_devStage == "dev5") {
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
          std::left << std::setw(20) << "mc h" << "test comm help" << std::endl <<
          std::left << std::setw(20) << "mc l" << "list detected sensors" << std::endl <<
          std::left << std::setw(20) << "mc ver" << "get version" << std::endl <<
          std::left << std::setw(20) << "mc flg" << "get flags" << std::endl <<
          std::left << std::setw(20) << "mc frc" << "get flags by FRC 1-bit" << std::endl <<
          std::left << std::setw(20) << "mc frc8" << "get flags by FRC 1-Byte" << std::endl <<
          std::left << std::setw(20) << "mc gt" << "get time" << std::endl <<
          std::left << std::setw(20) << "mc st <sec>" << "set time, sec: rel in secnds to actual" << std::endl <<
          std::left << std::setw(20) << "mc pst <sec>" << "preset time, sec: rel in seconds to actual" << std::endl <<
          std::left << std::setw(20) << "mc cpst" << "change to preset time" << std::endl <<
          std::left << std::setw(20) << "mc rtc" << "start RTC" << std::endl <<
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

        if (subcmd == "h") {
          usage(ostr);
        }
        else if (subcmd == "l") {
          //list sensors
          try {
            auto sensList = m_imp->getAllRepM3EdgeDevicesNames();
            //std::string dotstr;
            //istr >> dotstr;

            for (auto& sen : sensList) {
              ostr << std::endl << sen;
            }
            ostr << std::endl;
          }
          catch (std::exception& e) {
            ostr << e.what();
          }
        }
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
        else if (subcmd == "flg") {
          try {
            std::string dotstr;
            istr >> dotstr;

            auto flags = m_imp->getFlags();

            //updated value
            ostr << "flags: " << std::endl <<
              flags;
          }
          catch (std::exception& e) {
            ostr << e.what();
          }
        }
        else if (subcmd == "frc") {
          try {
            std::string dotstr;
            istr >> dotstr;

            auto flags = m_imp->getFlagsByFRC();

            //updated value
            ostr << "flags: " << std::endl <<
              flags;
          }
          catch (std::exception& e) {
            ostr << e.what();
          }
        }
        else if (subcmd == "frc8") {
          try {
            std::string dotstr;
            istr >> dotstr;

            auto flags = m_imp->getFlagsByFRC8();

            //updated value
            ostr << "flags: " << std::endl <<
              flags;
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

            auto tp = std::chrono::system_clock::now();
            tp += std::chrono::seconds(shift);

            int ret = m_imp->setTime(tp);

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

            int ret = m_imp->changeRtcToPresetTime();

            //updated value
            ostr << "status: " << ret;
            ;
          }
          catch (std::exception& e) {
            ostr << e.what();
          }
        }
        else if (subcmd == "cpst") {
          try {

            int ret = m_imp->changeRtcToPresetTime();

            //updated value
            ostr << "status: " << ret;
            ;
          }
          catch (std::exception& e) {
            ostr << e.what();
          }
        }
        else if (subcmd == "rtc") {
          try {

            int ret = m_imp->startRtc();

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

      ~TestCommCommand() {}
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
        2, 0xffff, UART_WR_TOUT, request));

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

    int setTime(const std::chrono::system_clock::time_point& tp)
    {
      TRC_FUNCTION_ENTER(NAME_PAR(tp, encodeTimestamp(tp)));

      std::lock_guard<std::mutex> lck(m_deviceMtx);

      SetTimeAndDate setTimeAndDate;
      std::vector<uint8_t> request;
      std::vector<uint8_t> response;

      setTimeAndDate.setTime(tp);

      setTimeAndDate.serialize(request);

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

    int changeRtcToPresetTime()
    {
      TRC_FUNCTION_ENTER("");

      std::lock_guard<std::mutex> lck(m_deviceMtx);

      ChangeRtcToPreset cmd;
      std::vector<uint8_t> request;
      std::vector<uint8_t> response;

      cmd.serialize(request);

      iqrfapi::embed::uart::WriteReadPtr writeRead(shape_new iqrfapi::embed::uart::WriteRead(
        //nadr, 0xffff, san->getReadTimeout(), writeDataVec));
        2, 0xffff, UART_WR_TOUT, request));

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

    int startRtc()
    {
      TRC_FUNCTION_ENTER("");

      std::lock_guard<std::mutex> lck(m_deviceMtx);

      StartRtcCmd cmd;
      std::vector<uint8_t> request;
      std::vector<uint8_t> response;

      cmd.serialize(request);

      iqrfapi::embed::uart::WriteReadPtr writeRead(shape_new iqrfapi::embed::uart::WriteRead(
        //nadr, 0xffff, san->getReadTimeout(), writeDataVec));
        2, 0xffff, UART_WR_TOUT, request));

      TRC_DEBUG(">>>>>>>>>>>>>>>>> StartRtc: " << NAME_PAR(request, encodeBinary(request.data(), request.size())));
      std::cout << NAME_PAR(request, encodeBinary(request.data(), request.size())) << std::endl;

      writeRead->setInstanceMsgId();
      writeRead->setVerbose(true);

      m_iIqrfAdapt->executeMsgTransaction(writeRead, 120000);
      response = writeRead->getReadData();

      TRC_DEBUG(">>>>>>>>>>>>>>>>> StartRtc: " << NAME_PAR(response, encodeBinary(response.data(), response.size())));
      std::cout << NAME_PAR(response, encodeBinary(response.data(), response.size())) << std::endl;

      cmd.deserialize(response);

      TRC_FUNCTION_LEAVE(NAME_PAR(status, cmd.getData().status));
      return cmd.getData().status;
    }

    std::string getFlags()
    {
      TRC_FUNCTION_ENTER("");

      std::lock_guard<std::mutex> lck(m_deviceMtx);

      GetFlags cmd;
      std::vector<uint8_t> request;
      std::vector<uint8_t> response;

      cmd.serialize(request);

      iqrfapi::embed::uart::WriteReadPtr writeRead(shape_new iqrfapi::embed::uart::WriteRead(
        0x02, 0xffff, UART_WR_TOUT, request));

      TRC_DEBUG(">>>>>>>>>>>>>>>>> GetFlags: " << NAME_PAR(request, encodeBinary(request.data(), request.size())));
      std::cout << NAME_PAR(request, encodeBinary(request.data(), request.size())) << std::endl;

      writeRead->setInstanceMsgId();
      writeRead->setVerbose(true);

      m_iIqrfAdapt->executeMsgTransaction(writeRead, 120000);
      response = writeRead->getReadData();

      TRC_DEBUG(">>>>>>>>>>>>>>>>> GetFlags: " << NAME_PAR(response, encodeBinary(response.data(), response.size())));
      std::cout << NAME_PAR(response, encodeBinary(response.data(), response.size())) << std::endl;

      cmd.deserialize(response);

      std::ostringstream os;
      os <<
        "green: " << (unsigned)cmd.getData().info_flags.data <<
        " yellow: " << (unsigned)cmd.getData().warning_flags.data <<
        " red: " << (unsigned)cmd.getData().error_flags.data <<
        " system: " << (unsigned)cmd.getData().system_status_info.data <<
        " additional: " << (unsigned)cmd.getData().additional_status_info.data;

      TRC_FUNCTION_LEAVE("");
      return os.str();
    }

    std::string getFlagsByFRC()
    {
      TRC_FUNCTION_ENTER("");

      std::lock_guard<std::mutex> lck(m_deviceMtx);

      /*
      {
        GetFlags cmd;
        std::vector<uint8_t> request;
        std::vector<uint8_t> response;

        cmd.serialize(request);

        iqrfapi::embed::uart::WriteReadPtr writeRead(shape_new iqrfapi::embed::uart::WriteRead(
          0xff, 0xffff, UART_WR_TOUT, request)); // broadcast NADR

        TRC_DEBUG(">>>>>>>>>>>>>>>>> GetFlags: " << NAME_PAR(request, encodeBinary(request.data(), request.size())));
        std::cout << NAME_PAR(request, encodeBinary(request.data(), request.size())) << std::endl;

        writeRead->setInstanceMsgId();
        writeRead->setVerbose(true);

        m_iIqrfAdapt->executeMsgTransaction(writeRead, 120000);

        response = writeRead->getReadData();
      }
      */

      //the same with ACK broadcast
      /*
      {
        std::set<int> unreachableNodes;
        std::set<int> ackNodes;

        GetFlags cmd;
        std::vector<uint8_t> request;

        cmd.serialize(request);

        const uint8_t pnum = 0x0C; // uart
        const uint8_t pcmd = 0x02; // ReadWrite
        uint16_t hwpid = 0xFFFF; //Hwpid
        std::vector<uint8_t> cmdVec = { 4 }; //prepend wait 40 ms when sent by selectiveBatch
        cmdVec.insert(cmdVec.end(), request.begin(), request.end());

        iqrfapi::embed::frc::AcknowledgedBroadcastBitsPtr frcMsg(shape_new iqrfapi::embed::frc::AcknowledgedBroadcastBits(pnum, pcmd, hwpid, cmdVec));

        std::ostringstream os;
        os << frcMsg->getMsgId() << "-extra";
        iqrfapi::embed::frc::ExtraResultPtr extraMsg(shape_new iqrfapi::embed::frc::ExtraResult());
        extraMsg->setMsgId(os.str());

        m_iIqrfAdapt->executeMsgTransaction(frcMsg, 60000);
        m_iIqrfAdapt->executeMsgTransaction(extraMsg, 3000);

        frcMsg->getFrcDataAs2bit(ackNodes, extraMsg->getFrcData());

        //unreachableNodes = oegw::iqrfadapt::substractSet(nadrSet, ackNodes);

        //unreachableNodes = oegw::iqrfadapt::substractSet(nadrSet, ackNodes);

        ////trace unreachable
        //if (unreachableNodes.size() > 0) {
        //  std::ostringstream os;
        //  for (int node : unreachableNodes) {
        //    os << node << ", ";
        //  }
        //  TRC_WARNING("DATA_FROM_NODE is not delivered to unreachable nodes: " << os.str())
        //}
      }
      */

      //we believe the TRs got it => now we can download flags status by FRC

      std::ostringstream osres;
      //getFlags FRC
      {
        std::map<int, int> nadrFlagsMap;

        iqrfapi::embed::frc::SendPtr frcMsg(shape_new iqrfapi::embed::frc::Send(0x10, { 0x5E, 0x81, 0x00 }));
        frcMsg->setVerbose(true);

        std::ostringstream os;
        os << frcMsg->getMsgId() << "-extra";
        iqrfapi::embed::frc::ExtraResultPtr extraMsg(shape_new iqrfapi::embed::frc::ExtraResult());
        extraMsg->setMsgId(os.str());

        m_iIqrfAdapt->executeMsgTransaction(frcMsg, 60000);
        m_iIqrfAdapt->executeMsgTransaction(extraMsg, 3000);

        frcMsg->getFrcDataAs2bit(nadrFlagsMap, extraMsg->getFrcData());

        int inc = 0;
        for (auto& it : nadrFlagsMap) {
          osres << '[' << it.first << ',' << it.second << "]  ";
          if (++inc % 8 == 0) osres << std::endl;
        }
      
      }

      TRC_FUNCTION_LEAVE(osres.str());
      return osres.str();
    }

    std::string getFlagsByFRC8()
    {
      TRC_FUNCTION_ENTER("");

      std::lock_guard<std::mutex> lck(m_deviceMtx);

      /*
      {
        GetFlags cmd;
        std::vector<uint8_t> request;
        std::vector<uint8_t> response;

        cmd.serialize(request);

        iqrfapi::embed::uart::WriteReadPtr writeRead(shape_new iqrfapi::embed::uart::WriteRead(
          0xFF, 0xffff, UART_WR_TOUT, request)); // broadcast NADR

        TRC_DEBUG(">>>>>>>>>>>>>>>>> GetFlags: " << NAME_PAR(request, encodeBinary(request.data(), request.size())));
        std::cout << NAME_PAR(request, encodeBinary(request.data(), request.size())) << std::endl;

        writeRead->setInstanceMsgId();
        writeRead->setVerbose(true);

        m_iIqrfAdapt->executeMsgTransaction(writeRead, 120000);

        response = writeRead->getReadData();
      }
      */

      //the same with ACK broadcast
      /*
      {
        std::set<int> unreachableNodes;
        std::set<int> ackNodes;

        GetFlags cmd;
        std::vector<uint8_t> request;

        cmd.serialize(request);

        const uint8_t pnum = 0x0C; // uart
        const uint8_t pcmd = 0x02; // ReadWrite
        uint16_t hwpid = 0xFFFF; //Hwpid
        std::vector<uint8_t> cmdVec = { 4 }; //prepend wait 40 ms when sent by selectiveBatch
        cmdVec.insert(cmdVec.end(), request.begin(), request.end());

        iqrfapi::embed::frc::AcknowledgedBroadcastBitsPtr frcMsg(shape_new iqrfapi::embed::frc::AcknowledgedBroadcastBits(pnum, pcmd, hwpid, cmdVec));

        std::ostringstream os;
        os << frcMsg->getMsgId() << "-extra";
        iqrfapi::embed::frc::ExtraResultPtr extraMsg(shape_new iqrfapi::embed::frc::ExtraResult());
        extraMsg->setMsgId(os.str());

        m_iIqrfAdapt->executeMsgTransaction(frcMsg, 60000);
        m_iIqrfAdapt->executeMsgTransaction(extraMsg, 3000);

        frcMsg->getFrcDataAs2bit(ackNodes, extraMsg->getFrcData());

        //unreachableNodes = oegw::iqrfadapt::substractSet(nadrSet, ackNodes);

        //unreachableNodes = oegw::iqrfadapt::substractSet(nadrSet, ackNodes);

        ////trace unreachable
        //if (unreachableNodes.size() > 0) {
        //  std::ostringstream os;
        //  for (int node : unreachableNodes) {
        //    os << node << ", ";
        //  }
        //  TRC_WARNING("DATA_FROM_NODE is not delivered to unreachable nodes: " << os.str())
        //}
      }
      */

      //we believe the TRs got it => now we can download flags status by FRC

      std::ostringstream osres;
      //getFlags FRC
      {
        std::map<int, uint8_t> nadrFlagsMap;

        iqrfapi::embed::frc::SendPtr frcMsg(shape_new iqrfapi::embed::frc::Send(0x90, { 0x5E, 0x81, 0x00 }));
        frcMsg->setVerbose(true);

        std::ostringstream os;
        os << frcMsg->getMsgId() << "-extra";
        iqrfapi::embed::frc::ExtraResultPtr extraMsg(shape_new iqrfapi::embed::frc::ExtraResult());
        extraMsg->setMsgId(os.str());

        m_iIqrfAdapt->executeMsgTransaction(frcMsg, 60000);
        m_iIqrfAdapt->executeMsgTransaction(extraMsg, 3000);

        frcMsg->getFrcDataAs(nadrFlagsMap, extraMsg->getFrcData());

        int inc = 0;
        for (auto& it : nadrFlagsMap) {
          osres << '[' << it.first << ',' << (int)it.second << "]  ";
          if (++inc % 8 == 0) osres << std::endl;
        }

      }

      TRC_FUNCTION_LEAVE(osres.str());
      return osres.str();
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
      m_iCommandService->addCommand("mc", std::shared_ptr<shape::ICommand>(shape_new TestCommCommand(this)));
    }

    void detachInterface(shape::ICommandService* iface)
    {
      if (m_iCommandService == iface) {
        m_iCommandService->removeCommand("mc");
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
