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
};