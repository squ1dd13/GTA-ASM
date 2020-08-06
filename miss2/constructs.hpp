/*
 * This file contains the basic structures that are essential for compiling and decompiling miss2 code,
 *  along with various functions and methods that are useful for working with these structures.
 */

#ifndef GTASM_CONSTRUCTS_HPP
#define GTASM_CONSTRUCTS_HPP

#include <vector>
#include <map>
#include "opcodes.hpp"
#include "../highlighting.hpp"

namespace miss2 {
    enum DataType : uint8_t {
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

        // Placeholder for until the decompiler knows the type.
        Unknown
    };

    inline bool isArrayType(DataType t) {
        return t == GlobalIntFloatArr
            or t == LocalIntFloatArr
            or t == GlobalString8Arr
            or t == LocalString8Arr
            or t == LocalString16Arr
            or t == GlobalString16Arr;
    }

    struct Value {
    private:
        //uint8_t *bytes = nullptr;
        std::vector<uint8_t> bytes;
        bool bytesSet = false;
    public:
        DataType type;

        // Optional (for the most part). Required for var-len strings.
        size_t size;

        Value() = default;
        Value(DataType _type) : type { _type }{};
        Value(DataType _type, uint8_t *_bytes, size_t _size) {
            type = _type;
            bytes = std::vector<uint8_t>(_bytes, _bytes + _size);
            size = _size;
        }

        template <typename T>
        T cast() const {
            return sizeof(T) <= bytes.size() ? *(T *)bytes.data() : T{};
        }

        void setBytes(uint8_t *bytesValue, size_t len) {
            bytesSet = true;
            bytes = std::vector<uint8_t>(bytesValue, bytesValue + len);
        }

        uint8_t *getBytes() {
            return bytes.data();
        }

        uint32_t sumBytes() {
            uint32_t sum {};
            for(uint8_t &b : bytes) sum += b;

            return sum;
        }

        bool operator==(const Value &rhs) const {
            return std::tie(bytes, bytesSet, type, size) == std::tie(rhs.bytes, rhs.bytesSet, rhs.type, rhs.size);
        }

        bool operator!=(const Value &rhs) const {
            return !(rhs == *this);
        }

        //virtual ~Value() {
            //if(bytesSet) delete[] bytes;
        //}
    };

    struct ArrayObject {
        // Signed for locals, unsigned for globals.
        uint16_t offset;

        int16_t arrayIndex;
        uint8_t arraySize;

        struct Properties {
            enum ElementType : uint8_t {
                Integer,
                Float,
                TextLabel,
                TextLabel16
            } elementType : 7;

            uint8_t isIndexGlobalVar : 1;

            std::string elementTypeStr() {
                static std::string strs[] = {"Int", "Float", "Char8", "Char16"};
                return strs[elementType];
            }
        } __attribute__((packed)) properties;
    } __attribute__((packed));

    std::string valueToString(Value &value) {
        switch(value.type) {
            case EOAL:
                return "end";
            case S32:
                return std::to_string(value.cast<int32_t>());
            case GlobalIntFloat:
                return std::to_string(value.cast<uint16_t>());
            case LocalIntFloat:
                return std::to_string(value.cast<uint16_t>());
            case S8:
                return std::to_string((int)*value.getBytes());
            case S16:
                return std::to_string(value.cast<int16_t>());
            case F32:
                return std::to_string(value.cast<float>());
            case GlobalIntFloatArr: {
                ArrayObject arr = value.cast<ArrayObject>();
                return "<global int/float array>[" + std::to_string(arr.arrayIndex) + "]";
            }
            case LocalIntFloatArr: {
                ArrayObject arr = value.cast<ArrayObject>();

                //static std::string nameFormat = "local" + arr.properties.elementTypeStr() + "Arr_" + std::to_string(arr.offset);
                return "[" + std::to_string(arr.arrayIndex) + "]";
            }
            case String8: {
                // Copy to another buffer to get rid of any junk in the string.
                char dest[8];
                std::strncpy(dest, (char *)value.getBytes(), 8);

                return "'" + std::string(dest) + "'";
            }
            case GlobalString8:
                return std::to_string(value.cast<uint16_t>());
            case LocalString8:
                return std::to_string(value.cast<uint16_t>());
            case GlobalString8Arr:
                return "<global string8 array>";
            case LocalString8Arr:
                return "<local string8 array>";
            case StringVar:
                //value.size = *value.getBytes();
                if(value.size == 0) {
                    //std::cerr << "error: valueToString() cannot be called for StringVar with size 0\n";
                    return "<error>";
                }

                return "'" + std::string(value.getBytes(), value.getBytes() + value.size) + "'";
            case String16:
                return "'" + std::string(value.getBytes(), value.getBytes() + 16) + "'";
            case GlobalString16:
                return std::to_string(value.cast<uint16_t>());
            case LocalString16:
                return std::to_string(value.cast<uint16_t>());
            case GlobalString16Arr:
                return "<global string16 array>";
            case LocalString16Arr:
                return "<local string16 array>";
            case Unknown:
                return "<unknown>";
        }

        // Hopefully we never get here...
        return "<unknown type>";
    }

