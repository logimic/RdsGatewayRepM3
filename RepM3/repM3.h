#pragma once

#include <string>
#include <vector>
#include <cstring>
#include <iostream> 
#include <algorithm>

namespace lgmc {

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

    void operator=(uint other) {
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

  struct System_Compressed_Date {
    UINT16 data;
    
    void set_day_of_month(uint day) {
      data |= (day & 0x1F);

    }
    uint day_of_month() {
      return uint(data &0x1F);
    }

    void set_month(uint month) {
      data |= (month & 0xF) << 5;
    }

    uint month() {
      return uint((data >> 5) & 0xF);
    }

    void set_year(uint year) {
      data |= (year & 0x7F) << 9;
    }

    uint year() {
      return uint((data >> 9) & 0x7F);
    }

    bool operator==(System_Compressed_Date other) const {
      return data == other.data;
    }
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
        CMD_SET_SETTINGS = 16,
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
          std::cout << "Wrong start or stop byte" << std::endl;
          return false;
        }

        // compute crc and verify crc
        std::vector<uint8_t> c{d.begin() + 1, d.end() - 2};
        uint8_t crc_data = crc(c);
        
        if (crc_data != d[d.size()-2]) {
          std::cout << "Wrong crc. Computed:" << crc_data << " received:" << d[d.size()-2];
          return false;
        }

        // check number of received elements
        uint8_t nr_of_elements = c[0] + 1;
        if ((c.size() - 2) != nr_of_elements) {
          std::cout << "Inconsistency in elements count. Expected:" << int(nr_of_elements) << " but get:" << (c.size() - 2) << std::endl;
          return false;
        }
        
        if (c[1] != m_id) {
          std::cout << "Invalid ID. Expected:" << m_id << " received:" << c[1] << std::endl;
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
        uint val = 0;
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

      struct data_send_t {
        UINT8 system_settings_page;
        System_Compressed_Date date;
      } __attribute__((packed));
      
      struct data_t {
        UINT8 status;
      } data;

    void set_system_settings_page(uint page = 0) { data_sent.data().system_settings_page = page;} 
    void set_date(uint day_of_month = 0, uint month = 0, uint year = 0) { 
      data_sent.data().date.set_day_of_month(day_of_month);
      data_sent.data().date.set_month(month);
      data_sent.data().date.set_year(year);
    } 
      BaseData<data_send_t> data_sent;
  };
}
