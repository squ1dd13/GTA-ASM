/*
 * Structures and functions related to the (de)serialisation of command objects.
 */

#ifndef GTASM_SERIALIZATION_HPP
#define GTASM_SERIALIZATION_HPP

namespace miss2 {
    static std::string typeEncodings[] {
        "-",
        "int32",
        "intfloatg",
        "intfloatl",
        "byte",
        "int16",
        "float",
        "intfloatarrg",
        "intfloatarrl",
        "char8",
        "char8g",
        "char8l",
        "char8arrg",
        "char8arrl",
        "char?",
        "char16",
        "char16g",
        "char16l",
        "char16arrg",
        "char16arrl",
        "wtf"
    };

    struct CommandInfo {
        std::string mainName = "mainName";

        struct ParamInfo {
            DataType type;
            std::string name;
        };

        std::vector<ParamInfo> parameterInfo;

        std::string serialize() {
            std::stringstream stream;
            stream << '[' << mainName << ']';

            for(auto &pinfo : parameterInfo) {
                stream << ",[" << typeEncodings[pinfo.type] << ":" << pinfo.name << "]";
            }

            return stream.str();
        }

        CommandInfo(Command &cmd) {
            mainName = cmd.name;

            if(cmd.name.find('(') == std::string::npos and cmd.name.find('%') != std::string::npos) {
                // SASCM-style notation (old). Try to get something useful.

            }
        }
    };
}

#endif //GTASM_SERIALIZATION_HPP
