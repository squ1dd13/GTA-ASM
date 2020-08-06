//
// Created by Alex Gallon on 19/07/2020.
//

#ifndef GTASM_SCRIPT_HPP
#define GTASM_SCRIPT_HPP

#include <set>
#include <iostream>
#include <sstream>
#include "context.hpp"
#include "../util.hpp"



namespace miss2 {
    //struct Goto;
    //struct FullIf;
    //struct Procedure;
    //struct Label;
    //struct GlobalVar;

    // Contains information about the script - commands, control flow, etc.
    struct Script {
        // The ordered commands of the script (as decompiled).
        std::vector<Command> commands;

        // Maps script offsets to indices in the commands vector.
        std::map<int32_t, size_t> offsetsToIndices;

        // Jumps with the source as the key.
        std::map<int32_t, std::set<Goto>> jumpSources;

        // Jumps with the destination as the key.
        std::map<int32_t, std::set<Goto>> jumpDestinations;

        // If statements with the offset of the if command as the key.
        std::map<int32_t, FullIf> ifStatements;

        // Procedures with the offset of the start as the key.
        std::map<int32_t, Procedure> allProcedures;

        // Labels with the offset as the key.
        std::map<int32_t, Label> labelLocations;

        // Globals with the offset as the key.
        std::map<uint16_t, GlobalVar> globals;

        // For loops with the offset as the key.
        std::map<int32_t, ForLoop> forLoops;

        std::set<int16_t> knownLocals;

        void addJump(Goto jump) {
            jumpSources[jump.source].insert(jump);
            jumpDestinations[jump.dest].insert(jump);
        }

        std::set<Goto> &jumpsFrom(int32_t src) {
            return jumpSources[src];
        }

        std::set<Goto> &jumpsTo(int32_t dst) {
            return jumpDestinations[dst];
        }

        Command optimizeJump(const Goto &jump, const Command &jumpCommand) {
            if(not offsetsToIndices.count(jump.source)) {
                return jumpCommand;
            }

            if(not offsetsToIndices.count(jump.dest)) {
                return jumpCommand;
            }

            Command &firstCommand = commands[offsetsToIndices[jump.source]];
            Command &secondCommand = commands[offsetsToIndices[jump.dest]];

            if(not Goto::isJumpOpcode(firstCommand.opcode) or not Goto::isJumpOpcode(secondCommand.opcode)) {
                // Can't do anything if either command is not a jump.
                // This should happen when jumps have been optimised as much as possible, and we have
                //  reached the stage where the jumped-to instruction is not itself a jump.
                return jumpCommand;
            }

            // We can only bypass secondCommand if it is an unconditional jump (without advanced control flow analysis).
            if(secondCommand.opcode == Opcode::Call || secondCommand.opcode == Opcode::Jump) {
                Command optimizedJumpCommand = firstCommand;
                optimizedJumpCommand.parameters[0] = secondCommand.parameters[0];

                // Optimize further if possible.
                // This recursive optimisation is slow, but improves output massively.
                return optimizeJump(Goto(optimizedJumpCommand), optimizedJumpCommand);
            }

            // secondCommand is conditional, so no optimization here.
            return jumpCommand;
        }

        // Clear and reload jumpSources and jumpDestinations. Call this after modifying jumps.
        void regenJumpInfo() {
            jumpSources.clear();
            jumpDestinations.clear();

            for(Command &cmd : commands) {
                if(Goto::isJumpOpcode(cmd.opcode)) {
                    addJump(Goto(cmd));
                }
            }
        }

        // Only use on compilation: there is no need to optimise decompiled code,
        //  and doing so only makes it harder to read.
        void optimizeScript() {
            if(optimize_jumps) {
                for(auto &jumpSet : jumpDestinations) {
                    for(auto &jump : jumpSet.second) {
                        Command &originalJump = commands[offsetsToIndices[jump.source]];
                        originalJump = optimizeJump(jump, originalJump);
                    }
                }

                // We've modified control flow (technically, though the script's function is the same),
                //  so we need to reload the jump information.
                regenJumpInfo();
            }
        }

