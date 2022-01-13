#pragma once

#include <repM3.h>

namespace lgmc {

/**********************************************/
/************** General Commands **************/
/**********************************************/
class GetVersion : public GetVersionCmd {
  public:
    class Version {
    public:
      int minor;
      int major;
      int release;
      int variant;

      rapidjson::Value encode(rapidjson::Document::AllocatorType & a) {
        using namespace rapidjson;
        Value val(Type::kObjectType);
        Pointer("/minor").Set(val, minor, a);
        Pointer("/major").Set(val, major, a);
        Pointer("/release").Set(val, release, a);
        Pointer("/variant").Set(val, variant, a);
        
        return val;
      }
    };

    Version getVersion() {
      GetVersionCmd::data_t version = getData();
      Version retval;
      
      retval.minor = version.fw_version_minor.data;
      retval.major = version.fw_version_major.data;
      retval.release = version.fw_pre_release_nr.data;
      retval.variant = version.hw_variant.data;

      return retval;
    };
  };


  /**********************************************/
  /************** Settings and schedules **************/
  /**********************************************/

  //TODO not prio now
  //GetSettings
  //SetSettings

  //TODO
  class SetSchedule : public SetScheduleCmd {
  public:
    SetSchedule() = default;
  };
    

  /****************************************************
  ************** Time and date commands ***************
  ****************************************************/
  class SetTimeAndDate : public SetTimeAndDateCmd {
  public:
    void setTime(const std::chrono::system_clock::time_point & tim) {
      m_time = convertFromTimePoint(tim);
    }
  };

  class GetTimeAndDate : public GetTimeAndDateCmd {
  public:
    std::chrono::system_clock::time_point getTime() const {
      GetTimeAndDateCmd::data_recv_t d = getData();
      return convertToTimePoint(d);
    }
  };

  class PresetTimeAndDate : public PresetTimeAndDateCmd {
  public:
    void setTime(const std::chrono::system_clock::time_point & tim) {
     m_time = convertFromTimePoint(tim);
    }
  };

  //will be sent via FRC ack broadcast
  //class StartRtc : public BaseCommand {
  //class ChangeRtcToPresetCmd : public BaseCommand {
  //class TimeSync : public BaseCommand {

  /************************************************
  ************** Status and reports ***************
  *************************************************/
  class GetFlags : public GetFlagsCmd {
  public:
    //support these directly
    bool isLongTestPass() const {
        return getData().info_flags.f0();
    }
    bool isShortTestPass() const {
        return getData().info_flags.f1();
    }
    bool isLongTestFail() const {
        return getData().info_flags.f2();
    }
    bool isShortTestFail() const {
        return getData().info_flags.f3();
    }
  };

  
  //will be sent via FRC ack broadcast
  //class ResetFlagsCmd : public BaseCommand {

  // TODO: response can have different sizes -> adjust code
  class GetReport : public GetReportCmd {
  public:
    class LongReport {
      std::chrono::system_clock::time_point startTime; //maybe we can keep in ss:mm:hh, .... to 
      //charge_status
      //cell_type
      float bottomVoltage;
      //...

      rapidjson::Value encode(rapidjson::Document::AllocatorType & a) {
        using namespace rapidjson;
        Value val(Type::kObjectType);
        //startTime directly as ss:mm:hh, .... or "ISO YYY-MM-DDThh:mm:ss" => will be stored to SQL DB?
        Pointer("/bottomVoltage").Set(val, 3.0 , a); //TODO decode voltage
        //...
        return val;
      }
    };

    class ShortReport {
      //....
    };

    //to  prepare for request
    void requestLongReport() {
      //...
    }
    void requestShortReport() {
      //...
    }

    //getting result
    LongReport getLongReport() {
      //...
    }
    ShortReport getShortReport() {
      //...
    }
  };

  class AcknowledgeReport : public AcknowledgeReportCmd {
  public:
    void ackLongTest() { //... 
    }
    void ackShortTest() { //...
    }
    bool getAckResult() {} //decode success/failure
  };

  //not prio now
  //class GetSystemStatus1 : public GetSystemStatus1Cmd {
  //public:
  //  //just debug, we don't care content
  //  std::vector<uint8_t> getStatus1() { return std::vector<uint8_t>(); }
  //};

  //class GetSystemStatus2 : public GetSystemStatus2Cmd {
  //public:
  //  //just debug, we don't care content
  //  std::vector<uint8_t> getStatus2() { return std::vector<uint8_t>(); }
  //};
};