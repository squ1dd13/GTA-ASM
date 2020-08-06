/*
 * Opcodes that may be useful in the decompilation process.
 */

#ifndef MISS2_OPCODES_HPP
#define MISS2_OPCODES_HPP

namespace miss2 {
    enum Opcode : uint16_t {
        Wait = 0x1,
        Jump = 0x2,
        JumpIfFalse = 0x4D,
        EndThread = 0x4E,
        Call = 0x50,
        Return = 0x51,
        If = 0xD6,
        DrivingCarWithModel = 0xDD, // Model ID is params[1]
        RandomCarWithModel = 0x327, // Model ID is params[0]
    };

    bool opcodeIsAssignment(uint16_t op) {
        return 0x4 <= op and op <= 0x7;
    }
}

#endif //MISS2_OPCODES_HPP