        std::pair<int32_t, int32_t> getJumpBounds(FullIf &theIf) {
            int32_t minBackJump = INT32_MAX;
            int32_t maxForwardJump = INT32_MIN;

            for(auto &ifPair : ifStatements) {
                if(ifPair.second.bodyStartOffset <= theIf.conditionStartOffset and theIf.conditionStartOffset < ifPair.second.bodyEndOffset) {
                    minBackJump = std::min(minBackJump, (int32_t)ifPair.second.bodyStartOffset);
                    maxForwardJump = std::max(maxForwardJump, (int32_t)ifPair.second.bodyEndOffset);
                }
            }

            return {minBackJump, maxForwardJump};
        }

        FullIf ifStatementFromIndex(size_t i) {
            FullIf defaultReturn = {
                    .combination = FullIf::Invalid
            };

            if(commands[i].opcode == Opcode::If and not ifStatements.count(commands[i].offset)) {
                FullIf fullIf;
                auto info = FullIf::ifInfo(commands[i]);
                fullIf.conditionCount = info.first;
                fullIf.combination = info.second;

                auto ifOffset = commands[i].offset;

                size_t maxConditionIndex = i + info.first;

                fullIf.conditionStartOffset = commands[i].offset;

                bool cancel = false;
                for(++i; i < maxConditionIndex; ++i) {
                    if(commands[i].name.empty()) {
                        // Unknown opcode, so probably a read error.
                        // Read errors throw off the counting, so we need to cancel.
                        cancel = true;
                        break;
                    }
                }

                if(cancel) {
                    return defaultReturn;
                }

                fullIf.conditionEndOffset = commands[i].offset;

                if(commands[++i].opcode != Opcode::JumpIfFalse) {
                    //size_t iBackup = i;
                    //size_t maxI = std::min(i + 5, commands.size());
                    //while(i <= maxI and commands[i].opcode != Opcode::JumpIfFalse) ++i;
                    return defaultReturn;
                    //if(commands[i].opcode != Opcode::JumpIfFalse) {
                    //    i = iBackup;
                    //    continue;
                    //}
                }

                // JiF *command* location
                fullIf.jifOffset = commands[i].offset;

                // Get the JiF location so we know where to read to.
                auto jifOffset = std::abs(commands[i++].parameters[0].cast<int32_t>());

                fullIf.bodyStartOffset = commands[i].offset;

                // Read until we reach the point that the JiF jumps to. This is the body of the if statement.
                // If statements are compiled such that the JiF jumps past the body.
                while(i < commands.size() and commands[i].offset != jifOffset) {
                    fullIf.bodyEndOffset = commands[i++].offset;
                }

                if(fullIf.bodyStartOffset == fullIf.bodyEndOffset
                   or fullIf.conditionStartOffset == fullIf.conditionEndOffset) {
                    // Empty body or condition.
                    //std::cout << fullIf.bodyStartOffset << " to " << fullIf.bodyEndOffset << "\n";
                    //continue;
                }

                return fullIf;
            }

            return defaultReturn;
        }

        void createIfStatements() {
            static std::set<size_t> ifCommandIndices;

            if(ifCommandIndices.empty()) {
                std::cout << "discovering if commands...\n";

                // Find all the if commands.
                for(size_t i = 0; i < commands.size(); ++i) {
                    if(commands[i].opcode == Opcode::If) {
                        ifCommandIndices.insert(i);
                    }
                }

                std::cout << "cache built\n";
            }

            for(size_t i : ifCommandIndices) {
                auto statement = ifStatementFromIndex(i);
                if(statement.combination == FullIf::Invalid) continue;

                ifStatements[statement.conditionStartOffset] = statement;
            }
        }

        inline Command &commandAtOffset(size_t offset) {
            return commands[offsetsToIndices[offset]];
        }

        inline Command &commandBefore(Command &cmd) {
            return commands[offsetsToIndices[cmd.offset] - 1];
        }

        inline int32_t offsetBefore(int32_t offset) {
            return commandBefore(commandAtOffset(offset)).offset;
        }

        // For loop structure:
        //  - FullIf with a JiF destination past the end of the loop (the end of the loop is unknown at this point)
        //  - ... Loop body code ...
        //  - Changes to variables in the condition (normally +=/-=, so ignore other changes)
        //  - A jump to the initial "if" command

