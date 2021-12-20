#pragma once

#include <string>
#include <vector>
#include <cstring>
#include <iostream> 
#include <sstream>
#include <algorithm>

namespace lgmc {

/* definitions for packed structures for GCC and MSC */
#ifdef __GNUC__
#define PACK( __Declaration__ ) __Declaration__ __attribute__((__packed__))
#endif

#ifdef _MSC_VER
#define PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop))
#endif

  struct UINT8 {
    uint8_t data;
    // compare operator
    bool operator==(UINT8 other) const {
      return data == other.data;
    }
    // compare for gtest EQUAL
    bool operator==(int other) const {
      return data == (uint8_t)other;
    }

    void operator=(uint32_t other) {
      data = uint8_t(other);
    }
  };

  struct UINT16 {
    uint16_t data = 0;
    // compare operator
    bool operator==(UINT16 other) const {
      return data == other.data;
    }
    // compare for gtest EQUAL
    bool operator==(int other) const {
      return data == (uint8_t)other;
    }

    uint16_t operator&(int other) {
      return data & other; 
    }

    uint16_t operator|=(int other) {
      return data |= uint16_t(other); 
    }

    uint16_t operator>>(int size) {
      return data >> size; 
    }
  };

  struct UINT14 {
    uint16_t data = 0;

    UINT14 & operator=(UINT14 & other) {
      if (this == &other) {
        return *this;
      }
      data = other.data & 0x3FFF; // only 14 byte
      return *this;
    }
  };

  struct FLAGS8 {
    uint8_t data;
  };

  struct FLAGS14 {
    uint16_t data;
  };

  struct FLAGS16 {
    uint16_t data;
  };

  struct FLAGS32 {
    uint32_t data;
  };

  struct COMPTIME8 {
    uint8_t data = 0;
  };

  struct BATTSTATUS8 {

  };

  struct System_Time {
    UINT8 second;
    UINT8 minute;
    UINT8 hour;
  };

  struct System_Settings_page0 {
    UINT14 current_maintained_mode;
    UINT14 voltage_maintained_mode;
    UINT14 power_maintained_mode;
    UINT14 current_emergency_mode;
    UINT14 voltage_emergency_mode;
    UINT14 power_emergency_mode;
    UINT14 test_voltage_lower_limit;
    UINT14 test_voltage_upper_limit;
    UINT14 reserved[7];
  };

  struct System_Compressed_Date {
    UINT16 data;
    
    void set_day_of_month(uint32_t day) {
      data |= (day & 0x1F);

    }
    uint32_t day_of_month() {
      return uint(data &0x1F);
    }

    void set_month(uint32_t month) {
      data |= (month & 0xF) << 5;
    }

    uint32_t month() {
      return uint((data >> 5) & 0xF);
    }

    void set_year(uint32_t year) {
      data |= (year & 0x7F) << 9;
    }

    uint32_t year() {
      return uint((data >> 9) & 0x7F);
    }

    bool operator==(System_Compressed_Date other) const {
      return data == other.data;
    }
  };

  struct Test_Schedule {
    System_Time start_time;
    FLAGS8 days_of_week;
    FLAGS32 days_of_month;
    FLAGS16 months;
    UINT8 year;
    UINT8 year_mask;
  };

  /* base data template class */
  template <typename T>
  class BaseData {
    public:
      explicit BaseData(T &d){m_data = d;}
      BaseData(){}
      static std::vector<uint8_t> serialize(T d)
      {
        std::vector<uint8_t> data(sizeof(d));
        std::memcpy(&data[0], &d, sizeof(d));

        return data;
      }
      // serialize to vector
      std::vector<uint8_t> serialize()
      {
        std::vector<uint8_t> d(sizeof(m_data));
        std::memcpy(&d[0], &m_data, sizeof(m_data));
        
        return d;
      }
      // deserialize back to data type
      T deserialize(std::vector<uint8_t> &d) const
      {
        T data;
        memcpy(&data, d.data(), d.size());
        return data;
      }

      bool operator==(T &other) {
        return m_data == other.data;
      }

      bool operator!=(T &other) {
        return m_data != other;
      }

      T& data() { return m_data; }

    private:
      T m_data;
  };

  template<typename N>
    int size_of_struct(N n) {return sizeof(n)/sizeof(N);}

