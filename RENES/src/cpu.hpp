
#include <stdlib.h>
#include <map>
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

    // 2A03
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
            regs.SP = 0;
            
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
        
        void exec()
        {
            if (error)
                return;
            
            // 检查中断和处理中断
            _mutex.lock();
            {
                // 处理中断
                if (_currentInterruptType != InterruptTypeNone)
                {
                    // Reset 不需要保存现场
                    if (_currentInterruptType != InterruptTypeReset)
                    {
                        saveStatus(); // 保存现场
                    }
                    
                    // 设置中断屏蔽标志(I)防止新的中断进来
                    regs.P.set(__registers::I, 1);
                    
                    // 处理中断
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
                    
                    // cpu周期 + 7
                    
                    _currentInterruptType = InterruptTypeNone;
                }
            }
            _mutex.unlock();
            
            
            
            
            // 从内存里面取出一条8bit指令，将PC移动到下一个内存地址
            uint8_t cmd = _mem->read8bitData(regs.PC);
            
            log("[%04X] cmd: %x => ", regs.PC, cmd);
            
            int cmdOffset = 0;
            
            if (CMD_LIST.size() == 0)
            {
                CMD_LIST = {
                    /* LDA #oper */ {0xA9, {CF_LD, (long)&regs.A, IMMIDIATE_ABSOLUTE_8bit, 2, 2}},
                    /* STA oper  */ {0x8D, {CF_ST, (long)&regs.A, ABSOLUTE_16bit, 3, 4}},
                    /* LDX #oper */ {0xA2, {CF_LD, (long)&regs.X, IMMIDIATE_ABSOLUTE_8bit, 2, 2}},
                    /* LDA oper,X*/ {0xBD, {CF_LD, (long)&regs.A, ABSOLUTE_16bit_X, 3, 4}},
                    /* INX */      {0xE8, {CF_IN, (long)&regs.X, NO_ADDRESSING, 1, 2}},
                    /* CPX #oper */ {0xE0, {CF_CP, (long)&regs.X, IMMIDIATE_ABSOLUTE_8bit, 2, 2}},
                    /* BNE oper */  {0xD0, {CF_B, (long)__registers::Z, IMMIDIATE_ABSOLUTE_8bit, 2, 2}}, // 这里的操作数是8bit，很奇怪
                    /* LDA oper */  {0xAD, {CF_LD, (long)&regs.A, ABSOLUTE_16bit, 3, 4}},
                    /* BPL oper */  {0x10, {CF_B, (long)__registers::N, IMMIDIATE_ABSOLUTE_8bit, 2, 2}}, // 这里的操作数是8bit，很奇怪
                };
            }

            auto& info = CMD_LIST[cmd];
            {
                uint8_t* dst = (uint8_t*)info.dst;
                AddressingMode mode = info.mode;
                
                logCmd(CF_NAME.at(info.cf), info.dst, mode);
                
                
                {
                    ////////////////////////////////
                    // 寻址
                    enum AddressingOp{
                        READ,
                        WRITE
                    };
                    
                    std::function<uint8_t*(AddressingMode, AddressingOp)> addressing = [this](AddressingMode mode, AddressingOp op){
                        
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
                        
                        // 非法读取
                        if (op == READ && (addr == 0x2000 || addr == 0x2001))
                        {
                            log("该内存只能写!\n");
                            error = true;
                            return (uint8_t*)0;
                        }
                        
                        data = _mem->getRealAddr(addr);
                        
                        if (dataAddr == addr)
                        {
                            log("直接寻址 %X 值 %d\n", addr, (int)*data);
                        }
                        else
                        {
                            log("间接寻址 %X 值 %d\n", addr, (int)*data);
                        }
                        
                        return data;
                    };
                    
                    
                    switch(info.cf)
                    {
                        case CF_LD:
                            cal(dst, CALCODE_SET, *addressing(mode, READ));
                            break;
                        case CF_ST:
                            cal(addressing(mode, WRITE), CALCODE_SET, *dst);
                            break;
                        case CF_IN:
                            cal(dst, CALCODE_IN, 1);
                            break;
                        case CF_CP:
                            cal(dst, CALCODE_CP, *addressing(mode, READ));
                            break;
                        case CF_B:
                        {
                            long p = info.dst;
                            
                            int value = 0;
                            int offset = 0;
                            
                            if (regs.P.get(p) == value)
                            {
                                offset = (int8_t)*addressing(mode, READ);
                            }
                            
                            log("%X == %X 则跳转到 %d\n", regs.P.get(p), value, offset);
                            
                            cmdOffset = offset;
                            break;
                        }
                        default:
                            log("未知的指令！");
                            error = true;
                            break;
                            
                    }
                }
            }
            
            // 移动PC指针到下一条指令位置
            regs.PC += info.bytes;
            
            // 执行跳转
            regs.PC += cmdOffset;
            if (cmdOffset != 0)
            {
                log("jmp %X\n", regs.PC);
            }
        }
        
    private:
        
        enum CF{
            CF_LD,
            CF_ST,
            CF_IN,
            CF_CP,
            CF_B,
        };
        
        const std::map<CF, std::string> CF_NAME = {
            {CF_LD, "LD"},
            {CF_ST, "ST"},
            {CF_IN, "IN"},
            {CF_CP, "CP"},
            {CF_B, "B"},
        };
        
        const std::map<uint8_t*, std::string> REGS_NAME = {
            {&regs.A, "A"},
            {&regs.X, "X"},
            {&regs.Y, "Y"},
        };
        
        // 执行指令
        struct CmdInfo {
            
            CF cf;
            long dst;
            AddressingMode mode;
            int bytes;
            int cyles;
        };

        
        // 定义指令长度、CPU周期
        std::map<uint8_t, CmdInfo> CMD_LIST;
        
        
        
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
        
        
        Memory* _mem;
        
        
        InterruptType _currentInterruptType;
        
        
        
        enum CALCODE{
            CALCODE_SET,
            CALCODE_IN,
            CALCODE_CP,
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
        
        #define SET_FIND(s, v) (s.find(v) != s.end())
        
        
        

        
        /**
         根据目标打印指令日志

         @param cmd 指令
         @param dst 目标
         @param mode 寻址模式
         */
        void logCmd(const std::string& cmd, long dst, AddressingMode mode)
        {
            if (cmd == "B")
            {
                if (dst == __registers::Z)
                {
                    log("BNE %d\n", regs.P.get(dst));
                }
                else if (dst == __registers::N)
                {
                    log("BPL %d\n", regs.P.get(dst));
                }
            }
            else
            {
                std::function<std::string(uint8_t*)> dstCode = [this](uint8_t* dst){
                    if (SET_FIND(REGS_NAME, dst))
                    {
                        return REGS_NAME.at(dst);
                    }
                    return std::string("C");
                };
                
                
                std::function<std::string(AddressingMode)> operCode = [this](AddressingMode mode)
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
                };
                
                
                log("%s%s %s\n", cmd.c_str(), dstCode((uint8_t*)dst).c_str(), operCode(mode).c_str());
            }
        }
        
        
        
        
        /**
         计算函数，对内存进行数值操作

         @param dst 目标内存地址
         @param op 操作符
         @param value 值
         */
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
        
    };
}
