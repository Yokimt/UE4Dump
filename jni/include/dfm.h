// dfm.h
#pragma once
#include "module.hpp"
#include <filesystem>
#include <map>
extern DumpInfo g_dumpInfo;
// ================= 初始化函数 =================
void DumpInfo::InitOffsets()
{
    int ret = driver->cmd_ctl();
    if (ret == 0)
    {
        InitDriver = true;
    }
    driver->get_pid("com.tencent.tmgp.dfm");
    uint64_t base = driver->get_mod_base("libUE4.so");
    GName += base;
    g_FNamePool.Initialize(GName);
    UObjectAddress = MemoryReader::Read<uint64_t>(base + UObjectOffset);
    InitUObjectBaseOffset();
    InitUStructFFieldOffset();
    InitUEnum0ffset();
    InitFProperty0ffset();
    // InitFFunc();
}

// ================= 初始化实现 =================
void DumpInfo::InitUObjectBaseOffset()
{
    // 初始化InternalIndex偏移
    for (int offset = 0; offset < 2000; offset += 2)
    {
        int matchCount = 0;
        for (int idx = 0; idx < 20; idx++)
        {
            UObject objAddr = DumpUtils::GetUObjectFromID(idx);
            if (!objAddr.IsValid())
                continue;

            if (MemoryReader::Read<uint32_t>(objAddr.GetAddress() + offset) == idx)
            {
                matchCount++;
            }
        }
        if (matchCount > 18)
        {
            UObject::SetInternalIndexOffset(offset);
            break;
        }
    }
    // 初始化NamePrivate偏移
    for (int offset = 0; offset < 2000; offset += 2)
    {
        for (int idx = 0; idx < 20; idx++)
        {
            UObject objAddr = DumpUtils::GetUObjectFromID(idx);
            if (!objAddr.IsValid())
                continue;
            uint32_t nameId = MemoryReader::Read<uint32_t>(objAddr.GetAddress() + offset);
            if (g_FNamePool.GetName(nameId) == "/Script/CoreUObject")
            {
                UObject::SetNameOffset(offset);
                goto FoundNamePrivate;
            }
        }
    }
FoundNamePrivate:
    // 初始化ClassPrivate偏移
    for (int offset = 0; offset < 2000; offset += 2)
    {
        for (int idx = 0; idx < 10; idx++)
        {
            UObject objAddr = DumpUtils::GetUObjectFromID(idx);
            if (!objAddr.IsValid())
                continue;
            UEClass classAddr = UEClass(MemoryReader::Read<uint64_t>(objAddr.GetAddress() + offset), idx);
            string name = classAddr.GetName();
            if (name == "Package")
            {
                UObject::SetClassOffset(offset);
                goto FoundClassPrivate;
            }
        }
    }
FoundClassPrivate:
    // 初始化OuterPrivate偏移
    UObject objectClass = DumpUtils::FindObject("Object");
    UObject coreUObject = DumpUtils::FindObject("/Script/CoreUObject");
    if (!objectClass.IsValid())
        return;
    for (int offset = 0; offset < 2000; offset += 2)
    {
        UEClass outer = UEClass(MemoryReader::Read<uint64_t>(objectClass.GetAddress() + offset), 0);
        if (outer.IsValid() && outer == coreUObject)
        {
            UObject::SetOuterOffset(offset);
            break;
        }
    }
}

