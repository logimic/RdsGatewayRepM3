#pragma once

#include "EnumStringConvertor.h"
#include "JsonParseEncodeSupport.h"
#include "DeviceApi.h" 
#include "RepM3Device.h"
#include "RepM3GwDevice.h"

#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/filewritestream.h"
#include "rapidjson/pointer.h"

#include <string>
#include <vector>
#include <typeinfo>

namespace lgmc {
  namespace repm3api {
    //////////////
    enum class MType {
      Undef,
      GatewayCmd,
      DeviceData,
      SensorCmd,
    };

    class MTypeConvertTable
    {
    public:
      static const std::vector<std::pair<MType, std::string>>& table()
      {
        static std::vector <std::pair<MType, std::string>> table = {
          { MType::Undef, "Undef" },
          { MType::GatewayCmd, "RepM3GatewayCmd" },
          { MType::DeviceData, "RepM3DeviceData" },
          { MType::SensorCmd, "RepM3SensorCmd" }
        };
        return table;
      }
      static MType defaultEnum()
      {
        return MType::Undef;
      }
      static const std::string& defaultStr()
      {
        static std::string u("Undef");
        return u;
      }
    };
    typedef shape::EnumStringConvertor<MType, MTypeConvertTable> MTypeConvert;

    
    //////////////
    class RepM3DeviceItem : public oegw::MsgItem
    {
    public:

      virtual ~RepM3DeviceItem() {}

      RepM3DeviceItem(RepM3DeviceShPtr repm3Device)
        :m_repm3Device(repm3Device)
      {}

      virtual void parse(const rapidjson::Value *v) override
      {
        THROW_EXC_TRC_WAR(std::logic_error, "not implemented")
      }

      virtual rapidjson::Value encode(rapidjson::Document::AllocatorType & a) const override
      {
        using namespace rapidjson;
        using namespace std::chrono;

        RepM3Device & s = *m_repm3Device;
        rapidjson::Value val;
        Pointer("/name").Set(val, s.getName(), a);
        Pointer("/node").Set(val, s.getNode().encode(a), a);
        Pointer("/dataFromNodeStr").Set(val, encodeBinary(s.getDataFromNode().data(), (int)s.getDataFromNode().size()), a);
        Pointer("/dataFromNode").Set(val, oegw::encodeVect(m_repm3Device->getDataFromNode(),a), a);
        Pointer("/paramGetStateStr").Set(val, encodeBinary(m_repm3Device->getParamGetState().data(), (int)m_repm3Device->getParamGetState().size()), a);
        Pointer("/paramGetState").Set(val, oegw::encodeVect(m_repm3Device->getParamGetState(), a), a);
        return val;
      }

    private:
      RepM3DeviceShPtr m_repm3Device;
    };

    //////////////
    class GatewayItem : public oegw::MsgItem
    {
    public:

      virtual ~GatewayItem() {}

      GatewayItem()
      {
      }

      virtual void parse(const rapidjson::Value *v) override
      {
        using namespace rapidjson;
        m_dataFromNodePeriodPtr = parseOptionalItem<int>("/dataFromNodePeriod", v);
        m_getStatePeriodPtr = parseOptionalItem<int>("/getStatePeriod", v);
        m_detectDevicesPeriodPtr = parseOptionalItem<int>("/detectDevicesPeriod", v);
        m_wakeUpPtr = parseOptionalItem<int>("/wakeUp", v);
        m_dataFromNodeLenPtr = parseOptionalItem<int>("/dataFromNodeLen", v);
        m_sleepPtr = parseOptionalItem<bool>("/sleep", v);
        m_getStatePeriodPtr = parseOptionalItem<int>("/dataFromNodePeriod", v);
        // sensor hwpid option not supported now
        //std::vector<int> hwpidVect = oegw::parseVect<int>(*v, "/sensorHwpid", false);
        //if (hwpidVect.size() > 0) {
        //  m_sensorHwpidVectPtr = std::make_shared<std::vector<int>>(hwpidVect);
        //}
      }

