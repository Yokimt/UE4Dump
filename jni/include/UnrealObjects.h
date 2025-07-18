#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include "MemoryReader.h"


class FNamePool
{
private:
    uint64_t poolAddress;
public:
    FNamePool(uint64_t address) : poolAddress(address) {}
    bool Initialize(uint64_t GName)
    {
        if(GName!=0)
        {
            poolAddress = GName;
            return true;
        }
        return false;
    }
    // 获取名字
    std::string GetName(uint32_t NameId)
    {
        uint64_t addr = MemoryReader::Read<uint64_t>(poolAddress + (((NameId >> 15) & 0x1FFF8) + 0x38)) +
                        (2 * NameId & 0x7FFFE);
        if (!addr)
            return "";

        uint64_t strPtr = addr + 2;
        uint32_t len = MemoryReader::Read<uint16_t>(addr) >> 6;
        return DecryptAnsiName(strPtr,len);
    }

    std::string GetName_Old(uint32_t NameId);

    // 解密字符串函数
    uint16_t CalculateKey(uint32_t len)
    {
        switch (len % 9)
        {
        case 0:
            return ((len & 0x1F) + len + 0x80) | 0x7F;
        case 1:
            return ((len ^ 0xDF) + len + 0x80) | 0x7F;
        case 2:
            return ((len | 0xCF) + len + 0x80) | 0x7F;
        case 3:
            return (33 * len + 0x80) | 0x7F;
        case 4:
            return (len + (len >> 2) + 0x80) | 0x7F;
        case 5:
            return (3 * len + 133) | 0x7F;
        case 6:
            return (((4 * len) | 5) + len + 0x80) | 0x7F;
        case 7:
            return (((len >> 4) | 7) + len + 0x80) | 0x7F;
        case 8:
            return ((len ^ 0x0C) + len + 0x80) | 0x7F;
        default:
            return ((len ^ 0x40) + len + 0x80) | 0x7F;
        }
    }
    std::string DecryptAnsiName(uintptr_t address, uint32_t len)
    {
        if (len == 0)
            return "";

        std::string name(len, '\0');
        MemoryReader::Read(address, name.data(), len);
        if (name[0] == '\0')
            return "";

        uint16_t key = CalculateKey(len);
        for (uint32_t i = 0; i < len; i++)
        {
            name[i] ^= key;
        }
        return name;
    }
    std::string DecryptWideName(uintptr_t address, uint16_t header);
};
inline FNamePool g_FNamePool(0);


// 1. 先定义UEObject
class UEObject
{
protected:
    uint64_t Address;
    int32_t Index;

public:
    UEObject() : Address(0), Index(-1) {}
    UEObject(uint64_t InAddress, int32_t InIndex) : Address(InAddress), Index(InIndex) {}
    virtual ~UEObject() = default;
    uint64_t GetAddress() const { return Address; }
    int32_t GetIndex() const { return Index; }
    bool IsValid() const { return Address != 0 && MemoryReader::IsValidAddress(Address); }
    template <typename T>
    T Read(uint64_t Offset) const
    {
        T Value;
        MemoryReader::Read(Address + Offset, &Value, sizeof(T));
        return Value;
    }
    void Read(uint64_t Offset, void *Buffer, int32_t Size) const
    {
        MemoryReader::Read(Address + Offset, Buffer, Size);
    }
    void Write(uint64_t Offset, void *Buffer, int32_t Size) const
    {
        MemoryReader::Write(Address + Offset, Buffer, Size);
    }
    virtual std::string GetName() const = 0;
    virtual std::string GetFullName() const = 0;
    template <typename T>
    T Cast() const { return T(Address, Index); }
};

// 2. 再定义UObject（具体类）
class UObject : public UEObject
{
private:
    static inline int32_t InternalIndexOffset = 0x34;
    static inline int32_t ClassOffset = 0x18;
    static inline int32_t OuterOffset = 0x20;
    static inline int32_t NameOffset = 0x2C;

public:
    using UEObject::UEObject;
    virtual ~UObject(){};
    static void SetInternalIndexOffset(int32_t offset) { InternalIndexOffset = offset; }
    static void SetClassOffset(int32_t offset) { ClassOffset = offset; }
    static void SetOuterOffset(int32_t offset) { OuterOffset = offset; }
    static void SetNameOffset(int32_t offset) { NameOffset = offset; }
    int32_t GetInternalIndex() const { return Read<int32_t>(InternalIndexOffset); }
    UObject GetClass() const{
        uint64_t ClassAddress = Read<uint64_t>(ClassOffset);
        return UObject(ClassAddress, -1);
    }
    UObject GetOuter() const
    {
        uint64_t OuterAddress = Read<uint64_t>(OuterOffset);
        return UObject(OuterAddress, -1);
    }
    int32_t GetNameIndex() const { return Read<int32_t>(NameOffset); }
    std::string GetName() const override { return g_FNamePool.GetName(GetNameIndex()); }
    std::string GetClassNameType(){
        return  GetClass().GetName();
    }
    std::string GetFullName() const override;
    static UObject StaticClass();
};

