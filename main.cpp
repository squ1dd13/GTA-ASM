#include <iostream>
#include <map>
#include <vector>
#include <fstream>
#include <sstream>
#include "util.hpp"
#include "opcodes.hpp"
#include "highlighting.hpp"
#include <set>
#include <unistd.h>
#include "miss2/constructs.hpp"
#include "miss2/context.hpp"
#include "gxt.hpp"
#include "miss2/decompiler.hpp"
#include "miss2/serialization.hpp"
#include "miss2/script.hpp"

// Stores all call destinations.
std::set<int32_t> procedureLocations;

/* https://gtamods.com/wiki/Opcode */
enum ParamType : uint8_t {
    // The values don't really need to be included, but they are important.
    // It is useful to be able to refer to this enum as a table.

    EOAL = 0x0,
    S32 = 0x1,  // Immediate
    GlobalIntFloat = 0x2, // 16-bit global offset
    LocalIntFloat = 0x3, // 16-bit local offset
    S8 = 0x4, // Immediate
    S16 = 0x5, // Immediate
    F32 = 0x6, // Immediate
    GlobalIntFloatArr = 0x7,
    LocalIntFloatArr = 0x8,
    String8 = 0x9, // Immediate
    GlobalString8 = 0xA, // 16-bit global offset
    LocalString8 = 0xB, // 16-bit local offset
    GlobalString8Arr = 0xC,
    LocalString8Arr = 0xD,
    StringVar = 0xE, // Immediate, 8-bit size followed by characters
    String16 = 0xF, // Immediate
    GlobalString16 = 0x10, // 16-bit global offset
    LocalString16 = 0x11, // 16-bit local offset
    GlobalString16Arr = 0x12,
    LocalString16Arr = 0x13,
};

bool isValidParamType(uint8_t toCheck) {
    return 0x1 <= toCheck and toCheck <= 0x13;
}

struct ParamInfo {
    uint8_t size;
    std::string name;
    std::function<std::string(uint8_t *)> stringRep;
};

template <typename typ>
std::string strrepFunc(uint8_t *bytes) {
    return std::to_string(*(typ *)bytes);
}

static int varStrSizeForRep = 0;

#define STRREP(typ) strrepFunc<typ>//[](uint8_t *bytes){return std::to_string(*(typ *)bytes);}
#define NUMREP(typ) [](uint8_t *bytes){return std::to_string((long long)(*(typ *)bytes));}
//#define ARRREP [](uint8_t *){return "<array>";}
std::string ARRREP(uint8_t *){return "<array>";}
static std::map<ParamType, ParamInfo> paramTypeInfo {
    { EOAL, {0, "end", [](uint8_t *){return "end";}} },
    { S32, {4, "S32", STRREP(int32_t)} },
    { GlobalIntFloat, {2, "GlobalIntFloat", STRREP(uint16_t)} },
    { LocalIntFloat, {2, "LocalIntFloat", STRREP(uint16_t)} },
    { S8, {1, "S8", NUMREP(int8_t)} },
    { S16, {2, "S16", STRREP(int16_t)} },
    { F32, {4, "Float", STRREP(float)} },
    { GlobalIntFloatArr, {6, "GlobalIntFloatArr", ARRREP} },
    { LocalIntFloatArr, {6, "LocalIntFloatArr", ARRREP} },
    { String8, {8, "Char[8]", [](uint8_t *bytes){return std::string(bytes, bytes + 8);}} },
    { GlobalString8, {2, "GlobalString8", STRREP(uint16_t)} },
    { LocalString8, {2, "LocalString8", STRREP(uint16_t)} },
    { GlobalString8Arr, {6, "GlobalString8Arr", ARRREP} },
    { LocalString8Arr, {6, "LocalString8Arr", ARRREP} },
    { StringVar, {0, "VarStr", [](uint8_t *bytes){return "'" + std::string(bytes, bytes + varStrSizeForRep) + "'";}} },
    { String16, {16, "Char[16]", [](uint8_t *bytes){return std::string(bytes, bytes + 16);}} },
    { GlobalString16, {2, "GlobalString16", STRREP(uint16_t)} },
    { LocalString16, {2, "LocalString16", STRREP(uint16_t)} },
    { GlobalString16Arr, {6, "GlobalString16Arr", ARRREP} },
    { LocalString16Arr, {6, "LocalString16Arr", ARRREP} },
};

