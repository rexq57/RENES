#pragma once

#include <stdlib.h>
#include <map>
#include <set>
#include "mem.hpp"
#include <string>
#include <vector>
#include "type.hpp"


#include <sstream>
#include <iomanip>
#include <iostream>

namespace ReNes {
    
    // RAM内存结构的偏移地址
    #define PRG_ROM_LOWER_BANK_OFFSET 0x8000
    #define PRG_ROM_UPPER_BANK_OFFSET 0xC000
    
    #define STACK_ADDR_OFFSET 0x0100
    
    #define RENES_REGS auto& AC = regs.A;auto& XR = regs.X;auto& YR = regs.Y;auto& SP = regs.SP;auto& PC = regs.PC;auto& SR = *(uint8_t*)&regs.P;(void)AC;(void)XR;(void)YR;(void)SP;(void)PC;(void)SR;

    // 寻址模式
    // IMMIDIATE = 立即，INDEXED = 间接，跳转时 RELATIVE = 相对8bit，ABSOLUTE = 16bit
    enum AddressingMode {
        
        IMPLIED,        // 默认
        
        ACCUMULATOR,    // 累加器寻址
        RELATIVE,       // 相对寻址 跳转
        
        ZERO_PAGE,     // 零页寻址，就是间接8bit寻址
        ZERO_PAGE_X,   // zeropage,X
        ZERO_PAGE_Y,   // zeropage,Y
        
        INDEXED_ABSOLUTE, // 索引绝对寻址，8bit数据
        INDEXED_ABSOLUTE_X, // 索引绝对寻址，8bit数据
        INDEXED_ABSOLUTE_Y, // 索引绝对寻址，8bit数据
        
        IMMIDIATE,     // 立即数，8bit数据
        
        
        INDIRECT,   // (oper)
        INDIRECT_X_INDEXED, // (oper,X)
        INDIRECT_INDEXED_Y, // 从零页找到16bit再加Y (oper),Y
        
        

        DATA_VALUE,
    };
    
    enum InsParam {
        
        PARAM_NONE,
        
        PARAM_0,
        PARAM_1,
        
        PARAM_REGS_A,
        PARAM_REGS_X,
        PARAM_REGS_Y,
    };
    
    template< typename T >
    std::string int_to_hex( T i )
    {
        std::stringstream stream;
        stream << "0x"
        << std::setfill ('0') << std::setw(sizeof(T)*2)
        << std::hex << std::uppercase << (int)i;
        return stream.str();
    }
    
    enum CF{
        
        CF_NON,
        
        CF_BCC, CF_BCS,
        CF_BEQ,
        CF_BMI, CF_BNE, CF_BPL, CF_BVC, CF_BVS,
        CF_CLC, CF_CLD, CF_CLI, CF_CLV,
        CF_CMP, CF_CPX, CF_CPY,
        CF_DEC, CF_DEX, CF_DEY,
        CF_EOR,
        CF_LDA, CF_LDX, CF_LDY,
        
        CF_INX, CF_INY, CF_INC,
        
        CF_ORA,
        
        CF_SEC, CF_SED, CF_SEI,
        CF_STA, CF_STX, CF_STY,
        CF_TAX, CF_TAY, CF_TXA, CF_TSX, CF_TXS, CF_TYA,
        
        CF_BRK,
        
        CF_ST,
        CF_RTI,
        CF_JSR,
        CF_RTS,
        CF_AND,
        CF_JMP,
        
        CF_DE,
        CF_SBC,
        CF_ADC,
        
        CF_BIT,
        
        CF_NOP,
        
        CF_ASL, // 累加器寻址，算数左移
        CF_LSR,
        
        CF_PHA,
        CF_PHP,
        CF_PLA, CF_PLP,
        CF_ROL,
        CF_ROR,
    };
    
    
    enum DST{
        
        DST_NONE,
        
        DST_REGS_SP,
        
        DST_REGS_A,
        DST_REGS_X,
        DST_REGS_Y,
        
        DST_REGS_P_C,
        DST_REGS_P_Z,
        DST_REGS_P_I,
        DST_REGS_P_D,
        DST_REGS_P_B,
        DST_REGS_P__,
        DST_REGS_P_V,
        DST_REGS_P_N,
        
        DST_M,
    };
    
    const std::map<DST, std::string> REGS_NAME = {
        {DST_REGS_A, "A"},
        {DST_REGS_X, "X"},
        {DST_REGS_Y, "Y"},
    };
    
    // 执行指令
    struct CmdInfo {
        
        std::string name;
        CF cf;
        AddressingMode mode;
        
        int bytes;
        int cycles;
        
        int flags; // 寄存器影响标签，用于检查执行错误
    };
    
    /**
     根据目标打印指令日志
     
     @param info 目标
     */
    std::string cmd_str(CmdInfo info, uint16_t pc, Memory* mem)
    {
        const std::string& cmd = info.name;
        AddressingMode mode = info.mode;
        
        //            DST dst = info.dst;
        
        std::function<std::string(DST)> dstCode = [](DST dst){
            if (RENES_SET_FIND(REGS_NAME, dst))
            {
                return REGS_NAME.at(dst);
            }
            return std::string("C");
        };
        
        
        std::function<std::string(AddressingMode)> operCode = [cmd, &info, pc, mem](AddressingMode mode)
        {
            uint16_t dataAddr = pc + 1; // 操作数位置 = PC + 1
            std::string res;
            bool immidiate = false;
            int8_t immidiate_value = 0;
            
            switch (mode)
            {
                case RELATIVE:
                case IMMIDIATE:
                {
                    immidiate_value = mem->get8bitData(dataAddr);
                    immidiate = true;
                    break;
                }
                case ZERO_PAGE:
                {
                    res = int_to_hex(mem->get8bitData(dataAddr));
                    break;
                }
                case ZERO_PAGE_X:
                {
                    res = int_to_hex(mem->get8bitData(dataAddr)) + ",X";
                    break;
                }
                case ZERO_PAGE_Y:
                {
                    res = int_to_hex(mem->get8bitData(dataAddr)) + ",Y";
                    break;
                }
                case INDEXED_ABSOLUTE:
                {
                    res = int_to_hex(mem->get16bitData(dataAddr));
                    break;
                }
                case INDEXED_ABSOLUTE_X:
                {
                    res = int_to_hex(mem->get16bitData(dataAddr)) + ",X";
                    break;
                }
                case INDEXED_ABSOLUTE_Y:
                {
                    res = int_to_hex(mem->get16bitData(dataAddr)) + ",Y";
                    break;
                }
                case INDIRECT:
                {
                    res = "(" + int_to_hex(mem->get16bitData(dataAddr)) + ")";
                    break;
                }
                case INDIRECT_X_INDEXED:
                {
                    res = "(" + int_to_hex(mem->get8bitData(dataAddr)) + ", X)";
                    break;
                }
                case INDIRECT_INDEXED_Y:
                {
                    res = "(" + int_to_hex(mem->get8bitData(dataAddr)) + "),Y";
                    break;
                }
                case IMPLIED:
                {
                    break;
                }
                case ACCUMULATOR:
                {
                    res = "A";//int_to_hex(regs.A);
                    break;
                }
                default:
                    assert(!"error!");
                    break;
            }
            
            if (immidiate)
            {
                // 将跳转地址计算出来
                int addr = 0;
                const static char* JMP_STR[] = {
                    "BCC",
                    "BCS",
                    "BMI",
                    "BEQ",
                    "BNE",
                    "BPL",
                    "BVC",
                    "BVS"
                };
                if (RENES_ARRAY_FIND(JMP_STR, cmd))
                {
                    addr = pc + info.bytes;
                    res = std::string("#") + int_to_hex((uint16_t)(immidiate_value + addr)) + "("+std::to_string(immidiate_value)+")";
                }
                else
                {
                    res = std::string("#") + int_to_hex((uint8_t)(immidiate_value + addr));
                }
                
            }
            
            return res;
        };
        
        std::string oper = operCode(mode);
        
        return cmd + " " + oper;
        //            log("%s %s\n", cmd.c_str(),  oper.c_str());
    }
    
