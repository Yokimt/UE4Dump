// module.hpp
#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <algorithm>
#include <fstream>
#include <list>
#include <unordered_set>
#include "UnrealObjects.h"
using namespace std;

class DumpInfo
{
public:
    void InitOffsets();
    void DumpSDK(string out);
    void DumpClass(UObject addr, string out, unordered_set<string> &BasicEngineTypes);
    void DumpStruct(UObject addr, string out, unordered_set<string> &BasicEngineTypes);
    // 偏移信息
    struct OffsetInfo
    {
        // Chunk
        uint32_t ChunkSize = 0x1D000;
        uint16_t NumElements = 0x14;
        uint16_t FUObjectItemSize = 0x18;
    };

    // 内存地址
    uint64_t GName = 0x1ADF9540;
    uint64_t GWorld =  0x1B109978;
    uint64_t UObjectOffset = 0x1A227BB8;
    uint16_t TUObjectArrayOffset = 0x20;
    uint64_t UObjectAddress = 0;

    bool InitDriver = false;
    OffsetInfo offsets;

    UObject UObjectStatic;
    UStruct UStructStatic;
    UEScriptStruct UScriptStructStatic;
    AActor AActorStatic;

private:
    void InitUObjectBaseOffset();
    void InitUStructFFieldOffset();
    void InitUEnum0ffset();
    void InitFProperty0ffset();
    void InitFFunc();
    uint32_t classCount = 0;
};

// 全局上下文
extern DumpInfo g_dumpInfo;

class ProgressBar {
    public:
        ProgressBar(int total, int bar_width = 50) 
            : total_(total), bar_width_(bar_width) {}
        
        void update(int current) {
            // 计算进度百分比
            float percentage = static_cast<float>(current) / total_ * 100.0f;
            
            // 计算已完成部分长度
            int filled_width = static_cast<int>(bar_width_ * percentage / 100.0f);
            
            // 构建进度条字符串
            std::string bar = "[";
            bar.append(filled_width, '=');  // 已完成部分
            bar.append(bar_width_ - filled_width, ' ');  // 未完成部分
            bar += "]";
            
            // 输出进度信息
            std::cout << "\r" << bar
                      << " " << std::setw(3) << static_cast<int>(percentage) << "%"
                      << " (" << std::setw(3) << current << "/" << total_ << ")"
                      << std::flush;
        }
        
        void finish() {
            update(total_);
            std::cout << "\nCompleted!" << std::endl;
        }
    
    private:
        int total_;
        int bar_width_;
    };
    




string BasicTypes_h = R"(// BasicTypes.h
#pragma once

#include <cstdint>
#include <string>

// 基本类型定义
using int8_t = signed char;
using int16_t = short;
using int32_t = int;
using int64_t = long long;
using uint8_t = unsigned char;
using uint16_t = unsigned short;
using uint32_t = unsigned int;
using uint64_t = unsigned long long;
using FIntPoint = uint64_t;


template<typename T> class TArray{};
template<typename K, typename V> class TMap{};
template<typename T> class TSet{};
template<typename T> class TWeakObjectPtr{};
template<typename T> class TSoftObjectPtr{};
// 引擎基础类型（前置声明）
struct FString{};
class FMulticastInlineDelegate{};
struct FName{};
struct FText{};
class FMulticastDelegate{};
class FMulticastSparseDelegate{};
class FDelegate{};
)";