void DumpInfo::InitUStructFFieldOffset()
{
    const UObject pawn = DumpUtils::FindObject("Pawn");
    const UObject actor = DumpUtils::FindObject("Actor");
    const UObject vector = DumpUtils::FindObject("Vector");
    const UObject rotator = DumpUtils::FindObject("Rotator");
    if (!pawn.IsValid() || !actor.IsValid() || !vector.IsValid() || !rotator.IsValid())
    {
        printf("[InitUStructFFieldOffset] Failed to find required objects.\n");
        return;
    }

    // 查找SuperStruct偏移
    for (int superStructOffset = 0; superStructOffset < 2000; superStructOffset += 2)
    {
        uint64_t superStruct = pawn.GetAddress() + superStructOffset;
        if (MemoryReader::Read<uint64_t>(superStruct) == actor.GetAddress())
        {
            UStruct::SetSuperStructOffset(superStructOffset);
            break;
        }
    }
    // 定位ChildProperties 和FFiled
    for (int propOffset = 0; propOffset < 2000; propOffset += 2)
    {
        uint64_t propAddr = vector.GetAddress() + propOffset;
        uint64_t firstField = MemoryReader::Read<uint64_t>(propAddr);
        if (!firstField)
            continue;

        // 尝试在第一个字段上找到名称偏移
        int nameOffset = -1;
        for (int nameCandidate = 0; nameCandidate < 2000; nameCandidate += 2)
        {
            uint32_t nameId = MemoryReader::Read<uint32_t>(firstField + nameCandidate);
            string name = g_FNamePool.GetName(nameId);

            if (DumpUtils::IsBasicAscii(name) &&
                (name == "X" || name == "Y" || name == "Z"))
            {
                nameOffset = nameCandidate;
                break;
            }
        }
        if (nameOffset == -1)
            continue; // 没找到有效名称偏移
        int classOffset = -1;
        for (int classCandidate = 0; classCandidate < 2000; classCandidate += 2)
        {
            uint64_t classPtr = MemoryReader::Read<uint64_t>(firstField + classCandidate);
            if (DumpUtils::IsUnvalid(classPtr))
                continue;

            string className = g_FNamePool.GetName(MemoryReader::Read<uint32_t>(classPtr));
            if (className == "FloatProperty")
            {
                classOffset = classCandidate;
                break;
            }
        }
        if (classOffset == -1)
            continue; // 没找到类偏移
        // 尝试找到 Next 偏移
        for (int nextCandidate = 0; nextCandidate < 2000; nextCandidate += 2)
        {
            uint64_t secondField = MemoryReader::Read<uint64_t>(firstField + nextCandidate);
            if (DumpUtils::IsUnvalid(secondField))
                continue;

            uint64_t thirdField = MemoryReader::Read<uint64_t>(secondField + nextCandidate);
            if (DumpUtils::IsUnvalid(thirdField))
                continue;

            // 验证字段名称
            string name1 = g_FNamePool.GetName(MemoryReader::Read<uint32_t>(firstField + nameOffset));
            string name2 = g_FNamePool.GetName(MemoryReader::Read<uint32_t>(secondField + nameOffset));
            string name3 = g_FNamePool.GetName(MemoryReader::Read<uint32_t>(thirdField + nameOffset));
            if ((name1 == "X" || name1 == "Y" || name1 == "Z") &&
                (name2 == "X" || name2 == "Y" || name2 == "Z") &&
                (name3 == "X" || name3 == "Y" || name3 == "Z") &&
                MemoryReader::Read<uint64_t>(thirdField + nextCandidate) == 0)
            {
                // 找到有效偏移组合
                UStruct::SetChildProperties(propOffset);
                UField::SetNextOffset(nextCandidate);
                UField::SetNameOffset(nameOffset);
                UField::SetClassOffset(classOffset);
                goto FindPropertiesSize;
            }
        }
    }
FindPropertiesSize:
    // 定位PropertiesSize
    for (int PropertiesSizeOffset = 0; PropertiesSizeOffset < 2000; PropertiesSizeOffset += 2)
    {
        uint32_t vector_size = MemoryReader::Read<uint32_t>(vector.GetAddress() + PropertiesSizeOffset);
        uint32_t rotator_size = MemoryReader::Read<uint32_t>(rotator.GetAddress() + PropertiesSizeOffset);
        if (vector_size == 12 && rotator_size == 12)
        {
            UStruct::SetPropertySizeOffset(PropertiesSizeOffset);
            break;
        }
    }
}