    // 以下指令从
    // 定义指令长度、CPU周期
    std::map<uint8_t, CmdInfo> CMD_LIST = {

        /* (immidiate) ADC #oper */ {0x69, {"ADC", CF_ADC, IMMIDIATE, 2, 2, 195}},
        /* (zeropage) ADC oper */ {0x65, {"ADC", CF_ADC, ZERO_PAGE, 2, 3, 195}},
        /* (zeropage,X) ADC oper,X */ {0x75, {"ADC", CF_ADC, ZERO_PAGE_X, 2, 4, 195}},
        /* (absolute) ADC oper */ {0x6D, {"ADC", CF_ADC, INDEXED_ABSOLUTE, 3, 4, 195}},
        /* (absolute,X) ADC oper,X */ {0x7D, {"ADC", CF_ADC, INDEXED_ABSOLUTE_X, 3, 4, 195}},
        /* (absolute,Y) ADC oper,Y */ {0x79, {"ADC", CF_ADC, INDEXED_ABSOLUTE_Y, 3, 4, 195}},
        /* ((indirect,X)) ADC (oper,X) */ {0x61, {"ADC", CF_ADC, INDIRECT_X_INDEXED, 2, 6, 195}},
        /* ((indirect),Y) ADC (oper),Y */ {0x71, {"ADC", CF_ADC, INDIRECT_INDEXED_Y, 2, 5, 195}},
        /* (immidiate) AND #oper */ {0x29, {"AND", CF_AND, IMMIDIATE, 2, 2, 130}},
        /* (zeropage) AND oper */ {0x25, {"AND", CF_AND, ZERO_PAGE, 2, 3, 130}},
        /* (zeropage,X) AND oper,X */ {0x35, {"AND", CF_AND, ZERO_PAGE_X, 2, 4, 130}},
        /* (absolute) AND oper */ {0x2D, {"AND", CF_AND, INDEXED_ABSOLUTE, 3, 4, 130}},
        /* (absolute,X) AND oper,X */ {0x3D, {"AND", CF_AND, INDEXED_ABSOLUTE_X, 3, 4, 130}},
        /* (absolute,Y) AND oper,Y */ {0x39, {"AND", CF_AND, INDEXED_ABSOLUTE_Y, 3, 4, 130}},
        /* ((indirect,X)) AND (oper,X) */ {0x21, {"AND", CF_AND, INDIRECT_X_INDEXED, 2, 6, 130}},
        /* ((indirect),Y) AND (oper),Y */ {0x31, {"AND", CF_AND, INDIRECT_INDEXED_Y, 2, 5, 130}},
        /* (accumulator) ASL A */ {0x0A, {"ASL", CF_ASL, ACCUMULATOR, 1, 2, 131}},
        /* (zeropage) ASL oper */ {0x06, {"ASL", CF_ASL, ZERO_PAGE, 2, 5, 131}},
        /* (zeropage,X) ASL oper,X */ {0x16, {"ASL", CF_ASL, ZERO_PAGE_X, 2, 6, 131}},
        /* (absolute) ASL oper */ {0x0E, {"ASL", CF_ASL, INDEXED_ABSOLUTE, 3, 6, 131}},
        /* (absolute,X) ASL oper,X */ {0x1E, {"ASL", CF_ASL, INDEXED_ABSOLUTE_X, 3, 7, 131}},
        /* (relative) BCC oper */ {0x90, {"BCC", CF_BCC, RELATIVE, 2, 2, 0}},
        /* (relative) BCS oper */ {0xB0, {"BCS", CF_BCS, RELATIVE, 2, 2, 0}},
        /* (relative) BEQ oper */ {0xF0, {"BEQ", CF_BEQ, RELATIVE, 2, 2, 0}},
        /* (zeropage) BIT oper */ {0x24, {"BIT", CF_BIT, ZERO_PAGE, 2, 3, 2}},
        /* (absolute) BIT oper */ {0x2C, {"BIT", CF_BIT, INDEXED_ABSOLUTE, 3, 4, 2}},
        /* (relative) BMI oper */ {0x30, {"BMI", CF_BMI, RELATIVE, 2, 2, 0}},
        /* (relative) BNE oper */ {0xD0, {"BNE", CF_BNE, RELATIVE, 2, 2, 0}},
        /* (relative) BPL oper */ {0x10, {"BPL", CF_BPL, RELATIVE, 2, 2, 0}},
        /* (implied) BRK */ {0x00, {"BRK", CF_BRK, IMPLIED, 1, 7, 0}},
        /* (relative) BVC oper */ {0x50, {"BVC", CF_BVC, RELATIVE, 2, 2, 0}},
        /* (relative) BVC oper */ {0x70, {"BVS", CF_BVS, RELATIVE, 2, 2, 0}},
        /* (implied) CLC */ {0x18, {"CLC", CF_CLC, IMPLIED, 1, 2, 0}},
        /* (implied) CLD */ {0xD8, {"CLD", CF_CLD, IMPLIED, 1, 2, 0}},
        /* (implied) CLI */ {0x58, {"CLI", CF_CLI, IMPLIED, 1, 2, 0}},
        /* (implied) CLV */ {0xB8, {"CLV", CF_CLV, IMPLIED, 1, 2, 0}},
        /* (immidiate) CMP #oper */ {0xC9, {"CMP", CF_CMP, IMMIDIATE, 2, 2, 131}},
        /* (zeropage) CMP oper */ {0xC5, {"CMP", CF_CMP, ZERO_PAGE, 2, 3, 131}},
        /* (zeropage,X) CMP oper,X */ {0xD5, {"CMP", CF_CMP, ZERO_PAGE_X, 2, 4, 131}},
        /* (absolute) CMP oper */ {0xCD, {"CMP", CF_CMP, INDEXED_ABSOLUTE, 3, 4, 131}},
        /* (absolute,X) CMP oper,X */ {0xDD, {"CMP", CF_CMP, INDEXED_ABSOLUTE_X, 3, 4, 131}},
        /* (absolute,Y) CMP oper,Y */ {0xD9, {"CMP", CF_CMP, INDEXED_ABSOLUTE_Y, 3, 4, 131}},
        /* ((indirect,X)) CMP (oper,X) */ {0xC1, {"CMP", CF_CMP, INDIRECT_X_INDEXED, 2, 6, 131}},
        /* ((indirect),Y) CMP (oper),Y */ {0xD1, {"CMP", CF_CMP, INDIRECT_INDEXED_Y, 2, 5, 131}},
        /* (immidiate) CPX #oper */ {0xE0, {"CPX", CF_CPX, IMMIDIATE, 2, 2, 131}},
        /* (zeropage) CPX oper */ {0xE4, {"CPX", CF_CPX, ZERO_PAGE, 2, 3, 131}},
        /* (absolute) CPX oper */ {0xEC, {"CPX", CF_CPX, INDEXED_ABSOLUTE, 3, 4, 131}},
        /* (immidiate) CPY #oper */ {0xC0, {"CPY", CF_CPY, IMMIDIATE, 2, 2, 131}},
        /* (zeropage) CPY oper */ {0xC4, {"CPY", CF_CPY, ZERO_PAGE, 2, 3, 131}},
        /* (absolute) CPY oper */ {0xCC, {"CPY", CF_CPY, INDEXED_ABSOLUTE, 3, 4, 131}},
        /* (zeropage) DEC oper */ {0xC6, {"DEC", CF_DEC, ZERO_PAGE, 2, 5, 130}},
        /* (zeropage,X) DEC oper,X */ {0xD6, {"DEC", CF_DEC, ZERO_PAGE_X, 2, 6, 130}},
        /* (absolute) DEC oper */ {0xCE, {"DEC", CF_DEC, INDEXED_ABSOLUTE, 3, 3, 130}},
        /* (absolute,X) DEC oper,X */ {0xDE, {"DEC", CF_DEC, INDEXED_ABSOLUTE_X, 3, 7, 130}},
        /* (implied) DEX */ {0xCA, {"DEX", CF_DEX, IMPLIED, 1, 2, 130}},
        /* (implied) DEY */ {0x88, {"DEY", CF_DEY, IMPLIED, 1, 2, 130}},
        /* (immidiate) EOR #oper */ {0x49, {"EOR", CF_EOR, IMMIDIATE, 2, 2, 130}},
        /* (zeropage) EOR oper */ {0x45, {"EOR", CF_EOR, ZERO_PAGE, 2, 3, 130}},
        /* (zeropage,X) EOR oper,X */ {0x55, {"EOR", CF_EOR, ZERO_PAGE_X, 2, 4, 130}},
        /* (absolute) EOR oper */ {0x4D, {"EOR", CF_EOR, INDEXED_ABSOLUTE, 3, 4, 130}},
        /* (absolute,X) EOR oper,X */ {0x5D, {"EOR", CF_EOR, INDEXED_ABSOLUTE_X, 3, 4, 130}},
        /* (absolute,Y) EOR oper,Y */ {0x59, {"EOR", CF_EOR, INDEXED_ABSOLUTE_Y, 3, 4, 130}},
        /* ((indirect,X)) EOR (oper,X) */ {0x41, {"EOR", CF_EOR, INDIRECT_X_INDEXED, 2, 6, 130}},
        /* ((indirect),Y) EOR (oper),Y */ {0x51, {"EOR", CF_EOR, INDIRECT_INDEXED_Y, 2, 5, 130}},
        /* (zeropage) INC oper */ {0xE6, {"INC", CF_INC, ZERO_PAGE, 2, 5, 130}},
        /* (zeropage,X) INC oper,X */ {0xF6, {"INC", CF_INC, ZERO_PAGE_X, 2, 6, 130}},
        /* (absolute) INC oper */ {0xEE, {"INC", CF_INC, INDEXED_ABSOLUTE, 3, 6, 130}},
        /* (absolute,X) INC oper,X */ {0xFE, {"INC", CF_INC, INDEXED_ABSOLUTE_X, 3, 7, 130}},
        /* (implied) INX */ {0xE8, {"INX", CF_INX, IMPLIED, 1, 2, 130}},
        /* (implied) INY */ {0xC8, {"INY", CF_INY, IMPLIED, 1, 2, 130}},
        /* (absolute) JMP oper */ {0x4C, {"JMP", CF_JMP, INDEXED_ABSOLUTE, 3, 3, 0}},
        /* (indirect) JMP (oper) */ {0x6C, {"JMP", CF_JMP, INDIRECT, 3, 5, 0}},
        /* (absolute) JSR oper */ {0x20, {"JSR", CF_JSR, INDEXED_ABSOLUTE, 3, 6, 0}},
        /* (immidiate) LDA #oper */ {0xA9, {"LDA", CF_LDA, IMMIDIATE, 2, 2, 130}},
        /* (zeropage) LDA oper */ {0xA5, {"LDA", CF_LDA, ZERO_PAGE, 2, 3, 130}},
        /* (zeropage,X) LDA oper,X */ {0xB5, {"LDA", CF_LDA, ZERO_PAGE_X, 2, 4, 130}},
        /* (absolute) LDA oper */ {0xAD, {"LDA", CF_LDA, INDEXED_ABSOLUTE, 3, 4, 130}},
        /* (absolute,X) LDA oper,X */ {0xBD, {"LDA", CF_LDA, INDEXED_ABSOLUTE_X, 3, 4, 130}},
        /* (absolute,Y) LDA oper,Y */ {0xB9, {"LDA", CF_LDA, INDEXED_ABSOLUTE_Y, 3, 4, 130}},
        /* ((indirect,X)) LDA (oper,X) */ {0xA1, {"LDA", CF_LDA, INDIRECT_X_INDEXED, 2, 6, 130}},
        /* ((indirect),Y) LDA (oper),Y */ {0xB1, {"LDA", CF_LDA, INDIRECT_INDEXED_Y, 2, 5, 130}},
        /* (immidiate) LDX #oper */ {0xA2, {"LDX", CF_LDX, IMMIDIATE, 2, 2, 130}},
        /* (zeropage) LDX oper */ {0xA6, {"LDX", CF_LDX, ZERO_PAGE, 2, 3, 130}},
        /* (zeropage,Y) LDX oper,Y */ {0xB6, {"LDX", CF_LDX, ZERO_PAGE_Y, 2, 4, 130}},
        /* (absolute) LDX oper */ {0xAE, {"LDX", CF_LDX, INDEXED_ABSOLUTE, 3, 4, 130}},
        /* (absolute,Y) LDX oper,Y */ {0xBE, {"LDX", CF_LDX, INDEXED_ABSOLUTE_Y, 3, 4, 130}},
        /* (immidiate) LDY #oper */ {0xA0, {"LDY", CF_LDY, IMMIDIATE, 2, 2, 130}},
        /* (zeropage) LDY oper */ {0xA4, {"LDY", CF_LDY, ZERO_PAGE, 2, 3, 130}},
        /* (zeropage,X) LDY oper,X */ {0xB4, {"LDY", CF_LDY, ZERO_PAGE_X, 2, 4, 130}},
        /* (absolute) LDY oper */ {0xAC, {"LDY", CF_LDY, INDEXED_ABSOLUTE, 3, 4, 130}},
        /* (absolute,X) LDY oper,X */ {0xBC, {"LDY", CF_LDY, INDEXED_ABSOLUTE_X, 3, 4, 130}},
        /* (accumulator) LSR A */ {0x4A, {"LSR", CF_LSR, ACCUMULATOR, 1, 2, 3}},
        /* (zeropage) LSR oper */ {0x46, {"LSR", CF_LSR, ZERO_PAGE, 2, 5, 3}},
        /* (zeropage,X) LSR oper,X */ {0x56, {"LSR", CF_LSR, ZERO_PAGE_X, 2, 6, 3}},
        /* (absolute) LSR oper */ {0x4E, {"LSR", CF_LSR, INDEXED_ABSOLUTE, 3, 6, 3}},
        /* (absolute,X) LSR oper,X */ {0x5E, {"LSR", CF_LSR, INDEXED_ABSOLUTE_X, 3, 7, 3}},
        /* (implied) NOP */ {0xEA, {"NOP", CF_NOP, IMPLIED, 1, 2, 0}},
        /* (immidiate) ORA #oper */ {0x09, {"ORA", CF_ORA, IMMIDIATE, 2, 2, 130}},
        /* (zeropage) ORA oper */ {0x05, {"ORA", CF_ORA, ZERO_PAGE, 2, 3, 130}},
        /* (zeropage,X) ORA oper,X */ {0x15, {"ORA", CF_ORA, ZERO_PAGE_X, 2, 4, 130}},
        /* (absolute) ORA oper */ {0x0D, {"ORA", CF_ORA, INDEXED_ABSOLUTE, 3, 4, 130}},
        /* (absolute,X) ORA oper,X */ {0x1D, {"ORA", CF_ORA, INDEXED_ABSOLUTE_X, 3, 4, 130}},
        /* (absolute,Y) ORA oper,Y */ {0x19, {"ORA", CF_ORA, INDEXED_ABSOLUTE_Y, 3, 4, 130}},
        /* ((indirect,X)) ORA (oper,X) */ {0x01, {"ORA", CF_ORA, INDIRECT_X_INDEXED, 2, 6, 130}},
        /* ((indirect),Y) ORA (oper),Y */ {0x11, {"ORA", CF_ORA, INDIRECT_INDEXED_Y, 2, 5, 130}},
        /* (implied) PHA */ {0x48, {"PHA", CF_PHA, IMPLIED, 1, 3, 0}},
        /* (implied) PHP */ {0x08, {"PHP", CF_PHP, IMPLIED, 1, 3, 0}},
        /* (implied) PLA */ {0x68, {"PLA", CF_PLA, IMPLIED, 1, 4, 130}},
        /* (implied) PHP */ {0x28, {"PLP", CF_PLP, IMPLIED, 1, 4, 0}},
        /* (accumulator) ROL A */ {0x2A, {"ROL", CF_ROL, ACCUMULATOR, 1, 2, 131}},
        /* (zeropage) ROL oper */ {0x26, {"ROL", CF_ROL, ZERO_PAGE, 2, 5, 131}},
        /* (zeropage,X) ROL oper,X */ {0x36, {"ROL", CF_ROL, ZERO_PAGE_X, 2, 6, 131}},
        /* (absolute) ROL oper */ {0x2E, {"ROL", CF_ROL, INDEXED_ABSOLUTE, 3, 6, 131}},
        /* (absolute,X) ROL oper,X */ {0x3E, {"ROL", CF_ROL, INDEXED_ABSOLUTE_X, 3, 7, 131}},
        /* (accumulator) ROR A */ {0x6A, {"ROR", CF_ROR, ACCUMULATOR, 1, 2, 131}},
        /* (zeropage) ROR oper */ {0x66, {"ROR", CF_ROR, ZERO_PAGE, 2, 5, 131}},
        /* (zeropage,X) ROR oper,X */ {0x76, {"ROR", CF_ROR, ZERO_PAGE_X, 2, 6, 131}},
        /* (absolute) ROR oper */ {0x6E, {"ROR", CF_ROR, INDEXED_ABSOLUTE, 3, 6, 131}},
        /* (absolute,X) ROR oper,X */ {0x7E, {"ROR", CF_ROR, INDEXED_ABSOLUTE_X, 3, 7, 131}},
        /* (implied) RTI */ {0x40, {"RTI", CF_RTI, IMPLIED, 1, 6, 0}},
        /* (implied) RTS */ {0x60, {"RTS", CF_RTS, IMPLIED, 1, 6, 0}},
        /* (immidiate) SBC #oper */ {0xE9, {"SBC", CF_SBC, IMMIDIATE, 2, 2, 195}},
        /* (zeropage) SBC oper */ {0xE5, {"SBC", CF_SBC, ZERO_PAGE, 2, 3, 195}},
        /* (zeropage,X) SBC oper,X */ {0xF5, {"SBC", CF_SBC, ZERO_PAGE_X, 2, 4, 195}},
        /* (absolute) SBC oper */ {0xED, {"SBC", CF_SBC, INDEXED_ABSOLUTE, 3, 4, 195}},
        /* (absolute,X) SBC oper,X */ {0xFD, {"SBC", CF_SBC, INDEXED_ABSOLUTE_X, 3, 4, 195}},
        /* (absolute,Y) SBC oper,Y */ {0xF9, {"SBC", CF_SBC, INDEXED_ABSOLUTE_Y, 3, 4, 195}},
        /* ((indirect,X)) SBC (oper,X) */ {0xE1, {"SBC", CF_SBC, INDIRECT_X_INDEXED, 2, 6, 195}},
        /* ((indirect),Y) SBC (oper),Y */ {0xF1, {"SBC", CF_SBC, INDIRECT_INDEXED_Y, 2, 5, 195}},
        /* (implied) SEC */ {0x38, {"SEC", CF_SEC, IMPLIED, 1, 2, 0}},
        /* (implied) SED */ {0xF8, {"SED", CF_SED, IMPLIED, 1, 2, 0}},
        /* (implied) SEI */ {0x78, {"SEI", CF_SEI, IMPLIED, 1, 2, 0}},
        /* (zeropage) STA oper */ {0x85, {"STA", CF_STA, ZERO_PAGE, 2, 3, 0}},
        /* (zeropage,X) STA oper,X */ {0x95, {"STA", CF_STA, ZERO_PAGE_X, 2, 4, 0}},
        /* (absolute) STA oper */ {0x8D, {"STA", CF_STA, INDEXED_ABSOLUTE, 3, 4, 0}},
        /* (absolute,X) STA oper,X */ {0x9D, {"STA", CF_STA, INDEXED_ABSOLUTE_X, 3, 5, 0}},
        /* (absolute,Y) STA oper,Y */ {0x99, {"STA", CF_STA, INDEXED_ABSOLUTE_Y, 3, 5, 0}},
        /* ((indirect,X)) STA (oper,X) */ {0x81, {"STA", CF_STA, INDIRECT_X_INDEXED, 2, 6, 0}},
        /* ((indirect),Y) STA (oper),Y */ {0x91, {"STA", CF_STA, INDIRECT_INDEXED_Y, 2, 6, 0}},
        /* (zeropage) STX oper */ {0x86, {"STX", CF_STX, ZERO_PAGE, 2, 3, 0}},
        /* (zeropage,Y) STX oper,Y */ {0x96, {"STX", CF_STX, ZERO_PAGE_Y, 2, 4, 0}},
        /* (absolute) STX oper */ {0x8E, {"STX", CF_STX, INDEXED_ABSOLUTE, 3, 4, 0}},
        /* (zeropage) STY oper */ {0x84, {"STY", CF_STY, ZERO_PAGE, 2, 3, 0}},
        /* (zeropage,X) STY oper,X */ {0x94, {"STY", CF_STY, ZERO_PAGE_X, 2, 4, 0}},
        /* (absolute) STY oper */ {0x8C, {"STY", CF_STY, INDEXED_ABSOLUTE, 3, 4, 0}},
        /* (implied) TAX */ {0xAA, {"TAX", CF_TAX, IMPLIED, 1, 2, 130}},
        /* (implied) TAY */ {0xA8, {"TAY", CF_TAY, IMPLIED, 1, 2, 130}},
        /* (implied) TSX */ {0xBA, {"TSX", CF_TSX, IMPLIED, 1, 2, 130}},
        /* (implied) TXA */ {0x8A, {"TXA", CF_TXA, IMPLIED, 1, 2, 130}},
        /* (implied) TXS */ {0x9A, {"TXS", CF_TXS, IMPLIED, 1, 2, 130}},
        /* (implied) TYA */ {0x98, {"TYA", CF_TYA, IMPLIED, 1, 2, 130}},
    };

