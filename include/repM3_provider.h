#pragma once

#include <repM3.h>
#include "rapidjson/pointer.h"

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
  class GetReport : public GetReportShortCmd , public GetReportLongCmd {
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

    //index from 0 - 63, 64 used for getting oldest report
    void requestLongReport(const int index) {
      // set index for long report is + 128
      m_idxLong.report_index = index + longReportOfsset;
    }

    //index from 0 - 63, 64 used for getting oldest report
    void requestShortReport(const int index) {
      m_idxShort.report_index = index;
    }

    //getting result
    LongReport getLongReport() {
      data_t_long report = cmdLong.getData();
      if (isReportValid(report) != true) {
        return LongReport{};
      }
      return LongReport{};
    }

    ShortReport getShortReport() {
      data_t_short report = cmdShort.getData();
      if (isReportValid(report) != true) {
        return ShortReport{};
      }
      return ShortReport{};
    }
    private:
      GetReportLongCmd cmdLong;
      GetReportShortCmd cmdShort;
      const int longReportOfsset = 128;

    private:
      template<typename T>
      bool isReportValid(T d) {
        return d.index != 0xFF;
      }
  };

  class AcknowledgeReport : public AcknowledgeReportCmd {
  public:
    //index from 0 - 63
    void ackLongTest(const int index) {
      m_data.report_index = index + longReportOfsset;
    }
 
    //index from 0 - 63
    void ackShortTest(const int index) { 
      m_data.report_index = index;
    }
  
    bool getAckResult() {

    }

    private:
      const int longReportOfsset = 128;
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