        void createForLoops(std::set<int32_t> &hiddenOffsets) {
            for(auto &ifPair : ifStatements) {
                FullIf &statement = ifPair.second;

                Goto falseJump = Goto(commandAtOffset(statement.jifOffset));
                auto jifTargetIndex = offsetsToIndices[falseJump.dest] - 1;

                Command &loopJump = commands[jifTargetIndex];

                bool notJump = not Goto::isJumpOpcode(loopJump.opcode);
                if(notJump or Goto(loopJump).dest != ifPair.first) {
                    continue;
                }

                ForLoop loop;
                loop.jumpRange = { commands[jifTargetIndex].offset };

                Command &incDecCommand = commandBefore(loopJump);
                if(incDecCommand.name.find_first_of("+-") == std::string::npos) {
                    continue;
                }

                loop.incRange = { incDecCommand.offset };
                loop.checkRange = { statement.conditionStartOffset, offsetBefore(statement.conditionEndOffset) };
                loop.counterValue = incDecCommand.parameters[0];

                Command backCommand = commandBefore(commandAtOffset(statement.conditionStartOffset));
                while(backCommand.name != "$0 = $1" and backCommand.parameters.size() != 2 and std::find_if(backCommand.parameters.begin(), backCommand.parameters.end(), [&](const Value &p){
                    return p == loop.counterValue;//p.type == loop.counterValue.type and p.cast<uint16_t>() == loop.counterValue.cast<uint16_t>();
                }) == backCommand.parameters.end()) {
                    if(backCommand.offset == 0) break;
                    backCommand = commandBefore(backCommand);
                }

                backCommand = commandBefore(backCommand);

                loop.setupRange = {backCommand.offset};

                // 14532

                forLoops[ifPair.first] = loop;
            }
        }



        std::string forString(ForLoop &loop) {
            static std::string fmt = "for($0; $1; $2)";

            std::string setupStr = commandToString(commandAtOffset(loop.setupRange.start), paramStringsForCommand(commandAtOffset(loop.setupRange.start)));

            std::string conditionStr = ifStatementString(ifStatements[loop.checkRange.start]);
            replaceAll(conditionStr, "while", "");
            replaceAll(conditionStr, "if_", "");
            replaceAll(conditionStr, "if", "");

            std::string incDecStr = commandToString(commandAtOffset(loop.incRange.start), paramStringsForCommand(commandAtOffset(loop.incRange.start)));

            return replaceTokens(fmt, {setupStr, conditionStr, incDecStr});
        }

        void createProcedures() {
            for(Command &cmd : commands) {
                if(cmd.opcode == Opcode::Call) {
                    // Found a call, so there must be a procedure here.
                    Goto call(cmd);

                    int32_t procOffset = call.dest;
                    if(allProcedures.count(procOffset)) {
                        // Procedure already exists.
                        continue;
                    }

                    Procedure procedure;
                    procedure.beginOffset = procOffset;
                    procedure.name = replaceTokens("proc_$0", {std::to_string(procOffset)});

                    size_t procedureStartIndex = offsetsToIndices[procOffset];

                    // The final return's level should match the start level.
                    // It is possible for there to be two reachable returns on the same level,
                    //  but we'll ignore that for now.
                    int startLevel = ifLevelForOffset(procOffset);

                    for(size_t i = procedureStartIndex; i < commands.size(); ++i) {
                        procedure.endOffset = commands[i].offset;

                        auto effectiveOpcode = commands[offsetsToIndices[commands[i].effectiveOffset()]].opcode;
                        if(effectiveOpcode == Opcode::Return) {
                            int level = ifLevelForOffset(commands[i].offset);

                            if(level == startLevel) break;
                        }
                    }

                    allProcedures[procOffset] = procedure;
                }
            }
        }

        void createWhileLoops(std::set<int32_t> &hiddenOffsets) {
            for(Command &cmd : commands) {
                if(Goto::isJumpOpcode(cmd.opcode)) {
                    Goto jump(cmd);
                    if(jump.dest < jump.source and commands[offsetsToIndices[jump.dest]].opcode == Opcode::If) {
                        // This is a while loop (effectively, even if it wasn't originally written as one).
                        FullIf &theIf = ifStatements[jump.dest];
                        theIf.flowType = FullIf::FlowWhile;

                        hiddenOffsets.insert(jump.source);
                    }
                }
            }
        }

