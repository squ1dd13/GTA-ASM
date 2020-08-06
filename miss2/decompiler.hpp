/*
 * Decompiler for code compiled with miss2.exe (or not, so long as the code is .SCM style).
 */

#ifndef GTASM_DECOMPILER_HPP
#define GTASM_DECOMPILER_HPP

#include <vector>
#include <fstream>
#include "constructs.hpp"
#include "context.hpp"
#include "../util.hpp"
#include "script.hpp"
#include <cmath>

namespace miss2 {
    class Decompiler {
    public:
        static Script decompile(string_ref filename) {
            Script script;

            std::cout << "loading file... ";

            auto bytesVector = readFileBytes(filename.c_str());

            uint8_t *bytes = new uint8_t[bytesVector.size()];
            std::copy(bytesVector.begin(), bytesVector.end(), bytes);
            uint8_t *scriptPointer = bytes;

            std::cout << "done.\n";
            std::cout << "decompiling 0%... ";

            float lastProgress = 0.f;
            for(size_t scriptOffset = 0; scriptPointer < bytes + bytesVector.size(); ++scriptOffset) {
                size_t opcodeOffset = scriptPointer - bytes;

                float progress = ((float)size_t(opcodeOffset) / (float)size_t(bytesVector.size())) * 100.f;

                if(progress - lastProgress >= 10.f) {
                    std::cout << int(progress) << "%... ";
                    lastProgress = int(progress);
                    std::cout.flush();
                }

                // Read a miss2 command.
                miss2::Command command = miss2::Command::read(scriptPointer);
                if(command.opcode == 0) {
                    // NOP
                    continue;
                }

                command.offset = opcodeOffset;
                command.scriptIndex = script.commands.size();

                script.offsetsToIndices[command.offset] = command.scriptIndex;

                script.commands.push_back(command);

                // Register a jump if there is one.
                if(Goto::isJumpOpcode(command.opcode)) {
                    script.addJump(Goto(command));
                }
            }

            std::cout << "100%\n";

            delete[] bytes;
            return script;
        }
    };
}

#endif //GTASM_DECOMPILER_HPP