class AActor:public UObject
{
public:
    using UObject::UObject;
    static AActor StaticClass();
};

// 3. UField
class UField : public UObject
{
private:
    static inline int32_t NextOffset = 0x18;
    static inline int32_t ClassOffset = 0x20;
    static inline int32_t NameOffset = 0x28;

public:
    using UObject::UObject;
    static void SetNextOffset(int32_t offset) { NextOffset = offset; }
    static void SetClassOffset(int32_t offset) { ClassOffset = offset; }
    static void SetNameOffset(int32_t offset) { NameOffset = offset; }
    UField GetNext() const
    {
        uint64_t NextAddress = Read<uint64_t>(NextOffset);
        return UField(NextAddress, -1);
    }
    int32_t GetNameIndex() const{ return Read<int32_t>(NameOffset); }
    std::string GetName() const override { return g_FNamePool.GetName(GetNameIndex()); }
    UObject GetClass() {return UObject(Read<uint64_t>(ClassOffset),-1);}
    std::string GetClassName(){
        uint32_t nameid = MemoryReader::Read<uint32_t>(GetClass().GetAddress());
        return  g_FNamePool.GetName(nameid);
    }
    std::string GetType();
};

// 4. UStruct
class UStruct : public UField
{
protected:
    static inline int32_t SuperStructOffset = 0x50;
    static inline int32_t ChildProperties = 0x78;
    static inline int32_t PropertySizeOffset = 0x4C;

public:
    using UField::UField;
    static void SetSuperStructOffset(int32_t offset) { SuperStructOffset = offset; }
    static void SetChildProperties(int32_t offset) { ChildProperties = offset; }
    static void SetPropertySizeOffset(int32_t offset) { PropertySizeOffset = offset; }
    UObject GetSuperStruct() const{
        uint64_t SuperAddress = Read<uint64_t>(SuperStructOffset);
        return SuperAddress ? UObject(SuperAddress, -1) : UObject{};
    }
    UField GetChildProperties() const
    {
        uint64_t ChildrenAddress = Read<uint64_t>(ChildProperties);
        return UField(ChildrenAddress, -1);
    }
    int32_t GetPropertySize() const { return Read<int32_t>(PropertySizeOffset); }
    static UStruct StaticClass();
    std::vector<class UEProperty> GetProperties() const;
};

// 5. UEClass
class UEClass : public UStruct
{
private:
    static inline int32_t ClassFlagsOffset = 0x80;
    static inline int32_t ClassWithinOffset = 0x84;
    static inline int32_t ClassConfigNameOffset = 0x85;
    static inline int32_t ClassGeneratedByOffset = 0x88;
    static inline int32_t NetFieldsOffset = 0x90;
    static inline int32_t InterfacesOffset = 0xA0;
    static inline int32_t ImplementedInterfacesOffset = 0xB0;

public:
    using UStruct::UStruct;
    uint32_t GetClassFlags() const { return Read<uint32_t>(ClassFlagsOffset); }
    uint8_t GetClassWithin() const { return Read<uint8_t>(ClassWithinOffset); }
    uint8_t GetClassConfigName() const { return Read<uint8_t>(ClassConfigNameOffset); }

    UObject GetClassGeneratedBy() const
    {
        uint64_t GeneratedByAddress = Read<uint64_t>(ClassGeneratedByOffset);
        return UObject(GeneratedByAddress, -1);
    }
};