void DumpInfo::InitFProperty0ffset()
{
    // 1. 准备已知属性对象
    const UObject vector = DumpUtils::FindObject("Vector");
    const UObject gameState = DumpUtils::FindObject("GameStateBase");

    if (!vector.IsValid() || !gameState.IsValid())
    {
        printf("[InitFProperty0ffset] Failed to find required objects.\n");
        return;
    }
    // 2. 获取已知属性字段
    UField vectorX, vectorY, vectorZ;
    UField gameStateArray;

    // 遍历Vector的属性
    for (UField prop = vector.Cast<UStruct>().GetChildProperties(); prop.IsValid(); prop = prop.GetNext())
    {
        string name = prop.GetName();
        if (name == "X")
            vectorX = prop;
        else if (name == "Y")
            vectorY = prop;
        else if (name == "Z")
            vectorZ = prop;
    }

    // 遍历GameStateBase的属性寻找数组属性
    for (UField prop = gameState.Cast<UStruct>().GetChildProperties(); prop.IsValid(); prop = prop.GetNext())
    {
        if (prop.GetClassName() == "ArrayProperty")
        {
            // std::cout<<"GetClassName"<<prop.GetClassName()<<std::endl;
            gameStateArray = prop;
            break;
        }
    }

    // 3. 定位Offset偏移
    if (vectorX.IsValid() && vectorY.IsValid() && vectorZ.IsValid())
    {
        for (int offset = 0; offset < 2000; offset += 2)
        {
            uint32_t xVal = MemoryReader::Read<uint32_t>(vectorX.GetAddress() + offset);
            uint32_t yVal = MemoryReader::Read<uint32_t>(vectorY.GetAddress() + offset);
            uint32_t zVal = MemoryReader::Read<uint32_t>(vectorZ.GetAddress() + offset);

            if (xVal == 0 && yVal == 4 && zVal == 8)
            {
                UEProperty::SetOffsetInternalOffset(offset);
                break;
            }
        }
    }
    // 4. 定位Size偏移
    if (gameStateArray.IsValid())
    {
        for (int offset = 0; offset < 2000; offset += 8)
        { // 步进8字节（指针大小）
            // 读取可能的 UClass 指针
            uint64_t innerPtr = MemoryReader::Read<uint64_t>(gameStateArray.GetAddress() + offset);
            string innerClassName = UEProperty(innerPtr, -1).GetName();
            if (innerClassName == "PlayerArray")
            {
                UEProperty::SetChildClassOffset(offset);
                break;
            }
        }
    }
    // 5. 定位ElementSize偏移(需要数组属性)
    if (gameStateArray.IsValid())
    {
        // 首先定位ArrayProperty的Inner属性偏移
        uint16_t innerOffset = 0;
        for (int offset = 0; offset < 2000; offset += 2)
        {
            uint64_t innerPtr = MemoryReader::Read<uint64_t>(gameStateArray.GetAddress() + offset);
            if (innerPtr)
            {
                // 验证innerPtr是否指向有效的FField对象
                string className = UField(innerPtr, -1).GetClassName();

                if (!className.empty() && className.find("FloatProperty") != string::npos)
                {
                    innerOffset = offset;
                    break;
                }
            }
        }
        if (innerOffset)
        {
            uint64_t innerProp = MemoryReader::Read<uint64_t>(gameStateArray.GetAddress() + innerOffset);

            // 在inner属性中定位ElementSize
            for (int offset = 0; offset < 2000; offset += 2)
            {
                uint32_t elemSize = MemoryReader::Read<uint32_t>(innerProp + offset);
                // 验证元素大小是否合理
                if (elemSize == 4)
                {
                    UEProperty::SetElementSizeOffset(offset);
                    break;
                }
            }
        }
    }

    // 输出定位结果
}

