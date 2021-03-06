#pragma once

#include <string>
#include <vector>
#include <cstring>
#include <iostream> 
#include <sstream>
#include <algorithm>
#include <exception>
#include <chrono>

namespace lgmc {

/* definitions for packed structures for GCC and MSC */
#ifdef __GNUC__
#define PACK( __Declaration__ ) __Declaration__ __attribute__((__packed__))
#endif

#ifdef _MSC_VER
#define PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop))
#endif

// define generic get/set methods for specific bit
#define F(nr) \
  bool f##nr() { \
    return data & (nr + 1); \
    } \
  void setf##nr(bool val) { \
    if (val) { \
      data |= (1 << nr); \
    } else { \
      data &= ~(1 << nr); \
    } \
  } \


  // 8-bit unsigned integer 
  struct UINT8 {
    UINT8(): data(0) {}
    UINT8(int d) : data(d) {}   
    
    uint8_t data;
    // compare operator
    bool operator==(UINT8 other) const {
      return data == other.data;
    }
    // compare for gtest EQUAL
    bool operator==(int other) const {
      return data == (uint8_t)other;
    }

    bool operator!=(int other) const {
      return data != (uint8_t)other;
    }

    void operator=(uint32_t other) {
      data = uint8_t(other);
    }

    operator int() const {
      return (int)data;
    }
  };

  // 14-bit unsigned integer
  // most significant 2 bits of the second byte are unused
  PACK(struct UINT14 {
    uint16_t data = 0;

    UINT14 & operator=(UINT14 & other) {
      if (this == &other) {
        return *this;
      }
      data = other.data & 0x3FFF; // only 14 byte
      return *this;
    }
  });

  // 16-bit unsigned integer
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

  // 1 byte containing a set of 8 boolean values (flags) designated F0 to F7
  struct FLAGS8 {
    uint8_t data;
    F(0); F(1); F(2); F(3);
    F(4); F(5); F(6); F(7);
  };

  // 2 bytes containing a set of 14 boolean values (flags) designated F0 to F13
  struct FLAGS14 {
    uint16_t data;
    F(0); F(1); F(2); F(3);
    F(4); F(5); F(6); F(7);
    F(8); F(9); F(10); F(11);
    F(12); F(13);
  };

  // 2 bytes containing a set of 16 boolean values (flags) designated F0 to F15
  struct FLAGS16 {
    uint16_t data;
    F(0); F(1); F(2); F(3);
    F(4); F(5); F(6); F(7);
    F(8); F(9); F(10); F(11);
    F(12); F(13); F(14); F(15);
  };

  // 4 byte containing a set of 32 boolean values (flags) designated F0 to F31
  struct FLAGS32 {
    uint32_t data;
    F(0); F(1); F(2); F(3);
    F(4); F(5); F(6); F(7);
    F(8); F(9); F(10); F(11);
    F(12); F(13); F(14); F(15);
    F(16); F(17); F(18); F(19);
    F(20); F(21); F(22); F(23);
    F(24); F(25); F(26); F(27);
    F(28); F(29); F(30); F(31);
  };
 
  // 1 byte representing an integer value between 1 and 1920.
  // The value of this data is determined by the following expression:
  // VALUE = (BASE_VALUE + 1) * M
  struct COMPTIME8 {
    uint8_t data;

    uint32_t value() {
      int base_value = data & 0x3F;
      int m = (data >> 6) & 0x3;

      switch (m) {
        case 0:
          m = 1;
          break;
        case 1:
          m = 5;
          break;
        case 2:
          m = 15;
          break;
        case 3:
          m = 30;
          break;
      }

      return (base_value + 1) * m;
    } 
  };

  // 1 byte containing the status of the cells connected to the RepM3
  // B0-B3: 4-bit CellStatus Top Cell
	// B7-B4: 4-bit CellStatus Bottom Cell
  struct BATTSTATUS8 {
    uint8_t data;

    enum charge_status {
      CELL_DISCONNECTED = 0,
      CELL_DISCHARGED = 1,
      CELL_PART_CHARGED = 2,
      CELL_FULLY_CHARGED = 3,
    };

    enum cell_type {
      CELL_NOT_FITTED = 0,
      CELL_SINGLE = 1,
      CELL_DUAL = 2,
      UNUSED = 3,
    };

    enum charge_status cell_status_val() {return charge_status(data & 0x0F);}
    enum cell_type cell_type_val() {return cell_type((data >> 4) & 0xF);}
  };

  // indicate information is available to be downloaded from the RepM3
  struct GreenFlags {
    FLAGS16 data;

    bool long_test_pass_report_avail() {return data.f0();}
    bool short_test_pass_report_avail() {return data.f1();}
    bool long_test_fail_report_avail() {return data.f2();}
    bool short_test_fail_report_avail() {return data.f3();}
  };

  // indicate potential problems have been detected
  struct YellowFlags {
    FLAGS16 data;

    bool long_test_report_overwritten() {return data.f0();}
    bool short_test_report_overwritten() {return data.f1();}
    bool bad_time_sync() {return data.f2();}
  };

  // indicate faults that have been detected
  struct RedFlags {
    FLAGS16 data;

    bool rtc_not_running() {return data.f0();}
    bool long_test_failed() {return data.f1();}
    bool short_test_failed() {return data.f2();}
    bool led_over_voltage_trip() {return data.f3();}
    bool led_Low_load() {return data.f4();}
    bool led_over_current_trip() {return data.f5();}
    bool cells_enable_failed() {return data.f6();}
  };

  // flags indicating system state
  struct SystemFlags {
    FLAGS8 data;

    bool analogue_up() {return data.f0();}
    bool psu_up() {return data.f1();}
    bool mains_on() {return data.f2();}
    bool switched_mains_on() {return data.f3();}
    bool cells_enabled() {return data.f4();}   
    bool low_led_current() {return data.f5();}
    bool long_test_running() {return data.f6();}
    bool short_test_running() {return data.f7();}       
  };

  // More flags indicating system state
  struct SystemFlags2 {
    FLAGS8 data;

    bool short_test_today() {return data.f0();}
    bool long_test_today() {return data.f1();}
    bool setting_reset_pending() {return data.f2();}
  };

  // Part of the settings structure
  struct SystemSettingsFlags {
    FLAGS14 data;

    bool low_led_current_enabled() {return data.f0();}
    bool has_external_driver() {return data.f1();}
    bool use_switched_mains_state() {return data.f2();}    
    bool reley_init_state() {return data.f3();}
    bool has_rf() {return data.f4();}    
    bool activate_on_failure() {return data.f5();}
  };

  // UINT16 formatted as follows:
  // B4-B0: 5 bit unsigned integer representing day of month. 0 = 1st of month, 30 = 31st of month.
  // B5-B8: 4 bit unsigned integer representing month. 0 = January, 11 = December
  // B9-B15: 7 bit unsigned integer representing year in 2 digit format (0-99)
  struct System_Compressed_Date {
    UINT16 data;
    
    void set_day_of_month(uint32_t day) {
      data |= (day & 0x1F);
    }

    uint32_t day_of_month() {
      return uint32_t(data &0x1F);
    }

    void set_month(uint32_t month) {
      data |= (month & 0xF) << 5;
    }

    uint32_t month() {
      return uint32_t((data >> 5) & 0xF);
    }

    void set_year(uint32_t year) {
      data |= (year & 0x7F) << 9;
    }

    uint32_t year() {
      return uint32_t((data >> 9) & 0x7F);
    }

    bool operator==(System_Compressed_Date other) const {
      return data == other.data;
    }
  };

  // UINT16 formatted as follows:
  // B4-B0: 5-bit unsigned integer representing seconds in units of 2 seconds (0-29)
  // B5-B10: 6-bit unsigned integer representing minute (0-59)
  // B11-B15: 5-bit unsigned integer representing hour (0-23)
  struct System_Compressed_Time {
    UINT16 data;
    
    void set_seconds(uint32_t secs) {
      data |= (secs & 0x1F);

    }
    uint32_t seconds() {
      return uint32_t(data &0x1F);
    }

    void set_minute(uint32_t minute) {
      data |= (minute & 0x3F) << 5;
    }

    uint32_t minute() {
      return uint32_t((data >> 5) & 0x3F);
    }

    void set_hours(uint32_t hour) {
      data |= (hour & 0x1F) << 11;
    }

    uint32_t hour() {
      return uint32_t((data >> 11) & 0x1F);
    }

    bool operator==(System_Compressed_Time other) const {
      return data == other.data;
    }
  };

  // A representation of time of day, with 1 second resolution.
  // Second: UINT8 - unsigned integer representing minute (0-59)
  // Minute: UINT8 - unsigned integer representing minute (0-59)
  // Hour: UINT8 - unsigned integer representing hour (0-23)
  struct SystemTime {
    UINT8 sec;
    UINT8 min;
    UINT8 hr;

    UINT8 seconds() {return sec;}
    void set_seconds(UINT8 seconds) {sec = seconds;}

    UINT8 minute() {return min;}
    void set_minute(UINT8 minute) {min = minute;}

    UINT8 hour() {return hr;}
    void set_hour(UINT8 hour) {hr = hour;}
  };

  // FLAGS14 ??? set of flags specifying boolean settings
  // LED Target Current Maintained Mode:  UINT14 ??? LED Current in 62.5uA units
  // LED Target Voltage Maintained Mode: UINT14 ??? LED Voltage in 5mV units
  // LED Target Power Maintained Mode:  UINT14 ??? LED Power in 1mW units
  // LED Target Current Emergency Mode:  UINT14 ??? LED Current in 62.5uA units
  // LED Target Voltage Emergency Mode:  UINT14 ??? LED Voltage in 5mV units
  // LED Target Power Emergency Mode:  UINT14 ??? LED Power in 1mW units
  // LED Test Voltage Lower Limit:  UINT14 ??? LED Voltage in 5mV units
  // LED Test Voltage Upper Limit:  UINT14 ??? LED Voltage in 5mV units
  // reserved
  PACK(struct System_Settings_page0 {
    UINT14 current_maintained_mode;
    UINT14 voltage_maintained_mode;
    UINT14 power_maintained_mode;
    UINT14 current_emergency_mode;
    UINT14 voltage_emergency_mode;
    UINT14 power_emergency_mode;
    UINT14 test_voltage_lower_limit;
    UINT14 test_voltage_upper_limit;
    UINT14 reserved[7];
  });

  // Report generated following a duration test
  PACK(struct Long_Test_Report {
    System_Compressed_Date start_date;
    System_Compressed_Time start_time;
    COMPTIME8 test_duration;
    BATTSTATUS8 start_cell_mode;
    UINT16 start_bottom_cell_volts;
    UINT8 start_load_volts;
    UINT8 start_load_current;
    UINT16 test_duration_achieved;
    UINT8 test_duration_achieved_with_both_cells;
    UINT16 end_bottom_cell_volts;
    UINT16 end_top_cell_volts;
    UINT8 end_load_volts;
    UINT8 end_load_current;
    UINT16 bot_cell_amp_hours;
    UINT16 top_cell_amp_hours;
    UINT16 bot_cell_watt_hours;
    UINT16 top_cell_watt_hours;
    UINT8 exponents;
    FLAGS8 test_flags;
  });

  // Report generated following a basic function test
  struct Short_Test_Report {
     System_Compressed_Date start_date;
    System_Compressed_Time start_time;
    BATTSTATUS8 start_cell_mode;
    UINT16 bottom_cell_volts;
    UINT16 top_cell_volts;
    UINT16 load_volts;
    UINT16 load_current;
    FLAGS8 test_flags;
  };

  // specifies when a test will begin
  struct Test_Schedule {
    SystemTime start_time;
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

      T data() { return m_data; }

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

template <typename S, typename R>
  class BaseCommand {
    public:
      // command ids
 
      BaseCommand(){}
      
      BaseCommand(uint8_t cmd_id)
      : m_id(cmd_id){}
      
      virtual ~BaseCommand() {
      }
      
      /* serialize data without parameters */
      std::vector<uint8_t> serialize(bool security_bytes = false) {
        m_data.push_back(start_byte);
        if (security_bytes != false) {
          // security bytes have always 4 bytes
          m_data.push_back(1 + 4);
          m_data.push_back(m_id);
          std::vector<uint8_t> security = computeSecurityBytes(m_id, 1 + 4);
          m_data.insert(m_data.end(), security.begin(), security.end());
        } else {
          m_data.push_back(1);
          m_data.push_back(m_id);
        }

        std::vector<uint8_t> c{m_data.begin() + 1, m_data.end()};

        m_data.push_back(crc(c));
        m_data.push_back(stop_byte);

        return m_data;
      }

      /* serialize data with parameters */
      //template <typename T>
      std::vector<uint8_t> serialize(S d, bool security_bytes = false) {
        BaseData<S> s(d);
        m_data.push_back(start_byte);
        if (security_bytes) {
          m_data.push_back(sizeof(d) + 1 + 4);
        } else {
          m_data.push_back(sizeof(d) + 1);
        }
        m_data.push_back(m_id);
        
        std::vector<uint8_t> ser = s.serialize();
        if (security_bytes) {
          std::vector<uint8_t> security = computeSecurityBytes(m_id, 1 + 4 + ser.size(), ser);
          m_data.insert(m_data.end(), security.begin(), security.end());
        }
        m_data.insert(m_data.end(), ser.begin(), ser.end());
        
        std::vector<uint8_t> c{m_data.begin() + 1, m_data.end()};

        m_data.push_back(crc(c));
        m_data.push_back(stop_byte);

        return m_data;
      }

      //template <typename T>
      bool deserialize(std::vector<uint8_t> d) {
        BaseData<R> s;
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
        std::vector<uint8_t> c{d.cbegin() + 1, d.cend() - 2};
        uint8_t crc_data = crc(c);
        
        if (crc_data != d[d.size()-2]) {
          std::ostringstream os;
          os << "Wrong crc. Computed:" << std::hex << unsigned(crc_data) << " received:" << unsigned(d[d.size()-2]);
          std::logic_error ex(os.str().c_str());
          throw ex;
        }

        // check number of received elements
        uint8_t nr_of_elements = c[0];
   
        if ((c.size() - 1) != nr_of_elements) {
          std::ostringstream os;
          os << "Inconsistency in elements count. Expected:" << unsigned(nr_of_elements) << " but get:" << unsigned(c.size() -1 );
          std::logic_error ex(os.str().c_str());
          throw ex;
        }
        // c[1] is command ID
        if (c[1] != m_id) {
          std::ostringstream os;
          os << "Invalid ID. Expected:" << std::hex << unsigned(m_id) << " received:" << unsigned(c[1]);
          std::logic_error ex(os.str().c_str());
          throw ex;
          
          return false;
        }
        
        // get data only
        c.erase (c.begin(),c.begin()+2);

        m_recv_type = s.deserialize(c);
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

      R getType() const {
        return m_recv_type;
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

      std::vector<uint8_t> computeSecurityBytes(uint8_t id, int len, std::vector<uint8_t> d = std::vector<uint8_t>()) {
        std::vector<uint8_t> ret{9, 227, 0, id};
        uint32_t val = 0;
        uint8_t lenb = 0xFF & len;

        //lenb += 1;
        val += 0xFF & (~(lenb+1));

        val += 0xFF & (~(id + 1));

        for (auto x : d) {
          val += 0xFF & (~(x + 1));
        }

        ret[2] = val % 256;

        return ret;
      }

    private:
      std::vector<uint8_t> m_data;
      std::vector<uint8_t> m_recv_data;
      uint8_t m_id;
      // deserialized type
      R m_recv_type;
      
      // constants
      const uint8_t start_byte = 0xB1;
      const uint8_t stop_byte = 0xB2;
  };

  /**********************************************/
  /************** General Commands **************/
  /**********************************************/

  // none struct used in argument when there is no send/receive payload
  struct none { struct value_type {}; };

  enum CommandNumber {
    CMD_GET_SYSTEM_STATUS_1 = 6,
    CMD_GET_VERSION = 9,
    CMD_SET_DATE_AND_TIME = 12,
    CMD_GET_DATE_AND_TIME = 13,
    CMD_START_RTC = 14,
    CMD_GET_SETTINGS = 15,
    CMD_SET_SETTINGS = 16,
    CMD_PRESET_DATE_AND_TIME = 18,
    CMD_SET_SCHEDULE = 19,
    CMD_CHANGE_RTC_TO_PRESET = 23,
    CMD_GET_REPORT = 24,
    CMD_ACKNOWLEDGE_REPORT = 24,
    CMD_GET_FLAGS = 26,
    CMD_TIME_SYNC = 27,
    CMD_RESET_FLAGS = 28,
  };

  template <typename T, typename S, uint8_t id>
  class Impl : public BaseCommand<T, S> {
    public:
      Impl()
      : BaseCommand<T,S>(id) {}
  };
  
  class GetVersionCmd {
    public:
      GetVersionCmd() {}
      PACK(struct data_t {
        UINT8 fw_version_minor;
        UINT8 fw_version_major;
        UINT8 fw_pre_release_nr;
        UINT8 hw_variant;
      });

      void serialize(std::vector<uint8_t> &d) {
        d = impl.serialize(); 
      }

      std::vector<uint8_t> serialize() {
        return impl.serialize();
      }

      void deserialize(const std::vector<uint8_t> &d) {
        impl.deserialize(d);
      }

      data_t getData() {
        return impl.getType();
      }

  private:
      Impl<none, GetVersionCmd::data_t, CMD_GET_VERSION> impl;
  };

  /**********************************************/
  /************** Settings and schedules **************/
  /**********************************************/

  class SetSettingsCmd {
    public:
     
      PACK(struct data_send_t {
        UINT8 system_settings_page;
        System_Compressed_Date date;
      });
      
      struct data_t {
        UINT8 status;
      } data;

      void serialize(std::vector<uint8_t> &d) {
        d = impl.serialize(); 
      }

      std::vector<uint8_t> serialize() {
        return impl.serialize();
      }

      void deserialize(const std::vector<uint8_t> &d) {
        impl.deserialize(d);
      }

      data_t getData() {
        return impl.getType();
      }

      bool getStatus() {
        return (getData().status.data == 170); 
      }

  private:
      Impl<SetSettingsCmd::data_send_t, SetSettingsCmd::data_t, CMD_SET_SETTINGS> impl;

    /*void set_system_settings_page(uint32_t page = 0) { data_sent.data().system_settings_page = page;} 
    void set_date(uint32_t day_of_month = 0, uint32_t month = 0, uint32_t year = 0) { 
      data_sent.data().date.set_day_of_month(day_of_month);
      data_sent.data().date.set_month(month);
      data_sent.data().date.set_year(year);
    }*/ 
  };

  class GetSettingsCmd {
    public:
      struct data_send_t {
        UINT8 system_settings_page;
      };

      PACK(struct data_t {
        UINT8 system_settings_page;
        System_Settings_page0 page;

        void operator=(struct data_t other) {
          system_settings_page = other.system_settings_page;
          page = other.page;
        };
        
      });

      void serialize(std::vector<uint8_t> &d) {
        d = impl.serialize(m_systemPage, true); 
      }

      std::vector<uint8_t> serialize() {
        return impl.serialize(m_systemPage, true);
      }

      void deserialize(const std::vector<uint8_t> &d) {
        impl.deserialize(d);
      }

      data_t getData() {
        return impl.getType();
      }

      void setPage(const UINT8 page) {
        m_systemPage.system_settings_page = page;
      }

  private:
    Impl<GetSettingsCmd::data_send_t, GetSettingsCmd::data_t, CMD_GET_SETTINGS> impl;
  
  protected:
    data_send_t m_systemPage;
  };

  class SetScheduleCmd {
      PACK(struct data_send_t {
        UINT8 schedule_selection;
        Test_Schedule schedule_data;
      });

      struct data_t{
        UINT8 status;
      };

      void serialize(std::vector<uint8_t> &d) {
        d = impl.serialize(); 
      }

      std::vector<uint8_t> serialize() {
        return impl.serialize();
      }

      void deserialize(const std::vector<uint8_t> &d) {
        impl.deserialize(d);
      }

      data_t getData() {
        return impl.getType();
      }

      bool getStatus() {
        return (getData().status.data >= 0 && getData().status.data <= 6);
      }

  private:
    Impl<SetScheduleCmd::data_send_t, SetScheduleCmd::data_t, CMD_SET_SCHEDULE> impl;
  };

/****************************************************
************** Time and date commands ***************
****************************************************/
  // helper class for defining common struct for date/time and conversions
  class DateTimeBase {
    public:
      PACK(struct data_send_t {
        //UINT8 sc0;
        //UINT8 sc1;
        //UINT8 sc2;
        //UINT8 sc3;
        UINT8 time_second;
        UINT8 time_minute;
        UINT8 time_hour;
        UINT8 date_day;
        UINT8 date_month;
        UINT8 date_year;
        UINT8 date_century;
      });

      PACK(struct data_recv_t {
        UINT8 time_second;
        UINT8 time_minute;
        UINT8 time_hour;
        UINT8 date_day;
        UINT8 date_day_of_week;
        UINT8 date_month;
        UINT8 date_year;
        UINT8 date_century;
      });

      // convert from timepoint to low level struct for sending data
      data_send_t convertFromTimePoint(const std::chrono::system_clock::time_point & tim) {
        data_send_t time;
        time_t rawtime = std::chrono::system_clock::to_time_t(tim);
        tm * tm = localtime(&rawtime);

        time.time_second = tm->tm_sec;
        time.time_hour = tm->tm_hour;
        time.time_minute = tm->tm_min;
        // day is from 1 - 31 (we expect 0 - 30)
        time.date_day = tm->tm_mday - 1;
        time.date_month = tm->tm_mon;
        // tm_year is number of years from 1900
        time.date_year = (tm->tm_year + 1900)% 100;
        time.date_century = centuryFromYear(tm->tm_year + 1900); 
        return time;
      }

      std::chrono::system_clock::time_point convertToTimePoint(data_recv_t data) const {
        tm time;
        time.tm_sec = data.time_second.data;
        time.tm_min = data.time_minute.data;
        time.tm_hour = data.time_hour.data;
        time.tm_mday = data.date_day.data + 1;
        time.tm_wday = data.date_day_of_week.data;
        time.tm_mon = data.date_month.data;
        
        time.tm_year = yearFromCentury(data.date_year.data, data.date_century.data) - 1900;

        time_t t = mktime(&time);

        return std::chrono::system_clock::from_time_t(t);
      }

      // helper for converting year to century
      static int centuryFromYear(int year) {
        if (year < 0 || year > 9999) {
          std::ostringstream os;
          os << "Year is expected > 0 && < 9999, but passed: " << year;
          std::logic_error ex(os.str().c_str());
          throw ex;
        }
        return year / 100;
      }

      static int yearFromCentury(uint8_t year, uint8_t century) {
        if (year > 99 || century > 99) {
          std::ostringstream os;
          os << "year and century is expected < 99, but passed year: " << year << " century: " << century;
          std::logic_error ex(os.str().c_str());
          throw ex;
        }
        return (century * 100) + year;
      }
  };

  class SetTimeAndDateCmd : public DateTimeBase {
    public:
      
      struct data_t{
        UINT8 status;
      };

      void serialize(std::vector<uint8_t> &d, bool security = false) {
        d = impl.serialize(m_time, security);
      }

      std::vector<uint8_t> serialize(bool security = false) {
        return impl.serialize(m_time, security);
      }

      void deserialize(const std::vector<uint8_t> &d) {
        impl.deserialize(d);
      }

      data_t getData() {
        return impl.getType();
      }

  private:
    Impl<SetTimeAndDateCmd::data_send_t, SetTimeAndDateCmd::data_t, CMD_SET_DATE_AND_TIME> impl;

  protected:
    data_send_t m_time;
  };

  class GetTimeAndDateCmd : public DateTimeBase {
    public:
     
      void serialize(std::vector<uint8_t> &d) {
        d = impl.serialize(); 
      }

      std::vector<uint8_t> serialize() {
        return impl.serialize();
      }

      void deserialize(const std::vector<uint8_t> &d) {
        impl.deserialize(d);
      }

      data_recv_t getData() const {
        return impl.getType();
      }

  private:
    Impl<none, GetTimeAndDateCmd::data_recv_t, CMD_GET_DATE_AND_TIME> impl;
  };

  class PresetTimeAndDateCmd : public DateTimeBase {
    public:
     
      struct data_t{
        UINT8 status;
      };

      void serialize(std::vector<uint8_t> &d) {
        d = impl.serialize(m_time); 
      }

      std::vector<uint8_t> serialize() {
        return impl.serialize(m_time);
      }

      void deserialize(const std::vector<uint8_t> &d) {
        impl.deserialize(d);
      }

      data_t getData() {
        return impl.getType();
      }

  private:
    Impl<PresetTimeAndDateCmd::data_send_t, PresetTimeAndDateCmd::data_t, CMD_PRESET_DATE_AND_TIME> impl;
  
  protected:
    data_send_t m_time;
  };

  class StartRtc {
    public:
     
      void serialize(std::vector<uint8_t> &d) {
        d = impl.serialize(); 
      }

      std::vector<uint8_t> serialize() {
        return impl.serialize();
      }

      void deserialize(const std::vector<uint8_t> &d) {
        impl.deserialize(d);
      }

  private:
    Impl<none, none, CMD_START_RTC> impl;
  };

  class ChangeRtcToPresetCmd {
    public:
     
      struct data_t {
        UINT8 status;
        FLAGS8 result;
      };

      void serialize(std::vector<uint8_t> &d, bool security = false) {
        d = impl.serialize(security); 
      }

      std::vector<uint8_t> serialize(bool security = false) {
        return impl.serialize(security);
      }

      void deserialize(const std::vector<uint8_t> &d) {
        impl.deserialize(d);
      }

      data_t getData() {
        return impl.getType();
      }

  private:
    Impl<none, ChangeRtcToPresetCmd::data_t, CMD_CHANGE_RTC_TO_PRESET> impl;
  };

  class TimeSyncCmd {
    public:
           
      void serialize(std::vector<uint8_t> &d) {
        d = impl.serialize(); 
      }

      std::vector<uint8_t> serialize() {
        return impl.serialize();
      }

      void deserialize(const std::vector<uint8_t> &d) {
        impl.deserialize(d);
      }
      
  private:
    Impl<none, none, CMD_TIME_SYNC> impl;
  };

/************************************************
************** Status and reports ***************
*************************************************/
class GetFlagsCmd {
    public:
      
      PACK(struct data_t {
        FLAGS16 info_flags;
        FLAGS16 warning_flags;
        FLAGS16 error_flags;
        FLAGS8 system_status_info;
        FLAGS8 additional_status_info;
      });

      void serialize(std::vector<uint8_t> &d) {
        d = impl.serialize(); 
      }

      std::vector<uint8_t> serialize() {
        return impl.serialize();
      }

      void deserialize(const std::vector<uint8_t> &d) {
        impl.deserialize(d);
      }

      data_t getData() const {
        return impl.getType();
      }

  private:
    Impl<none, GetFlagsCmd::data_t, CMD_GET_FLAGS> impl;
  };

  class ResetFlagsCmd {
    public:
          
      void serialize(std::vector<uint8_t> &d) {
        d = impl.serialize(); 
      }

      std::vector<uint8_t> serialize() {
        return impl.serialize();
      }

      void deserialize(const std::vector<uint8_t> &d) {
        impl.deserialize(d);
      }

  private:
    Impl<none, none, CMD_RESET_FLAGS> impl;
  };

// GetReport can return 2 different responses thus we use this as base
class GetReportCmd {
    public:
      struct data_send_t {
        UINT8 report_index;
      };
};

class GetReportShortCmd : public GetReportCmd {
  public:
   
      PACK(struct data_t_short{
        UINT8 index;
        Short_Test_Report rep;
      });

      void serialize(std::vector<uint8_t> &d) {
        d = impl.serialize(m_idxShort); 
      }

      std::vector<uint8_t> serialize() {
        return impl.serialize(m_idxShort);
      }

      void deserialize(const std::vector<uint8_t> &d) {
        impl.deserialize(d);
      }

      data_t_short getData() {
        return impl.getType();
      }

  private:
    Impl<GetReportShortCmd::data_send_t, GetReportShortCmd::data_t_short, CMD_GET_REPORT> impl;
  
  protected:
    data_send_t m_idxShort;
  };

  class GetReportLongCmd : public GetReportCmd{
    public:

      PACK(struct data_t_long{
        UINT8 index;
        Long_Test_Report rep;
      });

      void serialize(std::vector<uint8_t> &d) {
        d = impl.serialize(m_idxLong); 
      }

      std::vector<uint8_t> serialize() {
        return impl.serialize(m_idxLong);
      }

      void deserialize(const std::vector<uint8_t> &d) {
        impl.deserialize(d);
      }

      data_t_long getData() {
        return impl.getType();
      }

  private:
    Impl<GetReportCmd::data_send_t, GetReportLongCmd::data_t_long, CMD_GET_REPORT> impl;
  
  protected:
    data_send_t m_idxLong;
  };

  class AcknowledgeReportCmd {
    public:
      
      struct data_send_t {
        UINT8 report_index;
      };

      struct data_t{
        GreenFlags green_flags;
        RedFlags red_flags;
      };

      void serialize(std::vector<uint8_t> &d) {
        d = impl.serialize(m_data); 
      }

      std::vector<uint8_t> serialize() {
        return impl.serialize(m_data);
      }

      void deserialize(const std::vector<uint8_t> &d) {
        impl.deserialize(d);
      }

      data_t getData() {
        return impl.getType();
      }

  private:
    Impl<AcknowledgeReportCmd::data_send_t, AcknowledgeReportCmd::data_t, CMD_ACKNOWLEDGE_REPORT> impl;

  protected:
    data_send_t m_data;
  };

  class GetSystemStatus1Cmd {
    public:
      
      void serialize(std::vector<uint8_t> &d) {
        d = impl.serialize(); 
      }

      std::vector<uint8_t> serialize() {
        return impl.serialize();
      }

      void deserialize(const std::vector<uint8_t> &d) {
        impl.deserialize(d);
      }

  private:
    Impl<none, none, CMD_GET_SYSTEM_STATUS_1> impl;
  };
};