    // 2A03
    struct CPU {
        
        // 标准
        constexpr const static double StandardFrequency = 1.789773; // 标准频率 1.79Mhz
        const static uint32_t TimePerCpuCycle = (1.0/(1024*1000*StandardFrequency)) * 1e9 ; // 每个CPU时钟为 572 ns
        
        // 用于调试
        bool debug = false;
        
        struct __registers {
            
            // 3个特殊功能寄存器: PC寄存器(Program Counter)，SP寄存器(Stack Pointer)，P寄存器(Processor Status)
            uint16_t PC; // 存储16bit地址
            uint8_t SP;  // 由8bit表示[0,255]的栈空间
            
            // 数据下标
            enum __P{
                C = 0,
                Z,I,D,B,_,V,N
                
                //                C;   // 进位
                //                Z;   // 零
                //                I;   // 中断禁止
                //                D;   // Decimal Mode，无影响，但支持 SED CLD 指令
                //                B;   // 中断类型，软中断为1，硬中断为0
                //                _;   // 空
                //                V;   // 溢出
                //                N;   // 负数
            };
            struct bit8 P;
            
            // 3个8位寄存器，用于计算数值
            uint8_t A;
            uint8_t X;
            uint8_t Y;
            
        }regs;
        
        std::set<std::string> usedCmds;
        
