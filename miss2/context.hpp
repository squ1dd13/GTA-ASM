/*
 * This file contains structures that help the decompiler produce more readable and sensible output.
 * These are not strictly required for decompilation, but allow the output to look more like the
 * original code.
 */

#ifndef GTASM_CONTEXT_HPP
#define GTASM_CONTEXT_HPP

#include <vector>
#include "constructs.hpp"
#include "opcodes.hpp"
#include "../game/gtasa.hpp"
//#include "script.hpp"

namespace miss2 {
    // Actual if statement (not if...jump_if_false).
    struct FullIf {
        enum FlowType {
            FlowIf,
            FlowWhile
        } flowType = FlowIf;

        enum CombinationType {
            Invalid,
            None,
            And,
            Or
        } combination = None;
        uint8_t conditionCount;

        int32_t jifOffset;

        static std::pair<uint8_t, CombinationType> ifInfo(const Command &cmd) {
            auto numType = cmd.parameters[0].cast<uint8_t>();

            uint8_t numConditions {};
            CombinationType combinationType = None;

            if(numType == 0) {
                // Type 0 = 1 condition
                numConditions = 1;
            } else if(numType < 8) {
                // Types 1->7 = 2->8 conditions and combination AND.
                combinationType = And;
                numConditions = numType + 1;
            } else if(numType < 28) {
                // Types 21->27 = 2->8 conditions and combination OR.
                combinationType = Or;
                numConditions = numType - 19;
            } else {
                combinationType = Invalid;
            }

            //if(numConditions > 1) {
            //    std::cout << (int)numConditions << " conditions\n";
            //    exit(0);
            //}

            return { numConditions, combinationType };
        }

        // "x == y", key pressed, etc.
        //std::vector<Command> conditionCommands;
        size_t conditionStartOffset, conditionEndOffset;

        // Commands after the "if" command but before the "jump_if_false".
        //std::vector<Command> containedCommands;
        size_t bodyStartOffset, bodyEndOffset;
    };

    struct Goto {
        int32_t source, dest;
        uint16_t jumpOpcode;

        bool operator<(const Goto &rhs) const {
            return std::tie(source, dest, jumpOpcode) < std::tie(rhs.source, rhs.dest, rhs.jumpOpcode);
        }

        bool operator>(const Goto &rhs) const {
            return rhs < *this;
        }

        bool operator<=(const Goto &rhs) const {
            return !(rhs < *this);
        }

        bool operator>=(const Goto &rhs) const {
            return !(*this < rhs);
        }

        static bool isJumpOpcode(uint16_t opcode) {
            return opcode == Opcode::Jump or opcode == Opcode::JumpIfFalse or opcode == Opcode::Call;
        }

        Goto(Command jumpCommand) {
            if(not isJumpOpcode(jumpCommand.opcode)) {
                std::cerr << "error: cannnot create Goto from non-jump instruction\n";
                return;
            }

            source = std::abs(jumpCommand.offset);
            dest = std::abs(jumpCommand.parameters[0].cast<int32_t>());
            jumpOpcode = jumpCommand.opcode;
        }
    };

    struct Procedure {
        // Offset for calls.
        size_t beginOffset;

        // Final return.
        size_t endOffset;

        std::string name;
    };

    struct Label {
        int32_t offset;

        std::string name;
    };

    struct GlobalVar {
        DataType referenceType;

        // The type of the first value assigned to the variable.
        DataType valueType;

        uint16_t offset;
    };

    struct OffsetRange {
        size_t start, end;

        OffsetRange() = default;

        template <typename A, typename B>
        OffsetRange(A a, B b) {
            start = size_t(a);
            end = size_t(b);
        }

        template <typename A>
        OffsetRange(A a) {
            start = size_t(a);
            end = size_t(a);
        }

        //std::vector<Command> get(Script &script) {
        //    auto startIndex = script.offsetsToIndices[start];
        //    auto endIndex = script.offsetsToIndices[end];
        //
        //    std::vector<Command> commands;
        //    commands.reserve((endIndex - startIndex) + 1);
        //
        //    for(auto index = startIndex; index < script.commands.size() and index <= endIndex; ++index) {
        //        commands.push_back(script.commands[index]);
        //    }
        //
        //    return commands;
        //}
    };

    // /* 12383 */ if false
    // /* 12387 */ (LIntFloat)687 > (LIntFloat)745
    // /* 12395 */ jump_if_false((Int32)-12516)
    // /* 12402 */ get_shopping_item_with_textureCRC lIntArr_940[(LIntFloat)745] nametag_to lChar8Arr_9800[(LIntFloat)745]
    // /* 12418 */ get_shopping_item_with_textureCRC lIntArr_940[(LIntFloat)745] price_to lIntArr_540[(LIntFloat)745]
    // /* 12434 */ lIntArr_340[(LIntFloat)745] = lIntArr_940[(LIntFloat)745]
    // /* 12450 */ if false
    // /* 12454 */ (LIntFloat)790 == lIntArr_940[(LIntFloat)745]
    // /* 12466 */ jump_if_false((Int32)-12491)
    // /* 12473 */ lIntArr_320[(LIntFloat)745] = 0
    // /* 12484 */ jump((Int32)-12502)
    // /* 12491 */ lIntArr_320[(LIntFloat)745] = 1
    // /* 12502 */ (LIntFloat)745 += 1
    // /*       */ // Backwards jump
    // /* 12509 */ jump((Int32)-12383)

    // For loop structure:
    //  - FullIf with a JiF destination past the end of the loop (the end of the loop is unknown at this point)
    //  - ... Loop body code ...
    //  - Changes to variables in the condition (normally +=/-=, so ignore other changes)
    //  - A jump to the initial "if" command

    struct ForLoop {
        bool counterIsLocal {};
        Value counterValue;

        OffsetRange setupRange, checkRange, incRange, jumpRange;
    };

    // Compilation
    static bool optimize_jumps = false;

    // Decompilation
    static int indent_size = 4;
    static bool optimize_decompile = false; // Perform *some* optimisation to decompiled code (may decrease readability).
    static bool clean_decompile = false; // Remove dead code.
    static bool show_if_jumps = false; // Show jump_if_false calls for decompiled if statements.
    static int error_limit = 10; // Number of consecutive errors required for decompilation to stop.
}

#endif //GTASM_CONTEXT_HPP
