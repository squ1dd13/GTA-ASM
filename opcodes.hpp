//
// Created by Alex Gallon on 15/07/2020.
//

#ifndef GTASM_OPCODES_HPP
#define GTASM_OPCODES_HPP

// GTA opcodes that are useful for the decompiler to know.
enum Opcode : uint16_t {
    JumpIfFalse = 0x4D,
    EndThread = 0x4E,
    Call = 0x50,
    Return = 0x51,
    If = 0xD6
};

struct IfCommand {
    uint8_t numConditions {};

    enum CombinationType {
        Invalid,
        None,
        And,
        Or
    } combinationType = None;

    IfCommand(uint8_t *&scriptPtr) {
        uint8_t numType = *(scriptPtr++);

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
            numConditions = numType - 29;
        } else {
            combinationType = Invalid;
        }
    }

    operator bool() const {
        return combinationType != Invalid;
    }
};

#endif //GTASM_OPCODES_HPP
