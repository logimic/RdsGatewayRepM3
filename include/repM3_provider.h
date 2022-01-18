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
      public:
        std::chrono::system_clock::time_point startTime; //maybe we can keep in ss:mm:hh, .... to 
        int test_duration;
        BATTSTATUS8::charge_status cell_status;
        BATTSTATUS8::cell_type cell_type;
        uint16_t startBottomVols;
        uint8_t startLoadVolts;
        uint8_t startLoadCurrent;
        uint16_t testDurationAchieved;
        uint8_t testDurationAchievedWithBothCells;
        uint16_t endBottomVolts;
        uint16_t endTopVolts;
        uint8_t endLoadVolts;
        uint8_t endLoadCurrent;
        uint16_t bottomCellAmpHours;
        uint16_t topCellAmpHours;
        uint16_t bottomCellWattHours;
        uint16_t topCellWattHours;
        uint8_t exponents;
        uint8_t testFlags;

        rapidjson::Value encode(rapidjson::Document::AllocatorType & a) {
          using namespace rapidjson;
          Value val(Type::kObjectType);
          Pointer("/startTime").Set(val, "123", a);
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
      LongReport r;

      DateTimeBase dt;
      DateTimeBase::data_recv_t data;
      data.date_year = report.rep.start_date.year();
      data.date_month = report.rep.start_date.month();
      data.date_day = report.rep.start_date.day_of_month();
      data.time_hour = report.rep.start_time.hour();
      data.time_minute = report.rep.start_time.minute();
      data.time_second = report.rep.start_time.seconds();

      r.startTime = dt.convertToTimePoint(data);
      
      r.test_duration = report.rep.test_duration.value();
      r.cell_status = report.rep.start_cell_mode.cell_status_val();
      r.cell_type = report.rep.start_cell_mode.cell_type_val();
      r.startBottomVols = report.rep.start_bottom_cell_volts.data;

      r.startLoadCurrent = report.rep.start_load_current.data;
      r.testDurationAchieved = report.rep.test_duration_achieved.data;
      r.testDurationAchievedWithBothCells = report.rep.test_duration_achieved_with_both_cells.data;
      r.endBottomVolts = report.rep.end_bottom_cell_volts.data;
      r.endTopVolts = report.rep.end_top_cell_volts.data;
      r.endLoadVolts = report.rep.end_load_volts.data;
      r.endLoadCurrent = report.rep.end_load_current.data;
      r.bottomCellAmpHours = report.rep.bot_cell_amp_hours.data;
      r.topCellAmpHours = report.rep.top_cell_amp_hours.data;
      r.bottomCellWattHours = report.rep.bot_cell_watt_hours.data;
      r.topCellWattHours = report.rep.top_cell_watt_hours.data;
      r.exponents = report.rep.exponents.data;
      r.testFlags = report.rep.test_flags.data;

      return r;
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