        void createLabels(std::set<int32_t> &hiddenOffsets) {
            for(Command &cmd : commands) {
                if(hiddenOffsets.count(cmd.offset)) continue;
                if(Goto::isJumpOpcode(cmd.opcode) and cmd.opcode != Opcode::Call and not ifStatements.count(cmd.offset)) {
                    Goto jump(cmd);

                    Label label {
                            jump.dest,
                            replaceTokens("label_$0", {std::to_string(jump.dest)})
                    };

                    labelLocations[jump.dest] = label;
                }
            }
        }

        void createGlobals() {
            for(auto &cmd : commands) {
                for(int i = 0; i < cmd.parameters.size(); ++i) {
                    auto &obj = cmd.parameters[i];

                    auto typeName = dataTypeName(obj.type);

                    if(typeName.starts_with('G') and not typeName.ends_with("Arr")) {
                        uint16_t globalOffset = obj.cast<uint16_t>();

                        GlobalVar &var = globals[globalOffset];
                        var.referenceType = obj.type;
                        var.offset = globalOffset;

                        // If this is an assignment, we need to add the assigned type to the global var.
                        if(cmd.parameters.size() == 2 and std::count(cmd.name.begin(), cmd.name.end(), '=') == 1) {
                            // Probably an assignment.
                            auto &otherObj = cmd.parameters[!i];

                            var.valueType = otherObj.type;

                            auto otherTypeName = dataTypeName(obj.type);
                            if(otherTypeName.starts_with('G') and not otherTypeName.ends_with("Arr")) {
                                if(globals.count(otherObj.cast<uint16_t>())) {
                                    var.valueType = globals[otherObj.cast<uint16_t>()].valueType;
                                } else {
                                    //globals.erase(globalOffset);
                                    break;
                                }
                            }

                            if(var.valueType == EOAL) globals.erase(globalOffset);
                        }
                    }
                }
            }
        }

        int countLabelReferences(Label &lbl) {
            int refs = 0;

            for(auto &pair : jumpDestinations) {
                if(pair.first == lbl.offset) {
                    ++refs;
                }
            }

            return refs;
        }

        int32_t nextJumpedTo(int32_t startOffset) {
            size_t cmdIndex = offsetsToIndices[startOffset];

            for(size_t i = cmdIndex; i < commands.size(); ++i) {
                if(jumpDestinations.count(commands[i].offset)) {
                    return commands[i].offset;
                }
            }

            return -1;
        }

        void removeDeadCode(std::set<int32_t> &hiddenOffsets) {
            // Code that comes after a non-conditional jump (not a call) that is never jumped to will never execute.
            for(auto &jumpPair : jumpSources) {
                for(Goto jump : jumpPair.second) {
                    if(jump.jumpOpcode == Opcode::Jump) {
                        // Find the next jumped-to offset.
                        size_t cmdIndex = offsetsToIndices[jump.source] + 1;

                        bool needsBreak = false;
                        for(size_t i = cmdIndex; i < commands.size(); ++i) {
                            if(jumpDestinations.count(commands[i].offset)) {
                                needsBreak = true;
                                break;
                            }

                            //commands[cmdIndex].name += "_maybe_dead";
                            hiddenOffsets.insert(commands[cmdIndex].offset);
                        }

                        if(needsBreak) break;
                    }
                }
            }
        }

        int ifLevelForOffset(int32_t offset) {
            int lvl = 0;
            for(auto &ifPair : ifStatements) {
                if(ifPair.second.bodyStartOffset <= offset and offset <= ifPair.second.bodyEndOffset) {
                    ++lvl;
                }
            }

            return lvl;
        }