  void print_vector(std::vector<uint8_t> d) {
    printf("[");
    for (auto i : d) {
      printf("0x%x,", i);
    }
    printf("]\n");
  }

  class BaseCommand {
    public:
      // command ids
      enum COMMANDS_TYPE {
        CMD_GET_VERSION = 9,
        CMD_SET_DATE_AND_TIME = 12,
        CMD_GET_DATE_AND_TIME = 13,
        CMD_START_RTC = 14,
        CMD_GET_SETTINGS = 15,
        CMD_SET_SETTINGS = 16,
        CMD_PRESET_DATE_AND_TIME = 18,
        CMD_SET_SCHEDULE = 19,
        CMD_CHANGE_RTC_TO_PRESET = 23,
        CMD_TIME_SYNC = 27,
      };

      BaseCommand(){}
      
      BaseCommand(COMMANDS_TYPE cmd_id)
      : m_id(cmd_id){}
      
      virtual ~BaseCommand() {
      }
      
      /* serialize data without parameters */
      std::vector<uint8_t> serialize() {
        m_data.push_back(start_byte);
        m_data.push_back(1);
        m_data.push_back(m_id);
        std::vector<uint8_t> c{m_data.begin() + 1, m_data.end()};

        m_data.push_back(crc(c));
        m_data.push_back(stop_byte);

        return m_data;
      }
      
      /* serialize data with parameters */
      template <typename T>
      std::vector<uint8_t> serialize(T d) {

        m_data.push_back(start_byte);
        m_data.push_back(sizeof(d)/sizeof(T) + 1);
        m_data.push_back(m_id);
        
        std::vector<uint8_t> ser = d.serialize();
        m_data.insert(m_data.end(), ser.begin(), ser.end());
        
        std::vector<uint8_t> c{m_data.begin() + 1, m_data.end()};

        m_data.push_back(crc(c));
        m_data.push_back(stop_byte);

        return m_data;
      }
      
      template <typename T>
      bool deserialize(std::vector<uint8_t> d, T *t) {
        BaseData<T> s;
        // make copy of data to member variable
        m_recv_data.insert(m_recv_data.end(), d.begin(), d.end());

        // first and last bytes must be start/stop bytes
        if (d[0] != start_byte || d[d.size()-1] != stop_byte)  {
          std::ostringstream os;
          os << "Wrong start or stop byte: " << std::hex << unsigned(d[0]);
          std::logic_error ex(os.str().c_str());
          throw ex;
        }

        // compute crc and verify crc
        std::vector<uint8_t> c{d.begin() + 1, d.end() - 2};
        uint8_t crc_data = crc(c);
        
        if (crc_data != d[d.size()-2]) {
          std::ostringstream os;
          os << "Wrong crc. Computed:" << std::hex << unsigned(crc_data) << " received:" << unsigned(d[d.size()-2]);
          std::logic_error ex(os.str().c_str());
          throw ex;
        }

        // check number of received elements
        uint8_t nr_of_elements = c[0] + 1;
        if ((c.size() - 2) != nr_of_elements) {
          std::ostringstream os;
          os << "Inconsistency in elements count. Expected:" << unsigned(nr_of_elements) << " but get:" << unsigned(c.size() - 2);
          std::logic_error ex(os.str().c_str());
          throw ex;
        }
        
        if (c[1] != m_id) {
          std::ostringstream os;
          os << "Invalid ID. Expected:" << std::hex << unsigned(m_id) << " received:" << unsigned(c[1]);
          std::logic_error ex(os.str().c_str());
          throw ex;
          
          return false;
        }
        
        // get data only
        c.erase (c.begin(),c.begin()+2);

        *t = s.deserialize(c);
        return true;
      }

      void setCommandId(uint8_t id) {
        m_id = id;
      }

      std::vector<uint8_t> get_serialized_data() const {
        return m_data;
      }

      std::vector<uint8_t> get_raw_deserializied_data() const {
        return m_recv_data;
      }
    private:
      /* simple crc helper */
      uint8_t crc(const std::vector<uint8_t> &data) {
        uint32_t val = 0;
        for(auto x : data) {
          val += x;
        }
        return val % 256;
      }

    private:
      std::vector<uint8_t> m_data;
      std::vector<uint8_t> m_recv_data;
      uint8_t m_id;
      
      // constants
      const uint8_t start_byte = 0xB1;
      const uint8_t stop_byte = 0xB2;
  };

