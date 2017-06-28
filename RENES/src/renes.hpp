
#include "cpu.hpp"
#include "ppu.hpp"
#include <functional>
#include <stdio.h>
#include "type.hpp"
#include <thread>

namespace ReNes {
    
    
    // 多线程
    void cpu_working(CPU* cpu, std::function<bool()> callback)
    {
        // 主循环
        do {
            
            // 执行指令
            cpu->exec();
            
            // for Test
            usleep(1);
            //                log("sleep 1...\n");
            
        }while(callback() && !cpu->error);
    }
    
    void ppu_working(PPU* ppu, std::function<bool()> callback)
    {
        // 主循环
        do {
//            printf("ppu\n");
            // 执行绘图
            
            // 绘图完成，向
            
            usleep(1000 * 100);
            
        }while(callback());
    }
    
    
    
    class Nes {
        
    public:
        
        Nes()
        {
            
        }
        
        // 加载rom
        void loadRom(const uint8_t* rom, size_t length)
        {
            // 解析头文件
            int rom16kB_count = rom[4];
            
            bit8 b8_6 = *(bit8*)&rom[6];
            bit8 b8_7 = *(bit8*)&rom[7];
            
            log("文件长度 %d\n", length);
            
            log("[4] 16kB ROM: %d\n\
[5] 8kB VROM: %d\n\
[6] D0: %d D1: %d D2: %d D3: %d D4: %d D5: %d D6: %d D7: %d\n\
[7] 保留0: %d %d %d %d ROM Mapper高4位: %d %d %d %d\n\
[8-F] 保留8字节0: %d %d %d %d %d %d %d %d\n\
[16]\n",
                   rom16kB_count, rom[5],
                   b8_6.get(0), b8_6.get(1), b8_6.get(2), b8_6.get(3), b8_6.get(4), b8_6.get(5), b8_6.get(6), b8_6.get(7),
                   b8_7.get(0), b8_7.get(1), b8_7.get(2), b8_7.get(3), b8_7.get(4), b8_7.get(5), b8_7.get(6), b8_7.get(7),
                   rom[8], rom[9], rom[10], rom[11], rom[12], rom[13], rom[14], rom[15]);
            
            for (int i=0; i<rom16kB_count; i++)
            {
                const uint8_t* romAddr = &rom[16];
                
                _mem.writeData(PRG_ROM_LOWER_BANK_OFFSET, romAddr, 1024*16);
                
                // 如果只有1个16kB的bank，则需要再复制一份到0xC000处，让中断向量能够在0xFFFA出现
                if (rom16kB_count == 1)
                {
                    _mem.writeData(PRG_ROM_UPPER_BANK_OFFSET, romAddr, 1024*16);
                }
            }
        }
        
        // 执行当前指令
        void run() {
            
            // 初始化cpu
            _cpu.init(&_mem);
            _ppu.init(&_mem);
            
            std::thread cpu_thread(cpu_working, &_cpu, [this](){
                return callback();
            });
            
            std::thread ppu_thread(ppu_working, &_ppu, [this](){
                return true;
            });
            
            cpu_thread.join();
            ppu_thread.join();
            
            
            
            if (_cpu.error)
            {
                log("模拟器因故障退出!\n");
            }
            else
            {
                log("模拟器正常退出!\n");
            }

            
        }

        const CPU* cpu() const
        {
            return &_cpu;
        }
        
        const Memory* mem() const
        {
            return &_mem;
        }
        
        std::function<bool()> callback;
        
    private:
        
        CPU _cpu;
        PPU _ppu;
        Memory _mem;
    };
    
    
}