// ================= 全局辅助函数 =================
namespace DumpUtils
{
    enum
    {
        UObjectFlag,
        UStructFlag,
        UScriptStructFlag,
        AActorFlag,
    };
    bool IsUnvalid(uint64_t addr)
    {
        return driver->is_unvalid(addr);
    }
    UObject GetUObjectFromID(uint32_t index)
    {
        uint64_t arrayAddr = MemoryReader::Read<uint64_t>(g_dumpInfo.UObjectAddress + g_dumpInfo.TUObjectArrayOffset);
        uint64_t chunkAddr = MemoryReader::Read<uint64_t>(arrayAddr + (index / g_dumpInfo.offsets.ChunkSize) * 8);
        uint64_t objectAddr =  MemoryReader::Read<uint64_t>(chunkAddr + (index % g_dumpInfo.offsets.ChunkSize) * g_dumpInfo.offsets.FUObjectItemSize);
        return UObject(objectAddr,index);
    }
    UObject FindObject(const string &name)
    {
        uint32_t numElements = MemoryReader::Read<uint32_t>(g_dumpInfo.UObjectAddress + g_dumpInfo.offsets.NumElements);

        for (uint32_t idx = 0; idx < numElements; idx++)
        {
            UObject objAddr = GetUObjectFromID(idx);
            if (!objAddr.IsValid())
                continue;
            if (objAddr.GetName() == name)
                return objAddr;
        }
        return UObject{};
    }
    bool IsBasicAscii(const string &str)
    {
        if (str.empty())
            return false;
        for (unsigned char c : str)
        {
            if (c < 32 || c > 126)
                return false;
        }
        return true;
    }
    bool IsA(UObject addr, int flag)
    {
        uint64_t cmp = 0;
        switch (flag)
        {
        case UStructFlag:
            cmp = g_dumpInfo.UStructStatic.GetAddress();
            break;
        case UScriptStructFlag:
            cmp = g_dumpInfo.UScriptStructStatic.GetAddress();
            break;
        case AActorFlag:
            cmp = g_dumpInfo.AActorStatic.GetAddress();
            break;
        }

        // 如果静态类对象不存在，返回 false
        if (!cmp)
            return false;
        unordered_set<UEClass> visited;
        // 从当前对象的类开始，逐级获取父类，直到找到类型T的类或者到达最顶层类
        for (UEClass super = addr.GetClass().Cast<UEClass>(); super.IsValid(); super = super.GetSuperStruct().Cast<UEClass>())
        {
            if (visited.find(super) != visited.end())
            {
                // 有环，退出循环
                break;
            }
            visited.insert(super);
            if (!super.IsValid())
            {
                break;
            }
            if (super.GetAddress() == cmp)
                return true;
        }
        return false;
    }
    string GetCppName(UObject addr)
    {
        string Name = addr.GetName();
        if (Name == "" || Name == "None" || !IsBasicAscii(Name))
            return "";

        for (UStruct c = addr.Cast<UStruct>(); c.IsValid(); c = c.GetSuperStruct().Cast<UStruct>())
        {
            /*这个地方容易出问题*/
            if (!c.IsValid())
                break;

            if (c.GetAddress() == g_dumpInfo.AActorStatic.GetAddress())
                return "A" + Name;

            if (c.GetAddress() == g_dumpInfo.UObjectStatic.GetAddress())
                return "U" + Name;
        }
        return "F" + Name;
    }
    string ExtractBaseType(const string &typeStr)
    {
        if (typeStr.empty())
            return "";

        // 处理枚举类
        if (typeStr.find("enum class") == 0)
        {
            return typeStr;
        }

        // 处理模板类型
        if (typeStr.find('<') != string::npos)
        {
            // 保留完整模板信息
            return typeStr;
        }
        string cleanType = typeStr;

        if (cleanType.find("const ") == 0)
        {
            cleanType = cleanType.substr(6);
        }

        size_t ptrPos = cleanType.find('*');
        if (ptrPos != string::npos)
        {
            cleanType = cleanType.substr(0, ptrPos);
        }

        size_t refPos = cleanType.find('&');
        if (refPos != string::npos)
        {
            cleanType = cleanType.substr(0, refPos);
        }

        while (!cleanType.empty() && isspace(cleanType.back()))
        {
            cleanType.pop_back();
        }

        return cleanType;
    }