        // 错误标记
        bool error;
        
        long execCmdLine = 0;
        
        CPU()
        {
            // 初始化置0
            memset(&regs, 0, sizeof(__registers));
            
            // 检查数据尺寸
            assert(1 == sizeof(regs.P));
            
        }
        
        // 初始化，映射内存
        void init(Memory* mem)
        {
            _mem = mem;
            
            // 寄存器初始化
            regs.PC = 0;
            regs.SP = 0xFF; // 栈是自上而下增加
            
            error = false;
            
            interrupts(InterruptTypeReset); // 复位中断
        }
        
        // 中断类型
        enum InterruptType{
            InterruptTypeNone,  // 无
            InterruptTypeBreak, // 指令中断
            InterruptTypeIRQs,  // 软中断
            InterruptTypeNMI,   // NMI中断
            InterruptTypeReset  // 重置
        };
        
        std::mutex _interrupt_mtx;
        
        // 中断
        void interrupts(InterruptType type)
        {
            std::lock_guard<std::mutex> lock(_interrupt_mtx);
            
            bool hasInterrupts = false;
            {
                // 检查中断是否允许被执行
                switch(type)
                {
                    case InterruptTypeNMI:
                        // NMI中断可以被0x2000的第7位屏蔽，== 0 就是屏蔽，这里不通过read访问
                        if (((bit8*)&_mem->masterData()[0x2000])->get(7) == 0) break;
                    case InterruptTypeReset:
                        // 不可屏蔽中断
                        hasInterrupts = true;
                        break;
                    case InterruptTypeBreak:
                        // break中断由代码产生，这个也不能屏蔽
                        hasInterrupts = true;
                        break;
                    case InterruptTypeIRQs:
                    {
                        // 软中断，由系统硬件发出，可以屏蔽。如果中断禁止标记为0，才可继续此中断
                        if (regs.P.get(__registers::I) == 0)
                            hasInterrupts = true;
                        break;
                    }
                    default:
                        break;
                }
            }
            
            if (hasInterrupts) _currentInterruptType = type;
        }
        