// 6. UEProperty
class UEProperty : public UField
{
protected:
    static inline int32_t ArrayDimOffset = 0x30;
    static inline int32_t ElementSizeOffset = 0x3C;
    static inline int32_t PropertyFlagsOffset = 0x38;
    static inline int32_t RepIndexOffset = 0x40;
    static inline int32_t BlueprintReplicationConditionOffset = 0x42;
    static inline int32_t OffsetInternalOffset = 0x4C;
    static inline int32_t RepNotifyFuncOffset = 0x48;
    static inline int32_t PropertyLinkNextOffset = 0x50;
    static inline int32_t NextRefOffset = 0x58;
    static inline int32_t DestructorLinkNextOffset = 0x60;
    static inline int32_t PostConstructLinkNextOffset = 0x68;
    static inline int32_t SparseArrayIndexOffset = 0x70;
    static inline int32_t ChildClassOffset = 0x80;
public:
    using UField::UField;
    static void SetElementSizeOffset(int32_t offset) { ElementSizeOffset = offset; }
    static void SetOffsetInternalOffset(int32_t offset) { OffsetInternalOffset = offset; }
    static void SetChildClassOffset(int32_t offset) { ChildClassOffset = offset; }
    int32_t GetArrayDim() const { return Read<int32_t>(ArrayDimOffset); }
    int32_t GetElementSize() const { return Read<int32_t>(ElementSizeOffset); }
    uint16_t GetRepIndex() const { return Read<uint16_t>(RepIndexOffset); }
    uint8_t GetBlueprintReplicationCondition() const { return Read<uint8_t>(BlueprintReplicationConditionOffset); }
    int32_t GetOffset() const { return Read<int32_t>(OffsetInternalOffset); }
    int32_t GetRepNotifyFunc() const { return Read<int32_t>(RepNotifyFuncOffset); }
    UEProperty GetPropertyLinkNext() const
    {
        uint64_t PropertyAddress = Read<uint64_t>(PropertyLinkNextOffset);
        return UEProperty(PropertyAddress, -1);
    }
    UEProperty GetNextRef() const
    {
        uint64_t PropertyAddress = Read<uint64_t>(NextRefOffset);
        return UEProperty(PropertyAddress, -1);
    }
    UEProperty GetDestructorLinkNext() const
    {
        uint64_t PropertyAddress = Read<uint64_t>(DestructorLinkNextOffset);
        return UEProperty(PropertyAddress, -1);
    }
    UEProperty GetPostConstructLinkNext() const
    {
        uint64_t PropertyAddress = Read<uint64_t>(PostConstructLinkNextOffset);
        return UEProperty(PropertyAddress, -1);
    }
    UEProperty GetSparseArrayIndex() const
    {
        uint64_t PropertyAddress = Read<uint64_t>(SparseArrayIndexOffset);
        return UEProperty(PropertyAddress, -1);
    }
};

// 7. UEEnum
class UEEnum : public UField
{
private:
    static inline int32_t NameOffset = 0x0;
    static inline int32_t ValueOffset = 0x8;
    static inline int32_t SizeOffset = 0x10;
    static inline int32_t NamesOffset = 0x50;

public:
    using UField::UField;
    static void SetNameOffset(int32_t offset) { NameOffset = offset; }
    static void SetValueOffset(int32_t offset) { ValueOffset = offset; }
    static void SetSizeOffset(int32_t offset) { SizeOffset = offset; }
    static void SetNamesOffset(int32_t offset) { NamesOffset = offset; }
    struct FNameArray
    {
        uint64_t Data;
        int32_t Count;
        int32_t Max;
    };
    FNameArray GetNames() const { return Read<FNameArray>(NamesOffset); }
    int GetValue()const { return Read<int>(ValueOffset); }
    std::vector<std::pair<std::string,int>> GetNameList() const
    {
        FNameArray names = GetNames();
        std::vector<std::pair<std::string,int>> nameList;
        nameList.reserve(names.Count);
        for (int32_t i = 0; i < names.Count; ++i)
        {
            std::string name = g_FNamePool.GetName(MemoryReader::Read<uint32_t>(names.Data + i * SizeOffset+NameOffset));
            int value = MemoryReader::Read<uint32_t>(names.Data + i * SizeOffset+ValueOffset);

            // printf("Name: %s, Value: %d\n", name.c_str(), value);
            if (!name.empty())
            {
                nameList.push_back(std::pair<std::string,int>(name, value));
            }
        }
        return nameList;
    }
    static UEEnum StaticClass();
};

// 8. UEScriptStruct
class UEScriptStruct : public UStruct
{
public:
    using UStruct::UStruct;
    static UEScriptStruct StaticClass();
};