static uint8_t paramSize(ParamType type) {
    if(paramTypeInfo.count(type)) return paramTypeInfo[type].size;
    return 0;
}

struct Instruction {
    std::string name = "nop";
    uint16_t opcode = 0x0000;
    std::vector<ParamType> params;

    static std::pair<uint16_t, Instruction> create(string_ref mn, uint16_t op, const std::vector<ParamType> &types = {}) {
        Instruction instr;
        instr.name = mn;
        instr.opcode = op;
        instr.params = types;

        return {
            op,
            instr
        };
    }
};

struct PlaceholderInstruction {
    std::string name;
    uint16_t opcode;
    std::vector<uint8_t> paramSizes;

    std::string toString() {
        std::stringstream stream;

        stream << std::hex << opcode << ": " << name << std::dec;

        for(auto psize : paramSizes) {
            stream << (int)psize << ' ';
        }

        return stream.str();
    }
};

static std::map<uint16_t, Instruction> opcodeIndex {
    Instruction::create("nop", 0x0),
    Instruction::create("scriptname", 0x03A4, {String8}),
    Instruction::create("setlocalint", 0x0006, {LocalIntFloat, S8}),
    Instruction::create("goto", 0x0002, {S8}),
};

static std::vector<uint8_t> psizes;
static int foundTokenIndex = 0;
std::string removeTokens(std::string &dirty) {
    auto firstPercent = dirty.find("%");
    if(firstPercent == std::string::npos) return dirty;

    auto secondPercent = dirty.find_first_of('%', firstPercent + 1);
    if(secondPercent == firstPercent or secondPercent == std::string::npos) return dirty;

    //std::cout << dirty.substr(firstPercent, (secondPercent - firstPercent) + 1) << '\n';
    auto numstr = dirty.substr(firstPercent + 1, (secondPercent - firstPercent) - 2);
    //std::cout << numstr << '\n';
    psizes.push_back(std::stoi(numstr));

    //dirty.erase(firstPercent, (secondPercent - firstPercent) + 1);
    dirty.replace(firstPercent, (secondPercent - firstPercent) + 1, "$" +/* std::to_string(foundTokenIndex++)*/std::to_string(std::stoi(numstr) - 1));

    return removeTokens(dirty);
}

struct ScriptParam {
    int m_iIntValue {};
    unsigned short m_usGlobalOffset {};
    short m_sLocalVar {};
    float m_fFloatValue {};
    short m_sArrayIndexVar {};
    unsigned char m_ucArraySize {};
    char *m_szTextLabel = new char[256];

    virtual ~ScriptParam() {
        delete[] m_szTextLabel;
    }
};

std::map<uint16_t, PlaceholderInstruction> placeholderInstructions;

void parseOpcodeFile(string_ref path) {
    std::ifstream stream(path);

    while(stream) {
        std::stringstream thisLine;

        char c;
        while(stream and (c = stream.get()) != '\n') {
            thisLine << c;
        }

        std::string s = thisLine.str();

        if(s.starts_with(';') or s.starts_with('[')) continue;

        auto commentIndex = s.find(';');
        if(commentIndex != std::string::npos) {
            s = s.substr(0, commentIndex);
        }

        commentIndex = s.find("//");
        if(commentIndex != std::string::npos) {
            s = s.substr(0, commentIndex);
        }

        trim(s);

        PlaceholderInstruction instruction;

        auto equalsIndex = s.find('=');
        if(equalsIndex == std::string::npos) continue;

        std::string opcodeString = s.substr(0, equalsIndex);
        trim(opcodeString);

        if(opcodeString.find_first_not_of("abcdefABCDEF0123456789") != std::string::npos) continue;

        std::string infoString = s.substr(equalsIndex + 1);

        auto commaIndex = s.find(',');
        infoString = s.substr(commaIndex + 1);
        trim(infoString);

        instruction.opcode = std::stoi(opcodeString, 0, 16);
        std::string before = infoString;
        instruction.name = removeTokens(infoString);

        instruction.paramSizes = psizes;

        psizes.clear();
        foundTokenIndex = 0;

        placeholderInstructions[instruction.opcode] = instruction;

        miss2::Command m2cmd {
            .name = instruction.name,
            .opcode = instruction.opcode
        };

        int i = 0;
        for(auto &p : instruction.paramSizes) {
            // Add and unknown type for now. The real type will be added when the decompiler
            //  finds an instance of this command.
            m2cmd.parameters.push_back(miss2::Unknown);
            m2cmd.parameters.back().size = instruction.paramSizes[i++];
        }

        miss2::Command::registerOpcode(instruction.opcode, m2cmd);

        if(not (instruction.opcode & 0xF000)) {
            uint16_t otherOpcode = instruction.opcode | 0x8000;
            if(not miss2::Command::get(otherOpcode)) {
                m2cmd.opcode = otherOpcode;
                miss2::Command::registerOpcode(otherOpcode, m2cmd);
            }
        }
    }
}