        // 执行指令，返回当前指令所需的cpu周期数（周期数来自CMD_LIST定义）
        int exec()
        {
            if (error)
                return 0;
            
            RENES_REGS
            
            // 检查中断和处理中断
            _interrupt_mtx.lock();
            {
                // 处理中断
                if (_currentInterruptType != InterruptTypeNone)
                {
                    // 获取中断处理函数地址
                    uint16_t interruptHandlerAddr = _getInterruptHandlerAddr(_currentInterruptType);
                    {
                        log("中断函数地址: %04X\n", interruptHandlerAddr);
                        
                        // 地址修正
                        if (interruptHandlerAddr == 0)
                            interruptHandlerAddr = 0x8000;
                    }
                    
                    // 跳转到中断向量指向的处理函数地址
                    {
                        // Reset 不需要保存现场
                        if (_currentInterruptType != InterruptTypeReset)
                        {
                            if (_currentInterruptType == InterruptTypeBreak)
                            {
                                PC ++; // break 返回地址会+1，所以这里先增加
//                                printf("break 入栈PC %x\n", PC);
                            }
                            
                            push((PC >> 8) & 0xff);    /* Push return address onto the stack. */
                            push(PC & 0xff);
//                            push(*(uint8_t*)&regs.P);
//                            SET_BREAK((1));             /* Set BFlag before pushing */
                            
                            // 在推入之前修改状态
                            switch(_currentInterruptType)
                            {
                                case InterruptTypeBreak:
                                {
                                    push(SR | 0x30); // 4,5 = 1
                                    break;
                                }
                                case InterruptTypeNMI:
                                case InterruptTypeIRQs:
                                {
                                    push(SR | 0x10); // 4 = 1
                                    break;
                                }
                                default:
                                    break;
                            }
                        }
                        
                        // 设置中断屏蔽标志(I)防止新的中断进来
                        regs.P.set(__registers::I, 1);
                        
                        // 取消中断标记
                        _currentInterruptType = InterruptTypeNone;
                        
                        PC = interruptHandlerAddr;
                    }
                }
            }
            _interrupt_mtx.unlock();
            
            // 从内存里面取出一条8bit指令，将PC移动到下一个内存地址
            uint8_t cmd = _mem->read8bitData(regs.PC);
            
            
            log("[%ld][%04X] cmd: %x => ", execCmdLine, regs.PC, cmd);
            
            if (!RENES_SET_FIND(CMD_LIST, cmd))
            {
                log("未知的指令！");
                error = true;
                return 0;
            }
            
            auto& info = CMD_LIST.at(cmd);
            
            if (this->debug)
            {
                auto str = cmd_str(info, regs.PC, _mem);
                log("%s\n", str.c_str());
            }
            
            _runCmd(info);
            
            // 检查内存错误
            error = _mem->error;

#ifdef DEBUG
            log("A: %d X: %d Y: %d P: %d - ", regs.A, regs.X, regs.Y, regs.P);
            for (int i=0; i<8; i++)
            {
                log("%d ", regs.P.get(i));
            }
            log("\n");
#endif
            
            execCmdLine ++;
            
            return info.cycles;
        }
        