    // 判断类型是否需要包含头文件
    void NeedsHeader(const string &baseType, unordered_set<string> &BasicEngineTypes, unordered_set<string> &headerIncludes)
    {
        // 基本类型不需要额外头文件
        static const unordered_set<string> basicTypes = {
            "bool", "char", "int8_t", "int16_t", "int32_t", "int64_t",
            "uint8_t", "uint16_t", "uint32_t", "uint64_t", "float", "double"};

        // 引擎基础类型已在公共头文件中定义
        static const unordered_set<string> engineTypes = {
            "FName", "FString", "FText", "FDelegate", "FMulticastDelegate",
            "TArray", "TMap", "TSet", "TWeakObjectPtr", "TSoftObjectPtr", "FIntPoint","FMulticastInlineDelegate","FMulticastSparseDelegate"};

        // 1. 跳过基本类型
        if (basicTypes.count(baseType))
            return;
        if (baseType.empty() || baseType == "<>" || baseType == ">")
        {
            return;
        }
        // 2. 处理枚举类
        if (baseType.find("enum class") == 0)
        {
            // 提取枚举名
            // size_t pos = baseType.find_last_of(' ');
            // if (pos != string::npos)
            // {
            //     string enumName = baseType.substr(pos + 1);
            //     if (!enumName.empty())
            //     {
            //         BasicEngineTypes.insert("enum class " + enumName + ";");
            //     }
            // }
            return;
        }

        // 3. 递归处理模板类型
        auto processTemplate = [&](const string& type) {
            size_t start = type.find('<');
            size_t end = type.rfind('>');
            
            // 验证模板格式有效性
            if (start == string::npos || end == string::npos || start >= end) {
                return;
            }
            
            string params = type.substr(start + 1, end - start - 1);
            
            // 处理空模板参数
            if (params.empty()) {
                return;
            }
            
            // 分割模板参数（增加嵌套深度跟踪）
            vector<string> templateParams;
            int depth = 0;
            size_t last = 0;
            bool inQuotes = false; // 可选：处理带引号的参数
            
            for (size_t i = 0; i < params.length(); ++i) {
                char c = params[i];
                
                // 处理引号（可选）
                if (c == '"') inQuotes = !inQuotes;
                
                if (!inQuotes) {
                    if (c == '<') depth++;
                    else if (c == '>') depth--;
                    else if (c == ',' && depth == 0) {
                        // 提取有效参数段
                        string param = params.substr(last, i - last);
                        if (!param.empty()) {
                            templateParams.push_back(param);
                        }
                        last = i + 1;
                    }
                }
            }
            
            // 添加最后一个参数
            string lastParam = params.substr(last);
            if (!lastParam.empty()) {
                templateParams.push_back(lastParam);
            }
            
            // 递归处理每个参数
            for (auto& param : templateParams) {
                // 清理参数：移除首尾空格
                size_t first = param.find_first_not_of(" ");
                size_t last = param.find_last_not_of(" ");
                if (first != string::npos && last != string::npos) {
                    param = param.substr(first, (last - first + 1));
                }
                
                // 跳过空参数
                if (!param.empty()) {
                    NeedsHeader(param, BasicEngineTypes, headerIncludes);
                }
            }
        };

        // 4. 处理模板类型
        if (baseType.find('<') != string::npos)
        {
            processTemplate(baseType);
            
            // 如果是已知模板类型则跳过
            if (baseType.find("TArray") == 0 || baseType.find("TMap") == 0 ||
                baseType.find("TSet") == 0||baseType.find("TSoftObjectPtr") == 0||baseType.find("TWeakObjectPtr") == 0)
            {
                return;
            }
        }

        // 5. 处理自定义类型
        // if (baseType[0] == 'F')
        // {
        //     string type = baseType;
        //     size_t ptr = type.find("*");
        //     if (ptr != string::npos)
        //     {
        //         type = type.substr(0, ptr);
        //     }
        //     BasicEngineTypes.insert("class " + type + " {};");

            // if (!engineTypes.count(baseType) && !basicTypes.count(baseType))
            // {
            //     headerIncludes.insert("\"" + baseType + "_class.h\"");
            // }
        //     return;
        // }

        // 6. 默认处理：非基本/非引擎类型需要头文件
        if (!engineTypes.count(baseType) && !basicTypes.count(baseType))
        {
            string type = baseType;
            size_t ptr = type.find("*");
            if (ptr != string::npos)
            {
                type = type.substr(0, ptr);
            }
            headerIncludes.insert(type);
        }
    }
}


//UnrealObjects中的一些实现