    std::string primitiveVtoS(Value &value) {
        switch(value.type) {
            case EOAL:
                return "E";
            case S32:
                return "S" + std::to_string(value.cast<int32_t>());
            case GlobalIntFloat:
                return "G" + std::to_string(value.cast<uint16_t>());
            case LocalIntFloat:
                return "L" + std::to_string(value.cast<uint16_t>());
            case S8:
                return "B" + std::to_string((int)value.cast<int8_t>());
            case S16:
                return "T" + std::to_string(value.cast<int16_t>());
            case F32:
                return "F" + std::to_string(value.cast<float>());
            case GlobalIntFloatArr: {
                std::stringstream stream;

                for(int i = 0; i < value.size; ++i) {
                    stream << '.' << std::dec << int(value.getBytes()[i]);
                }

                return "A" + (stream.str().empty() ? "!" : stream.str().substr(1));
            }
            case LocalIntFloatArr: {
                std::stringstream stream;

                for(int i = 0; i < value.size; ++i) {
                    stream << '.' << std::dec << int(value.getBytes()[i]);
                }

                return "X" + (stream.str().empty() ? "!" : stream.str().substr(1));
            }
            case String8: {
                // Copy to another buffer to get rid of any junk in the string.
                char dest[8];
                std::strncpy(dest, (char *)value.getBytes(), 8);

                return "'" + std::string(dest) + "'";
            }

            case GlobalString8:
                return "M" + std::to_string(value.cast<uint16_t>());
            case LocalString8:
                return "N" + std::to_string(value.cast<uint16_t>());

            case GlobalString8Arr: {
                std::stringstream stream;

                for(int i = 0; i < value.size; ++i) {
                    stream << '.' << std::dec << int(value.getBytes()[i]);
                }

                return "V" + (stream.str().empty() ? "!" : stream.str().substr(1));
            }
            case LocalString8Arr: {
                std::stringstream stream;

                for(int i = 0; i < value.size; ++i) {
                    stream << '.' << std::dec << int(value.getBytes()[i]);
                }

                return "W" + (stream.str().empty() ? "!" : stream.str().substr(1));
            }

            case StringVar:
                //value.size = *value.getBytes();
                if(value.size == 0) {
                    //std::cerr << "error: valueToString() cannot be called for StringVar with size 0\n";
                    return "''";
                }

                return "'" + std::string(value.getBytes(), value.getBytes() + value.size) + "'";
            case String16:
                return "'" + std::string(value.getBytes(), value.getBytes() + 16) + "'";
            case GlobalString16:
                return "K" + std::to_string(value.cast<uint16_t>());
            case LocalString16:
                return "J" + std::to_string(value.cast<uint16_t>());
            case GlobalString16Arr: {
                std::stringstream stream;

                for(int i = 0; i < value.size; ++i) {
                    stream << '.' << std::dec << int(value.getBytes()[i]);
                }

                return "R" + (stream.str().empty() ? "!" : stream.str().substr(1));
            }
            case LocalString16Arr: {
                std::stringstream stream;

                for(int i = 0; i < value.size; ++i) {
                    stream << '.' << std::dec << int(value.getBytes()[i]);
                }

                return "Z" + (stream.str().empty() ? "!" : stream.str().substr(1));
            }
            case Unknown:
                return "U!";
        }

        // Hopefully we never get here...
        return "<unknown type>";
    }

    std::string dataTypeName(DataType type) {
        switch(type) {
            case EOAL:
                return "<null type>";
            case S32:
                return "Int32";
            case GlobalIntFloat:
                return "GIntFloat";
            case LocalIntFloat:
                return "LIntFloat";
            case S8:
                return "Int8";
            case S16:
                return "Int16";
            case F32:
                return "Float";
            case GlobalIntFloatArr:
                return "GIntFloatArr";
            case LocalIntFloatArr:
                return "LIntFloatArr";
            case String8:
                return "Char[8]";
            case GlobalString8:
                return "GChar8";
            case LocalString8:
                return "LChar8";
            case GlobalString8Arr:
                return "GChar8Arr";
            case LocalString8Arr:
                return "LChar8Arr";
            case StringVar:
                return "Char[]";
            case String16:
                return "Char[16]";
            case GlobalString16:
                return "GChar16";
            case LocalString16:
                return "LChar16";
            case GlobalString16Arr:
                return "GChar16Arr";
            case LocalString16Arr:
                return "LChar16Arr";
            case Unknown:
                return "<unknown type>";
        }

        // Hopefully we never get here...
        return "<unknown type>";
    }