    private:
        
        //////////////////////////////////////////////////////////////////
        // 操作函数
        inline void SET_SR(uint8_t src)
        {
            // 忽略4/5位
            src &= ~0x30;
            regs.P = *(bit8*)&src;
        };
        inline void SET_CARRY(int src) { regs.P.set(__registers::C, src != 0 ? 1 : 0); };
        inline void SET_DECIMAL(int src) { regs.P.set(__registers::D, src != 0 ? 1 : 0); };
        inline void SET_INTERRUPT(int src) { regs.P.set(__registers::I, src != 0 ? 1 : 0); };
        inline void SET_SIGN(int8_t src) { regs.P.set(__registers::N, src & 0x80 ? 1 : 0); };
        inline void SET_ZERO(int8_t src) { regs.P.set(__registers::Z, src == 0 ? 1 : 0); };
        inline void SET_OVERFLOW(bool src) { regs.P.set(__registers::V, src); };
        inline void STORE(uint16_t addr, int8_t src) { _mem->write8bitData(addr, src); };
        inline uint16_t REL_ADDR(uint16_t addr, int8_t src) const { return addr + src; };
        inline bool IF_CARRY() const { return regs.P.get(__registers::C); };
        inline bool IF_DECIMAL() const { return regs.P.get(__registers::D); };
        inline bool IF_ZERO() const { return regs.P.get(__registers::Z); };
        inline bool IF_SIGN() const { return regs.P.get(__registers::N); };
        inline bool IF_OVERFLOW() const { return regs.P.get(__registers::V); };
        
        //////////////////////////////////////////////////////////////////
        