std::string UObject::GetFullName() const
{
    std::string path = GetName();
    if (path == "" || path == "None"||!DumpUtils::IsBasicAscii(path))
        return std::string();
    UObject Outer = GetOuter();
    while (Outer.IsValid())
    {
        std::string tmp_path = Outer.GetName();
        if (DumpUtils::IsBasicAscii(tmp_path))
        {
            path = tmp_path + "." + path;
            Outer = Outer.GetOuter();
        }
        else
        {
            return path;
        }
    }
    return path;
}
std::string UField::GetType()
{
    string Name = this->GetClassName();
    if (Name == "NameProperty")
    {
        return "FName";
    }
    if (Name == "StrProperty")
    {
        return "FString";
    }
    if (Name == "TextProperty")
    {
        return "FText";
    }
    if (Name == "Int8Property")
    {
        return "int8_t";
    }
    if (Name == "FloatProperty")
    {
        return "float";
    }
    if (Name == "DoubleProperty")
    {
        return "double";
    }
    if (Name == "Int16Property")
    {
        return "int16_t";
    }
    if (Name == "IntProperty")
    {
        return "int32_t";
    }
    if (Name == "Int64Property")
    {
        return "int64_t";
    }
    if (Name == "UInt16Property")
    {
        return "uint16_t";
    }
    if (Name == "UInt32Property")
    {
        return "uint32_t";
    }
    if (Name == "UInt64Property")
    {
        return "uint64_t";
    }
    if (Name == "DelegateProperty")
    {
        return "FDelegate";
    }
    if (Name == "SoftClassProperty")
    {
        return "TSoftClassPtr<UObject>";
    }
    if (Name == "MulticastDelegateProperty")
    {
        return "FMulticastDelegate";
    }
    if (Name == "MulticastSparseDelegateProperty")
    {
        return "FMulticastSparseDelegate";
    }
    if (Name == "MulticastInlineDelegateProperty")
    {
        return "FMulticastInlineDelegate";
    }

    if (Name == "MapProperty")
    {
        return "TMap<" +this->Cast<UEMapProperty>().GetKeyProperty().GetType()+ ","+this->Cast<UEMapProperty>().GetValueProperty().GetType()+">";
    }
    if (Name == "SetProperty")
    {
        return "TSet<" + this->Cast<UESetProperty>().GetElementProperty().GetType() + ">";
    }
    if (Name == "EnumProperty")
    {
        return "enum class " +this->Cast<UEEnumProperty>().GetElementProperty().GetName();
    }
    if (Name == "BoolProperty")
    {
        if (this->Cast<UEBoolProperty>().GetFieldMask() == 0xFF)
        {
            return "bool";
        }
        return "char";
    }
    if (Name == "ByteProperty")
    {
        UObject obj = this->Cast<UEByteProperty>().GetEnum();
        if (obj.IsValid())
            return "enum class " +obj.GetName();
        return "char";
    }
    if (Name == "ClassProperty")
    {
        return DumpUtils::GetCppName(this->Cast<UEClassProperty>().GetMetaClass())+ "*";
    }
    if (Name == "StructProperty")
    {
        return DumpUtils::GetCppName( this->Cast<UStructProperty>().GetStruct());
    }
    if (Name == "InterfaceProperty")
    {
        return "TScriptInterface<I" + this->Cast<UEInterfaceProperty>().GetInterfaceClass().GetName() + ">";
    }
    if (Name == "ObjectProperty")
    {
        return  DumpUtils::GetCppName(this->Cast<UEObjectProperty>().GetPropertyClass())+"*";
    }
    if (Name == "ArrayProperty")
    {
        return "TArray<" +this->Cast<UEArrayProperty>().GetInner().GetType()  + ">";
    }
    if (Name == "WeakObjectProperty")
    {
        return "TWeakObjectPtr<" + DumpUtils::GetCppName(this->Cast<UStructProperty>().GetStruct()) + ">";
    }
    if (Name == "SoftObjectProperty")
    {
        return "TSoftObjectPtr<" + DumpUtils::GetCppName(this->Cast<UEObjectProperty>().GetPropertyClass()) + ">";
    }

    return DumpUtils::GetCppName(this->Cast<UEObjectProperty>().GetPropertyClass()) + "*";
}


UStruct UStruct::StaticClass()
{
    return DumpUtils::FindObject("Class").Cast<UStruct>();
}

UEScriptStruct UEScriptStruct::StaticClass()
{
    return DumpUtils::FindObject("ScriptStruct").Cast<UEScriptStruct>();
}
UObject UObject::StaticClass()
{
    return DumpUtils::FindObject("Object").Cast<UObject>();
}
AActor AActor::StaticClass()
{
    return DumpUtils::FindObject("Actor").Cast<AActor>();
}
