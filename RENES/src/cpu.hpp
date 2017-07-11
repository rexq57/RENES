
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
    
    size_t highBit(uint8_t a) {
        size_t bits=0;
        while (a!=0) {
            ++bits;
            a>>=1;
        };
        return bits;
    }
    

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
        
        CF_BCC, CF_BCS,
        CF_BEQ,
        CF_BNE, CF_BPL,
        CF_CLC, CF_CLD, CF_CLI,
        CF_CMP, CF_CPX, CF_CPY,
        CF_DEX, CF_DEY,
        CF_LDA, CF_LDX, CF_LDY,
        
        CF_INX, CF_INY, CF_INC,
        
        CF_ORA,
        
        CF_SEC, CF_SED, CF_SEI,
        CF_STA, CF_STX, CF_STY,
        CF_TAX, CF_TAY, CF_TXA, CF_TXS, CF_TYA,
        
        
        CF_BRK,
//        CF_LD,
        CF_ST,
//        CF_IN,
//        CF_CP,
//        CF_B,
        CF_RTI,
        CF_JSR,
        CF_RTS,
        CF_AND,
        CF_JMP,
        
//        CF_SE,
//        CF_CL,
//        CF_T,
        
        CF_DE,
        CF_SBC,
        CF_ADC,
        
        CF_BIT,
        