        inline
        void _runCmd(const CmdInfo& info)
        {
            RENES_REGS
            
            // 得到 dst、src、address
            DST dst = DST_NONE;
            unsigned int src = 0;
            uint16_t address = 0;
            
            // 发生错误的话（可能由部分监听器阻止）, 这里检测实现错误
            _addressing(info, &dst, &src, &address);
            
            switch(info.cf)
            {
                case CF_NON:
                {
                    break;
                }
                case CF_AND:
                {
                    src &= AC;
                    SET_SIGN(src);
                    SET_ZERO(src);
                    AC = src;
                    
                    break;
                }
                case CF_ASL:
                {
                    assert(dst != DST_NONE);
                    
                    SET_CARRY(src & 0x80);
                    src <<= 1;
                    src &= 0xff;
                    SET_SIGN(src);
                    SET_ZERO(src);
                    
                    ValueToDST(dst, src, address);
                    break;
                }
                case CF_BCC:
                {
                    if (!IF_CARRY()) {
                        
                        PC = REL_ADDR(PC, src);
                    }
                    
                    break;
                }
                case CF_BCS:
                {
                    if (IF_CARRY()) {
                        
                        PC = REL_ADDR(PC, src);
                    }
                    
                    break;
                }
                case CF_BEQ:
                {
                    if (IF_ZERO()) {
                        
                        PC = REL_ADDR(PC, src);
                    }
                    
                    break;
                }
                case CF_BMI:
                {
                    if (IF_SIGN()) {
                        
                        PC = REL_ADDR(PC, src);
                    }
                    
                    break;
                }
                case CF_BNE:
                {
                    if (!IF_ZERO()) {
                        
                        PC = REL_ADDR(PC, src);
                    }
                    
                    break;
                }
                case CF_BPL:
                {
                    if (!IF_SIGN()) {
                        
                        PC = REL_ADDR(PC, src);
                    }
                    
                    break;
                }
                case CF_BVC:
                {
                    if (!IF_OVERFLOW()) {
                        
                        PC = REL_ADDR(PC, src);
                    }
                    
                    break;
                }
                case CF_BVS:
                {
                    if (IF_OVERFLOW()) {
                        
                        PC = REL_ADDR(PC, src);
                    }
                    
                    break;
                }
                case CF_EOR:
                {
                    src ^= AC;
                    SET_SIGN(src);
                    SET_ZERO(src);
                    AC = src;
                    break;
                }
                case CF_LSR:
                {
                    SET_CARRY(src & 0x01);
                    src >>= 1;
                    SET_SIGN(src);
                    SET_ZERO(src);
                    
                    ValueToDST(dst, src, address);
                    break;
                }
                case CF_ROL:
                {
                    src <<= 1;
                    if (IF_CARRY()) src |= 0x1;
                    SET_CARRY(src > 0xff);
                    src &= 0xff;
                    SET_SIGN(src);
                    SET_ZERO(src);
                    
                    ValueToDST(dst, src, address);
                    break;
                }
                case CF_ROR:
                {
                    if (IF_CARRY()) src |= 0x100;
                    SET_CARRY(src & 0x01);
                    src >>= 1;
                    SET_SIGN(src);
                    SET_ZERO(src);
                    
                    ValueToDST(dst, src, address);
                    break;
                }
                case CF_ADC:
                {
                    unsigned int temp = src + AC + (IF_CARRY() ? 1 : 0);
                    SET_ZERO(temp & 0xff);    /* This is not valid in decimal mode */
                    //                                if (IF_DECIMAL()) {
                    //                                    if (((AC & 0xf) + (src & 0xf) + (IF_CARRY() ? 1 : 0)) > 9) temp += 6;
                    //                                    SET_SIGN(temp);
                    //                                    SET_OVERFLOW(!((AC ^ src) & 0x80) && ((AC ^ temp) & 0x80));
                    //                                    if (temp > 0x99) temp += 96;
                    //                                    SET_CARRY(temp > 0x99);
                    //                                } else
                    {
                        SET_SIGN(temp);
                        SET_OVERFLOW(!((AC ^ src) & 0x80) && ((AC ^ temp) & 0x80));
                        SET_CARRY(temp > 0xff);
                    }
                    AC = ((uint8_t) temp);
                    break;
                }
                case CF_BIT:
                {
                    SET_SIGN(src);
                    SET_OVERFLOW(0x40 & src);    /* Copy bit 6 to OVERFLOW flag. */
                    SET_ZERO(src & AC);
                    break;
                }
                case CF_CLC:
                {
                    SET_CARRY((0));
                    
                    break;
                }
                case CF_CLD:
                {
                    SET_DECIMAL((0));
                    
                    break;
                }
                case CF_CLI:
                {
                    SET_INTERRUPT((0));
                    
                    break;
                }
                case CF_CLV:
                {
                    SET_OVERFLOW((0));
                    
                    break;
                }
                case CF_CMP:
                {
                    src = AC - src;
                    SET_CARRY(src < 0x100);
                    SET_SIGN(src);
                    SET_ZERO(src &= 0xff);
                    
                    break;
                }
                case CF_CPX:
                {
                    src = XR - src;
                    SET_CARRY(src < 0x100);
                    SET_SIGN(src);
                    SET_ZERO(src &= 0xff);
                    
                    break;
                }
                case CF_CPY:
                {
                    src = YR - src;
                    SET_CARRY(src < 0x100);
                    SET_SIGN(src);
                    SET_ZERO(src &= 0xff);
                    
                    break;
                }
                case CF_DEC:
                {
                    src = (src - 1) & 0xff;
                    SET_SIGN(src);
                    SET_ZERO(src);
                    STORE(address, (src));
                    
                    break;
                }
                case CF_DEX:
                {
                    unsigned src = XR;
                    src = (src - 1) & 0xff;
                    SET_SIGN(src);
                    SET_ZERO(src);
                    XR = (src);
                    
                    break;
                }
                case CF_DEY:
                {
                    unsigned src = YR;
                    src = (src - 1) & 0xff;
                    SET_SIGN(src);
                    SET_ZERO(src);
                    YR = (src);
                    
                    break;
                }
                case CF_INC:
                {
                    src = (src + 1) & 0xff;
                    SET_SIGN(src);
                    SET_ZERO(src);
                    STORE(address, (src));
                    
                    break;
                }
                case CF_INX:
                {
                    unsigned src = XR;
                    src = (src + 1) & 0xff;
                    SET_SIGN(src);
                    SET_ZERO(src);
                    XR = (src);
                    
                    break;
                }
                case CF_INY:
                {
                    unsigned src = YR;
                    src = (src + 1) & 0xff;
                    SET_SIGN(src);
                    SET_ZERO(src);
                    YR = (src);
                    
                    break;
                }
                case CF_LDA:
                {
                    SET_SIGN(src);
                    SET_ZERO(src);
                    AC = (src);
                    
                    break;
                }
                case CF_LDX:
                {
                    SET_SIGN(src);
                    SET_ZERO(src);
                    XR = (src);
                    
                    break;
                }
                case CF_LDY:
                {
                    SET_SIGN(src);
                    SET_ZERO(src);
                    YR = (src);
                    
                    break;
                }
                case CF_NOP:
                {
                    break;
                }
                case CF_ORA:
                {
                    src |= AC;
                    SET_SIGN(src);
                    SET_ZERO(src);
                    AC = src;
                    
                    break;
                }
                case CF_PHA:
                {
                    push(AC);
                    break;
                }
                case CF_PHP:
                {
                    //                                push(*(uint8_t*)&SR);
                    src = SR | 0x30; // P入栈需要设置副本第4、5bit为1，而自身不变(https://wiki.nesdev.com/w/index.php/CPU_status_flag_behavior)
                    push(src);
                    break;
                }
                case CF_PLA:
                {
                    src = pop();
                    SET_SIGN(src);    /* Change sign and zero flag accordingly. */
                    SET_ZERO(src);
                    
                    AC = src;
                    
                    break;
                }
                case CF_PLP:
                {
                    src = pop();
                    SET_SR((src));
                    
                    break;
                }
                case CF_SBC:
                {
                    unsigned int temp = AC - src - (IF_CARRY() ? 0 : 1);
                    SET_SIGN(temp);
                    SET_ZERO(temp & 0xff);    /* Sign and Zero are invalid in decimal mode */
                    SET_OVERFLOW(((AC ^ temp) & 0x80) && ((AC ^ src) & 0x80));
                    //                                if (IF_DECIMAL()) {
                    //                                    if ( ((AC & 0xf) - (IF_CARRY() ? 0 : 1)) < (src & 0xf)) /* EP */ temp -= 6;
                    //                                    if (temp > 0x99) temp -= 0x60;
                    //                                }
                    SET_CARRY(temp < 0x100);
                    AC = (temp & 0xff);
                    break;
                }
                case CF_SEC:
                {
                    SET_CARRY((1));
                    break;
                }
                case CF_SED:
                {
                    SET_DECIMAL((1));
                    break;
                }
                case CF_SEI:
                {
                    SET_INTERRUPT((1));
                    break;
                }
                case CF_STA:
                {
                    STORE(address, (AC));
                    
                    break;
                }
                case CF_STX:
                {
                    STORE(address, (XR));
                    
                    break;
                }
                case CF_STY:
                {
                    STORE(address, (YR));
                    
                    break;
                }
                case CF_TAX:
                {
                    unsigned src = AC;
                    SET_SIGN(src);
                    SET_ZERO(src);
                    XR = (src);
                    
                    break;
                }
                case CF_TAY:
                {
                    unsigned src = AC;
                    SET_SIGN(src);
                    SET_ZERO(src);
                    YR = (src);
                    
                    break;
                }
                case CF_TSX:
                {
                    unsigned src = SP;
                    SET_SIGN(src);
                    SET_ZERO(src);
                    XR = (src);
                    
                    break;
                }
                case CF_TXA:
                {
                    unsigned src = XR;
                    SET_SIGN(src);
                    SET_ZERO(src);
                    AC = (src);
                    
                    break;
                }
                case CF_TXS:
                {
                    unsigned src = XR;
                    SP = (src);
                    
                    break;
                }
                case CF_TYA:
                {
                    unsigned src = YR;
                    SET_SIGN(src);
                    SET_ZERO(src);
                    AC = (src);
                    
                    break;
                }
                case CF_BRK:
                {
                    // BRK 执行时，产生BRK中断
                    interrupts(InterruptTypeBreak);
                    
                    //                                PC++;
                    //                                push((PC >> 8) & 0xff);    /* Push return address onto the stack. */
                    //                                push(PC & 0xff);
                    //                                SET_BREAK((1));             /* Set BFlag before pushing */
                    //                                push(SR);
                    //                                SET_INTERRUPT((1));
                    //                                PC = (LOAD(0xFFFE) | (LOAD(0xFFFF) << 8));
                    
                    
                    break;
                }
                case CF_JSR:
                {
                    PC--;
                    push((PC >> 8) & 0xff);    /* Push return address onto the stack. */
                    push(PC & 0xff);
                    PC = address;//(src);
                    
                    break;
                }
                case CF_JMP:
                {
                    PC = address;
                    //                                PC = (src);
                    break;
                }
                case CF_RTS:
                {
                    // 允许溢出 https://wiki.nesdev.com/w/index.php/Stack
                    //                                if (regs.SP <= 0xFD) // 255 - 2
                    {
                        //                                    uint8_t PC_low = pop();
                        //                                    uint8_t PC_high = pop();
                        //                                    PC = (PC_high << 8) + PC_low + 1;
                        src = pop();
                        src += ((pop()) << 8) + 1;    /* Load return address from stack and add 1. */
                        PC = (src);
                    }
                    //                                else
                    //                                {
                    //                                    assert(!"error!");
                    //                                }
                    break;
                }
                case CF_RTI:
                {
                    //                                if (regs.SP <= 0xFC) // 255-3
                    {
                        src = pop();
                        //                                    regs.P = *(bit8*)&data;
                        //                                    printf("P出栈 %x\n", src);
                        
                        SET_SR(src);
                        src = pop();
                        src |= (pop() << 8);
                        PC = (src);
                        
                        //                                    printf("PC出栈 %x\n", PC);
                        
                        //                                    uint8_t PC_low = pop();
                        //                                    uint8_t PC_high = pop();
                        //
                        //                                    regs.PC = (PC_high << 8) + PC_low + 1;
                    }
                    //                                else
                    //                                {
                    //                                    assert(!"error!");
                    //                                }
                    
                    break;
                }
                    
                default:
                    log("未知的指令！");
                    error = true;
                    break;
                    
            }
            
            
        }
        