    size_t getValueSize(Value &value) {
        switch(value.type) {
            case EOAL:
                return 0;
            case S32:
                return 4;
            case GlobalIntFloat:
                return 2;
            case LocalIntFloat:
                return 2;
            case S8:
                return 1;
            case S16:
                return 2;
            case F32:
                return 4;
            case GlobalIntFloatArr:
                return 6;
            case LocalIntFloatArr:
                return 6;
            case String8:
                return 8;
            case GlobalString8:
                return 2;
            case LocalString8:
                return 2;
            case GlobalString8Arr:
                return 6;
            case LocalString8Arr:
                return 6;
            case StringVar:
                return 0;//value.getBytes() ? *value.getBytes() : 0;
            case String16:
                return 16;
            case GlobalString16:
                return 2;
            case LocalString16:
                return 2;
            case GlobalString16Arr:
                return 6;
            case LocalString16Arr:
                return 6;
            case Unknown:
                return 0;
        }

        return 0;
    }

    struct Command {
    private:
        static std::map<uint16_t, Command> knownCommands;
        static Command nullCommand;

    public:
        std::string name;
        uint16_t opcode;
        int32_t offset = -1;
        std::vector<Value> parameters;
        size_t scriptIndex;

        static std::pair<uint16_t, Command> create(string_ref mn, uint16_t op, const std::vector<Value> &types = {}) {
            Command instr;
            instr.name = mn;
            instr.opcode = op;
            instr.parameters = types;

            return {
                op,
                instr
            };
        }

        operator bool() const {
            return not name.empty();
        }

        static Command &get(uint16_t op) {
            auto iter = knownCommands.find(op);

            if(iter != knownCommands.end()) {
                return iter->second;
            }

            return nullCommand;
        }

        /*
         * The offset of this command UNLESS the command is an unconditional jump,
         * in which case the jumped-to command's offset is returned.
         */
        int32_t effectiveOffset() {
            if(opcode == miss2::Opcode::Jump) return parameters[0].cast<int32_t>();

            return offset;
        }

        static Command read(uint8_t *&scriptPointer) {
            uint16_t opcode = *(uint16_t *)scriptPointer;
            scriptPointer += 2;

            Command foundCommand = get(opcode);
            if(not foundCommand) {
                foundCommand.opcode = opcode;

                // Try to read the arguments. Max 40 bytes so we don't get stuck.
                //uint8_t *ptrBackup = scriptPointer;
                //
                //bool broken = false;
                //for(int i = 0; i < 40; ++i) {
                //    if(get(*(uint16_t *)scriptPointer)) {
                //        broken = true;
                //        break;
                //    }
                //
                //    Value paramValue = Value((DataType)*scriptPointer);
                //
                //    auto expectedSize = getValueSize(paramValue);
                //    paramValue.setBytes(scriptPointer, expectedSize);
                //
                //    scriptPointer += expectedSize;
                //}
                //
                //if(not broken) {
                //    // Got nowhere, so restore the pointer.
                //    scriptPointer = ptrBackup;
                //}

                return foundCommand;
            }

            // Get a reference so we can update parameter types.
            Command &commandRef = get(opcode);

            int pIndex = 0;
            for(Value &param : foundCommand.parameters) {
                param.type = (DataType)*(scriptPointer++);
                commandRef.parameters[pIndex++].type = param.type;

                if(param.type != Unknown and param.type != EOAL) {
                    param.size = getValueSize(param);
                }

                if(param.type == StringVar) {
                    param.size = *(scriptPointer++);
                }

                if(param.size) {
                    param.setBytes(new uint8_t[param.size], param.size);
                    for(int i = 0; i < param.size; ++i) {
                        param.getBytes()[i] = *(scriptPointer++);
                    }
                }
            }

            return foundCommand;
        }

        static void registerOpcode(uint16_t opcode, const Command &cmd) {
            knownCommands[opcode] = cmd;
        }
    };

    Command Command::nullCommand {};
    std::map<uint16_t, Command> Command::knownCommands {};
}

#endif //GTASM_CONSTRUCTS_HPP