      virtual rapidjson::Value encode(rapidjson::Document::AllocatorType & a) const override
      {
        using namespace rapidjson;
        rapidjson::Value val;
        val.SetObject();
        if (m_dataFromNodePeriodPtr) Pointer("/dataFromNodePeriod").Set(val, *m_dataFromNodePeriodPtr, a);
        if (m_getStatePeriodPtr) Pointer("/getStatePeriod").Set(val, *m_getStatePeriodPtr, a);
        if (m_detectDevicesPeriodPtr) Pointer("/detectDevicesPeriod").Set(val, *m_detectDevicesPeriodPtr, a);
        if (m_wakeUpPtr) Pointer("/wakeUp").Set(val, *m_wakeUpPtr, a);
        if (m_dataFromNodeLenPtr) Pointer("/dataFromNodeLen").Set(val, *m_dataFromNodeLenPtr, a);
        if (m_sleepPtr) Pointer("/sleep").Set(val, *m_sleepPtr, a);
        // sensor hwpid option not supported now
        //if (m_sensorHwpidVectPtr) Pointer("/sensorHwpid").Set(val, oegw::encodeVect(*m_sensorHwpidVectPtr, a), a);

        return val;
      }

      void setGateway(const RepM3GwDevice & gw) {
        m_dataFromNodePeriodPtr = std::make_shared<int>(gw.m_dataFromNodePeriod);
        m_getStatePeriodPtr = std::make_shared<int>(gw.m_getStatePeriod);
        m_detectDevicesPeriodPtr = std::make_shared<int>(gw.m_detectDevicesPeriod);
        m_wakeUpPtr = std::make_shared<int>(gw.m_wakeUp);
        m_dataFromNodeLenPtr = std::make_shared<int>(gw.m_dataFromNodeLen);
        m_sleepPtr = std::make_shared<bool>(gw.m_sleep);
        m_sensorHwpidVectPtr = std::make_shared<std::vector<int>>(gw.m_sensorHwpidVect);
        m_gwSet = true;
      }

      std::shared_ptr<int> & getDataFromNodePeriodPtr() { return m_dataFromNodePeriodPtr; }
      std::shared_ptr<int> & getGetStatePeriodPtr() { return m_getStatePeriodPtr; }
      std::shared_ptr<int> & getDetectDevicesPeriodPtr() { return m_detectDevicesPeriodPtr; }
      std::shared_ptr<int> & getWakeUpPtr(){ return m_wakeUpPtr; }
      std::shared_ptr<int> & getDataFromNodeLenPtr() { return m_dataFromNodeLenPtr; }
      std::shared_ptr<bool> & getSleepPtr() { return m_sleepPtr; }
      std::shared_ptr<std::vector<int>> & getSensorHwpidVectPtr() { return m_sensorHwpidVectPtr; }

    private:
      template <typename T>
      std::shared_ptr<T> parseOptionalItem(const std::string & key, const rapidjson::Value *v)
      {
        using namespace rapidjson;
        std::shared_ptr<T> retval;
        const Value* val = Pointer(key).Get(*v);
        if (val) {
          if (!val->Is<T>()) {
            THROW_EXC_TRC_WAR(std::logic_error, "expected" << PAR(key) << "as" << typeid(T).name());
          }
          retval = std::make_shared<T>(val->Is<T>());
        }
        return retval;
      }

      std::shared_ptr<int> m_dataFromNodePeriodPtr;
      std::shared_ptr<int> m_getStatePeriodPtr;
      std::shared_ptr<int> m_detectDevicesPeriodPtr;
      std::shared_ptr<int> m_wakeUpPtr;
      std::shared_ptr<int> m_dataFromNodeLenPtr;
      std::shared_ptr<bool> m_sleepPtr;
      std::shared_ptr<std::vector<int>> m_sensorHwpidVectPtr;
      bool m_gwSet = false;
    };

    class DeviceDataMsg : public oegw::GenericMsg
    {
    public:
      DeviceDataMsg()
      {
        setType(repm3api::MTypeConvert::enum2str(repm3api::MType::DeviceData));
      }

      virtual ~DeviceDataMsg() {}

      void parseRequest(const rapidjson::Value &v) override
      {
        GenericMsg::parseRequest(v);
      }