template <typename T>
T readAndAdvance(uint8_t *&ptr) {
    T val = *(T *)ptr;
    ptr += sizeof(T);

    return val;
}

void showComment(string_ref s) {
    std::cout << asComment("// " + s) << '\n';
}

struct DecompiledParam {
    ParamType type;
    std::vector<uint8_t> bytes;
    std::string stringRep;

    operator std::string() const {
        return stringRep;
    }
};

std::string formatAsTypename(ParamType type) {
    return codeColor + "(" + blue + paramTypeInfo[type].name + codeColor + ")";
}

std::vector<DecompiledParam> getParamStrings(PlaceholderInstruction &instruction, uint8_t *&scriptPointer, uint8_t *&bytes) {
    std::vector<DecompiledParam> paramStrings;

    for(auto &psz : instruction.paramSizes) {
        size_t paramOffset = scriptPointer - bytes;
        ParamType type = readAndAdvance<ParamType>(scriptPointer);

        auto info = paramTypeInfo[type];
        if(info.name.empty()) {
            //std::string errComment = asComment("// Unknown parameter type 0x");
            //std::cout << errComment << std::hex << type << " at offset " << std::dec << paramOffset << '\n';
            continue;
        }

        bool typeIsVStr = type == StringVar;

        auto size = paramTypeInfo[type].size;
        if(typeIsVStr) {
            // Size currently 0, so read the size.
            size = readAndAdvance<uint8_t>(scriptPointer);// + 1;
        }

        varStrSizeForRep = size;

        uint8_t *paramBytes = new uint8_t[size];

        bool varStrDidEnd = false;
        for(int i = 0; i < size; ++i) {
            uint8_t thisByte = *(scriptPointer++);

            if(typeIsVStr and not std::isprint(thisByte)) {
                varStrDidEnd = true;
            }

            paramBytes[i] = varStrDidEnd ? 0 : thisByte;
        }

        std::string stringRep = info.stringRep(paramBytes);

        std::vector<uint8_t> bytesVector(paramBytes, paramBytes + size);
        delete[] paramBytes;

        std::string paramString = "(" + blue + info.name + codeColor + ")" + green + stringRep + codeColor;

        paramStrings.push_back({type, bytesVector, paramString});
    }

    return paramStrings;
}

// Holds information about a global variable. Allows for better understanding of context.
struct GlobalVariable {
    uint16_t offset;
    ParamType type;
    std::set<ParamType> assignedTypes;

    std::string toString() {
        static std::set<ParamType> integerTypes = { S8, S16, S32 };

        if(assignedTypes.empty()) {
            // No assignment info, so we can't produce a meaningful name.
            return formatAsTypename(type) + green + paramTypeInfo[type].stringRep((uint8_t *)&offset) + codeColor;
        }

        ParamType assignedType = *assignedTypes.begin();

        static std::string globalVarFormat = varColor + "global$0_$1" + codeColor;

        std::string typeName = integerTypes.count(assignedType) ? "Int" : "Float";
        return replaceTokens(globalVarFormat, {typeName, std::to_string((int)offset)});
    }
};

static std::map<uint16_t, GlobalVariable> allGlobalVariables;