        // ifLevelForOffset + procedure level
        int fullIndentLevelForOffset(int32_t offset) {
            //if(allProcedures.count(offset)) {
            //    return 1;
            //} else {
            //    for(auto &procPair : allProcedures) {
            //        if(procPair.second.beginOffset <= offset and offset <= procPair.second.endOffset) {
            //            return ifLevelForOffset(offset) - ifLevelForOffset(procPair.second.beginOffset);
            //        }
            //    }
            //}

            int lvl = 0;

            bool insideProc = false;
            Procedure proc;
            for(auto &procPair : allProcedures) {
                if(procPair.second.beginOffset <= offset and offset <= procPair.second.endOffset) {
                    proc = procPair.second;
                    insideProc = true;
                    ++lvl;
                }
            }

            for(auto &ifPair : ifStatements) {
                if(ifPair.second.bodyStartOffset <= offset and offset <= ifPair.second.bodyEndOffset) {
                    if(insideProc) {
                        if(proc.beginOffset <= ifPair.second.conditionStartOffset
                           and ifPair.second.bodyEndOffset <= proc.endOffset) {
                            ++lvl;
                        }
                    } else {
                        ++lvl;
                    }
                }
            }

            return lvl;
        }

        void printInfo(string_ref padStr, string_ref info) {
            std::cout << asComment(padStr + "// " + info) << codeColor << '\n';
        }

        inline std::string vehicleModelComment(int16_t id) {
            return asComment(replaceTokens("/* Car $0 = '$1' */ ", {
                    std::to_string(id),
                    vehicleNameForID(id)
            }));
        }

        std::string globStr(GlobalVar &global) {
            static std::string format = orange + "g$0_$1" + codeColor;

            return replaceTokens(format, {miss2::dataTypeName(global.valueType), std::to_string(global.offset)});
        }

        std::string globalToString(const Command &cmd, Value &p) {
            if(p.size == 2 and globals.count(p.cast<uint16_t>()) and globals[p.cast<uint16_t>()].valueType) {
                return globStr(globals[p.cast<uint16_t>()]);
            }

            return "";
        }

        std::string valueParamToString(const Command &cmd, Value p) {
            std::string globalStr = globalToString(cmd, p);
            if(not globalStr.empty()) return globalStr;

            bool printType = true;
            uint32_t sum = p.sumBytes();
            if(sum < 2) {
                // Don't print the type for 0 or 1.
                printType = false;
            }

            //if(p.type == F32) printType = false;

            std::string valueStr = valueToString(p);

            if(cmd.parameters.size() == 1 and p.type == S8 and cmd.opcode != Opcode::Wait) {
                // 1 single-byte param may mean it's a Boolean. wait() is explicitly excluded because it's common.
                if(sum < 2) {
                    static std::string boolStrs[] = { "false", "true" };
                    valueStr = pink + boolStrs[sum] + codeColor;
                }
            }

            auto typeName = miss2::dataTypeName(p.type);

            if(typeName.starts_with('L')) {
                printType = false;
                valueStr = varColor + "local" + typeName.substr(1) + "_" + std::to_string(p.cast<int16_t>());

                if(p == cmd.parameters.front() and not knownLocals.count(p.cast<int16_t>()) and opcodeIsAssignment(cmd.opcode)) {
                    valueStr = blue + typeName + codeColor + ' ' + valueStr;
                }

                knownLocals.insert(p.cast<int16_t>());
            }

            std::string highlightedParam = (printType ? ("("
                                                         + blue
                                                         + typeName
                                                         + codeColor
                                                         + ")") : (""))
                                           + green
                                           + valueStr
                                           + codeColor;

            return highlightedParam;
        }

        std::vector<std::string> paramStringsForCommand(Command &cmd) {
            if(cmd.opcode == Opcode::Call) {
                int32_t offset = std::abs(cmd.parameters.front().cast<int32_t>());
                if(allProcedures.count(offset)) {
                    return {callColor + allProcedures[offset].name + "()" + codeColor};
                }
            }

            std::vector<std::string> paramStrs;
            for(auto &p : cmd.parameters) {
                if(isArrayType(p.type)) {
                    ArrayObject arr = p.cast<ArrayObject>();
                    static std::string format = orange + "l$0Arr_$1" + codeColor + "[$2]" + codeColor;

                    std::string indexString = arr.properties.isIndexGlobalVar
                                              ? globStr(globals[arr.arrayIndex])
                                              : valueParamToString(cmd, Value(LocalIntFloat, (uint8_t *)&arr.arrayIndex, sizeof(arr.arrayIndex)));
                    std::string s = replaceTokens(format, {arr.properties.elementTypeStr(), std::to_string(arr.offset), indexString});


                    //s += "[" + green + std::to_string(arr.arrayIndex) + codeColor + "]";

                    if(arr.properties.isIndexGlobalVar) {
                        s += "_index_is_global";
                    }

                    paramStrs.push_back(s);
                    continue;
                }

                paramStrs.push_back(valueParamToString(cmd, p));
            }

            return paramStrs;
        }

