
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
        
        
        INDIRECT_INDEXED,   // (oper)
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
        CF_BRK,
        CF_LD,
        CF_ST,
        CF_IN,
        CF_CP,
        CF_B,
        CF_RTI,
        CF_JSR,
        CF_RTS,
        CF_AND,
        CF_JMP,
        
        CF_SE,
        CF_CL,
        CF_T,
        
        CF_DE,
        CF_SBC,
        CF_ADC,
        
        CF_BIT,
        
        CF_OR,
        
        CF_ASL, // 累加器寻址，算数左移
        CF_LSR,
        
        CF_PH,
        CF_PL,
        
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
                case INDIRECT_INDEXED:
                {
                    res = "(" + int_to_hex(mem->read8bitData(dataAddr)) + ")";
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
#if 0
        /* BRK */ {0x00, {"BRK", CF_BRK, DST_NONE, IMPLIED, PARAM_NONE, 1, 7}},
        /* LDA #oper */ {0xA9, {"LDA", CF_LD, DST_REGS_A, IMMIDIATE, PARAM_NONE, 2, 2}},
        /* LDA oper */  {0xAD, {"LDA", CF_LD, DST_REGS_A, INDEXED_ABSOLUTE, PARAM_NONE, 3, 4}},
        /* LDA oper,X*/ {0xBD, {"LDA", CF_LD, DST_REGS_A, INDEXED_ABSOLUTE_X, PARAM_NONE, 3, 4}},
        /* LDA (oper),Y */ {0xB1, {"LDA", CF_LD, DST_REGS_A, INDIRECT_INDEXED_Y, PARAM_NONE, 2, 5}},
        
        
        /* LDX #oper */ {0xA2, {"LDX", CF_LD, DST_REGS_X, IMMIDIATE, PARAM_NONE, 2, 2}},
        /* LDX oper */ {0xAE, {"LDX", CF_LD, DST_REGS_X, INDEXED_ABSOLUTE, PARAM_NONE, 3, 4}},
        
        /* LDY #oper */ {0xA0, {"LDY", CF_LD, DST_REGS_Y, IMMIDIATE, PARAM_NONE, 2, 2}},
        /* LDY oper */ {0xAC, {"LDY", CF_LD, DST_REGS_Y, INDEXED_ABSOLUTE, PARAM_NONE, 3, 4}},
        
        
        /* STA oper  */ {0x85, {"STA", CF_ST, DST_REGS_A, ZERO_PAGE, PARAM_NONE, 2, 3}},
        /* STA oper,X  */ {0x95, {"STA", CF_ST, DST_REGS_A, ZERO_PAGE_X, PARAM_NONE, 2, 4}},
        /* STA oper  */ {0x8D, {"STA", CF_ST, DST_REGS_A, INDEXED_ABSOLUTE, PARAM_NONE, 3, 4}},
        /* STA oper,X  */ {0x9D, {"STA", CF_ST, DST_REGS_A, INDEXED_ABSOLUTE_X, PARAM_NONE, 3, 5}},
        /* STA oper,Y  */ {0x99, {"STA", CF_ST, DST_REGS_A, INDEXED_ABSOLUTE_Y, PARAM_NONE, 3, 5}},
        /* STA (oper),Y  */ {0x91, {"STA", CF_ST, DST_REGS_A, INDIRECT_INDEXED_Y, PARAM_NONE, 2, 6}},
        
        /* STX oper  */ {0x86, {"STX", CF_ST, DST_REGS_X, ZERO_PAGE, PARAM_NONE, 2, 3}},
        /* STX oper,Y  */ {0x96, {"STX", CF_ST, DST_REGS_X, ZERO_PAGE_Y, PARAM_NONE, 2, 4}},
        /* STX oper  */ {0x8E, {"STX", CF_ST, DST_REGS_X, INDEXED_ABSOLUTE, PARAM_NONE, 3, 4}},
        
        /* STY oper  */ {0x8C, {"STY", CF_ST, DST_REGS_Y, INDEXED_ABSOLUTE, PARAM_NONE, 3, 4}},
        
        
        /* INX */      {0xE8, {"INX", CF_IN, DST_REGS_X, IMPLIED, PARAM_NONE, 1, 2}},
        /* INY */      {0xC8, {"INY", CF_IN, DST_REGS_Y, IMPLIED, PARAM_NONE, 1, 2}},
        
        /* INC oper */  {0xE6, {"INC", CF_IN, DST_M, ZERO_PAGE, PARAM_NONE, 2, 5}},
        /* INC oper */  {0xEE, {"INC", CF_IN, DST_M, INDEXED_ABSOLUTE, PARAM_NONE, 3, 6}},
        
        /* DEX */      {0xCA, {"DEX", CF_DE, DST_REGS_X, IMPLIED, PARAM_NONE, 1, 2}},
        /* DEY */      {0x88, {"DEY", CF_DE, DST_REGS_Y, IMPLIED, PARAM_NONE, 1, 2}},
        /* SBC */      {0xE9, {"SBC", CF_SBC, DST_REGS_A, IMMIDIATE, PARAM_NONE, 2, 2}},
        
        /* ADC #oper */ {0x69, {"ADC", CF_ADC, DST_REGS_A, IMMIDIATE, PARAM_NONE, 2, 2}},
        /* ADC oper */  {0x65, {"ADC", CF_ADC, DST_REGS_A, ZERO_PAGE, PARAM_NONE, 2, 3}},
        
        /* CMP #oper */ {0xC9, {"CMP", CF_CP, DST_REGS_A, IMMIDIATE, PARAM_NONE, 2, 2}},
        /* CPX #oper */ {0xE0, {"CPX", CF_CP, DST_REGS_X, IMMIDIATE, PARAM_NONE, 2, 2}},
        /* CPY #oper */ {0xC0, {"CPY", CF_CP, DST_REGS_Y, IMMIDIATE, PARAM_NONE, 2, 2}},
        
        /* BNE oper */  {0xD0, {"BNE", CF_B, DST_REGS_P_Z, RELATIVE, PARAM_0, 2, 2}},
        /* BPL oper */  {0x10, {"BPL", CF_B, DST_REGS_P_N, RELATIVE, PARAM_0, 2, 2}},
        /* BCS oper */  {0xB0, {"BCS", CF_B, DST_REGS_P_C, RELATIVE, PARAM_1, 2, 2}},
        /* BEQ oper */  {0xF0, {"BEQ", CF_B, DST_REGS_P_Z, RELATIVE, PARAM_1, 2, 2}},
        
        /* RTI */      {0x40, {"RTI", CF_RTI, DST_NONE, IMPLIED, PARAM_NONE, 1, 6}},
        /* JSR oper */  {0x20, {"JSR", CF_JSR, DST_NONE, INDEXED_ABSOLUTE, PARAM_NONE, 3, 6}},
        /* RTS */      {0x60, {"RTS", CF_RTS, DST_NONE, IMPLIED, PARAM_NONE, 1, 6}},
        /* AND #oper */ {0x29, {"AND", CF_AND, DST_REGS_A, IMMIDIATE, PARAM_NONE, 2, 2}},
        /* JMP oper */ {0x4C, {"JMP", CF_JMP, DST_NONE, INDEXED_ABSOLUTE, PARAM_NONE, 3, 3}},
        
        /* SEC */ {0x38, {"SEC", CF_SE, DST_REGS_P_C, IMPLIED, PARAM_NONE, 1, 2}},
        /* SED */ {0xF8, {"SED", CF_SE, DST_REGS_P_D, IMPLIED, PARAM_NONE, 1, 2}},
        /* SEI */ {0x78, {"SEI", CF_SE, DST_REGS_P_I, IMPLIED, PARAM_NONE, 1, 2}},
        /* CLC */ {0x18, {"CLC", CF_CL, DST_REGS_P_C, IMPLIED, PARAM_NONE, 1, 2}},
        /* CLD */ {0xD8, {"CLD", CF_CL, DST_REGS_P_D, IMPLIED, PARAM_NONE, 1, 2}},
        /* CLI */ {0x58, {"CLI", CF_CL, DST_REGS_P_I, IMPLIED, PARAM_NONE, 1, 2}},
        
        /* TXS */ {0x9A, {"TXS", CF_T, DST_REGS_SP, IMPLIED, PARAM_REGS_X, 1, 2}},
        /* TXA */ {0x8A, {"TXA", CF_T, DST_REGS_A, IMPLIED, PARAM_REGS_X, 1, 2}},
        /* TAX */ {0xAA, {"TAX", CF_T, DST_REGS_X, IMPLIED, PARAM_REGS_A, 1, 2}},
        /* TYA */ {0x98, {"TYA", CF_T, DST_REGS_A, IMPLIED, PARAM_REGS_Y, 1, 2}},
        
        /* BIT oper */ {0x2C, {"BIT", CF_BIT, DST_REGS_P_Z, INDEXED_ABSOLUTE, PARAM_NONE, 3, 4}},
        
        /* ORA #oper */ {0x09, {"ORA", CF_OR, DST_REGS_A, IMMIDIATE, PARAM_NONE, 2, 2}},
        
        /* ASL A */ {0x0A, {"ASL", CF_ASL, DST_REGS_A, ACCUMULATOR, PARAM_NONE, 1, 2}},
        /* LSR A */ {0x4A, {"LSR", CF_LSR, DST_REGS_A, ACCUMULATOR, PARAM_NONE, 1, 2}},
        
        /* PHA */ {0x48, {"PHA", CF_PH, DST_REGS_A, IMPLIED, PARAM_NONE, 1, 3}},
        /* PLA */ {0x68, {"PLA", CF_PL, DST_REGS_A, IMPLIED, PARAM_NONE, 1, 4}},
        
        /* BCC oper */ {0x90, {"BCC", CF_B, DST_REGS_P_C, RELATIVE, PARAM_0, 2, 2}},
#else
        /* (immidiate) ADC #oper */ {0x69, {"ADC", CF_ADC, DST_REGS_A, IMMIDIATE, PARAM_NONE, 2, 2}},
        /* (zeropage) ADC oper */ {0x65, {"ADC", CF_ADC, DST_REGS_A, ZERO_PAGE, PARAM_NONE, 2, 3}},
        /* (zeropage,X) ADC oper,X */ {0x75, {"ADC", CF_ADC, DST_REGS_A, ZERO_PAGE_X, PARAM_NONE, 2, 4}},
        /* (absolute) ADC oper */ {0x6D, {"ADC", CF_ADC, DST_REGS_A, INDEXED_ABSOLUTE, PARAM_NONE, 3, 4}},
        /* (absolute,X) ADC oper,X */ {0x7D, {"ADC", CF_ADC, DST_REGS_A, INDEXED_ABSOLUTE_X, PARAM_NONE, 3, 4}},
        /* (absolute,Y) ADC oper,Y */ {0x79, {"ADC", CF_ADC, DST_REGS_A, INDEXED_ABSOLUTE_Y, PARAM_NONE, 3, 4}},
        /* ((indirect,X)) ADC (oper,X) */ {0x61, {"ADC", CF_ADC, DST_REGS_A, INDIRECT_X_INDEXED, PARAM_NONE, 2, 6}},
        /* ((indirect),Y) ADC (oper),Y */ {0x71, {"ADC", CF_ADC, DST_REGS_A, INDIRECT_INDEXED_Y, PARAM_NONE, 2, 5}},
        /* (immidiate) AND #oper */ {0x29, {"AND", CF_AND, DST_REGS_A, IMMIDIATE, PARAM_NONE, 2, 2}},
        /* (zeropage) AND oper */ {0x25, {"AND", CF_AND, DST_REGS_A, ZERO_PAGE, PARAM_NONE, 2, 3}},
        /* (zeropage,X) AND oper,X */ {0x35, {"AND", CF_AND, DST_REGS_A, ZERO_PAGE_X, PARAM_NONE, 2, 4}},
        /* (absolute) AND oper */ {0x2D, {"AND", CF_AND, DST_REGS_A, INDEXED_ABSOLUTE, PARAM_NONE, 3, 4}},
        /* (absolute,X) AND oper,X */ {0x3D, {"AND", CF_AND, DST_REGS_A, INDEXED_ABSOLUTE_X, PARAM_NONE, 3, 4}},
        /* (absolute,Y) AND oper,Y */ {0x39, {"AND", CF_AND, DST_REGS_A, INDEXED_ABSOLUTE_Y, PARAM_NONE, 3, 4}},
        /* ((indirect,X)) AND (oper,X) */ {0x21, {"AND", CF_AND, DST_REGS_A, INDIRECT_X_INDEXED, PARAM_NONE, 2, 6}},
        /* ((indirect),Y) AND (oper),Y */ {0x31, {"AND", CF_AND, DST_REGS_A, INDIRECT_INDEXED_Y, PARAM_NONE, 2, 5}},
        /* (accumulator) ASL A */ {0x0A, {"ASL", CF_ASL, DST_REGS_A, ACCUMULATOR, PARAM_NONE, 1, 2}},
        /* (zeropage) ASL oper */ {0x06, {"ASL", CF_ASL, DST_REGS_A, ZERO_PAGE, PARAM_NONE, 2, 5}},
        /* (zeropage,X) ASL oper,X */ {0x16, {"ASL", CF_ASL, DST_REGS_A, ZERO_PAGE_X, PARAM_NONE, 2, 6}},
        /* (absolute) ASL oper */ {0x0E, {"ASL", CF_ASL, DST_REGS_A, INDEXED_ABSOLUTE, PARAM_NONE, 3, 6}},
        /* (absolute,X) ASL oper,X */ {0x1E, {"ASL", CF_ASL, DST_REGS_A, INDEXED_ABSOLUTE_X, PARAM_NONE, 3, 7}},
        /* (relative) BCC oper */ {0x90, {"BCC", CF_B, DST_REGS_P_C, RELATIVE, PARAM_0, 2, 2}},
        /* (relative) BCS oper */ {0xB0, {"BCS", CF_B, DST_REGS_P_C, RELATIVE, PARAM_1, 2, 2}},
        /* (relative) BEQ oper */ {0xF0, {"BEQ", CF_B, DST_REGS_P_Z, RELATIVE, PARAM_1, 2, 2}},
        /* (zeropage) BIT oper */ {0x24, {"BIT", CF_BIT, DST_REGS_P_Z, ZERO_PAGE, PARAM_NONE, 2, 3}},
        /* (absolute) BIT oper */ {0x2C, {"BIT", CF_BIT, DST_REGS_P_Z, INDEXED_ABSOLUTE, PARAM_NONE, 3, 4}},
        /* (relative) BNE oper */ {0xD0, {"BNE", CF_B, DST_REGS_P_Z, RELATIVE, PARAM_0, 2, 2}},
        /* (relative) BPL oper */ {0x10, {"BPL", CF_B, DST_REGS_P_N, RELATIVE, PARAM_0, 2, 2}},
        /* (implied) BRK */ {0x00, {"BRK", CF_BRK, DST_NONE, IMPLIED, PARAM_NONE, 1, 7}},
        /* (implied) CLC */ {0x18, {"CLC", CF_CL, DST_REGS_P_C, IMPLIED, PARAM_NONE, 1, 2}},
        /* (implied) CLD */ {0xD8, {"CLD", CF_CL, DST_REGS_P_D, IMPLIED, PARAM_NONE, 1, 2}},
        /* (implied) CLI */ {0x58, {"CLI", CF_CL, DST_REGS_P_I, IMPLIED, PARAM_NONE, 1, 2}},
        /* (immidiate) CMP #oper */ {0xC9, {"CMP", CF_CP, DST_REGS_A, IMMIDIATE, PARAM_NONE, 2, 2}},
        /* (zeropage) CMP oper */ {0xC5, {"CMP", CF_CP, DST_REGS_A, ZERO_PAGE, PARAM_NONE, 2, 3}},
        /* (zeropage,X) CMP oper,X */ {0xD5, {"CMP", CF_CP, DST_REGS_A, ZERO_PAGE_X, PARAM_NONE, 2, 4}},
        /* (absolute) CMP oper */ {0xCD, {"CMP", CF_CP, DST_REGS_A, INDEXED_ABSOLUTE, PARAM_NONE, 3, 4}},
        /* (absolute,X) CMP oper,X */ {0xDD, {"CMP", CF_CP, DST_REGS_A, INDEXED_ABSOLUTE_X, PARAM_NONE, 3, 4}},
        /* (absolute,Y) CMP oper,Y */ {0xD9, {"CMP", CF_CP, DST_REGS_A, INDEXED_ABSOLUTE_Y, PARAM_NONE, 3, 4}},
        /* ((indirect,X)) CMP (oper,X) */ {0xC1, {"CMP", CF_CP, DST_REGS_A, INDIRECT_X_INDEXED, PARAM_NONE, 2, 6}},
        /* ((indirect),Y) CMP (oper),Y */ {0xD1, {"CMP", CF_CP, DST_REGS_A, INDIRECT_INDEXED_Y, PARAM_NONE, 2, 5}},
        /* (immidiate) CPX #oper */ {0xE0, {"CPX", CF_CP, DST_REGS_X, IMMIDIATE, PARAM_NONE, 2, 2}},
        /* (zeropage) CPX oper */ {0xE4, {"CPX", CF_CP, DST_REGS_X, ZERO_PAGE, PARAM_NONE, 2, 3}},
        /* (absolute) CPX oper */ {0xEC, {"CPX", CF_CP, DST_REGS_X, INDEXED_ABSOLUTE, PARAM_NONE, 3, 4}},
        /* (immidiate) CPY #oper */ {0xC0, {"CPY", CF_CP, DST_REGS_Y, IMMIDIATE, PARAM_NONE, 2, 2}},
        /* (zeropage) CPY oper */ {0xC4, {"CPY", CF_CP, DST_REGS_Y, ZERO_PAGE, PARAM_NONE, 2, 3}},
        /* (absolute) CPY oper */ {0xCC, {"CPY", CF_CP, DST_REGS_Y, INDEXED_ABSOLUTE, PARAM_NONE, 3, 4}},
        /* (implied) DEX */ {0xCA, {"DEX", CF_DE, DST_REGS_X, IMPLIED, PARAM_NONE, 1, 2}},
        /* (implied) DEY */ {0x88, {"DEY", CF_DE, DST_REGS_Y, IMPLIED, PARAM_NONE, 1, 2}},
        /* (zeropage) INC oper */ {0xE6, {"INC", CF_IN, DST_M, ZERO_PAGE, PARAM_NONE, 2, 5}},
        /* (zeropage,X) INC oper,X */ {0xF6, {"INC", CF_IN, DST_M, ZERO_PAGE_X, PARAM_NONE, 2, 6}},
        /* (absolute) INC oper */ {0xEE, {"INC", CF_IN, DST_M, INDEXED_ABSOLUTE, PARAM_NONE, 3, 6}},
        /* (absolute,X) INC oper,X */ {0xFE, {"INC", CF_IN, DST_M, INDEXED_ABSOLUTE_X, PARAM_NONE, 3, 7}},
        /* (implied) INX */ {0xE8, {"INX", CF_IN, DST_REGS_X, IMPLIED, PARAM_NONE, 1, 2}},
        /* (implied) INY */ {0xC8, {"INY", CF_IN, DST_REGS_Y, IMPLIED, PARAM_NONE, 1, 2}},
        /* (absolute) JMP oper */ {0x4C, {"JMP", CF_JMP, DST_NONE, INDEXED_ABSOLUTE, PARAM_NONE, 3, 3}},
        /* (indirect) JMP (oper) */ {0x6C, {"JMP", CF_JMP, DST_NONE, INDIRECT_INDEXED, PARAM_NONE, 3, 5}},
        /* (absolute) JSR oper */ {0x20, {"JSR", CF_JSR, DST_NONE, INDEXED_ABSOLUTE, PARAM_NONE, 3, 6}},
        /* (immidiate) LDA #oper */ {0xA9, {"LDA", CF_LD, DST_REGS_A, IMMIDIATE, PARAM_NONE, 2, 2}},
        /* (zeropage) LDA oper */ {0xA5, {"LDA", CF_LD, DST_REGS_A, ZERO_PAGE, PARAM_NONE, 2, 3}},
        /* (zeropage,X) LDA oper,X */ {0xB5, {"LDA", CF_LD, DST_REGS_A, ZERO_PAGE_X, PARAM_NONE, 2, 4}},
        /* (absolute) LDA oper */ {0xAD, {"LDA", CF_LD, DST_REGS_A, INDEXED_ABSOLUTE, PARAM_NONE, 3, 4}},
        /* (absolute,X) LDA oper,X */ {0xBD, {"LDA", CF_LD, DST_REGS_A, INDEXED_ABSOLUTE_X, PARAM_NONE, 3, 4}},
        /* (absolute,Y) LDA oper,Y */ {0xB9, {"LDA", CF_LD, DST_REGS_A, INDEXED_ABSOLUTE_Y, PARAM_NONE, 3, 4}},
        /* ((indirect,X)) LDA (oper,X) */ {0xA1, {"LDA", CF_LD, DST_REGS_A, INDIRECT_X_INDEXED, PARAM_NONE, 2, 6}},
        /* ((indirect),Y) LDA (oper),Y */ {0xB1, {"LDA", CF_LD, DST_REGS_A, INDIRECT_INDEXED_Y, PARAM_NONE, 2, 5}},
        /* (immidiate) LDX #oper */ {0xA2, {"LDX", CF_LD, DST_REGS_X, IMMIDIATE, PARAM_NONE, 2, 2}},
        /* (zeropage) LDX oper */ {0xA6, {"LDX", CF_LD, DST_REGS_X, ZERO_PAGE, PARAM_NONE, 2, 3}},
        /* (zeropage,Y) LDX oper,Y */ {0xB6, {"LDX", CF_LD, DST_REGS_X, ZERO_PAGE_Y, PARAM_NONE, 2, 4}},
        /* (absolute) LDX oper */ {0xAE, {"LDX", CF_LD, DST_REGS_X, INDEXED_ABSOLUTE, PARAM_NONE, 3, 4}},
        /* (absolute,Y) LDX oper,Y */ {0xBE, {"LDX", CF_LD, DST_REGS_X, INDEXED_ABSOLUTE_Y, PARAM_NONE, 3, 4}},
        /* (immidiate) LDY #oper */ {0xA0, {"LDY", CF_LD, DST_REGS_Y, IMMIDIATE, PARAM_NONE, 2, 2}},
        /* (zeropage) LDY oper */ {0xA4, {"LDY", CF_LD, DST_REGS_Y, ZERO_PAGE, PARAM_NONE, 2, 3}},
        /* (zeropage,X) LDY oper,X */ {0xB4, {"LDY", CF_LD, DST_REGS_Y, ZERO_PAGE_X, PARAM_NONE, 2, 4}},
        /* (absolute) LDY oper */ {0xAC, {"LDY", CF_LD, DST_REGS_Y, INDEXED_ABSOLUTE, PARAM_NONE, 3, 4}},
        /* (absolute,X) LDY oper,X */ {0xBC, {"LDY", CF_LD, DST_REGS_Y, INDEXED_ABSOLUTE_X, PARAM_NONE, 3, 4}},
        /* (accumulator) LSR A */ {0x4A, {"LSR", CF_LSR, DST_REGS_A, ACCUMULATOR, PARAM_NONE, 1, 2}},
        /* (zeropage) LSR oper */ {0x46, {"LSR", CF_LSR, DST_REGS_A, ZERO_PAGE, PARAM_NONE, 2, 5}},
        /* (zeropage,X) LSR oper,X */ {0x56, {"LSR", CF_LSR, DST_REGS_A, ZERO_PAGE_X, PARAM_NONE, 2, 6}},
        /* (absolute) LSR oper */ {0x4E, {"LSR", CF_LSR, DST_REGS_A, INDEXED_ABSOLUTE, PARAM_NONE, 3, 6}},
        /* (absolute,X) LSR oper,X */ {0x5E, {"LSR", CF_LSR, DST_REGS_A, INDEXED_ABSOLUTE_X, PARAM_NONE, 3, 7}},
        /* (immidiate) ORA #oper */ {0x09, {"ORA", CF_OR, DST_REGS_A, IMMIDIATE, PARAM_NONE, 2, 2}},
        /* (zeropage) ORA oper */ {0x05, {"ORA", CF_OR, DST_REGS_A, ZERO_PAGE, PARAM_NONE, 2, 3}},
        /* (zeropage,X) ORA oper,X */ {0x15, {"ORA", CF_OR, DST_REGS_A, ZERO_PAGE_X, PARAM_NONE, 2, 4}},
        /* (absolute) ORA oper */ {0x0D, {"ORA", CF_OR, DST_REGS_A, INDEXED_ABSOLUTE, PARAM_NONE, 3, 4}},
        /* (absolute,X) ORA oper,X */ {0x1D, {"ORA", CF_OR, DST_REGS_A, INDEXED_ABSOLUTE_X, PARAM_NONE, 3, 4}},
        /* (absolute,Y) ORA oper,Y */ {0x19, {"ORA", CF_OR, DST_REGS_A, INDEXED_ABSOLUTE_Y, PARAM_NONE, 3, 4}},
        /* ((indirect,X)) ORA (oper,X) */ {0x01, {"ORA", CF_OR, DST_REGS_A, INDIRECT_X_INDEXED, PARAM_NONE, 2, 6}},
        /* ((indirect),Y) ORA (oper),Y */ {0x11, {"ORA", CF_OR, DST_REGS_A, INDIRECT_INDEXED_Y, PARAM_NONE, 2, 5}},
        /* (implied) PHA */ {0x48, {"PHA", CF_PH, DST_REGS_A, IMPLIED, PARAM_NONE, 1, 3}},
        /* (implied) PLA */ {0x68, {"PLA", CF_PL, DST_REGS_A, IMPLIED, PARAM_NONE, 1, 4}},
        /* (implied) RTI */ {0x40, {"RTI", CF_RTI, DST_NONE, IMPLIED, PARAM_NONE, 1, 6}},
        /* (implied) RTS */ {0x60, {"RTS", CF_RTS, DST_NONE, IMPLIED, PARAM_NONE, 1, 6}},
        /* (immidiate) SBC #oper */ {0xE9, {"SBC", CF_SBC, DST_REGS_A, IMMIDIATE, PARAM_NONE, 2, 2}},
        /* (zeropage) SBC oper */ {0xE5, {"SBC", CF_SBC, DST_REGS_A, ZERO_PAGE, PARAM_NONE, 2, 3}},
        /* (zeropage,X) SBC oper,X */ {0xF5, {"SBC", CF_SBC, DST_REGS_A, ZERO_PAGE_X, PARAM_NONE, 2, 4}},
        /* (absolute) SBC oper */ {0xED, {"SBC", CF_SBC, DST_REGS_A, INDEXED_ABSOLUTE, PARAM_NONE, 3, 4}},
        /* (absolute,X) SBC oper,X */ {0xFD, {"SBC", CF_SBC, DST_REGS_A, INDEXED_ABSOLUTE_X, PARAM_NONE, 3, 4}},
        /* (absolute,Y) SBC oper,Y */ {0xF9, {"SBC", CF_SBC, DST_REGS_A, INDEXED_ABSOLUTE_Y, PARAM_NONE, 3, 4}},
        /* ((indirect,X)) SBC (oper,X) */ {0xE1, {"SBC", CF_SBC, DST_REGS_A, INDIRECT_X_INDEXED, PARAM_NONE, 2, 6}},
        /* ((indirect),Y) SBC (oper),Y */ {0xF1, {"SBC", CF_SBC, DST_REGS_A, INDIRECT_INDEXED_Y, PARAM_NONE, 2, 5}},
        /* (implied) SEC */ {0x38, {"SEC", CF_SE, DST_REGS_P_C, IMPLIED, PARAM_NONE, 1, 2}},
        /* (implied) SED */ {0xF8, {"SED", CF_SE, DST_REGS_P_D, IMPLIED, PARAM_NONE, 1, 2}},
        /* (implied) SEI */ {0x78, {"SEI", CF_SE, DST_REGS_P_I, IMPLIED, PARAM_NONE, 1, 2}},
        /* (zeropage) STA oper */ {0x85, {"STA", CF_ST, DST_REGS_A, ZERO_PAGE, PARAM_NONE, 2, 3}},
        /* (zeropage,X) STA oper,X */ {0x95, {"STA", CF_ST, DST_REGS_A, ZERO_PAGE_X, PARAM_NONE, 2, 4}},
        /* (absolute) STA oper */ {0x8D, {"STA", CF_ST, DST_REGS_A, INDEXED_ABSOLUTE, PARAM_NONE, 3, 4}},
        /* (absolute,X) STA oper,X */ {0x9D, {"STA", CF_ST, DST_REGS_A, INDEXED_ABSOLUTE_X, PARAM_NONE, 3, 5}},
        /* (absolute,Y) STA oper,Y */ {0x99, {"STA", CF_ST, DST_REGS_A, INDEXED_ABSOLUTE_Y, PARAM_NONE, 3, 5}},
        /* ((indirect,X)) STA (oper,X) */ {0x81, {"STA", CF_ST, DST_REGS_A, INDIRECT_X_INDEXED, PARAM_NONE, 2, 6}},
        /* ((indirect),Y) STA (oper),Y */ {0x91, {"STA", CF_ST, DST_REGS_A, INDIRECT_INDEXED_Y, PARAM_NONE, 2, 6}},
        /* (zeropage) STX oper */ {0x86, {"STX", CF_ST, DST_REGS_X, ZERO_PAGE, PARAM_NONE, 2, 3}},
        /* (zeropage,Y) STX oper,Y */ {0x96, {"STX", CF_ST, DST_REGS_X, ZERO_PAGE_Y, PARAM_NONE, 2, 4}},
        /* (absolute) STX oper */ {0x8E, {"STX", CF_ST, DST_REGS_X, INDEXED_ABSOLUTE, PARAM_NONE, 3, 4}},
        /* (zeropage) STY oper */ {0x84, {"STY", CF_ST, DST_REGS_Y, ZERO_PAGE, PARAM_NONE, 2, 3}},
        /* (zeropage,X) STY oper,X */ {0x94, {"STY", CF_ST, DST_REGS_Y, ZERO_PAGE_X, PARAM_NONE, 2, 4}},
        /* (absolute) STY oper */ {0x8C, {"STY", CF_ST, DST_REGS_Y, INDEXED_ABSOLUTE, PARAM_NONE, 3, 4}},
        /* (implied) TAX */ {0xAA, {"TAX", CF_T, DST_REGS_X, IMPLIED, PARAM_REGS_A, 1, 2}},
        /* (implied) TXA */ {0x8A, {"TXA", CF_T, DST_REGS_A, IMPLIED, PARAM_REGS_X, 1, 2}},
        /* (implied) TXS */ {0x9A, {"TXS", CF_T, DST_REGS_SP, IMPLIED, PARAM_REGS_X, 1, 2}},
        /* (implied) TYA */ {0x98, {"TYA", CF_T, DST_REGS_A, IMPLIED, PARAM_REGS_Y, 1, 2}},
#endif
        
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
                            saveStatus(); // 保存现场
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
            
            int jumpPC = 0;
            
//            if (CMD_LIST.size() == 0)
//            {
//                
//            }

            if (!SET_FIND(CMD_LIST, cmd))
            {
                log("未知的指令！");
                error = true;
                return 0;
            }
            
            bool pushPC = false;
            
            auto info = CMD_LIST.at(cmd);
            {
                auto dst = info.dst;
                AddressingMode mode = info.mode;
                auto cf = info.cf;
                
                
                auto str = cmd_str(info, regs.PC, _mem);
                log("%s\n", str.c_str());
                
                {
                    ////////////////////////////////
                    // 寻址
                    std::function<void(DST, CALCODE, int)> calFunc = [this, &mode, cf](DST dst, CALCODE code, int val){
                        
                        uint8_t dst_value = 0;
                        
                        if (cf != CF_ST) // set 函数不需要获取原来的值
                        {
                            dst_value = valueFromDST(dst, mode);
                        }
                        
                        cal(&dst_value, code, val);
                        
                        valueToDST(dst, dst_value, mode);
                    };
                    
                    switch(info.cf)
                    {
                        case CF_BRK:
                            // BRK 执行时，产生BRK中断
                            interrupts(InterruptTypeIRQs);
                            break;
                        case CF_LD:
                            calFunc(dst, CALCODE_SET, addressingValue(mode));
                            break;
                        case CF_ST:
                            assert(dst != DST_M);
                            calFunc(DST_M, CALCODE_SET, valueFromDST(dst, mode));
                            break;
                        case CF_IN:
                            calFunc(dst, CALCODE_IN, 1);
                            break;
                        case CF_DE:
                            calFunc(dst, CALCODE_IN, -1);
                            break;
                        case CF_CP:
                            calFunc(dst, CALCODE_CP, addressingValue(mode));
                            break;
                        case CF_JSR:
                        {
                            pushPC = true;
                        }
                        case CF_JMP:
                        {
                            int offset = addressingOnly(mode);//addressingValue(mode);
                            
                            log("跳转到 %04X\n", offset);
                            
                            jumpPC = offset;
                            break;
                        }
                        case CF_AND:
                        {
                            calFunc(dst, CALCODE_AND, addressingValue(mode));
                            break;
                        }
                        case CF_SE:
                        {
                            regs.P.set(dst - DST_REGS_P_C, 1);
                            break;
                        }
                        case CF_CL:
                        {
                            regs.P.set(dst - DST_REGS_P_C, 0);
                            break;
                        }
                        case CF_T:
                        {
                            DST src;
                            switch (info.param)
                            {
                                case PARAM_REGS_X:
                                    src = DST_REGS_X;
                                    break;
                                case PARAM_REGS_Y:
                                    src = DST_REGS_Y;
                                    break;
                                case PARAM_REGS_A:
                                    src = DST_REGS_A;
                                    break;
                                default:
                                    assert(!"error!");
                                    break;
                            }
                            
                            calFunc(dst, CALCODE_SET, valueFromRegs(src));
                            break;
                        }
                        case CF_SBC:
                        {
                            calFunc(dst, CALCODE_AD, -(addressingValue(mode)+valueFromRegs(DST_REGS_P_C)));
                            break;
                        }
                        case CF_ADC:
                        {
                            calFunc(dst, CALCODE_AD, addressingValue(mode)+valueFromRegs(DST_REGS_P_C));
                            break;
                        }
                        case CF_BIT:
                        {
//                            calFunc(DST_REGS_P_Z, CALCODE_BIT, addressingValue(mode));
                            
                            uint8_t dst_value = valueFromRegs(dst);
                            cal(&dst_value, CALCODE_BIT, addressingValue(mode));
                            
                            break;
                        }
                        case CF_B:
                        {
//                            int value = mode - IMMIDIATE_VALUE_0; // 得到具体数值
                            
                            assert(info.param != PARAM_NONE);
                            int value = info.param - PARAM_0; // 得到具体数值
                            int8_t offset = 0;
                            
                            if (valueFromDST(dst, mode) == value)
                            {
                                offset = addressingValue(mode);
                            }
                            
                            log("%X == %X 则跳转到 %d\n", valueFromDST(dst, mode), value, offset);
                            
                            jumpPC = offset;
                            break;
                        }
                        case CF_OR:
                        {
                            calFunc(dst, CALCODE_OR, addressingValue(mode));
                            break;
                        }
                        case CF_ASL:
                        {
                            calFunc(dst, CALCODE_ASL, 1);
                            break;
                        }
                        case CF_LSR:
                        {
                            calFunc(dst, CALCODE_LSR, 1);
                            break;
                        }
                        case CF_PH:
                        {
                            push(valueFromRegs(dst));
                            break;
                        }
                        case CF_PL:
                        {
                            switch (dst)
                            {
                                case DST_REGS_A:
                                    regs.A = pop();
                                    break;
                                default:
                                    assert(!"error!");
                                    break;
                            }
                            
                            break;
                        }
                        case CF_RTS:
                        {
//                            if (regs.SP >= 2)
                            if (regs.SP <= 0xFD) // 255 - 2
                            {
                                uint8_t PC_high = pop();
                                uint8_t PC_low = pop();
                                regs.PC = (PC_high << 8) + PC_low;
                            }
                            else
                            {
                                assert(!"error!");
                            }
                            break;
                        }
                        case CF_RTI:
                        {
                            restoreStatus();
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
            
            if (info.cf != CF_RTI && info.cf != CF_RTS)
            {
                // 移动PC指针到下一条指令位置
                regs.PC += info.bytes;
                
                // push PC
                if (pushPC)
                {
                    push((const uint8_t*)&regs.PC, 2); // 移动到下一句指令才存储
                    
                    
                }
                
//                uint16_t stack_addr = STACK_ADDR_OFFSET + regs.SP+1;
//                
//                int a = _mem->read8bitData(stack_addr);
//                int b = _mem->read8bitData(stack_addr+1);
//                
//                if (a != 128)
//                {
//                    log("fuck\n");
//                }

                if (jumpPC != 0)
                {
                    // 检查调整类型：相对，绝对
                    switch (info.mode)
                    {
                        case RELATIVE:
                        case IMMIDIATE:
                            // 相对跳转
                            regs.PC += jumpPC;
                            break;
                        case INDEXED_ABSOLUTE:
                            // 绝对跳转
                            regs.PC = jumpPC;
                            break;
                        default:
                            log("未知的指令！");
                            error = true;
                            break;
                    }
                    
                    log("jmp %X\n", regs.PC);
                }
            }
            
            execCmdLine ++;
            
            return info.cyles;
        }
        
    private:
        
        
        
        
        
//        std::function<uint8_t(DST)> valueFromDST = [this](DST dst) {
        
        uint8_t valueFromRegs(DST dst) {
            
            if (dst == DST_M)
            {
                assert(!"不支持内存寻址！");
                return 0;
            }

            // ACCUMULATOR 进行内存寻址会报错
            return valueFromDST(dst, ACCUMULATOR);
        };
        
        uint8_t valueFromDST(DST dst, AddressingMode mode) {
        
            uint8_t dst_value;
            switch (dst)
            {
                case DST_REGS_SP:
                    dst_value = regs.SP;
                    break;
                case DST_REGS_A:
                    dst_value = regs.A;
                    break;
                case DST_REGS_X:
                    dst_value = regs.X;
                    break;
                case DST_REGS_Y:
                    dst_value = regs.Y;
                    break;
                case DST_REGS_P_Z:
                    dst_value = regs.P.get(__registers::__P::Z);
                    break;
                case DST_REGS_P_N:
                    dst_value = regs.P.get(__registers::__P::N);
                    break;
                case DST_REGS_P_C:
                    dst_value = regs.P.get(__registers::__P::C);
                    break;
                case DST_REGS_P_I:
                    dst_value = regs.P.get(__registers::__P::I);
                    break;
                case DST_REGS_P_B:
                    dst_value = regs.P.get(__registers::__P::B);
                    break;
                case DST_REGS_P_V:
                    dst_value = regs.P.get(__registers::__P::V);
                    break;
                case DST_REGS_P_D:
                    dst_value = regs.P.get(__registers::__P::D);
                    break;
                case DST_M:
                    dst_value = addressingValue(mode);
                    break;
                default:
                    assert(!"error!");
                    break;
            }
            
            return dst_value;
        };
        
        void valueToDST(DST dst, uint8_t value, AddressingMode mode)
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
                    _mem->write8bitData(addressingOnly(mode), value);
                    break;
                default:
                    assert(!"error!");
                    break;
            }
        }
        
//        std::function<uint8_t()> addressing = [this, &mode](){
        
        uint16_t addressingOnly(AddressingMode mode)
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
//                case IMMIDIATE_16bit:
//                {
//                    addr = dataAddr;
//                    data16bit = true;
//                    break;
//                }
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
                case INDIRECT_INDEXED:
                {
                    addr = _mem->read16bitData(_mem->read8bitData(dataAddr));
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
        
//        uint8_t addressingValue(AddressingMode mode)
//        {
//            return _mem->read8bitData(addressingOnly(mode));
//        };
        
        int8_t addressingValue(AddressingMode mode)
        {
//            if (mode == IMMIDIATE_16bit)
//            {
//                return _mem->read16bitData(addressingOnly(mode));
//            }
//            else
            {
                return _mem->read8bitData(addressingOnly(mode));
            }
        };
        
        
        

        
        
        
        
        
        // 入栈
        void push(const uint8_t* addr, size_t length)
        {
            // 循环推入8bit数据
            for (int i=0; i<length; i++)
            {
                uint16_t stack_addr = STACK_ADDR_OFFSET + regs.SP;
                
                _mem->write8bitData(stack_addr, *(addr+i));
                
//                regs.SP ++;
                regs.SP --;
            }
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
//            regs.SP --;
            regs.SP ++;
            return _mem->read8bitData(STACK_ADDR_OFFSET + regs.SP);
        }
        
        // 保存现场
        void saveStatus()
        {
            // 保存现场（把PC、 P两个寄存器压栈）
            push((const uint8_t*)&regs.PC, 2);
            push(*(uint8_t*)&regs.P);
        }
        
        // 恢复现场()
        void restoreStatus()
        {
            if (regs.SP >= 3)
            {
                uint8_t data = pop();
                regs.P = *(bit8*)&data;
                
                uint8_t PC_high = pop();
                uint8_t PC_low = pop();
                
                regs.PC = (PC_high << 8) + PC_low;
            }
            else
            {
                assert(!"error!");
            }
        }
        
        
        Memory* _mem;
        
        
        InterruptType _currentInterruptType;
        
        
        
        enum CALCODE{
            CALCODE_SET,
            CALCODE_IN,
            CALCODE_AD,
            CALCODE_CP,
            CALCODE_AND,
            
            CALCODE_BIT,
            CALCODE_OR,
            CALCODE_ASL,
            CALCODE_LSR,
        };
        
        
        
        #define SET_FIND(s, v) (s.find(v) != s.end())
        
        
        

        
        
        
        
        
        
        /**
         计算函数，对内存进行数值操作

         @param dst 目标内存地址
         @param op 操作符
         @param value 值
         */
        void cal(uint8_t* dst, CALCODE op, int8_t value)
        {
            int8_t old_value = *dst; // [0, 255]
            int new_value;
            bool applyToDST = false;
            bool checkFLAG[8] = {0};
            
            switch (op)
            {
                case CALCODE_SET:
                {
                    log("%d = %d\n", (uint8_t)old_value, (uint8_t)value); // 为了显示
                    new_value = (int8_t)value;
                    checkFLAG[__registers::N] = true;
                    checkFLAG[__registers::Z] = true;
                    applyToDST = true;
                    break;
                }
                case CALCODE_IN:
                {
                    if (value < 0)
                    {
                       log("%d - %d\n", (uint8_t)old_value, abs((int8_t)value));
                    }
                    else
                    {
                        log("%d + %d\n", (uint8_t)old_value, (uint8_t)value);
                    }
                    new_value = (int8_t)old_value + (int8_t)value; // -128 - 2 最终会限制到8bit -1 + 1 = 255 + 1
                    checkFLAG[__registers::N] = true;
                    checkFLAG[__registers::Z] = true;
                    applyToDST = true;
                    break;
                }
                case CALCODE_AD:
                {
                    if (value < 0)
                    {
                        log("%d - %d\n", (int8_t)old_value, abs((int8_t)value));
                    }
                    else
                    {
                        log("%d + %d\n", (int8_t)old_value, (int8_t)value);
                    }
                    new_value = (int8_t)old_value + (int8_t)value;
                    
                    // 检查
                    checkFLAG[__registers::N] = true;
                    checkFLAG[__registers::Z] = true;
                    checkFLAG[__registers::C] = true;
                    checkFLAG[__registers::V] = true;
                    applyToDST = true;
                    break;
                }
                case CALCODE_CP:
                {
                    log("%d 比较 %d\n", (int8_t)old_value, (int8_t)value);
                    new_value = (int8_t)old_value - (int8_t)value;

                    checkFLAG[__registers::N] = true;
                    checkFLAG[__registers::Z] = true;
                    checkFLAG[__registers::C] = true;
                    break;
                }
                case CALCODE_AND:
                {
                    log("%d AND %d\n", (uint8_t)old_value, (uint8_t)value);
                    new_value = (uint8_t)old_value & (uint8_t)value;
                    checkFLAG[__registers::N] = true;
                    checkFLAG[__registers::Z] = true;
                    applyToDST = true;
                    break;
                }
                case CALCODE_BIT:
                {
                    // Z = A AND M
                    new_value = regs.A & (uint8_t)value;
                    checkFLAG[__registers::Z] = true;
                    
                    // P(b,7) = M(6,7)
                    regs.P.set(__registers::N, ((bit8*)&value)->get(__registers::N));
                    regs.P.set(__registers::V, ((bit8*)&value)->get(__registers::V));
                    break;
                }
                case CALCODE_OR:
                {
                    log("%d OR %d\n", (uint8_t)old_value, (uint8_t)value);
//                    if (value < 0)
//                        log("");
                    new_value = (uint8_t)old_value | (uint8_t)value;
                    checkFLAG[__registers::N] = true;
                    checkFLAG[__registers::Z] = true;
                    applyToDST = true;
                    break;
                }
                case CALCODE_ASL:
                {
                    log("%d << 1\n", (uint8_t)old_value);
                    
                    new_value = regs.A << 1;
                    checkFLAG[__registers::N] = true;
                    checkFLAG[__registers::Z] = true;
                    checkFLAG[__registers::C] = true;
                    
                    break;
                }
                case CALCODE_LSR:
                {
                    log("%d >> 1\n", (uint8_t)old_value);
                    
                    new_value = regs.A >> 1;
                    checkFLAG[__registers::Z] = true;
                    checkFLAG[__registers::C] = true;
                    
                    break;
                }
                default:
                    assert(!"error!");
                    break;
            }
            
            // 修改寄存器标记
            if (checkFLAG[__registers::Z]) // 限定最后的结果在8bit，则 0 == 0 可检查
                regs.P.set(__registers::Z, (int8_t)new_value == 0);
            
            if (checkFLAG[__registers::N]) // 限定最后的结果在8bit，则小于0在负数表示范围
                regs.P.set(__registers::N, (int8_t)new_value < 0);
            
            if (checkFLAG[__registers::V]) // 计算溢出，需要在符号计算情况下，更高的范围来判断
                regs.P.set(__registers::V, new_value < -128 || new_value > 127);

            if (checkFLAG[__registers::C]) // 检查高位是否发生进位或借位，判断前后最高位的位置，不等则发生了错位
            {
//                if (old_value < 0 || new_value < 0)
//                    log("fuck\n");
                int c = highBit(old_value) != highBit(new_value);
                regs.P.set(__registers::C, c);
            }
            

            // 影响目标值
            if (applyToDST)
            {
                *dst = new_value;
            }
        }
        
    };
    
    
}