void printEmptyLine(size_t offset, string_ref comment = "") {
    // Match the number of digits in the offset with spaces.
    std::string spaceStr(countDigits(offset), ' ');

    static std::string procedureFormatStr = "/* $0 */ $1\n";
    std::cout << asComment(replaceTokens(procedureFormatStr, {spaceStr, comment.empty() ? "" : "// " + comment}));
}

struct CompiledParameter {
    ParamType type;
    uint8_t *data = nullptr;

    // This is *NOT* designed for proper decompilation. Use only when no better methods
    //  are available (e.g. when the instruction is not known).
    static CompiledParameter read(uint8_t *&scriptPointer) {
        CompiledParameter param;

        param.type = readAndAdvance<ParamType>(scriptPointer);

        std::vector<uint8_t> readData;

        uint8_t readByte;// = *(scriptPointer++);

        while(
            not isValidParamType(readByte = *(scriptPointer + 1))
            and not placeholderInstructions.count(*(uint16_t *)(scriptPointer + 1))
            ) {

            readData.push_back(readByte);
            ++scriptPointer;
        };

        if(not readData.empty()) {
            param.data = new uint8_t[readData.size()];
            std::copy(readData.begin(), readData.end(), param.data);
        }

        return param;
    }

    virtual ~CompiledParameter() {
        delete[] data;
    }
};

void printDisassembly(string_ref filename) {
    std::string topCommentFormat = "/*\n  $0\n  Decompiled by miss3 on $1.\n*/\n";
    std::string lpc = lastPathComponent(filename);
    std::string dateTime = currentDateString();

    std::cout << asComment(replaceTokens(topCommentFormat, {lpc, dateTime})) << '\n';

    auto bytesVector = readFileBytes(filename.c_str());

    uint8_t *bytes = new uint8_t[bytesVector.size()];
    std::copy(bytesVector.begin(), bytesVector.end(), bytes);
    uint8_t *scriptPointer = bytes;

    // Becomes true on entering 'if', becomes false when something that should end the 'if' is found (call, jump, etc.)
    bool inIfCondition = false;

    for(size_t scriptOffset = 0; scriptPointer < bytes + bytesVector.size(); ++scriptOffset) {
        size_t opcodeOffset = scriptPointer - bytes;

        // Remember script point from before reading opcode.
        auto backup = scriptPointer;

        if(procedureLocations.count(opcodeOffset)) {
            printEmptyLine(opcodeOffset, "Procedure");
        }

        uint16_t opcode = readAndAdvance<uint16_t>(scriptPointer);
        if(opcode == 0) continue;

        // Read a miss2 command.
        //miss2::Command command = miss2::Command::read(backup);
        //command.offset = opcodeOffset;

        //std::cout << command.name << '\n';
        //for(miss2::Value &param : command.parameters) {
        //    std::cout << " p: " << miss2::valueToString(param) << '\n';
        //}

        if(opcode == Opcode::JumpIfFalse or opcode == Opcode::Call) {
            inIfCondition = false;
            printEmptyLine(opcodeOffset);
        }

        if(not placeholderInstructions.count(opcode)) {
            std::string offsetStr = asComment(replaceTokens("/* $0 */ ", { std::to_string(opcodeOffset) }));
            std::cout << offsetStr << std::hex << "// 0x" << opcode << std::dec << '\n';

            inIfCondition = false;

            continue;
        }

        PlaceholderInstruction instruction = placeholderInstructions[opcode];

        auto paramObjects = getParamStrings(instruction, scriptPointer, bytes);

        std::vector<std::string> paramStrings;
        for(int i = 0; i < paramObjects.size(); ++i) {//auto &obj : paramObjects) {
            auto &obj = paramObjects[i];

            if(opcode == Opcode::Call) {
                // Remember the call location as a procedure.
                procedureLocations.insert(std::abs(*(int32_t *)obj.bytes.data()));
            }

            if(obj.type == GlobalIntFloat) {
                // Create a more intelligent string representation if we can.
                uint16_t globalOffset = *(uint16_t *)obj.bytes.data();

                GlobalVariable &var = allGlobalVariables[globalOffset];
                var.type = GlobalIntFloat;
                var.offset = globalOffset;

                // If this is an assignment, we need to add the assigned type to the global var.
                if(paramObjects.size() == 2 and instruction.name.find("=") != std::string::npos) {
                    // Probably an assignment.
                    auto &otherObj = paramObjects[!i];

                    static std::set<ParamType> compatibleTypes { S8, S16, S32, F32 };
                    if(compatibleTypes.count(otherObj.type)) {
                        // Types are compatible.
                        var.assignedTypes.insert(otherObj.type);
                    }
                }

                if(allGlobalVariables.count(globalOffset)) {
                    paramStrings.push_back(var.toString());
                    continue;
                }

                allGlobalVariables[globalOffset] = var;
            }

            paramStrings.push_back(obj.stringRep);
        }

        //std::cout << gray << "// Opcode 0x" << std::hex << instruction.opcode << std::dec << '\n';
        std::string formatted = replaceTokens(instruction.name, paramStrings);
        if(inIfCondition) {
            formatted = "    " + formatted;
        }

        std::string offsetStr = asComment(replaceTokens("/* $0 */ ", { std::to_string(opcodeOffset) }));
        std::cout << offsetStr << /*std::hex << instruction.opcode << std::dec << ':' << */codeColor << formatted << '\n';

        // Break up the output with an extra newline after returns.
        if(opcode == Opcode::Return) {
            printEmptyLine(opcodeOffset);
        }

        // Switch after print so 'if' doesn't get indented.
        if(opcode == Opcode::If) {
            inIfCondition = true;
        }
    }

    delete[] bytes;
}