void DumpInfo::InitUEnum0ffset()
{
    // 1. 准备已知枚举对象和参考值
    const UObject collisionChannelEnum = DumpUtils::FindObject("ECollisionChannel");
    if (!collisionChannelEnum.IsValid())
    {
        printf("[InitUEnum0ffset] Failed to find ECollisionChannel enum\n");
        return;
    }
    string referenceItems[6] = {"ECC_WorldStatic", "ECC_WorldDynamic", "ECC_Pawn", "ECC_Visibility", "ECC_Camera", "ECC_PhysicsBody"};
    // collisionChannelEnum.Cast<UEEnum>().GetNameList();
    int name_offset = 0;
    int value_offset = 0;
    int size_offset = 0;
    int names_offset = 0;
    for (names_offset = 0; names_offset < 200; names_offset += 2)
    {
        UEEnum::FNameArray names = MemoryReader::Read<UEEnum::FNameArray>(collisionChannelEnum.GetAddress() + names_offset);
        for (size_offset = 0; size_offset < 100; size_offset += 2)
        {
            for (name_offset = 0; name_offset < 100; name_offset += 2)
            {
                bool allmatch = true;
                for (int i = 0; i < 6; i++)
                {
                    std::string name = g_FNamePool.GetName(MemoryReader::Read<uint32_t>(names.Data + i * size_offset + name_offset));
                    if (name != referenceItems[i])
                    {
                        allmatch = false;
                        break;
                    }
                }
                if (allmatch)
                {
                    for (value_offset = 0; value_offset < 100; value_offset += 2)
                    {
                        bool allfound = true;
                        for (size_t i = 0; i < 6; i++)
                        {
                            int value = MemoryReader::Read<int>(names.Data + i * size_offset + value_offset);
                            if (value != i)
                            {
                                allfound = false;
                                break;
                            }
                        }
                        if (allfound)
                        {
                            break; // 找到所有值匹配，退出循环
                        }
                    }
                    UEEnum::SetNameOffset(name_offset);
                    UEEnum::SetValueOffset(value_offset);
                    UEEnum::SetSizeOffset(size_offset);
                    UEEnum::SetNamesOffset(names_offset);
                    return; // 成功找到偏移，退出函数
                }
            }
        }
    }
}
// TODO
void DumpInfo::InitFFunc()
{
}
// ================= SDK生成 =================

