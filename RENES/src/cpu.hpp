
#include <stdlib.h>
#include <map>
#include "mem.hpp"
#include <string>
#include <vector>
#include "type.hpp"

#include <sstream>
#include <iomanip>

namespace ReNes {
    
    // RAM内存结构的偏移地址
    #define PRG_ROM_LOWER_BANK_OFFSET 0x8000
    #define PRG_ROM_UPPER_BANK_OFFSET 0xC000
    
    #define STACK_ADDR_OFFSET 0x0100
    
    struct bit8 {
        
        void set(uint8_t offset, int value)
        {
            assert(value == 0 || value == 1);
            
            _data &= ~(0x1 << offset); // set 0
            _data |= ((value % 2) << offset);
        }
        
        uint8_t get(uint8_t offset) const
        {
            return (_data >> offset) & 0x1;
        }
        
    private:
        uint8_t _data; // 8bit
    };
    
    
    enum AddressingMode {
        
        NO_ADDRESSING,
        
//        ZERO_PAGE_8bit, // 零页寻址
//        ABSOLUTE_8bit,
        ABSOLUTE_16bit,
        IMMIDIATE_ABSOLUTE_8bit, // 立即寻址s
        ABSOLUTE_16bit_X,
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

    struct CPU {
        
        struct __registers {
            
            // 3个特殊功能寄存器: PC寄存器(Program Counter)，SP寄存器(Stack Pointer)，P寄存器(Processor Status)
            uint16_t PC; // 存储16bit地址
            uint8_t SP;  // 由8bit表示[0,255]的栈空间
            
            // 数据下标
            enum {
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
        
        CPU()
        {
            // 初始化置0
            memset(this, 0, sizeof(CPU));
            
            // 检查数据尺寸
            assert(1 == sizeof(regs.P));
        }
        
        // 初始化，映射内存
        void init(Mem* mem, uint16_t addr)
        {
            _mem = mem;
            
            // 映射内存
            regs.PC = addr;
            regs.SP = 0;
            
            error = false;
            
            _currentInterruptType = InterruptTypeReset; // 默认是复位
            interrupts(); // 中断
        }
        