        std::string ifStatementString(FullIf &statement) {
            std::stringstream stream;

            stream << pink << (statement.flowType == FullIf::FlowIf ? "if" : "while");

            switch (statement.combination) {
                case FullIf::Invalid:
                    break;
                case FullIf::None:
                    break;
                case FullIf::And:
                    stream << "_all";
                    break;
                case FullIf::Or:
                    stream << "_one_of";
                    break;
            }

            stream << codeColor;

            stream << "(";
            size_t conditionStartIndex = offsetsToIndices[statement.conditionStartOffset] + 1;
            size_t conditionEndIndex = offsetsToIndices[statement.conditionEndOffset];

            for(size_t i = conditionStartIndex; i <= conditionEndIndex; ++i) {
                auto cmd = commands[i];

                if(cmd.name.empty()) {
                    cmd.name = "unknown condition";
                }

                if(cmd.opcode == Opcode::DrivingCarWithModel) {
                    auto id = cmd.parameters[1].cast<int16_t>();
                    stream << vehicleModelComment(id);
                } else if(cmd.opcode == Opcode::RandomCarWithModel) {
                    auto id = cmd.parameters[0].cast<int16_t>();
                    stream << vehicleModelComment(id);
                }


                stream << codeColor << replaceTokens(cmd.name, paramStringsForCommand(cmd));

                if(i != conditionEndIndex) {
                    stream << ", ";
                }
            }

            stream << ")";

            return stream.str();
        }

        std::string commandToString(Command &cmd, std::vector<std::string> paramStrs) {
            if(cmd.opcode == Opcode::Call) {
                return paramStrs[0];
            }

            return replaceTokens(cmd.name, paramStrs);;
        }

