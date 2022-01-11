#include "repM3.h"
#include "repM3_provider.h"
#include <iostream>
#include <string>

#include <gtest/gtest.h>

#include "rapidjson/writer.h"

using namespace lgmc;
using namespace rapidjson;

TEST(uint8, data_types) {
    StringBuffer sb;
    PrettyWriter<StringBuffer> writer(sb);
    UINT8 d {8};

    d.Serialize(writer);
    
    EXPECT_STREQ(sb.GetString(), "{\n    \"data\": 8\n}");
}

TEST(flags8, data_types) {
    FLAGS8 f;
    f.setf0(true);
    EXPECT_EQ(f.data, 0x1);

    f.setf1(true);
    EXPECT_EQ(f.data, 0x3);

    f.setf2(true);
    EXPECT_EQ(f.data, 0x7);

    f.setf3(true);
    EXPECT_EQ(f.data, 0xF);

    f.setf4(true);
    EXPECT_EQ(f.data, 0x1F);

    f.setf5(true);
    EXPECT_EQ(f.data, 0x3F);

    f.setf6(true);
    EXPECT_EQ(f.data, 0x7F);

    f.setf7(true);
    EXPECT_EQ(f.data, 0xFF);
}

TEST(green_flags, data_types) {
    GreenFlags f;
    f.data.setf0(true);

    EXPECT_EQ(f.long_test_pass_report_avail(), true);
}

TEST(uint14_struct, data_types) {
    //UINT14 t;

    //t.data = 0xffff;

    //EXPECT_EQ(t.data, 0x3FFF);
}


TEST(comptime8, data_types) {
    COMPTIME8 t;
    t.data = 0x00;

    EXPECT_EQ(t.value(), 1);

    t.data = 0x40;

    EXPECT_EQ(t.value(), 5);

    t.data = 0x80;
    EXPECT_EQ(t.value(), 15);

    t.data = 0xc0;
    EXPECT_EQ(t.value(), 30);
}

TEST(batt_status, data_types) {
    BATTSTATUS8 t;
    t.data = 0x01;

    EXPECT_EQ(t.cell_status_val(), BATTSTATUS8::CELL_DISCHARGED);

    t.data = 0x03;

    EXPECT_EQ(t.cell_status_val(), BATTSTATUS8::CELL_FULLY_CHARGED);

    t.data = 0x10;

    EXPECT_EQ(t.cell_type_val(), BATTSTATUS8::CELL_SINGLE);
}

TEST(data_serialize, data_handler) {
    UINT8 test_data{123};
    UINT8 test_data_ret;
    BaseData<UINT8> obj{test_data};
    
    std::vector<uint8_t> res = obj.serialize();
    test_data_ret = obj.deserialize(res);
    
    ASSERT_EQ(test_data, test_data_ret) << "Not equal";

}

TEST(data_serialize_nested, data_handler) {
    struct test {
        UINT8 a;
        System_Compressed_Date b;
        bool operator==(test other) const {
            return a == other.a && b == other.b;
        }
    };
    
    test t;
    t.a = 5;
    t.b.set_month(7);

    BaseData<test> t1;
    
    std::vector<uint8_t> res = t1.serialize(t);

    test t2 = t1.deserialize(res);
    EXPECT_EQ(t, t2);
}

TEST(data_system_compressed_date, data_handler) {
    
    for (int i = 0; i <= 30; i++) {
        System_Compressed_Date c;
        c.set_day_of_month(i);
        EXPECT_EQ(c.day_of_month(), i);
    }

    for (int i = 0; i <= 11; i++) {
        System_Compressed_Date c;
        c.set_month(i);
        EXPECT_EQ(c.month(), i);
    }

    for (int i = 0; i <= 99; i++) {
        System_Compressed_Date c;
        c.set_year(i);
        EXPECT_EQ(c.year(), i);
    }
}

