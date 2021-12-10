#include "repM3.h"
#include "repM3provider.h"
#include <iostream>
#include <string>

#include <gtest/gtest.h>

using namespace lgmc;

TEST(data_serialize, data_handler) {
    UINT8 test_data{123};
    UINT8 test_data_ret;
    BaseData<UINT8> obj{test_data};
    
    std::vector<uint8_t> res = obj.serialize();
    test_data_ret = obj.deserialize(res);

    ASSERT_EQ(test_data, test_data_ret) << "Not equal";
}

TEST(command_serialize_base_no_data, command_handler) {
    BaseCommand cmd;
    
    struct test {
        UINT8 a;
        UINT8 b;
    };

    test t = {5,7};
    BaseData<test> td{t};

    std::vector<uint8_t> buffer = cmd.serialize<BaseData<test>>(td, 10);

    // expected buffer
    std::vector<uint8_t> exp{0xb1,0x2,0xa,0x5,0x7, 0x18, 0xb2};

    ASSERT_EQ(buffer, exp) << "Buffers are not equal";    
}

TEST(command_serialize_base_with_data, command_handler) {
    BaseCommand cmd;
    std::vector<uint8_t> buffer = cmd.serialize(0x9);

    // expected buffer
    std::vector<uint8_t> exp{0xb1,0x1,0x9,0xa,0xb2};

    ASSERT_EQ(buffer, exp) << "Buffers are not equal";
}

TEST(get_version_cmd, command_handler) {
    GetVersionCmd cmd;
    cmd.serialize();
}