        // 寻址操作，得到 目标、源数据、地址
        inline
        void _addressing(const CmdInfo& info, DST* dst, unsigned int* src, uint16_t* address)
        {
            RENES_REGS
            
            auto mode = info.mode;
            switch(mode)
            {
                case IMPLIED:       // 没有寻址，不操作任何内存
                    break;
                case ACCUMULATOR:   // 累加器寻址，不访问内存
                {
                    *src = (uint8_t)AC;
                    *dst = DST_REGS_A;
                    break;
                }
                default:
                {
                    // 先得到地址，该地址可用于读写访问
                    *address = memoryAddressingByMode(mode);
                    
                    // 寄存器 存储到 地址
                    const static CF _noSrcAccess[] = {
                        CF_STA, CF_STX, CF_STY
                    };
                    
                    if (!RENES_ARRAY_FIND(_noSrcAccess, info.cf))
                    {
                        *src = (uint8_t)_mem->read8bitData(*address, true);
                    }
                    
                    *dst = DST_M;
                    break;
                }
            }
            
            // 由于读取操作码会变更PC，读取操作数也会变更PC，所以这里统一移动PC指针到下一条指令位置
            PC += info.bytes;
        }
        
        // 将值写入目标
        inline
        void ValueToDST(DST dst, uint8_t value, uint16_t address)
        {
            RENES_REGS
            
            switch (dst)
            {
                case DST_REGS_SP:
                    SP = value;
                    break;
                case DST_REGS_A:
                    AC = value;
                    break;
                case DST_REGS_X:
                    XR = value;
                    break;
                case DST_REGS_Y:
                    YR = value;
                    break;
                case DST_M:
                    // 如果写入目标是一个内存地址，就需要进行寻址
                    _mem->write8bitData(address, value);
                    break;
                default:
                    assert(!"error!");
                    break;
            }
        };
        
        // 得到中断处理函数的地址
        inline
        uint16_t _getInterruptHandlerAddr(InterruptType interruptType) const
        {
            // 函数地址是一个16bit数值，分别存储在两个8bit的内存上，这里定义一个结构体表示内存地址偏移
            struct ADDR2{
                uint16_t low;
                uint16_t high;
            };
            
            const static std::map<int, ADDR2> handler = {
                {InterruptTypeNMI, {0xFFFA, 0xFFFB}},
                {InterruptTypeReset, {0xFFFC, 0xFFFD}},
                {InterruptTypeIRQs, {0xFFFE, 0xFFFF}},
                {InterruptTypeBreak, {0xFFFE, 0xFFFF}},
            };
            
            const ADDR2& addr2 = handler.at(interruptType);
            uint16_t interruptHandlerAddr = (_mem->read8bitData(addr2.high) << 8) + _mem->read8bitData(addr2.low);
            return interruptHandlerAddr;
        }
        
        void SET_BREAK(int src)
        {
            regs.P.set(__registers::B, src != 0 ? 1 : 0);
        };

        // 内存寻址，得到操作数所在的地址
        inline
        uint16_t memoryAddressingByMode(AddressingMode mode) const
        {
            // PC: cmd (8bit)
            // PC+1: 数据
            uint16_t dataAddr = regs.PC + 1; // 操作数位置 = PC + 1
            
            // 操作数实际存在的地址
            uint16_t addr;
            bool data16bit = false;
            
            switch (mode)
            {
                case RELATIVE:      // 相对寻址  [8bit] 跳转
                case IMMIDIATE:     // 立即数寻址 [8bit] 直接
                {
                    addr = dataAddr;
                    break;
                }
                case ZERO_PAGE:
                {
                    addr = _mem->read8bitData(dataAddr);
                    break;
                }
                case ZERO_PAGE_X:
                {
                    addr = (_mem->read8bitData(dataAddr) + regs.X) % 0x100; // [0, 255] 循环
                    break;
                }
                case ZERO_PAGE_Y:
                {
                    addr = (_mem->read8bitData(dataAddr) + regs.Y) % 0x100;
                    break;
                }
                case INDEXED_ABSOLUTE:
                {
                    addr = _mem->get16bitData(dataAddr);
                    break;
                }
                case INDEXED_ABSOLUTE_X:
                {
                    addr = _mem->get16bitData(dataAddr) + regs.X;
                    break;
                }
                case INDEXED_ABSOLUTE_Y:
                {
                    addr = _mem->get16bitData(dataAddr) + regs.Y;
                    break;
                }
                case INDIRECT:
                {
                    auto oper = _mem->get16bitData(dataAddr);
                    
                    // 6502 JMP Indirect bug
                    if ((oper & 0xff) == 0xff && _mem->masterData()[regs.PC] == 0x6C) // 低字节是0xFF，高位就从当前页读取
                    {
                        addr = _mem->read8bitData(oper) | (_mem->read8bitData(oper-0xff) << 8);
                    }
                    else
                    {
                        addr = _mem->get16bitData(oper);
                    }
                    
                    break;
                }
                case INDIRECT_X_INDEXED:
                {
                    addr = _mem->get16bitData((_mem->read8bitData(dataAddr) + regs.X) % 0x100);
                    break;
                }
                case INDIRECT_INDEXED_Y:
                {
                    addr = _mem->get16bitData(_mem->read8bitData(dataAddr)) + regs.Y;
                    break;
                }
                default:
                    assert(!"该寻址模式不支持内存寻址!");
                    break;
            }
            
            if (dataAddr == addr)
            {
                uint8_t* d = &_mem->masterData()[addr];
                int dd;
                if (data16bit)
                    dd = (int)*((uint16_t*)d);
                else
                    dd = (int)*d;
                
                log("立即数 %X 值 %d\n", addr, dd);
            }
            else
            {
                log("间接寻址 %X 值 %d\n", addr, (int)_mem->masterData()[addr]);
            }
            
            return addr;
        }
        
        // 入栈
        inline
        void push(uint8_t value)
        {
            uint16_t stack_addr = STACK_ADDR_OFFSET + regs.SP;
            _mem->write8bitData(stack_addr, value);
            regs.SP --;
        }
        
        // 出栈
        inline
        uint8_t pop()
        {
            regs.SP ++;
            return _mem->read8bitData(STACK_ADDR_OFFSET + regs.SP);
        }

        Memory* _mem;
        InterruptType _currentInterruptType;
    };
    
    
}