// 9. UEFunction (必须在DelegateProperty之前定义)
class UEFunction : public UStruct
{
private:
    static inline int32_t FunctionFlagsOffset = 0x80;
    static inline int32_t RepOffsetOffset = 0x84;
    static inline int32_t NumParmsOffset = 0x86;
    static inline int32_t ParmsSizeOffset = 0x88;
    static inline int32_t ReturnValueOffset = 0x8A;
    static inline int32_t RPCIdOffset = 0x8C;
    static inline int32_t RPCResponseIdOffset = 0x8E;
    static inline int32_t FirstPropertyToInitOffset = 0x90;
    static inline int32_t EventGraphFunctionOffset = 0x98;
    static inline int32_t EventGraphCallOffset = 0xA0;
    static inline int32_t FuncOffset = 0xF0;

public:
    using UStruct::UStruct;
    uint16_t GetRepOffset() const { return Read<uint16_t>(RepOffsetOffset); }
    uint8_t GetNumParms() const { return Read<uint8_t>(NumParmsOffset); }
    uint16_t GetParmsSize() const { return Read<uint16_t>(ParmsSizeOffset); }
    uint16_t GetReturnValueOffset() const { return Read<uint16_t>(ReturnValueOffset); }
    uint16_t GetRPCId() const { return Read<uint16_t>(RPCIdOffset); }
    uint16_t GetRPCResponseId() const { return Read<uint16_t>(RPCResponseIdOffset); }
    UEProperty GetFirstPropertyToInit() const
    {
        uint64_t PropertyAddress = Read<uint64_t>(FirstPropertyToInitOffset);
        return UEProperty(PropertyAddress, -1);
    }
    UEFunction GetEventGraphFunction() const
    {
        uint64_t FunctionAddress = Read<uint64_t>(EventGraphFunctionOffset);
        return UEFunction(FunctionAddress, -1);
    }
    int32_t GetEventGraphCallOffset() const { return Read<int32_t>(EventGraphCallOffset); }
    uint64_t GetFunc() const { return Read<uint64_t>(FuncOffset); }
};

// 10. 其它Property子类、Const等（略，按原有顺序即可）
// Property type implementations
class UEByteProperty : public UEProperty
{
private:
    static inline int32_t EnumOffset = 0x80;

public:
    using UEProperty::UEProperty;

    UObject GetEnum() const
    {
        uint64_t EnumAddress = Read<uint64_t>(ChildClassOffset);
        return UEEnum(EnumAddress, -1);
    }
};

class UEBoolProperty : public UEProperty
{
private:
    static inline int32_t FieldSizeOffset = 0x80;
    static inline int32_t ByteOffsetOffset = 0x79;
    static inline int32_t ByteMaskOffset = 0x7a;
    static inline int32_t FieldMaskOffset = 0x83;

public:
    using UEProperty::UEProperty;

    uint8_t GetFieldSize() const
    {
        return Read<uint8_t>(FieldSizeOffset);
    }

    uint8_t GetByteOffset() const
    {
        return Read<uint8_t>(ByteOffsetOffset);
    }

    uint8_t GetByteMask() const
    {
        return Read<uint8_t>(ByteMaskOffset);
    }

    uint8_t GetFieldMask() const
    {
        return Read<uint8_t>(ChildClassOffset+3);
    }
};

class UEIntProperty : public UEProperty
{
public:
    using UEProperty::UEProperty;
};

class UEFloatProperty : public UEProperty
{
public:
    using UEProperty::UEProperty;
};

class UEDoubleProperty : public UEProperty
{
public:
    using UEProperty::UEProperty;
};

class UENameProperty : public UEProperty
{
public:
    using UEProperty::UEProperty;
};

class UEStrProperty : public UEProperty
{
public:
    using UEProperty::UEProperty;
};

class UEArrayProperty : public UEProperty
{
private:
    static inline int32_t InnerOffset = 0x80;

public:
    using UEProperty::UEProperty;

    UEProperty GetInner() const
    {
        uint64_t InnerAddress = Read<uint64_t>(ChildClassOffset);
        return UEProperty(InnerAddress, -1);
    }
};

class UEMapProperty : public UEProperty
{
private:
    static inline int32_t KeyPropertyOffset = 0x80;
    static inline int32_t ValuePropertyOffset = 0x88;
public:
    using UEProperty::UEProperty;

    UEProperty GetKeyProperty() const
    {
        uint64_t KeyAddress = Read<uint64_t>(ChildClassOffset);
        return UEProperty(KeyAddress, -1);
    }

    UEProperty GetValueProperty() const
    {
        uint64_t ValueAddress = Read<uint64_t>(ChildClassOffset+8);
        return UEProperty(ValueAddress, -1);
    }
};

class UESetProperty : public UEProperty
{
private:
    static inline int32_t ElementPropertyOffset = 0x80;

public:
    using UEProperty::UEProperty;