        void prettyPrint() {
            // Offsets of commands that shouldn't be printed.
            std::set<int32_t> hiddenOffsets;

            // !!
            if(optimize_decompile) {
                std::cout << "optimising...\n";
                optimizeScript();
            }

            std::cout << "creating conditionals...\n";
            auto lastIfSize = ifStatements.size();

            // Keep creating if statements until no more can be generated.
            int ifPass = 1;
            while(true) {
                std::cout << "pass " << ifPass++ << '\n';
                createIfStatements();
                auto sizeNow = ifStatements.size();

                // If the number of ifs doesn't change, we've finished.
                if(sizeNow == lastIfSize) break;
                lastIfSize = sizeNow;
            }

            std::cout << "creating for-loops...\n";
            createForLoops(hiddenOffsets);

            std::cout << "creating procedures...\n";
            createProcedures();

            std::cout << "creating while-loops...\n";
            createWhileLoops(hiddenOffsets);


            if(clean_decompile) {
                std::cout << "removing dead code...\n";
                removeDeadCode(hiddenOffsets);
            }

            std::cout << "creating labels...\n";
            createLabels(hiddenOffsets);

            std::cout << "creating globals...\n";
            createGlobals();

            std::cout << labelLocations.size() << " labels\n";
            std::cout << globals.size() << " globals\n";

            //for(auto &ifPair : ifStatements) {
            // Hide 'if' jumps.
            //hiddenOffsets.insert(ifPair.second.jifOffset);
            //}

            std::string topCommentFormat = "/*\n  Decompiled by miss3 on $0.\n*/\n";
            std::string dateTime = currentDateString();

            std::cout << asComment(replaceTokens(topCommentFormat, {dateTime})) << '\n';

            int consecErrors = 0;

            bool lastWasIf = false;
            int lastIfLevel = 0;
            for(size_t commandIndex = 0; commandIndex < commands.size(); ++commandIndex) {
                auto &cmd = commands[commandIndex];

                if(hiddenOffsets.count(cmd.offset)) {
                    continue;
                }

                int ifLevel = fullIndentLevelForOffset(cmd.offset);

                std::string lineOffsetFormat = "/* $0 */ ";//labelLocations.count(cmd.offset) ? ("/* " + blueGreen + "$0 " + gray + "*/ ") : "/* $0 */ ";

                std::string linePadStr = replaceTokens("/* $0 */ ", {std::string(countDigits(cmd.offset), ' ')});
                linePadStr = gray + linePadStr + std::string(ifLevel * indent_size, ' ');

                if(ifLevel != lastIfLevel) {
                    //std::cout << linePadStr << '\n';
                    //std::cout << linePadStr << "{\n";
                }

                std::string lineOffsetStr = replaceTokens(lineOffsetFormat, {std::to_string(cmd.offset)});
                lineOffsetStr = gray + lineOffsetStr + std::string(ifLevel * indent_size, ' ');

                if(labelLocations.count(cmd.offset)) {
                    std::cout << linePadStr << '\n';
                    std::cout << linePadStr << blueGreen << labelLocations[cmd.offset].name << ':' << codeColor << '\n';
                }

                if(allProcedures.count(cmd.offset)) {
                    lastWasIf = true;
                    std::string declPad = replaceTokens("/* $0 */ ", {std::string(countDigits(cmd.offset), ' ')});
                    declPad = gray + declPad + std::string(std::max(0, ifLevel - 1) * indent_size, ' ');

                    std::cout << declPad << pink << "proc " << codeColor << allProcedures[cmd.offset].name << codeColor << "()\n";
                }

                if(ifStatements.count(cmd.offset)) {
                    FullIf &statement = ifStatements[cmd.offset];

                    // Add a new line before an if statement only when the last thing we printed was not an if.
                    if(not lastWasIf) {
                        std::cout << linePadStr << '\n';
                    }

                    if(forLoops.count(cmd.offset)) {
                        printInfo(linePadStr, forString(forLoops[cmd.offset]));
                    }

                    std::cout << lineOffsetStr << ifStatementString(statement) << '\n';

                    //show_if_jumps = true;
                    commandIndex = offsetsToIndices[statement.bodyStartOffset] - (show_if_jumps ? 2 : 1);
                    lastWasIf = true;
                    continue;
                }

                lastWasIf = false;

                if(cmd.opcode == Opcode::DrivingCarWithModel) {
                    auto id = cmd.parameters[1].cast<int16_t>();
                    std::cout << linePadStr << vehicleModelComment(id) << '\n';
                } else if(cmd.opcode == Opcode::RandomCarWithModel) {
                    auto id = cmd.parameters[0].cast<int16_t>();
                    std::cout << linePadStr << vehicleModelComment(id) << '\n';
                }

                if(Goto::isJumpOpcode(cmd.opcode)) {
                    Goto jump(cmd);
                    if(jump.dest < jump.source) {//} and commands[offsetsToIndices[jump.dest]].opcode == Opcode::If) {
                        printInfo(linePadStr, "Backwards jump");
                    }

                    if(jump.jumpOpcode != Opcode::Call and labelLocations.count(jump.dest)) {
                        std::cout << lineOffsetStr << codeColor << replaceTokens(cmd.name, {blueGreen + labelLocations[jump.dest].name + codeColor}) << '\n';
                        continue;
                    }
                }

                //if(allProcedures.count(cmd.offset)) {
                //    std::cout << linePadStr << asComment("// " + allProcedures[cmd.offset].name) << codeColor << '\n';
                //}

                std::vector<std::string> paramStrs = paramStringsForCommand(cmd);

                if(cmd.name.empty()) {
                    cmd.name = asComment("/* Unknown: 0x" + to_string_hex(cmd.opcode) + " */");
                    if(++consecErrors >= error_limit) {
                        std::cerr << "Too many errors, stopping now.\n";
                        return;
                    }
                } else {
                    consecErrors = 0;
                }

                std::string commandString = commandToString(cmd, paramStrs);
                std::cout << lineOffsetStr << codeColor << commandString << ";\n";

                //if(lastIfLevel > ifLevel) {
                //    std::cout << linePadStr << "}\n";
                //}
                //size_t nextIndex = commandIndex + 1;
                //if(nextIndex == commands.size()) {
                //
                //}

                lastIfLevel = ifLevel;
                if(cmd.opcode == Opcode::Return) std::cout << linePadStr << '\n';
            }
        }
    };
}

#endif //GTASM_SCRIPT_HPP