  /**********************************************/
  /************** General Commands **************/
  /**********************************************/

  class GetVersionCmd : public BaseCommand {
    public:
      GetVersionCmd() 
      : BaseCommand(COMMANDS_TYPE::CMD_GET_VERSION) {}
      
      typedef struct data_t {
        UINT8 fw_version_minor;
        UINT8 fw_version_major;
        UINT8 fw_pre_release_nr;
        UINT8 hw_variant;
      } data;
  };

  /**********************************************/
  /************** Settings and schedules **************/
  /**********************************************/

  class SetSettingsCmd : public BaseCommand {
    public:
      SetSettingsCmd() 
      : BaseCommand(COMMANDS_TYPE::CMD_SET_SETTINGS) {}

      PACK(struct data_send_t {
        UINT8 system_settings_page;
        System_Compressed_Date date;
      });
      
      struct data_t {
        UINT8 status;
      } data;

    void set_system_settings_page(uint32_t page = 0) { data_sent.data().system_settings_page = page;} 
    void set_date(uint32_t day_of_month = 0, uint32_t month = 0, uint32_t year = 0) { 
      data_sent.data().date.set_day_of_month(day_of_month);
      data_sent.data().date.set_month(month);
      data_sent.data().date.set_year(year);
    } 
      BaseData<data_send_t> data_sent;
  };

  class GetSettingsCmd : public BaseCommand {
    public:
      GetSettingsCmd()
      : BaseCommand(COMMANDS_TYPE::CMD_GET_SETTINGS) {}

      struct data_send_t {
        UINT8 system_settings_page;
      };

      PACK(struct data_t {
        UINT8 system_settings_page;
        System_Settings_page0 page;
      });
  };

  class SetScheduleCmd : public BaseCommand {
    public:
      SetScheduleCmd()
      : BaseCommand(COMMANDS_TYPE::CMD_SET_SCHEDULE) {}

      PACK(struct data_send_t {
        UINT8 schedule_selection;
        Test_Schedule schedule_data;
      });

      struct data_t{
        UINT8 status;
      };
  };

/****************************************************
************** Time and date commands ***************
****************************************************/
  class SetTimeAndDateCmd : public BaseCommand {
    public:
      SetTimeAndDateCmd()
      : BaseCommand(COMMANDS_TYPE::CMD_SET_DATE_AND_TIME) {}

      PACK(struct data_send_t {
        UINT8 time_second;
        UINT8 time_minute;
        UINT8 time_hour;
        UINT8 date_day;
        UINT8 date_month;
        UINT8 date_year;
        UINT8 date_century;
      });

      struct data_t{
        UINT8 status;
      };
  };

  class GetTimeAndDateCmd : public BaseCommand {
    public:
      GetTimeAndDateCmd()
      : BaseCommand(COMMANDS_TYPE::CMD_SET_DATE_AND_TIME) {}

      PACK(struct data_t {
        UINT8 time_second;
        UINT8 time_minute;
        UINT8 time_hour;
        UINT8 date_day;
        UINT8 date_month;
        UINT8 date_year;
        UINT8 date_century;
      });
  };

  class PresetTimeAndDateCmd : public BaseCommand {
    public:
      PresetTimeAndDateCmd()
      : BaseCommand(COMMANDS_TYPE::CMD_PRESET_DATE_AND_TIME) {}

      PACK(struct data_send_t {
        UINT8 time_second;
        UINT8 time_minute;
        UINT8 time_hour;
        UINT8 date_day;
        UINT8 date_month;
        UINT8 date_year;
        UINT8 date_century;
      });

      struct data_t{
        UINT8 status;
      };
  };

  class StartRtc : public BaseCommand {
    public:
      StartRtc()
      : BaseCommand(COMMANDS_TYPE::CMD_PRESET_DATE_AND_TIME) {}
  };

  class ChangeRtcToPresetCmd : public BaseCommand {
    public:
      ChangeRtcToPresetCmd()
      : BaseCommand(COMMANDS_TYPE::CMD_CHANGE_RTC_TO_PRESET) {}

      struct data_t {
        UINT8 status;
        FLAGS8 result;
      };
  };

  class TimeSync : public BaseCommand {
    public:
      TimeSync()
      : BaseCommand(COMMANDS_TYPE::CMD_PRESET_DATE_AND_TIME) {}
  };
}