//        CF_OR,
        
        CF_ASL, // 累加器寻址，算数左移
        CF_LSR,
        
        CF_PHA,
        CF_PLA,
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
        
        //            C = 0,
        //            Z,I,D,B,_,V,N
        
        
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
        DST dst; // 寄存器目标
        AddressingMode mode;
        InsParam param;
        
        int bytes;
        int cyles;
        
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
            if (SET_FIND(REGS_NAME, dst))
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
            int immidiate_value = 0;
            
            switch (mode)
            {
                case RELATIVE:
                case IMMIDIATE:
                {
                    immidiate_value = mem->read8bitData(dataAddr);
                    immidiate = true;
                    break;
                }
                case ZERO_PAGE:
                {
                    res = int_to_hex(mem->read8bitData(dataAddr));
                    break;
                }
                case ZERO_PAGE_X:
                {
                    res = int_to_hex(mem->read8bitData(dataAddr)) + ",X";
                    break;
                }
                case ZERO_PAGE_Y:
                {
                    res = int_to_hex(mem->read8bitData(dataAddr)) + ",Y";
                    break;
                }
                case INDEXED_ABSOLUTE:
                {
                    res = int_to_hex(mem->read16bitData(dataAddr));
                    //                    addr = readMem16bit();
                    break;
                }
                case INDEXED_ABSOLUTE_X:
                {
                    res = int_to_hex(mem->read16bitData(dataAddr)) + ",X";
                    break;
                }
                case INDEXED_ABSOLUTE_Y:
                {
                    res = int_to_hex(mem->read16bitData(dataAddr)) + ",Y";
                    break;
                }
                case INDIRECT:
                {
                    res = "(" + int_to_hex(mem->read16bitData(dataAddr)) + ")";
                    break;
                }
                case INDIRECT_X_INDEXED:
                {
                    res = "(" + int_to_hex(mem->read8bitData(dataAddr)) + ", X)";
                    break;
                }
                case INDIRECT_INDEXED_Y:
                {
                    res = "(" + int_to_hex(mem->read8bitData(dataAddr)) + "),Y";
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
                if (cmd == "B")
                {
                    addr = pc + info.bytes;
                }
                
                res = std::string("#") + int_to_hex((uint16_t)(immidiate_value + addr));
            }
            
            return res;
        };
        
        std::string oper = operCode(mode);
        
        return cmd + " " + oper;
        //            log("%s %s\n", cmd.c_str(),  oper.c_str());
    }
    
    // 定义指令长度、CPU周期
    std::map<uint8_t, CmdInfo> CMD_LIST = {

        /* (immidiate) ADC #oper */ {0x69, {"ADC", CF_ADC, DST_REGS_A, IMMIDIATE, PARAM_NONE, 2, 2, 195}},
        /* (zeropage) ADC oper */ {0x65, {"ADC", CF_ADC, DST_REGS_A, ZERO_PAGE, PARAM_NONE, 2, 3, 195}},
        /* (zeropage,X) ADC oper,X */ {0x75, {"ADC", CF_ADC, DST_REGS_A, ZERO_PAGE_X, PARAM_NONE, 2, 4, 195}},
        /* (absolute) ADC oper */ {0x6D, {"ADC", CF_ADC, DST_REGS_A, INDEXED_ABSOLUTE, PARAM_NONE, 3, 4, 195}},
        /* (absolute,X) ADC oper,X */ {0x7D, {"ADC", CF_ADC, DST_REGS_A, INDEXED_ABSOLUTE_X, PARAM_NONE, 3, 4, 195}},
        /* (absolute,Y) ADC oper,Y */ {0x79, {"ADC", CF_ADC, DST_REGS_A, INDEXED_ABSOLUTE_Y, PARAM_NONE, 3, 4, 195}},
        /* ((indirect,X)) ADC (oper,X) */ {0x61, {"ADC", CF_ADC, DST_REGS_A, INDIRECT_X_INDEXED, PARAM_NONE, 2, 6, 195}},
        /* ((indirect),Y) ADC (oper),Y */ {0x71, {"ADC", CF_ADC, DST_REGS_A, INDIRECT_INDEXED_Y, PARAM_NONE, 2, 5, 195}},
        /* (immidiate) AND #oper */ {0x29, {"AND", CF_AND, DST_REGS_A, IMMIDIATE, PARAM_NONE, 2, 2, 130}},
        /* (zeropage) AND oper */ {0x25, {"AND", CF_AND, DST_REGS_A, ZERO_PAGE, PARAM_NONE, 2, 3, 130}},
        /* (zeropage,X) AND oper,X */ {0x35, {"AND", CF_AND, DST_REGS_A, ZERO_PAGE_X, PARAM_NONE, 2, 4, 130}},
        /* (absolute) AND oper */ {0x2D, {"AND", CF_AND, DST_REGS_A, INDEXED_ABSOLUTE, PARAM_NONE, 3, 4, 130}},
        /* (absolute,X) AND oper,X */ {0x3D, {"AND", CF_AND, DST_REGS_A, INDEXED_ABSOLUTE_X, PARAM_NONE, 3, 4, 130}},
        /* (absolute,Y) AND oper,Y */ {0x39, {"AND", CF_AND, DST_REGS_A, INDEXED_ABSOLUTE_Y, PARAM_NONE, 3, 4, 130}},
        /* ((indirect,X)) AND (oper,X) */ {0x21, {"AND", CF_AND, DST_REGS_A, INDIRECT_X_INDEXED, PARAM_NONE, 2, 6, 130}},
        /* ((indirect),Y) AND (oper),Y */ {0x31, {"AND", CF_AND, DST_REGS_A, INDIRECT_INDEXED_Y, PARAM_NONE, 2, 5, 130}},
        /* (accumulator) ASL A */ {0x0A, {"ASL", CF_ASL, DST_M, ACCUMULATOR, PARAM_NONE, 1, 2, 131}},
        /* (zeropage) ASL oper */ {0x06, {"ASL", CF_ASL, DST_M, ZERO_PAGE, PARAM_NONE, 2, 5, 131}},
        /* (zeropage,X) ASL oper,X */ {0x16, {"ASL", CF_ASL, DST_M, ZERO_PAGE_X, PARAM_NONE, 2, 6, 131}},
        /* (absolute) ASL oper */ {0x0E, {"ASL", CF_ASL, DST_M, INDEXED_ABSOLUTE, PARAM_NONE, 3, 6, 131}},
        /* (absolute,X) ASL oper,X */ {0x1E, {"ASL", CF_ASL, DST_M, INDEXED_ABSOLUTE_X, PARAM_NONE, 3, 7, 131}},
        /* (relative) BCC oper */ {0x90, {"BCC", CF_BCC, DST_REGS_P_C, RELATIVE, PARAM_0, 2, 2, 0}},
        /* (relative) BCS oper */ {0xB0, {"BCS", CF_BCS, DST_REGS_P_C, RELATIVE, PARAM_1, 2, 2, 0}},
        /* (relative) BEQ oper */ {0xF0, {"BEQ", CF_BEQ, DST_REGS_P_Z, RELATIVE, PARAM_1, 2, 2, 0}},
        /* (zeropage) BIT oper */ {0x24, {"BIT", CF_BIT, DST_REGS_A, ZERO_PAGE, PARAM_NONE, 2, 3, 2}},
        /* (absolute) BIT oper */ {0x2C, {"BIT", CF_BIT, DST_REGS_A, INDEXED_ABSOLUTE, PARAM_NONE, 3, 4, 2}},
        /* (relative) BNE oper */ {0xD0, {"BNE", CF_BNE, DST_REGS_P_Z, RELATIVE, PARAM_0, 2, 2, 0}},
        /* (relative) BPL oper */ {0x10, {"BPL", CF_BPL, DST_REGS_P_N, RELATIVE, PARAM_0, 2, 2, 0}},
        /* (implied) BRK */ {0x00, {"BRK", CF_BRK, DST_NONE, IMPLIED, PARAM_NONE, 1, 7, 0}},
        /* (implied) CLC */ {0x18, {"CLC", CF_CLC, DST_REGS_P_C, IMPLIED, PARAM_NONE, 1, 2, 0}},
        /* (implied) CLD */ {0xD8, {"CLD", CF_CLD, DST_REGS_P_D, IMPLIED, PARAM_NONE, 1, 2, 0}},
        /* (implied) CLI */ {0x58, {"CLI", CF_CLI, DST_REGS_P_I, IMPLIED, PARAM_NONE, 1, 2, 0}},
        /* (immidiate) CMP #oper */ {0xC9, {"CMP", CF_CMP, DST_REGS_A, IMMIDIATE, PARAM_NONE, 2, 2, 131}},
        /* (zeropage) CMP oper */ {0xC5, {"CMP", CF_CMP, DST_REGS_A, ZERO_PAGE, PARAM_NONE, 2, 3, 131}},
        /* (zeropage,X) CMP oper,X */ {0xD5, {"CMP", CF_CMP, DST_REGS_A, ZERO_PAGE_X, PARAM_NONE, 2, 4, 131}},
        /* (absolute) CMP oper */ {0xCD, {"CMP", CF_CMP, DST_REGS_A, INDEXED_ABSOLUTE, PARAM_NONE, 3, 4, 131}},
        /* (absolute,X) CMP oper,X */ {0xDD, {"CMP", CF_CMP, DST_REGS_A, INDEXED_ABSOLUTE_X, PARAM_NONE, 3, 4, 131}},
        /* (absolute,Y) CMP oper,Y */ {0xD9, {"CMP", CF_CMP, DST_REGS_A, INDEXED_ABSOLUTE_Y, PARAM_NONE, 3, 4, 131}},
        /* ((indirect,X)) CMP (oper,X) */ {0xC1, {"CMP", CF_CMP, DST_REGS_A, INDIRECT_X_INDEXED, PARAM_NONE, 2, 6, 131}},
        /* ((indirect),Y) CMP (oper),Y */ {0xD1, {"CMP", CF_CMP, DST_REGS_A, INDIRECT_INDEXED_Y, PARAM_NONE, 2, 5, 131}},
        /* (immidiate) CPX #oper */ {0xE0, {"CPX", CF_CPX, DST_REGS_X, IMMIDIATE, PARAM_NONE, 2, 2, 131}},
        /* (zeropage) CPX oper */ {0xE4, {"CPX", CF_CPX, DST_REGS_X, ZERO_PAGE, PARAM_NONE, 2, 3, 131}},
        /* (absolute) CPX oper */ {0xEC, {"CPX", CF_CPX, DST_REGS_X, INDEXED_ABSOLUTE, PARAM_NONE, 3, 4, 131}},
        /* (immidiate) CPY #oper */ {0xC0, {"CPY", CF_CPY, DST_REGS_Y, IMMIDIATE, PARAM_NONE, 2, 2, 131}},
        /* (zeropage) CPY oper */ {0xC4, {"CPY", CF_CPY, DST_REGS_Y, ZERO_PAGE, PARAM_NONE, 2, 3, 131}},
        /* (absolute) CPY oper */ {0xCC, {"CPY", CF_CPY, DST_REGS_Y, INDEXED_ABSOLUTE, PARAM_NONE, 3, 4, 131}},
        /* (implied) DEX */ {0xCA, {"DEX", CF_DEX, DST_REGS_X, IMPLIED, PARAM_NONE, 1, 2, 130}},
        /* (implied) DEY */ {0x88, {"DEY", CF_DEY, DST_REGS_Y, IMPLIED, PARAM_NONE, 1, 2, 130}},
        /* (zeropage) INC oper */ {0xE6, {"INC", CF_INC, DST_M, ZERO_PAGE, PARAM_NONE, 2, 5, 130}},
        /* (zeropage,X) INC oper,X */ {0xF6, {"INC", CF_INC, DST_M, ZERO_PAGE_X, PARAM_NONE, 2, 6, 130}},
        /* (absolute) INC oper */ {0xEE, {"INC", CF_INC, DST_M, INDEXED_ABSOLUTE, PARAM_NONE, 3, 6, 130}},
        /* (absolute,X) INC oper,X */ {0xFE, {"INC", CF_INC, DST_M, INDEXED_ABSOLUTE_X, PARAM_NONE, 3, 7, 130}},
        /* (implied) INX */ {0xE8, {"INX", CF_INX, DST_REGS_X, IMPLIED, PARAM_NONE, 1, 2, 130}},
        /* (implied) INY */ {0xC8, {"INY", CF_INY, DST_REGS_Y, IMPLIED, PARAM_NONE, 1, 2, 130}},
        /* (absolute) JMP oper */ {0x4C, {"JMP", CF_JMP, DST_NONE, INDEXED_ABSOLUTE, PARAM_NONE, 3, 3, 0}},
        /* (indirect) JMP (oper) */ {0x6C, {"JMP", CF_JMP, DST_NONE, INDIRECT, PARAM_NONE, 3, 5, 0}},
        /* (absolute) JSR oper */ {0x20, {"JSR", CF_JSR, DST_NONE, INDEXED_ABSOLUTE, PARAM_NONE, 3, 6, 0}},
        /* (immidiate) LDA #oper */ {0xA9, {"LDA", CF_LDA, DST_REGS_A, IMMIDIATE, PARAM_NONE, 2, 2, 130}},
        /* (zeropage) LDA oper */ {0xA5, {"LDA", CF_LDA, DST_REGS_A, ZERO_PAGE, PARAM_NONE, 2, 3, 130}},
        /* (zeropage,X) LDA oper,X */ {0xB5, {"LDA", CF_LDA, DST_REGS_A, ZERO_PAGE_X, PARAM_NONE, 2, 4, 130}},
        /* (absolute) LDA oper */ {0xAD, {"LDA", CF_LDA, DST_REGS_A, INDEXED_ABSOLUTE, PARAM_NONE, 3, 4, 130}},
        /* (absolute,X) LDA oper,X */ {0xBD, {"LDA", CF_LDA, DST_REGS_A, INDEXED_ABSOLUTE_X, PARAM_NONE, 3, 4, 130}},
        /* (absolute,Y) LDA oper,Y */ {0xB9, {"LDA", CF_LDA, DST_REGS_A, INDEXED_ABSOLUTE_Y, PARAM_NONE, 3, 4, 130}},
        /* ((indirect,X)) LDA (oper,X) */ {0xA1, {"LDA", CF_LDA, DST_REGS_A, INDIRECT_X_INDEXED, PARAM_NONE, 2, 6, 130}},
        /* ((indirect),Y) LDA (oper),Y */ {0xB1, {"LDA", CF_LDA, DST_REGS_A, INDIRECT_INDEXED_Y, PARAM_NONE, 2, 5, 130}},
        /* (immidiate) LDX #oper */ {0xA2, {"LDX", CF_LDX, DST_REGS_X, IMMIDIATE, PARAM_NONE, 2, 2, 130}},
        /* (zeropage) LDX oper */ {0xA6, {"LDX", CF_LDX, DST_REGS_X, ZERO_PAGE, PARAM_NONE, 2, 3, 130}},
        /* (zeropage,Y) LDX oper,Y */ {0xB6, {"LDX", CF_LDX, DST_REGS_X, ZERO_PAGE_Y, PARAM_NONE, 2, 4, 130}},
        /* (absolute) LDX oper */ {0xAE, {"LDX", CF_LDX, DST_REGS_X, INDEXED_ABSOLUTE, PARAM_NONE, 3, 4, 130}},
        /* (absolute,Y) LDX oper,Y */ {0xBE, {"LDX", CF_LDX, DST_REGS_X, INDEXED_ABSOLUTE_Y, PARAM_NONE, 3, 4, 130}},
        /* (immidiate) LDY #oper */ {0xA0, {"LDY", CF_LDY, DST_REGS_Y, IMMIDIATE, PARAM_NONE, 2, 2, 130}},
        /* (zeropage) LDY oper */ {0xA4, {"LDY", CF_LDY, DST_REGS_Y, ZERO_PAGE, PARAM_NONE, 2, 3, 130}},
        /* (zeropage,X) LDY oper,X */ {0xB4, {"LDY", CF_LDY, DST_REGS_Y, ZERO_PAGE_X, PARAM_NONE, 2, 4, 130}},
        /* (absolute) LDY oper */ {0xAC, {"LDY", CF_LDY, DST_REGS_Y, INDEXED_ABSOLUTE, PARAM_NONE, 3, 4, 130}},
        /* (absolute,X) LDY oper,X */ {0xBC, {"LDY", CF_LDY, DST_REGS_Y, INDEXED_ABSOLUTE_X, PARAM_NONE, 3, 4, 130}},
        /* (accumulator) LSR A */ {0x4A, {"LSR", CF_LSR, DST_M, ACCUMULATOR, PARAM_NONE, 1, 2, 3}},
        /* (zeropage) LSR oper */ {0x46, {"LSR", CF_LSR, DST_M, ZERO_PAGE, PARAM_NONE, 2, 5, 3}},
        /* (zeropage,X) LSR oper,X */ {0x56, {"LSR", CF_LSR, DST_M, ZERO_PAGE_X, PARAM_NONE, 2, 6, 3}},
        /* (absolute) LSR oper */ {0x4E, {"LSR", CF_LSR, DST_M, INDEXED_ABSOLUTE, PARAM_NONE, 3, 6, 3}},
        /* (absolute,X) LSR oper,X */ {0x5E, {"LSR", CF_LSR, DST_M, INDEXED_ABSOLUTE_X, PARAM_NONE, 3, 7, 3}},
        /* (immidiate) ORA #oper */ {0x09, {"ORA", CF_ORA, DST_REGS_A, IMMIDIATE, PARAM_NONE, 2, 2, 130}},
        /* (zeropage) ORA oper */ {0x05, {"ORA", CF_ORA, DST_REGS_A, ZERO_PAGE, PARAM_NONE, 2, 3, 130}},
        /* (zeropage,X) ORA oper,X */ {0x15, {"ORA", CF_ORA, DST_REGS_A, ZERO_PAGE_X, PARAM_NONE, 2, 4, 130}},
        /* (absolute) ORA oper */ {0x0D, {"ORA", CF_ORA, DST_REGS_A, INDEXED_ABSOLUTE, PARAM_NONE, 3, 4, 130}},
        /* (absolute,X) ORA oper,X */ {0x1D, {"ORA", CF_ORA, DST_REGS_A, INDEXED_ABSOLUTE_X, PARAM_NONE, 3, 4, 130}},
        /* (absolute,Y) ORA oper,Y */ {0x19, {"ORA", CF_ORA, DST_REGS_A, INDEXED_ABSOLUTE_Y, PARAM_NONE, 3, 4, 130}},
        /* ((indirect,X)) ORA (oper,X) */ {0x01, {"ORA", CF_ORA, DST_REGS_A, INDIRECT_X_INDEXED, PARAM_NONE, 2, 6, 130}},
        /* ((indirect),Y) ORA (oper),Y */ {0x11, {"ORA", CF_ORA, DST_REGS_A, INDIRECT_INDEXED_Y, PARAM_NONE, 2, 5, 130}},
        /* (implied) PHA */ {0x48, {"PHA", CF_PHA, DST_REGS_A, IMPLIED, PARAM_NONE, 1, 3, 0}},
        /* (implied) PLA */ {0x68, {"PLA", CF_PLA, DST_REGS_A, IMPLIED, PARAM_NONE, 1, 4, 130}},
        /* (accumulator) ROL A */ {0x2A, {"ROL", CF_ROL, DST_M, ACCUMULATOR, PARAM_NONE, 1, 2, 131}},
        /* (zeropage) ROL oper */ {0x26, {"ROL", CF_ROL, DST_M, ZERO_PAGE, PARAM_NONE, 2, 5, 131}},
        /* (zeropage,X) ROL oper,X */ {0x36, {"ROL", CF_ROL, DST_M, ZERO_PAGE_X, PARAM_NONE, 2, 6, 131}},
        /* (absolute) ROL oper */ {0x2E, {"ROL", CF_ROL, DST_M, INDEXED_ABSOLUTE, PARAM_NONE, 3, 6, 131}},
        /* (absolute,X) ROL oper,X */ {0x3E, {"ROL", CF_ROL, DST_M, INDEXED_ABSOLUTE_X, PARAM_NONE, 3, 7, 131}},
        /* (accumulator) ROR A */ {0x6A, {"ROR", CF_ROR, DST_M, ACCUMULATOR, PARAM_NONE, 1, 2, 131}},
        /* (zeropage) ROR oper */ {0x66, {"ROR", CF_ROR, DST_M, ZERO_PAGE, PARAM_NONE, 2, 5, 131}},
        /* (zeropage,X) ROR oper,X */ {0x76, {"ROR", CF_ROR, DST_M, ZERO_PAGE_X, PARAM_NONE, 2, 6, 131}},
        /* (absolute) ROR oper */ {0x6E, {"ROR", CF_ROR, DST_M, INDEXED_ABSOLUTE, PARAM_NONE, 3, 6, 131}},
        /* (absolute,X) ROR oper,X */ {0x7E, {"ROR", CF_ROR, DST_M, INDEXED_ABSOLUTE_X, PARAM_NONE, 3, 7, 131}},
        /* (implied) RTI */ {0x40, {"RTI", CF_RTI, DST_NONE, IMPLIED, PARAM_NONE, 1, 6, 0}},
        /* (implied) RTS */ {0x60, {"RTS", CF_RTS, DST_NONE, IMPLIED, PARAM_NONE, 1, 6, 0}},
        /* (immidiate) SBC #oper */ {0xE9, {"SBC", CF_SBC, DST_REGS_A, IMMIDIATE, PARAM_NONE, 2, 2, 195}},
        /* (zeropage) SBC oper */ {0xE5, {"SBC", CF_SBC, DST_REGS_A, ZERO_PAGE, PARAM_NONE, 2, 3, 195}},
        /* (zeropage,X) SBC oper,X */ {0xF5, {"SBC", CF_SBC, DST_REGS_A, ZERO_PAGE_X, PARAM_NONE, 2, 4, 195}},
        /* (absolute) SBC oper */ {0xED, {"SBC", CF_SBC, DST_REGS_A, INDEXED_ABSOLUTE, PARAM_NONE, 3, 4, 195}},
        /* (absolute,X) SBC oper,X */ {0xFD, {"SBC", CF_SBC, DST_REGS_A, INDEXED_ABSOLUTE_X, PARAM_NONE, 3, 4, 195}},
        /* (absolute,Y) SBC oper,Y */ {0xF9, {"SBC", CF_SBC, DST_REGS_A, INDEXED_ABSOLUTE_Y, PARAM_NONE, 3, 4, 195}},
        /* ((indirect,X)) SBC (oper,X) */ {0xE1, {"SBC", CF_SBC, DST_REGS_A, INDIRECT_X_INDEXED, PARAM_NONE, 2, 6, 195}},
        /* ((indirect),Y) SBC (oper),Y */ {0xF1, {"SBC", CF_SBC, DST_REGS_A, INDIRECT_INDEXED_Y, PARAM_NONE, 2, 5, 195}},
        /* (implied) SEC */ {0x38, {"SEC", CF_SEC, DST_REGS_P_C, IMPLIED, PARAM_NONE, 1, 2, 0}},
        /* (implied) SED */ {0xF8, {"SED", CF_SED, DST_REGS_P_D, IMPLIED, PARAM_NONE, 1, 2, 0}},
        /* (implied) SEI */ {0x78, {"SEI", CF_SEI, DST_REGS_P_I, IMPLIED, PARAM_NONE, 1, 2, 0}},
        /* (zeropage) STA oper */ {0x85, {"STA", CF_STA, DST_REGS_A, ZERO_PAGE, PARAM_NONE, 2, 3, 0}},
        /* (zeropage,X) STA oper,X */ {0x95, {"STA", CF_STA, DST_REGS_A, ZERO_PAGE_X, PARAM_NONE, 2, 4, 0}},
        /* (absolute) STA oper */ {0x8D, {"STA", CF_STA, DST_REGS_A, INDEXED_ABSOLUTE, PARAM_NONE, 3, 4, 0}},
        /* (absolute,X) STA oper,X */ {0x9D, {"STA", CF_STA, DST_REGS_A, INDEXED_ABSOLUTE_X, PARAM_NONE, 3, 5, 0}},
        /* (absolute,Y) STA oper,Y */ {0x99, {"STA", CF_STA, DST_REGS_A, INDEXED_ABSOLUTE_Y, PARAM_NONE, 3, 5, 0}},
        /* ((indirect,X)) STA (oper,X) */ {0x81, {"STA", CF_STA, DST_REGS_A, INDIRECT_X_INDEXED, PARAM_NONE, 2, 6, 0}},
        /* ((indirect),Y) STA (oper),Y */ {0x91, {"STA", CF_STA, DST_REGS_A, INDIRECT_INDEXED_Y, PARAM_NONE, 2, 6, 0}},
        /* (zeropage) STX oper */ {0x86, {"STX", CF_STX, DST_REGS_X, ZERO_PAGE, PARAM_NONE, 2, 3, 0}},
        /* (zeropage,Y) STX oper,Y */ {0x96, {"STX", CF_STX, DST_REGS_X, ZERO_PAGE_Y, PARAM_NONE, 2, 4, 0}},
        /* (absolute) STX oper */ {0x8E, {"STX", CF_STX, DST_REGS_X, INDEXED_ABSOLUTE, PARAM_NONE, 3, 4, 0}},
        /* (zeropage) STY oper */ {0x84, {"STY", CF_STY, DST_REGS_Y, ZERO_PAGE, PARAM_NONE, 2, 3, 0}},
        /* (zeropage,X) STY oper,X */ {0x94, {"STY", CF_STY, DST_REGS_Y, ZERO_PAGE_X, PARAM_NONE, 2, 4, 0}},
        /* (absolute) STY oper */ {0x8C, {"STY", CF_STY, DST_REGS_Y, INDEXED_ABSOLUTE, PARAM_NONE, 3, 4, 0}},
        /* (implied) TAX */ {0xAA, {"TAX", CF_TAX, DST_REGS_X, IMPLIED, PARAM_REGS_A, 1, 2, 130}},
        /* (implied) TAY */ {0xA8, {"TAY", CF_TAY, DST_REGS_Y, IMPLIED, PARAM_REGS_A, 1, 2, 130}},
        /* (implied) TXA */ {0x8A, {"TXA", CF_TXA, DST_REGS_A, IMPLIED, PARAM_REGS_X, 1, 2, 130}},
        /* (implied) TXS */ {0x9A, {"TXS", CF_TXS, DST_REGS_SP, IMPLIED, PARAM_REGS_X, 1, 2, 130}},
        /* (implied) TYA */ {0x98, {"TYA", CF_TYA, DST_REGS_A, IMPLIED, PARAM_REGS_Y, 1, 2, 130}},
        
    };
    
    static
    std::vector< std::string > split(const std::string& s, const std::string& delim)
    {
        std::vector< std::string > ret;
        
        size_t last = 0;
        size_t index=s.find_first_of(delim,last);
        while (index!=std::string::npos)
        {
            ret.push_back(s.substr(last,index-last));
            last=index+1;
            index=s.find_first_of(delim,last);
        }
        if (index-last>0)
        {
            ret.push_back(s.substr(last,index-last));
        }
        
        return ret;
    }

    // 2A03
    struct CPU {
        
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
        
        bool error; // 错误标记
        
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
            
//            _currentInterruptType = InterruptTypeReset; // 复位中断
            
            interrupts(InterruptTypeReset);
        }
        
        enum InterruptType{
            InterruptTypeNone,
            InterruptTypeIRQs,
            InterruptTypeNMI,
            InterruptTypeReset
        };
        
        std::mutex _mutex;
        
        // 中断
        void interrupts(InterruptType type)
        {
            _mutex.lock();
            {
                bool hasInterrupts = false;
                {
                    // 检查中断是否允许被执行
                    switch(type)
                    {
                        case InterruptTypeNMI:
                        {
                            // NMI中断可以被0x2000的第7位屏蔽，== 0 就是屏蔽，这里不通过read访问
                            if (((bit8*)&_mem->masterData()[0x2000])->get(7) == 0)
                            {
                                break;
                            }
                        }
                        case InterruptTypeReset:
                            // 不可屏蔽中断
                            hasInterrupts = true;
                            break;
                        case InterruptTypeIRQs:
                        {
                            // 如果中断禁止标记为0，才可继续此中断
                            if (regs.P.get(__registers::I) == 0)
                                hasInterrupts = true;
                            break;
                        }
                        default:
                            break;
                    }
                }
                
                if (hasInterrupts)
                    _currentInterruptType = type;
            }
            _mutex.unlock();
        }
        
        int exec()
        {
            if (error)
                return 0;
            
            // 检查中断和处理中断
            _mutex.lock();
            {
                // 处理中断
                if (_currentInterruptType != InterruptTypeNone)
                {
                    // 获取中断处理函数地址
                    uint16_t interruptHandlerAddr;
                    {
                        // 函数地址是一个16bit数值，分别存储在两个8bit的内存上，这里定义一个结构体表示内存地址偏移
                        struct ADDR2{
                            uint16_t low;
                            uint16_t high;
                        };
                        
                        std::map<int, ADDR2> handler = {
                            {InterruptTypeNMI, {0xFFFA, 0xFFFB}},
                            {InterruptTypeReset, {0xFFFC, 0xFFFD}},
                            {InterruptTypeIRQs, {0xFFFE, 0xFFFF}},
                        };
                        
                        ADDR2 addr2 = handler[_currentInterruptType];
                        interruptHandlerAddr = (_mem->read8bitData(addr2.high) << 8) + _mem->read8bitData(addr2.low);
                    }
                    
                    log("中断函数地址: %04X\n", interruptHandlerAddr);
                    
                    // 跳转到中断向量指向的处理函数地址
                    if (interruptHandlerAddr == 0)
                        interruptHandlerAddr = 0x8000;
                        
                    {
                        // Reset 不需要保存现场
                        if (_currentInterruptType != InterruptTypeReset)
                        {
//                            saveStatus(); // 保存现场
                            
                            auto& PC = regs.PC;
                            PC --;
                            push((PC >> 8) & 0xff);    /* Push return address onto the stack. */
                            push(PC & 0xff);
                            push(*(uint8_t*)&regs.P);
                        }
                        
                        // 设置中断屏蔽标志(I)防止新的中断进来
                        regs.P.set(__registers::I, 1);
                        
                        // 取消中断标记
                        _currentInterruptType = InterruptTypeNone;
                        
                        regs.PC = interruptHandlerAddr;
                    }
                }
            }
            _mutex.unlock();
            
            
            
            
            // 从内存里面取出一条8bit指令，将PC移动到下一个内存地址
            uint8_t cmd = _mem->read8bitData(regs.PC);
            
            log("[%ld][%04X] cmd: %x => ", execCmdLine, regs.PC, cmd);
            
            if (!SET_FIND(CMD_LIST, cmd))
            {
                log("未知的指令！");
                error = true;
                return 0;
            }
            
            auto info = CMD_LIST.at(cmd);
            {
                auto dst = info.dst;
                AddressingMode mode = info.mode;
                
                auto str = cmd_str(info, regs.PC, _mem);
                log("%s\n", str.c_str());
                
                {
                  
                    
                    std::function<void(int)> SET_CARRY = [this](int src)
                    {
                        regs.P.set(__registers::C, src != 0 ? 1 : 0);
                    };
                    
                    std::function<void(int)> SET_DECIMAL = [this](int src)
                    {
                        regs.P.set(__registers::D, src != 0 ? 1 : 0);
                    };
                    
                    std::function<void(int)> SET_INTERRUPT = [this](int src)
                    {
                        regs.P.set(__registers::I, src != 0 ? 1 : 0);
                    };
                    
                    std::function<void(int8_t)> SET_SIGN = [this](int8_t src)
                    {
                        regs.P.set(__registers::N, src & 0x80 ? 1 : 0);
                    };
                    
                    std::function<void(int8_t)> SET_ZERO = [this](int8_t src)
                    {
                        regs.P.set(__registers::Z, src == 0 ? 1 : 0);
                    };
                    
                    std::function<void(bool)> SET_OVERFLOW = [this](bool src)
                    {
                        regs.P.set(__registers::V, src);
                    };
                    
                    std::function<void(uint16_t, int8_t)> STORE = [this](uint16_t addr, int8_t src)
                    {
                        _mem->write8bitData(addr, src);
                    };
                    
                    std::function<uint16_t(uint16_t, int8_t)> REL_ADDR = [this](uint16_t addr, int8_t src)
                    {
                        return addr + src;
                    };

                    std::function<bool()> IF_CARRY = [this]()
                    {
                        return regs.P.get(__registers::C);
                    };
                    
                    std::function<bool()> IF_DECIMAL = [this]()
                    {
                        return regs.P.get(__registers::D);
                    };
                    
                    
                    
                    std::function<bool()> IF_ZERO = [this]()
                    {
                        return regs.P.get(__registers::Z);
                    };
                    
                    std::function<bool()> IF_SIGN = [this]()
                    {
                        return regs.P.get(__registers::N);
                    };
                    
                    
                    uint16_t address = 0;
                    int8_t src = 0;
                    {
                        if (mode != IMPLIED)
                        {
                            address = addressingByMode(mode);
                            src = _mem->read8bitData(address, true);
                        }
                    }
                    
                    
                    std::function<void(DST, uint8_t, AddressingMode)> valueToDST = [this, &address](DST dst, uint8_t value, AddressingMode mode)
                    {
                        switch (dst)
                        {
                            case DST_REGS_SP:
                                regs.SP = value;
                                break;
                            case DST_REGS_A:
                                regs.A = value;
                                break;
                            case DST_REGS_X:
                                regs.X = value;
                                break;
                            case DST_REGS_Y:
                                regs.Y = value;
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
                    
                    

                    
                    auto& AC = regs.A;
                    auto& XR = regs.X;
                    auto& YR = regs.Y;
                    auto& SP = regs.SP;
                    auto& PC = regs.PC;
                    
                    
                    
                    // 移动PC指针到下一条指令位置
                    PC += info.bytes;
                    
                    
                    switch(info.cf)
                    {
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
                            SET_CARRY(src & 0x80);
                            src <<= 1;
                            src &= 0xff;
                            SET_SIGN(src);
                            SET_ZERO(src);
                            
                            valueToDST(dst, src, mode);
                            break;
                        }
                        case CF_BCC:
                        {
                            if (!IF_CARRY()) {
//                                clk += ((PC & 0xFF00) != (REL_ADDR(PC, src) & 0xFF00) ? 2 : 1);
                                PC = REL_ADDR(PC, src);
                            }
                            
                            break;
                        }
                        case CF_BCS:
                        {
                            if (IF_CARRY()) {
//                                clk += ((PC & 0xFF00) != (REL_ADDR(PC, src) & 0xFF00) ? 2 : 1);
                                PC = REL_ADDR(PC, src);
                            }
                            
                            break;
                        }
                        case CF_BEQ:
                        {
                            if (IF_ZERO()) {
//                                clk += ((PC & 0xFF00) != (REL_ADDR(PC, src) & 0xFF00) ? 2 : 1);
                                PC = REL_ADDR(PC, src);
                            }
                            
                            break;
                        }
                        case CF_BNE:
                        {
                            if (!IF_ZERO()) {
//                                clk += ((PC & 0xFF00) != (REL_ADDR(PC, src) & 0xFF00) ? 2 : 1);
                                PC = REL_ADDR(PC, src);
                            }
                            
                            break;
                        }
                        case CF_BPL:
                        {
                            if (!IF_SIGN()) {
//                                clk += ((PC & 0xFF00) != (REL_ADDR(PC, src) & 0xFF00) ? 2 : 1);
                                PC = REL_ADDR(PC, src);
                            }
                            
                            break;
                        }
                        case CF_LSR:
                        {
                            SET_CARRY(src & 0x01);
                            src >>= 1;
                            SET_SIGN(src);
                            SET_ZERO(src);
                            
                            valueToDST(dst, src, mode);
                            break;
                        }
                        case CF_ROL:
                        {
                            src <<= 1;
                            if (IF_CARRY()) src |= 0x1;
                            SET_CARRY((src<<1) > 0xff);
                            src &= 0xff;
                            SET_SIGN(src);
                            SET_ZERO(src);
                            
                            valueToDST(dst, src, mode);
                            break;
                        }
                        case CF_ROR:
                        {
                            if (IF_CARRY()) src |= 0x100;
                            SET_CARRY(src & 0x01);
                            src >>= 1;
                            SET_SIGN(src);
                            SET_ZERO(src);
                            
                            valueToDST(dst, src, mode);
                            break;
                        }
                        case CF_ADC:
                        {
                            unsigned int temp = src + AC + (IF_CARRY() ? 1 : 0);
                            SET_ZERO(temp & 0xff);    /* This is not valid in decimal mode */
                            if (IF_DECIMAL()) {
                                if (((AC & 0xf) + (src & 0xf) + (IF_CARRY() ? 1 : 0)) > 9) temp += 6;
                                SET_SIGN(temp);
                                SET_OVERFLOW(!((AC ^ src) & 0x80) && ((AC ^ temp) & 0x80));
                                if (temp > 0x99) temp += 96;
                                SET_CARRY(temp > 0x99);
                            } else {
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
                        case CF_CMP:
                        {
                            src = AC - src;
                            SET_CARRY((AC - src) < 0x100);
                            SET_SIGN(src);
                            SET_ZERO(src &= 0xff);
                            
                            break;
                        }
                        case CF_CPX:
                        {
                            src = XR - src;
                            SET_CARRY((XR - src) < 0x100);
                            SET_SIGN(src);
                            SET_ZERO(src &= 0xff);
                            
                            break;
                        }
                        case CF_CPY:
                        {
                            src = YR - src;
                            SET_CARRY((YR - src) < 0x100);
                            SET_SIGN(src);
                            SET_ZERO(src &= 0xff);
                            
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
                            assert(dst == DST_REGS_A);
                            
                            push(regs.A);
                            break;
                        }
                        case CF_PLA:
                        {
                            assert(dst == DST_REGS_A);
                            
                            src = pop();
                            SET_SIGN(src);    /* Change sign and zero flag accordingly. */
                            SET_ZERO(src);
                            
                            break;
                        }
                        case CF_SBC:
                        {
                            unsigned int temp = AC - src - (IF_CARRY() ? 0 : 1);
                            SET_SIGN(temp);
                            SET_ZERO(temp & 0xff);    /* Sign and Zero are invalid in decimal mode */
                            SET_OVERFLOW(((AC ^ temp) & 0x80) && ((AC ^ src) & 0x80));
                            if (IF_DECIMAL()) {
                                if ( ((AC & 0xf) - (IF_CARRY() ? 0 : 1)) < (src & 0xf)) /* EP */ temp -= 6;
                                if (temp > 0x99) temp -= 0x60;
                            }
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
                            // BRK 执行时，产生BRK中断
                            interrupts(InterruptTypeIRQs);
                            break;
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
                            break;
                        }
                        case CF_RTS:
                        {
                            if (regs.SP <= 0xFD) // 255 - 2
                            {
                                uint8_t PC_low = pop();
                                uint8_t PC_high = pop();
                                PC = (PC_high << 8) + PC_low + 1;
                            }
                            else
                            {
                                assert(!"error!");
                            }
                            break;
                        }
                        case CF_RTI:
                        {
                            if (regs.SP <= 0xFC) // 255-3
                            {
                                uint8_t data = pop();
                                regs.P = *(bit8*)&data;
                                
                                uint8_t PC_low = pop();
                                uint8_t PC_high = pop();
                                
                                regs.PC = (PC_high << 8) + PC_low + 1;
                            }
                            else
                            {
                                assert(!"error!");
                            }
                            
                            break;
                        }
                        
                        default:
                            log("未知的指令！");
                            error = true;
                            break;
                            
                    }
                }
            }
            
            // 检查内存错误
            error = _mem->error;
            
            log("P: %d\n", regs.P);
            
            execCmdLine ++;
            
            return info.cyles;
        }
        
    private:

        uint16_t addressingByMode(AddressingMode mode)
        {
            uint16_t dataAddr = regs.PC + 1; // 操作数位置 = PC + 1
            
            uint16_t addr;
            bool data16bit = false;
            
            //                        uint8_t* data;
            switch (mode)
            {
                    //                case ZERO_PAGE:
                    //                {
                    //                    addr = _mem->read8bitData(dataAddr);
                    //                }
                case RELATIVE:
                case IMMIDIATE:
                {
                    addr = dataAddr;
                    break;
                }
                case ACCUMULATOR:
                {
                    addr = regs.A;
                    break;
                }
                case ZERO_PAGE:
                {
                    addr = _mem->read8bitData(dataAddr);
                    break;
                }
                case ZERO_PAGE_X:
                {
                    addr = _mem->read8bitData(dataAddr) + regs.X;
                    break;
                }
                case ZERO_PAGE_Y:
                {
                    addr = _mem->read8bitData(dataAddr) + regs.Y;
                    break;
                }
                case INDEXED_ABSOLUTE:
                {
                    addr = _mem->read16bitData(dataAddr);
                    break;
                }
                case INDEXED_ABSOLUTE_X:
                {
                    addr = _mem->read16bitData(dataAddr) + regs.X;
                    break;
                }
                case INDEXED_ABSOLUTE_Y:
                {
                    addr = _mem->read16bitData(dataAddr) + regs.Y;
                    break;
                }
                case INDIRECT:
                {
                    addr = _mem->read16bitData(_mem->read16bitData(dataAddr));
                    break;
                }
                case INDIRECT_X_INDEXED:
                {
                    addr = _mem->read16bitData(_mem->read8bitData(dataAddr) + regs.X);
                    break;
                }
                case INDIRECT_INDEXED_Y:
                {
                    addr = _mem->read16bitData(_mem->read8bitData(dataAddr)) + regs.Y;
                    break;
                }
                default:
                    assert(!"error!");
                    break;
            }
            
            if (dataAddr == addr)
            {
                uint8_t* d = &_mem->masterData()[addr];
                int dd;
                if (data16bit)
                {
                    dd = (int)*((uint16_t*)d);
                }
                else
                {
                    dd = (int)*d;
                }
                
                log("立即数 %X 值 %d\n", addr, dd);
            }
            else
            {
                log("间接寻址 %X 值 %d\n", addr, (int)_mem->masterData()[addr]);
            }
            
            return addr;
        }
        
        void push(uint8_t value)
        {
            uint16_t stack_addr = STACK_ADDR_OFFSET + regs.SP;
            _mem->write8bitData(stack_addr, value);
            regs.SP --;
        }
        
        // 出栈
        uint8_t pop()
        {
            regs.SP ++;
            return _mem->read8bitData(STACK_ADDR_OFFSET + regs.SP);
        }
        

        
        
        Memory* _mem;
        
        
        InterruptType _currentInterruptType;
        

        #define SET_FIND(s, v) (s.find(v) != s.end())

   
        
    };
    
    
}