int main(int argc, char **argv) {
    if(argc > 2) {
        // argv[1] is the input file
        // argv[2] is the output file

        // Decompile the file to an intermediate representation and output
        //  that representation to a file for further processing.

        parseOpcodeFile("/Users/squ1dd13/CLionProjects/gtasm/Opcodes.ini");

        unlink(argv[2]);
        std::ofstream outFile(argv[2]);

        miss2::Script script = miss2::Decompiler::decompile(argv[1]);

        bool first = true;
        for(miss2::Command &command : script.commands) {
            if(not first) {
                outFile << '\n';
            }

            first = false;

            outFile << command.offset << ':' << command.opcode << '[';

            int i = 0;
            for(miss2::Value &param : command.parameters) {
                outFile << miss2::primitiveVtoS(param);

                // << ','

                if(i++ != command.parameters.size() - 1) {
                    outFile  << ',';
                }
            }

            outFile << ']';
        }

        return 0;
    }

    // /Users/squ1dd13/CLionProjects/gtasm/GTA Scripts/planes.scm

    // /Users/squ1dd13/Library/Application Support/
    // CrossOver/Bottles/Grand Theft Auto San Andreas/
    // drive_c/Program Files/Steam/steamapps/common/
    // Grand Theft Auto San Andreas/data/script/main.scm
// /Users/squ1dd13/gta_wine/drive_c/Program Files/Rockstar Games/GTA San Andreas/data/script/main.scm
    miss2::optimize_jumps = true;

    // /Users/squ1dd13/Downloads/1300424050_pimpmycar/PimpCarA_FULL/TuningA.cm

    bool testingDecompilation = true;

    if(testingDecompilation) {
        parseOpcodeFile("/Users/squ1dd13/CLionProjects/gtasm/Opcodes.ini");

        miss2::Script script = miss2::Decompiler::decompile("/Users/squ1dd13/CLionProjects/gtasm/GTA Scripts/debt.scm");
        script.prettyPrint();
    } else {

    }
    //printDisassembly("/Users/squ1dd13/Documents/GTA Scripts/trains.scm")
    //for(auto &cmd : script.commands) {
    //    std::vector<std::string> paramStrs;
    //    for(auto &p : cmd.parameters) {
    //        paramStrs.push_back(miss2::valueToString(p));
    //    }
    //
    //    if(cmd.name.empty()) {
    //        cmd.name = "0x" + to_string_hex(cmd.opcode);
    //    }
    //
    //    std::cout << replaceTokens(cmd.name, paramStrs) << '\n';
    //}

    //GXT gxt = GXT::read("/Users/squ1dd13/Library"
    //                    "/Application Support/CrossOver"
    //                    "/Bottles/Grand Theft Auto San Andreas"
    //                    "/drive_c/Program Files/Steam"
    //                    "/steamapps/common/GTA San Andreas"
    //                    "/text/american.gxt");

    return 0;
}