    UEProperty GetElementProperty() const
    {
        uint64_t ElementAddress = Read<uint64_t>(ChildClassOffset);
        return UEProperty(ElementAddress, -1);
    }
};
class UEEnumProperty : public UEProperty
{
private:
    static inline int32_t ElementPropertyOffset = 0x88;

public:
    using UEProperty::UEProperty;
    UObject GetElementProperty() const
    {
        uint64_t ElementAddress = Read<uint64_t>(ChildClassOffset);
        return UEProperty(ElementAddress, -1);
    }
};
class UStructProperty : public UEProperty
{
private:
    static inline int32_t StructOffset = 0x80;

public:
    using UEProperty::UEProperty;

    UObject GetStruct() const
    {
        uint64_t StructAddress = Read<uint64_t>(ChildClassOffset);
        return UEScriptStruct(StructAddress, -1);
    }
};

class UEObjectProperty : public UEProperty
{
private:
    static inline int32_t PropertyClassOffset = 0x80;

public:
    using UEProperty::UEProperty;

    UObject GetPropertyClass() const
    {
        uint64_t ClassAddress = Read<uint64_t>(ChildClassOffset);
        return UEClass(ClassAddress, -1);
    }
};

class UEClassProperty : public UEObjectProperty
{
private:
    static inline int32_t MetaClassOffset = 0x88;

public:
    using UEObjectProperty::UEObjectProperty;

    UObject GetMetaClass() const
    {
        uint64_t MetaClassAddress = Read<uint64_t>(ChildClassOffset);
        return UEClass(MetaClassAddress, -1);
    }
};

class UEInterfaceProperty : public UEProperty
{
private:
    static inline int32_t InterfaceClassOffset = 0x80;

public:
    using UEProperty::UEProperty;

    UObject GetInterfaceClass() const
    {
        uint64_t InterfaceClassAddress = Read<uint64_t>(ChildClassOffset);
        return UEClass(InterfaceClassAddress, -1);
    }
};

class UEWeakObjectProperty : public UEObjectProperty
{
public:
    using UEObjectProperty::UEObjectProperty;
};

class UELazyObjectProperty : public UEObjectProperty
{
public:
    using UEObjectProperty::UEObjectProperty;
};

class UEAssetObjectProperty : public UEObjectProperty
{
public:
    using UEObjectProperty::UEObjectProperty;
};

class UEAssetClassProperty : public UEAssetObjectProperty
{
private:
    static inline int32_t MetaClassOffset = 0x80;

public:
    using UEAssetObjectProperty::UEAssetObjectProperty;

    UEClass GetMetaClass() const
    {
        uint64_t MetaClassAddress = Read<uint64_t>(MetaClassOffset);
        return UEClass(MetaClassAddress, -1);
    }
};

class UEDelegateProperty : public UEProperty
{
private:
    static inline int32_t SignatureFunctionOffset = 0x78;

public:
    using UEProperty::UEProperty;

    UEFunction GetSignatureFunction() const
    {
        uint64_t FunctionAddress = Read<uint64_t>(SignatureFunctionOffset);
        return UEFunction(FunctionAddress, -1);
    }
};

class UEMulticastDelegateProperty : public UEProperty
{
private:
    static inline int32_t SignatureFunctionOffset = 0x78;

public:
    using UEProperty::UEProperty;

    UEFunction GetSignatureFunction() const
    {
        uint64_t FunctionAddress = Read<uint64_t>(SignatureFunctionOffset);
        return UEFunction(FunctionAddress, -1);
    }
};

// UEConst implementation
class UEConst : public UField
{
private:
    static inline int32_t ValueOffset = 0x30;

public:
    using UField::UField;

    std::string GetValue() const;
};

// Utility functions
inline bool operator==(const UEObject &Lhs, const UEObject &Rhs)
{
    return Lhs.GetAddress() == Rhs.GetAddress();
}

inline bool operator!=(const UEObject &Lhs, const UEObject &Rhs)
{
    return !(Lhs == Rhs);
}

inline bool operator<(const UEObject &Lhs, const UEObject &Rhs)
{
    return Lhs.GetAddress() < Rhs.GetAddress();
}


// 为UEClass添加哈希函数和相等比较函数，以支持在std::unordered_set中使用
namespace std {
    template<>
    struct hash<UEClass> {
        size_t operator()(const UEClass& obj) const {
            return hash<uint64_t>{}(obj.GetAddress());
        }
    };
}

inline bool operator==(const UEClass &Lhs, const UEClass &Rhs)
{
    return Lhs.GetAddress() == Rhs.GetAddress();
}

inline bool operator!=(const UEClass &Lhs, const UEClass &Rhs)
{
    return !(Lhs == Rhs);
}