TEST(data_system_compressed_time, data_handler) {
    
    for (int i = 0; i <= 29; i++) {
        System_Compressed_Time c;
        c.set_seconds(i);
        EXPECT_EQ(c.seconds(), i);
    }

    for (int i = 0; i <= 59; i++) {
        System_Compressed_Time c;
        c.set_minute(i);
        EXPECT_EQ(c.minute(), i);
    }

    for (int i = 0; i <= 23; i++) {
        System_Compressed_Time c;
        c.set_hours(i);
        EXPECT_EQ(c.hour(), i);
    }
}

TEST(command_serialize_base_no_data, command_handler) {
    struct test {
        UINT8 a;
        UINT8 b;
    };

    BaseCommand<BaseData<test>, none> cmd;

    test t = {5,7};
    BaseData<test> td{t};

    cmd.setCommandId(10);
    std::vector<uint8_t> buffer = cmd.serialize(td);

    // expected buffer
    std::vector<uint8_t> exp{0xb1,0x2,0xa,0x5,0x7, 0x18, 0xb2};

    ASSERT_EQ(buffer, exp) << "Buffers are not equal";    
}

TEST(command_serialize_base_with_data, command_handler) {
    BaseCommand<none, none> cmd;
    cmd.setCommandId(0x9);
    std::vector<uint8_t> buffer = cmd.serialize();

    // expected buffer
    std::vector<uint8_t> exp{0xb1,0x1,0x9,0xa,0xb2};

    ASSERT_EQ(buffer, exp) << "Buffers are not equal";
}

TEST(get_version_cmd, command_handler) {
    GetVersionCmd cmd;
    std::vector<uint8_t> exp{0xB1, 0x1, 0x9, 0xA, 0xB2};
    // verify serialization
    std::vector<uint8_t> sd;
    cmd.serialize(sd);

    EXPECT_EQ(sd, exp);

    // verify deserialization
    GetVersionCmd::data_t d;
    cmd.deserialize(std::vector<uint8_t>{0xB1,0x3,0x9,0x5,0x1,0x2,0x0,0x14,0xB2});
    d = cmd.getData();
    //EXPECT_EQ(ok, true);
    EXPECT_EQ(d.fw_version_minor, 0x5);
    EXPECT_EQ(d.fw_version_major, 0x1);
    EXPECT_EQ(d.fw_pre_release_nr, 0x2);
    EXPECT_EQ(d.hw_variant, 0x0);
}
#if 0
TEST(set_settings_cmd, command_handler) {
    SetSettingsCmd cmd;
    
    cmd.set_system_settings_page();
    cmd.set_date(10,5,22);

    std::vector<uint8_t> d = cmd.serialize(cmd.data_sent);
    
    std::vector<uint8_t> exp{0xb1,0x2,0x10,0x0,0xaa,0x2c,0xe8,0xb2};

    EXPECT_EQ(d, exp);

    SetSettingsCmd::data_t r;
    bool ok = cmd.deserialize(std::vector<uint8_t>{0xB1, 0x0, 0x10, 0xAA, 0xBA, 0xB2}, &r);
    EXPECT_EQ(ok, true);
    EXPECT_EQ(r.status, 170);
}
#endif
TEST(get_version, cmd_test) {
    GetVersion ver;
    std::vector<uint8_t> exp{0xB1, 0x1, 0x9, 0xA, 0xB2};
    std::vector<uint8_t> d;
    ver.serialize(d);

    EXPECT_EQ(d, exp);

    // receive response
    ver.deserialize(std::vector<uint8_t>{0xB1,0x3,0x9,0x5,0x1,0x2,0x0,0x14,0xB2});
        
    // read data
    GetVersion::Version v = ver.getVersion();
  
    EXPECT_EQ(v.minor, 0x5);
    EXPECT_EQ(v.major, 0x1);
    EXPECT_EQ(v.release, 0x2);
    EXPECT_EQ(v.variant, 0x0);
}