      void encodeResponse(rapidjson::Value &v, rapidjson::Document::AllocatorType & a) const override
      {
        GenericMsg::encodeResponse(v, a);
        using namespace std::chrono;
        rapidjson::Pointer("/data/time").Set(v, duration_cast<seconds>(m_tp.time_since_epoch()).count(), a);
        rapidjson::Pointer("/data/sensors").Set(v, oegw::encodeVect(m_repM3DeviceVect, a), a);
        rapidjson::Pointer("/data/gateway").Set(v, m_gateway.encode(a), a);
      }

      void parseResponse(const rapidjson::Value &v) override
      {
        THROW_EXC_TRC_WAR(std::logic_error, "not implemented")
      }

      void setTime(const std::chrono::system_clock::time_point & tp) { m_tp = tp; }

      void setRepM3Devices(std::map < std::string, RepM3DeviceShPtr > & repM3DevicesMap) {
        for (auto & it : repM3DevicesMap) {
          m_repM3DeviceVect.push_back(RepM3DeviceItem(it.second));
        }
      }

      void setGateway(const RepM3GwDevice & gw) {
        m_gateway.setGateway(gw);
      }
    private:
      std::chrono::system_clock::time_point m_tp;
      std::vector<RepM3DeviceItem> m_repM3DeviceVect;
      GatewayItem m_gateway;
    };

    //////////////
    class SensorCmdMsg : public oegw::GenericMsg
    {
    public:
      SensorCmdMsg()
      {
        setType(repm3api::MTypeConvert::enum2str(repm3api::MType::SensorCmd));
      }

      virtual ~SensorCmdMsg() {}

      // just for test
      void encodeRequest(rapidjson::Value &v, rapidjson::Document::AllocatorType & a) const override
      {
        GenericMsg::encodeRequest(v, a);
        rapidjson::Pointer("/data/sensors").Set(v, oegw::encodeVect(m_sensorsNamesVect, a), a);
        rapidjson::Pointer("/data/cmd").Set(v, oegw::encodeVect(m_commandDataVect,a), a);
      }

      void parseRequest(const rapidjson::Value &v) override
      {
        GenericMsg::parseRequest(v);
        m_sensorsNamesVect = oegw::parseVect<std::string>(v, "/data/sensors", true);
        m_commandDataVect = oegw::parseVect<uint8_t>(v, "/data/cmd", true);
        if (m_commandDataVect.size() < 4 || 2 != m_commandDataVect.front() || 3 != m_commandDataVect.back()) {
          THROW_EXC_TRC_WAR(std::logic_error, "expected command: STX, len, cmd, ..., ETX");
        }
      }

      void encodeResponse(rapidjson::Value &v, rapidjson::Document::AllocatorType & a) const override
      {
        using namespace rapidjson;
        GenericMsg::encodeResponse(v, a);
      }

      const std::vector<uint8_t> & getCommandData() const { return m_commandDataVect; }
      void setCommandData(const std::vector<uint8_t> & commandData) { m_commandDataVect = commandData; }

      const std::vector<std::string> & getSensorDeviceNames() const { return m_sensorsNamesVect; }
      void setSensorDeviceNames(const std::vector<std::string> & names) { m_sensorsNamesVect = names; }

    private:
      std::vector<std::string> m_sensorsNamesVect;
      std::vector<uint8_t> m_commandDataVect;
    };

    //////////////
    class GatewayCmdMsg : public oegw::GenericMsg
    {
    public:
      GatewayCmdMsg()
      {
        setType(repm3api::MTypeConvert::enum2str(repm3api::MType::GatewayCmd));
      }

      virtual ~GatewayCmdMsg() {}

      // just for test
      void encodeRequest(rapidjson::Value &v, rapidjson::Document::AllocatorType & a) const override
      {
        GenericMsg::encodeRequest(v, a);
        rapidjson::Pointer("/data/gatewayCmd").Set(v, m_gateway.encode(a), a);
      }

      void parseRequest(const rapidjson::Value &v) override
      {
        GenericMsg::parseRequest(v);
        m_gateway.parse(oegw::getJsonMandatory(v, "/data/gatewayCmd"));
      }

      void encodeResponse(rapidjson::Value &v, rapidjson::Document::AllocatorType & a) const override
      {
        using namespace rapidjson;
        GenericMsg::encodeResponse(v, a);
      }

      GatewayItem & getGatewayItem() { return m_gateway; }

    private:
      GatewayItem m_gateway;
    };
  }
}