void DumpInfo::DumpClass(UObject addr, string out, unordered_set<string> &BasicEngineTypes)
{
    UEClass tmp = addr.Cast<UEClass>();
    string CppName = DumpUtils::GetCppName(tmp);
    if (CppName.empty() || !DumpUtils::IsBasicAscii(CppName))
        return;

    ofstream Class(out + "/" + CppName + ".h");
    if (!Class.is_open())
        return;

    // 收集所有依赖的类型头文件
    unordered_set<string> headerIncludes;
    headerIncludes.insert("BasicTypes"); // 公共基础头文件
    UEClass SuperStruct = tmp.GetSuperStruct().Cast<UEClass>();
    if (SuperStruct.IsValid())
    {
        string superType = DumpUtils::GetCppName(SuperStruct);
        string baseType = DumpUtils::ExtractBaseType(superType);

        // 基类总是需要包含头文件
        if (!baseType.empty())
        {
            headerIncludes.insert(baseType);
        }
    }
    // 遍历属性收集类型
    vector<pair<string, UField>> properties; // <类型, 属性地址>
    for (UField temp = tmp.GetChildProperties(); temp.IsValid(); temp = temp.GetNext())
    {
        string type = temp.GetType();
        properties.push_back({type, temp});

        // 提取基础类型名
        string baseType = DumpUtils::ExtractBaseType(type);
        DumpUtils::NeedsHeader(baseType, BasicEngineTypes, headerIncludes);
    }

    // 写入头文件包含
    Class << "#pragma once\n\n";
    for (const auto &include : headerIncludes)
    {
        if (include.empty())
        {
            continue;
        }
        Class << "#include  \"" << include << ".h\"\n";
    }
    Class << "\n";

    // 写入类头部信息
    Class << "// Class: " << tmp.Cast<UObject>().GetFullName() << "\n";
    Class << "// Size: 0x" << hex << tmp.GetPropertySize() << dec << "\n\n";

    Class << "class " << CppName;
    if (SuperStruct.IsValid())
    {
        Class << " : public " << DumpUtils::GetCppName(SuperStruct);
    }
    Class << "\n{\npublic:\n";

    string Body;
    uint32_t Pos = SuperStruct.IsValid() ? SuperStruct.GetPropertySize() : 0;

    // 遍历属性生成定义
    for (const auto &prop : properties)
    {
        string Type = prop.first;
        UField temp = prop.second;
        string Name = temp.GetName();
        uint32_t Offset = temp.Cast<UEProperty>().GetOffset();
        uint32_t Size = temp.Cast<UEProperty>().GetElementSize();

        // 格式化属性行
        string PropLine = "    " + Type;
        if (PropLine.size() < 50)
        {
            PropLine.append(50 - PropLine.size(), ' ');
        }
        PropLine += Name + ";";

        // 格式化注释
        if (PropLine.size() < 50)
        {
            PropLine.append(50 - PropLine.size(), ' ');
        }
        char Comment[128];
        sprintf(Comment, " // 0x%04X(0x%04X)\n", Offset, Size);
        PropLine += Comment;

        // 检查并添加填充字段
        if (Pos < Offset)
        {
            uint32_t diff = Offset - Pos;
            string PadLine = "    char";
            if (PadLine.size() < 50)
            {
                PadLine.append(50 - PadLine.size(), ' ');
            }

            char PadName[128];
            sprintf(PadName, "pad_0x%04X[0x%04X];", Pos, diff);
            PadLine += PadName;

            if (PadLine.size() < 50)
            {
                PadLine.append(50 - PadLine.size(), ' ');
            }

            char PadComment[128];
            sprintf(PadComment, " // 0x%04X(0x%04X)\n", Pos, diff);
            PadLine += PadComment;

            Body += PadLine;
        }

        Pos = Offset + Size;
        Body += PropLine;
    }

    // 添加尾部填充
    uint32_t TotalSize = tmp.GetPropertySize();
    if (Pos < TotalSize)
    {
        uint32_t diff = TotalSize - Pos;
        string PadLine = "    char";
        if (PadLine.size() < 50)
        {
            PadLine.append(50 - PadLine.size(), ' ');
        }

        char PadName[128];
        sprintf(PadName, "pad_0x%04X[0x%04X];", Pos, diff);
        PadLine += PadName;

        if (PadLine.size() < 50)
        {
            PadLine.append(50 - PadLine.size(), ' ');
        }

        char PadComment[128];
        sprintf(PadComment, " // 0x%04X(0x%04X)\n", Pos, diff);
        PadLine += PadComment;

        Body += PadLine;
    }
    // 写入主体并关闭类定义
    Class << Body;
    Class << "};\n";
    Class.close();
}
void DumpInfo::DumpStruct(UObject addr, string out, unordered_set<string> &BasicEngineTypes)
{
    UStruct tmp = addr.Cast<UStruct>();
    string CppName = DumpUtils::GetCppName(tmp);
    if (CppName.empty() || !DumpUtils::IsBasicAscii(CppName))
        return;

    ofstream Struct(out + "/" + CppName + ".h");
    if (!Struct.is_open())
        return;

    // 收集所有依赖的类型头文件
    unordered_set<string> headerIncludes;
    headerIncludes.insert("BasicTypes"); // 公共基础头文件
    UEClass SuperStruct = tmp.GetSuperStruct().Cast<UEClass>();
    if (SuperStruct.IsValid())
    {
        string superType = DumpUtils::GetCppName(SuperStruct);
        string baseType = DumpUtils::ExtractBaseType(superType);

        // 基类总是需要包含头文件
        if (!baseType.empty())
        {
            headerIncludes.insert(baseType);
        }
    }
    // 遍历属性收集类型
    vector<pair<string, UField>> properties; // <类型, 属性地址>
    for (UField temp = tmp.GetChildProperties(); temp.IsValid(); temp = temp.GetNext())
    {
        string type = temp.GetType();
        properties.push_back({type, temp});

        // 提取基础类型名
        string baseType = DumpUtils::ExtractBaseType(type);
        DumpUtils::NeedsHeader(baseType, BasicEngineTypes, headerIncludes);
    }

    // 写入头文件包含
    Struct << "#pragma once\n\n";
    for (const auto &include : headerIncludes)
    {
        if (include.empty())
        {
            continue;
        }
        Struct << "#include  \"" << include << ".h\"\n";
    }
    Struct << "\n";
    // 写入类头部信息
    Struct << "// Struct: " << tmp.Cast<UObject>().GetFullName() << "\n";
    Struct << "// Size: 0x" << hex << tmp.GetPropertySize() << dec << "\n\n";

    Struct << "struct " << CppName;
    if (SuperStruct.IsValid())
    {
        Struct << " : public " << DumpUtils::GetCppName(SuperStruct);
    }
    Struct << "\n{\npublic:\n";

    string Body;
    uint32_t Pos = SuperStruct.IsValid() ? SuperStruct.GetPropertySize() : 0;

    // 遍历属性生成定义
    for (const auto &prop : properties)
    {
        string Type = prop.first;
        UField temp = prop.second;
        string Name = temp.GetName();
        uint32_t Offset = temp.Cast<UEProperty>().GetOffset();
        uint32_t Size = temp.Cast<UEProperty>().GetElementSize();

        // 格式化属性行
        string PropLine = "    " + Type;
        if (PropLine.size() < 50)
        {
            PropLine.append(50 - PropLine.size(), ' ');
        }
        PropLine += Name + ";";

        // 格式化注释
        if (PropLine.size() < 50)
        {
            PropLine.append(50 - PropLine.size(), ' ');
        }
        char Comment[128];
        sprintf(Comment, " // 0x%04X(0x%04X)\n", Offset, Size);
        PropLine += Comment;

        // 检查并添加填充字段
        if (Pos < Offset)
        {
            uint32_t diff = Offset - Pos;
            string PadLine = "    char";
            if (PadLine.size() < 50)
            {
                PadLine.append(50 - PadLine.size(), ' ');
            }

            char PadName[128];
            sprintf(PadName, "pad_0x%04X[0x%04X];", Pos, diff);
            PadLine += PadName;

            if (PadLine.size() < 50)
            {
                PadLine.append(50 - PadLine.size(), ' ');
            }

            char PadComment[128];
            sprintf(PadComment, " // 0x%04X(0x%04X)\n", Pos, diff);
            PadLine += PadComment;

            Body += PadLine;
        }

        Pos = Offset + Size;
        Body += PropLine;
    }

    // 添加尾部填充
    uint32_t TotalSize = tmp.GetPropertySize();
    if (Pos < TotalSize)
    {
        uint32_t diff = TotalSize - Pos;
        string PadLine = "    char";
        if (PadLine.size() < 50)
        {
            PadLine.append(50 - PadLine.size(), ' ');
        }

        char PadName[128];
        sprintf(PadName, "pad_0x%04X[0x%04X];", Pos, diff);
        PadLine += PadName;

        if (PadLine.size() < 50)
        {
            PadLine.append(50 - PadLine.size(), ' ');
        }

        char PadComment[128];
        sprintf(PadComment, " // 0x%04X(0x%04X)\n", Pos, diff);
        PadLine += PadComment;

        Body += PadLine;
    }
    // 写入主体并关闭类定义
    Struct << Body;
    Struct << "};\n";
    Struct.close();
}
void DumpInfo::DumpEnum(UObject addr, string out, unordered_set<string> &BasicEngineTypes)
{
    UEEnum tmp = addr.Cast<UEEnum>();
    string Name = addr.GetName();
    if (Name.empty() || !DumpUtils::IsBasicAscii(Name)) return;
    ofstream Enum(out + "/" + Name + ".h");
    if (!Enum.is_open()) return;
    // 写入头文件包含
    Enum << "#pragma once\n\n";
    Enum << "#include \"BasicTypes.h\"\n\n";
    Enum << "/" << tmp.Cast<UObject>().GetFullName() << "\n";
    Enum << "enum " << Name <<endl;
    Enum << "{\n";
    // 遍历枚举值
    std::vector<std::pair<std::string,int>>  enumValues = tmp.GetNameList(); // 存储枚举值和对应的整数
    
    for (const auto &pair : enumValues)
    {
        const std::string &tmp_name = pair.first;
        int value = pair.second;
        size_t pos = tmp_name.find("::");
        string name = (pos != std::string::npos) ? tmp_name.substr(pos + 2) : tmp_name; // 去除命名空间前缀
        // 格式化枚举行
        Enum << "    " << name;
        if (name.size() < 30)
        {
            Enum <<std::left <<std::setw(30 - name.size()) << std::setfill(' ') << "";
        }
        Enum << " = " << value << ",\n";
    }
    Enum << "};\n";
    Enum.close();
}
void DumpInfo::DumpSDK(string out)
{
    if (!std::filesystem::exists(out))
    {
        bool success = std::filesystem::create_directories(out);
        bool success1 = std::filesystem::create_directories(out + "/sdk");
        if (!success || !success1)
        {
            std::cerr << "Failed to create directory: " << out << std::endl;
            return;
        }
        std::cout << "Created directory: " << out << std::endl;
    }

    ofstream Object(out + "/Object.h");
    ofstream BasicTypes(out + "/sdk/BasicTypes.h");
    unordered_set<string> BasicEngineTypes;

    if (!Object.is_open() || !BasicTypes.is_open())
        return;

    cout << "Dumping Object List" << endl;
    clock_t begin = clock();
    BasicTypes << BasicTypes_h << endl;
    g_dumpInfo.UStructStatic = UStruct::StaticClass();
    g_dumpInfo.UScriptStructStatic = UEScriptStruct::StaticClass();
    g_dumpInfo.UObjectStatic = UObject::StaticClass();
    g_dumpInfo.AActorStatic = AActor::StaticClass();
    g_dumpInfo.UEEnumStatic = UEEnum::StaticClass();

    uint32_t oCount = MemoryReader::Read<uint32_t>(g_dumpInfo.UObjectAddress + g_dumpInfo.offsets.NumElements);
    cout << "Objects Counts: " << std::dec << oCount << endl;
    ProgressBar progress(oCount);
    for (uint32_t i = 0; i < oCount; i++)
    {
        UObject uobj = DumpUtils::GetUObjectFromID(i);
        if (uobj.IsValid())
        {
            if (DumpUtils::IsA(uobj, DumpUtils::UStructFlag))
            {
                DumpInfo::DumpClass(uobj, out + "/sdk", BasicEngineTypes);
            }
            else if (DumpUtils::IsA(uobj, DumpUtils::UScriptStructFlag))
            {
                DumpInfo::DumpStruct(uobj, out + "/sdk", BasicEngineTypes);
            }
            else if (DumpUtils::IsA(uobj, DumpUtils::UEEnumFlag))
            {
                DumpInfo::DumpEnum(uobj, out + "/sdk", BasicEngineTypes);
            }
            // TODO dump enum func
            Object << "{" << std::right << std::setw(6) << std::setfill('0') << i << "}\t[" << uobj.GetAddress() << "]\t" << uobj.GetName() << "\t" << uobj.GetFullName() << endl;
            classCount++;
        }
        progress.update(i);
    }
    for (const auto &Type : BasicEngineTypes)
    {
        BasicTypes << Type << endl;
    }
    progress.finish();
    BasicTypes.close();
    Object.close();
    clock_t end = clock();
    double elapsed_secs = double(end - begin) / CLOCKS_PER_SEC;
    cout << classCount << " Items Dumped in SDK in " << elapsed_secs << "S" << endl;
}