        void exec()
        {
            if (error)
                return;
            
            bool hasInterrupts = false;
            
            InterruptType interruptsType = InterruptTypeNone;
            
            // 从内存里面取出一条8bit指令，将PC移动到下一个内存地址
//            uint16_t pc = regs.PC;
            uint8_t cmd = _mem->read8bitData(regs.PC);
//            uint8_t cmd = readMem8bit();
            
            log("[%04X] cmd: %x => ", regs.PC, cmd);
            
//            regs.PC++;
            
            // 执行指令
            struct CmdInfo {
                
                int bytes;
                int cyles;
            };
            
            const static std::map<uint8_t, CmdInfo> CMD_LIST = {
                {0xA9, {2, 2}},
                {0x8D, {3, 4}},
                {0xA2, {2, 2}},
                {0xBD, {3, 4}},
                {0xE8, {1, 2}},
                {0xE0, {2, 2}},
                {0xD0, {2, 2}},
                {0xAD, {3, 4}}
            };
            
            int jmpOffset = 0;
            
            switch (cmd)
            {
//                case 0x40: // RTI
//                {
//                    // 恢复现场
//                    restoreStatus();
//                    
//                    break;
//                }
                case 0xA9: // LDA #oper
                {
                    LD(&regs.A, IMMIDIATE_ABSOLUTE_8bit);
                    break;
                }
                case 0x8D: // STA oper
                {
                    ST(&regs.A, ABSOLUTE_16bit);
                    break;
                }
                case 0xA2: // LDX #oper
                {
                    LD(&regs.X, IMMIDIATE_ABSOLUTE_8bit);
                    break;
                }
                case 0xBD: // LDA oper,X
                {
                    LD(&regs.A, ABSOLUTE_16bit_X);
                    break;
                }
                case 0xE8: // INX
                {
                    IN(&regs.X, NO_ADDRESSING);
                    break;
                }
                case 0xE0: // CPX #oper
                {
                    CP(&regs.X, IMMIDIATE_ABSOLUTE_8bit);
                    break;
                }
                case 0xD0: // BNE oper（貌似这个是错的，必须是直接8bit寻址才对）
                {
                    // Z == 0 时发生跳转，Z 是上次计算结果，结果为0，则 Z == 1，Z == 0，结果不为0
                    // 所以这里是 不等于0时跳转
                    jmpOffset = B(__registers::Z, 0, IMMIDIATE_ABSOLUTE_8bit);
                    break;
                }
                case 0xAD: // LDA oper
                {
                    LD(&regs.A, ABSOLUTE_16bit);
                    break;
                }
                default:
                    log("未知指令！\n");
                    error = true;
                    return;
                    break;
            }
            
            // 移动PC指针到下一条指令位置
            regs.PC += CMD_LIST.at(cmd).bytes;
            
            // 执行跳转
            regs.PC += jmpOffset;
            if (jmpOffset != 0)
            {
                log("jmp %X\n", regs.PC);
            }
            
            
            // 检查中断
            switch(interruptsType)
            {
                case InterruptTypeNMI:
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
            
            // 处理中断
            if (hasInterrupts || _currentInterruptType != InterruptTypeNone)
            {
                // 设置当前中断
                if (interruptsType > _currentInterruptType)
                {
                    _currentInterruptType = interruptsType;
                    _interruptCount = 0;
                }
                
                // 当中断出现后，运行了7个cpu周期，则触发中断
                if (_interruptCount++ == 7)
                {
                    saveStatus(); // 保存现场
                    interrupts();
                }
            }
            
            
        }
        
    private:
        
        enum InterruptType{
            InterruptTypeNone,
            InterruptTypeIRQs,
            InterruptTypeNMI,
            InterruptTypeReset
        };
        
        // 入栈
        void push(const uint8_t* addr, size_t length)
        {
            // 循环推入8bit数据
            for (int i=0; i<length; i++)
            {
                uint16_t stack_addr = STACK_ADDR_OFFSET + regs.SP;
                
                _mem->writeData(stack_addr, addr+i, 1);
                
                regs.SP ++;
            }
        }
        
        // 出栈
        uint8_t pop()
        {
            regs.SP --;
            return _mem->read8bitData(STACK_ADDR_OFFSET + regs.SP);
        }
        
        // 保存现场
        void saveStatus()
        {
            // 保存现场（把PC、 P两个寄存器压栈）
            push((const uint8_t*)&regs.PC, 2);
            push((const uint8_t*)&regs.P, 1);
        }
        
        // 恢复现场()
        void restoreStatus()
        {
            if (regs.SP >= 2)
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
        
//        uint8_t readMem8bit()
//        {
//            return _mem->read8bitData(regs.PC++);
//        }
        
//        uint16_t readMem16bit()
//        {
//            uint16_t data = _mem->read16bitData(regs.PC);
//            regs.PC += 2;
//            return data;
//
//        }
        
        void interrupts()
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
            regs.PC = interruptHandlerAddr;
            
            // 取消中断标记
            _currentInterruptType = InterruptTypeNone;
        }
        
        Mem* _mem;
        
        
        int _interruptCount; // 中断发生计数器
        InterruptType _currentInterruptType;
        
        ////////////////////////////////
        // 寻址
        uint8_t* addressing(AddressingMode mode)
        {
            uint16_t dataAddr = regs.PC + 1; // 操作数位置 = PC + 1
            
            uint16_t addr;
            uint8_t* data;
            switch (mode)
            {
//                case ZERO_PAGE_8bit:
//                {
//                    addr = _mem->read8bitData(dataAddr);
//                }
                case IMMIDIATE_ABSOLUTE_8bit:
                {
                    addr = dataAddr;
                    break;
                }
//                case ABSOLUTE_8bit:
//                {
//                    addr = _mem->read8bitData(dataAddr);
//                    break;
//                }
                case ABSOLUTE_16bit:
                {
                    addr = _mem->read16bitData(dataAddr);
                    break;
                }
                case ABSOLUTE_16bit_X:
                {
                    addr = _mem->read16bitData(dataAddr) + regs.X;
                    break;
                }
                default:
                    assert(!"error!");
                    break;
            }
            data = _mem->getAddr(addr);
            
            if (dataAddr == addr)
            {
                log("直接寻址 %X 值 %d\n", addr, *data);
            }
            else
            {
                log("间接寻址 %X 值 %d\n", addr, *data);
            }
            
            return data;
        }
        
        enum CALCODE{
            CALCODE_SET,
            CALCODE_IN,
            CALCODE_CP,
        };
        
//        void INC(AddressingMode mode)
//        {
//            uint8_t& data = *addressing(mode);
//            if (data == 0xff)
//            {
//                regs.P.set(__registers::N, 1); // 255越界，产生负数标记
//            }
//            data ++;
//        }
        
        template< typename T >
        std::string int_to_hex( T i )
        {
            std::stringstream stream;
            stream << "0x"
            << std::setfill ('0') << std::setw(sizeof(T)*2)
            << std::hex << std::uppercase << (int)i;
            return stream.str();
            
            
        }
        
        #define SET_FIND(s, v) (s.find(v) != s.end())
        
        
        
        std::string dstCode(uint8_t* dst)
        {
            std::map<uint8_t*, std::string> regsNames = {
                {&regs.A, "A"},
                {&regs.X, "X"},
                {&regs.Y, "Y"},
            };
            
            if (SET_FIND(regsNames, dst))
            {
                return regsNames.at(dst);
            }
            return "C";
        }
        
//        std::string PCode(uint8_t p)
//        {
//            std::map<uint8_t, std::string> regsNames = {
//                {__registers::C, "C"},
//                {__registers::Z, "Z"},
//                {__registers::I, "I"},
//                {__registers::D, "D"},
//                {__registers::B, "B"},
//                {__registers::_, "_"},
//                {__registers::V, "V"},
//                {__registers::N, "N"},
//            };
//            if (SET_FIND(regsNames, p))
//            {
//                return regsNames.at(p);
//            }
//            return "?";
//        }

        
        
        std::string operCode(AddressingMode mode)
        {
            uint16_t dataAddr = regs.PC + 1; // 操作数位置 = PC + 1
            std::string res;
            
            switch (mode)
            {
                case IMMIDIATE_ABSOLUTE_8bit:
                {
                    res = std::string("#") + int_to_hex(_mem->read8bitData(dataAddr));
                    break;
                }
//                case ABSOLUTE_8bit:
//                {
//                    res = int_to_hex(_mem->read8bitData(dataAddr));
//                    break;
//                }
                case ABSOLUTE_16bit:
                {
                    res = int_to_hex(_mem->read16bitData(dataAddr));
                    //                    addr = readMem16bit();
                    break;
                }
                case ABSOLUTE_16bit_X:
                {
                    res = int_to_hex(_mem->read16bitData(dataAddr)) + ",X";
                    break;
                }
                case NO_ADDRESSING:
                {
                    break;
                }
                default:
                    assert(!"error!");
                    break;
            }
            return res;
        }
        
        void logCmd(const char* cmd, uint8_t* dst, AddressingMode mode)
        {
            log("%s%s %s\n", cmd, dstCode(dst).c_str(), operCode(mode).c_str());
        }
        
        
        void LD(uint8_t* dst, AddressingMode mode)
        {
            logCmd("LD", dst, mode);
            
            cal(dst, CALCODE_SET, *addressing(mode));
        }
        
        void ST(uint8_t* dst, AddressingMode mode)
        {
            logCmd("ST", dst, mode);
            
            cal(addressing(mode), CALCODE_SET, *dst);
        }
        
        void IN(uint8_t* dst, AddressingMode mode)
        {
            logCmd("IN", dst, mode);
            
            cal(dst, CALCODE_IN, 1);
        }
        
        void CP(uint8_t* dst, AddressingMode mode)
        {
            logCmd("CP", dst, mode);
            
            cal(dst, CALCODE_CP, *addressing(mode));
        }
        
        int B(uint8_t p, int value, AddressingMode mode)
        {
            int jmpOffset = 0;

            if (p == __registers::Z && value == 0)
            {
                log("BNE\n");
            }
            
            log("%d\n", regs.P.get(p));
            
            if (regs.P.get(p) == value)
            {
                jmpOffset = *addressing(mode);
                if (regs.P.get(__registers::N) == 1)
                    jmpOffset = (int8_t)jmpOffset;
            }
            
            log("%X == %X 则跳转到 %d\n", regs.P.get(p), value, jmpOffset);
            
            return jmpOffset;
        }
        
        void cal(uint8_t* dst, CALCODE op, int8_t value)
        {
            uint8_t old_value = *dst;
            int8_t new_value;
            
            switch (op)
            {
                case CALCODE_SET:
                {
                    log("0x%X = 0x%X\n", old_value, value);
                    new_value = value;
                    break;
                }
                case CALCODE_IN:
                {
                    log("0x%X + 0x%X\n", old_value, value);
                    new_value = old_value + value;
                    break;
                }
                case CALCODE_CP:
                {
                    log("0x%X 比较 0x%X\n", old_value, value);
                    new_value = old_value - value;
                    
                    // 减操作，检查借位
                    regs.P.set(__registers::C, (int8_t)new_value < 0);
                    
                    break;
                }
                default:
                    assert(!"error!");
                    break;
            }
            
            // 修改寄存器标记
            regs.P.set(__registers::Z, (int8_t)new_value == 0);
            regs.P.set(__registers::N, (int8_t)new_value < 0);
            
            // 影响目标值
            switch (op)
            {
                case CALCODE_SET:
                case CALCODE_IN:
                    *dst = new_value;
                    break;
                default:
                    break;
            }
        }
        
//        void INX()
//        {
//            if (regs.X == 0xFF)
//            {
//                regs.P.set(__registers::N, 1);
//            }
//            
//            regs.X ++;
//            
//        }
    };
}
