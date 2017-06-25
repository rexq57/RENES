
#include <stdlib.h>
#include <map>
#include "mem.hpp"

namespace ReNes {
    
    // RAM内存结构的偏移地址
    #define PRG_ROM_LOWER_BANK_OFFSET 0x8000
    #define PRG_ROM_UPPER_BANK_OFFSET 0xC000
    
    #define STACK_ADDR_OFFSET 0x0100
    
    struct bit8 {
        
        void set(uint8_t offset, int value)
        {
            assert(value == 0 || value == 1);
            
            _data &= (0x0 << offset); // set 0
            _data |= ((value % 2) << offset);
        }
        
        uint8_t get(int8_t offset) const
        {
            return (_data >> offset) & 0x1;
        }
        
    private:
        uint8_t _data; // 8bit
    };
    
    
    enum AddressingMode {
//        ZERO_PAGE_8bit, // 零页寻址
        ABSOLUTE_16bit,
        IMMIDIATE_ABSOLUTE_8bit, // 立即寻址s
        ABSOLUTE_16bit_X,
    };

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
        
        CPU()
        {
            // 初始化置0
            memset(this, 0, sizeof(CPU));
            
            // 检查数据尺寸
            assert(1 == sizeof(regs.P));
        }
        
        // 初始化，映射内存
        void init(Mem* mem, int16_t addr)
        {
            _mem = mem;
            
            // 映射内存
            regs.PC = addr;
            regs.SP = 0;
            
            _currentInterruptType = Reset; // 默认是复位
            interrupts(); // 中断
        }
        
        void exec()
        {
            bool hasInterrupts = false;
            
            InterruptType interruptsType = None;
            
            // 从内存里面取出一条8bit指令，将PC移动到下一个内存地址
            uint8_t cmd = _mem->read8bitData(regs.PC);
            
            printf("cmd: [%04X] %x\n", regs.PC, cmd);
            
            regs.PC++;
            
            // 执行指令
            const static std::map<uint8_t, int> CMD_LENGTH = {
                {0xA9, 2},
                {0x8D, 3},
                {0xA2, 2},
                {0xBD, 3}
            };
            
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
                    LDA(IMMIDIATE_ABSOLUTE_8bit);
                    break;
                }
                case 0x8D: // STA oper
                {
                    STA(ABSOLUTE_16bit);
                    break;
                }
                case 0xA2: // LDX #oper
                {
                    LDX(IMMIDIATE_ABSOLUTE_8bit);
                    break;
                }
                case 0xBD: // LDA oper,X
                {
                    LDA(ABSOLUTE_16bit_X);
                    break;
                }
                default:
                    printf("未知指令！%x\n", cmd);
                    break;
            }
            
            // 移动PC指针到下一条指令位置
            regs.PC += CMD_LENGTH.at(cmd) - 1;
            
            // 检查中断
            switch(interruptsType)
            {
                case NMI:
                case Reset:
                    // 不可屏蔽中断
                    hasInterrupts = true;
                    break;
                case IRQs:
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
            if (hasInterrupts || _currentInterruptType != None)
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
            None,
            IRQs,
            NMI,
            Reset
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
                    {NMI, {0xFFFA, 0xFFFB}},
                    {Reset, {0xFFFC, 0xFFFD}},
                    {IRQs, {0xFFFE, 0xFFFF}},
                };
                
                ADDR2 addr2 = handler[_currentInterruptType];
                interruptHandlerAddr = (_mem->read8bitData(addr2.high) << 8) + _mem->read8bitData(addr2.low);
            }
            
            printf("中断函数地址: %04X\n", interruptHandlerAddr);
            
            // 跳转到中断向量指向的处理函数地址
            regs.PC = interruptHandlerAddr;
            
            // 取消中断标记
            _currentInterruptType = None;
        }
        
        Mem* _mem;
        
        
        int _interruptCount; // 中断发生计数器
        InterruptType _currentInterruptType;
        
        ////////////////////////////////
        // 寻址
        uint8_t* addressing(AddressingMode mode)
        {
            uint16_t dataAddr = regs.PC;
            
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
            
            
            printf("寻址(%d) [%X] -> %X  %d\n", mode, dataAddr, addr, *data);
            
            return data;
        }
        
        
        
        void INC(AddressingMode mode)
        {
            uint8_t& data = *addressing(mode);
            if (data == 0xff)
            {
                regs.P.set(__registers::N, 1);
            }
            data ++;
        }
        
        void LDA(AddressingMode mode)
        {
            regs.A = *addressing(mode);
        }
        
        void LDY(AddressingMode mode)
        {
            regs.Y = *addressing(mode);
        }
        
        void LDX(AddressingMode mode)
        {
            regs.X = *addressing(mode);
        }
        
        void STA(AddressingMode mode)
        {
            *addressing(mode) = regs.A;
        }
    };
}
