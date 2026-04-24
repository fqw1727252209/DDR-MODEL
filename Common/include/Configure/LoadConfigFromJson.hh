#ifndef __LOAD_CONFIG_FROM_JSON_HH__
#define __LOAD_CONFIG_FROM_JSON_HH__

#include <iostream>
#include <fstream>
#include <string>

#include <systemc>
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"

#include "Common/CommonDefine.hh"

// #ifdef DEBUG
// // 定义 DEBUG_PRINT 宏

// #define DEBUG_PRINT_VALUE(_message) \
// std::cout<<"debug info: "<< #_message <<" = " <<_message <<std::endl;

// #define DEBUG_PRINT(_message) \
// std::cout<<"debug info: "<<_message <<std::endl;

// #define DEBUG_PRINT_TIME(_message) \
// std::cout<<"debug info: "<<"time: "<<sc_core::sc_time_stamp()<<":\t "<<_message <<std::endl;
// #else
// // 如果没有定义 DEBUG 宏，DEBUG_PRINT 宏不做任何事
// #define DEBUG_PRINT_VALUE(_message)
// #define DEBUG_PRINT(_message)
// #define DEBUG_PRINT_TIME(_message)
// #endif

// #define ABORT_MESSAGE(_message) \
//     std::cerr<<"illegal action: "<<_message<<std::endl; \
//     std::abort();

constexpr const std::string_view headline = 
    "=======================================================================";

#define BEGIN_JSON_MAP(StructType) \
bool ParseStruct(const rapidjson::Value& json, StructType& obj) { \
    if (!json.IsObject()) return false; \
    bool success = true;

#define JSON_FIELD(Type, FieldName) \
    success &= GetValue(json, #FieldName, obj.FieldName); 
    // std::cout<<#FieldName<<": "<<obj.FieldName<<std::endl;

#define JSON_NESTED_STRUCT(FieldName) \
    if (!json.HasMember(#FieldName)) { \
        std::cerr << "Missing JSON struct member: " << #FieldName << std::endl; \
        std::abort(); \
    } \
    success &= ParseStruct(json[#FieldName], obj.FieldName);

#define END_JSON_MAP() \
    return success; \
}

namespace dmu{

class LoadConfigFromJson{

    protected:
        rapidjson::Document doc;
        std::string _filename;
        virtual ~LoadConfigFromJson(){}

    public:
        virtual void LoadFromJson(const std::string& filename)
        {
            _filename = filename;
            std::ifstream file(filename, std::ios::binary);
            if (!file.is_open()) {
                std::cerr << "Failed to open file: " << filename << std::endl;
                std::abort();
            }

            std::string buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

            // rapidjson::Document doc;
            doc.Parse(buffer.c_str());

            if (doc.HasParseError()) {
                std::cerr << "JSON parse error: " << doc.GetParseError() << std::endl;
                std::abort();
            }
        }

        virtual bool ParseJson() = 0;

        // ===== 类型特化：处理不同类型的JSON值 =====
        template<typename T>
        bool GetValue(const rapidjson::Value& parent, const char* name, T& value) {
            if (!parent.HasMember(name)) {
                std::cerr << "Missing JSON member: " << name << std::endl;
                return false;
            }

            const rapidjson::Value& val = parent[name];

            // 基本类型处理,TODO: 需要添加对std::vector的处理，来支持address mapping
            if constexpr (std::is_same_v<T, std::string>) {
                if (!val.IsString())
                {
                    std::cerr<<"Json File '"<<_filename<<"': ["<< name <<"] dont match String type" <<std::endl;
                    return false;
                }
                value = val.GetString();
            }
            else if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
                if (std::is_unsigned_v<T>) {
                    if (!val.IsUint())
                    {
                        std::cerr<<"Json File '"<<_filename<<"': ["<< name <<"] dont match Uint type" <<std::endl;
                        return false;
                    }
                    value = val.GetUint();
                } else {
                    if (!val.IsInt())
                    {
                        std::cerr<<"Json File '"<<_filename<<"': ["<< name <<"] dont match Int type" <<std::endl;
                        return false;
                    }
                    value = val.GetInt();
                }
            }
            else if constexpr (std::is_floating_point_v<T>) {
                if (!val.IsDouble())
                {
                    std::cerr<<"Json File '"<<_filename<<"': ["<< name <<"] dont match Double type" <<std::endl;
                    return false;
                }
                value = val.GetDouble();
            }
            else if constexpr (std::is_same_v<T, bool>) {
                if (!val.IsBool())
                {
                    std::cerr<<"Json File '"<<_filename<<"': ["<< name <<"] dont match Bool type" <<std::endl;
                    return false;
                }
                value = val.GetBool();
            }
            else if constexpr (std::is_same_v<T, std::vector<typename T::value_type>>) {
                // std::vector<ElementType> 解析：JSON Array -> C++ vector
                if (!val.IsArray()) {
                    std::cerr << "Json File '" << _filename << "': [" << name << "] dont match Array type" << std::endl;
                    return false;
                }
                value.clear();
                for (const auto& elem : val.GetArray()) {
                    typename T::value_type item{};
                    if constexpr (std::is_floating_point_v<typename T::value_type>) {
                        if (elem.IsDouble())      item = elem.GetDouble();
                        else if (elem.IsInt())    item = static_cast<typename T::value_type>(elem.GetInt());
                        else if (elem.IsUint())   item = static_cast<typename T::value_type>(elem.GetUint());
                        else { std::cerr << "Array element type mismatch in [" << name << "]" << std::endl; return false; }
                    } else if constexpr (std::is_integral_v<typename T::value_type> && std::is_unsigned_v<typename T::value_type>) {
                        if (!elem.IsUint()) { std::cerr << "Array element type mismatch in [" << name << "]" << std::endl; return false; }
                        item = elem.GetUint();
                    } else if constexpr (std::is_integral_v<typename T::value_type>) {
                        if (!elem.IsInt()) { std::cerr << "Array element type mismatch in [" << name << "]" << std::endl; return false; }
                        item = elem.GetInt();
                    }
                    value.push_back(item);
                }
            }
            else {
                // 假设是嵌套结构体，需要实现ParseStruct函数
                return ParseStruct(val, value);
            }

            return true;
        }
};

} // namespace dmu

#endif