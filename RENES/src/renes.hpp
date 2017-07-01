
#include "cpu.hpp"
#include "ppu.hpp"
#include <functional>
#include <stdio.h>
#include "type.hpp"
#include <thread>
#include <unistd.h>
#include <chrono>

namespace ReNes {
    
    
    // 多线程
    void cpu_working(CPU* cpu, std::function<bool(int)> callback)
    {
        int cyles;
        
        // 主循环
        do {
            // 执行指令
            cyles = cpu->exec();
            
        }while(callback(cyles) && !cpu->error);
    }
    
    void ppu_working(PPU* ppu, std::function<bool()> callback)
    {
        // 主循环
        do {
//            printf("ppu\n");
            // 执行绘图
            ppu->draw();
            
            usleep(1000 * (1000/60));
            
        }while(callback());
    }
    
    
    
    class Nes {
        
    public:
        
        Nes()
        {
            setDebug(false);
        }
        
        void stop(std::function<void()> stopedCallback)
        {
            _stoped = true;
            _stopedCallback = stopedCallback;
        }
        
        // 加载rom
        void loadRom(const uint8_t* rom, size_t length)
        {
            // 解析头文件
            int rom16kB_count = rom[4];
            int vrom8kB_count = rom[5];
            
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
            
            // 将rom载入内存
            for (int i=0; i<rom16kB_count; i++)
            {
                const uint8_t* romAddr = &rom[16];
                
                memcpy(_mem.masterData() + PRG_ROM_LOWER_BANK_OFFSET, romAddr, 1024*16);
//                _mem.writeData(PRG_ROM_LOWER_BANK_OFFSET, romAddr, 1024*16);
                
                // 如果只有1个16kB的bank，则需要再复制一份到0xC000处，让中断向量能够在0xFFFA出现
                if (rom16kB_count == 1)
                {
                    memcpy(_mem.masterData() + PRG_ROM_UPPER_BANK_OFFSET, romAddr, 1024*16);
//                    _mem.writeData(PRG_ROM_UPPER_BANK_OFFSET, romAddr, 1024*16);
                }
            }
            
            // 将图案表数据载入VRAM
            for (int i=0; i<vrom8kB_count; i++)
            {
                const uint8_t* vromAddr = &rom[16]+1024*16*rom16kB_count;
                
                memcpy(_ppu.vram()->masterData(), vromAddr, 1024*8);
            }
        }

        
        // 执行当前指令
        void run() {
            
            _stoped = false;
            
            // 初始化cpu
            _cpu.init(&_mem);
            _ppu.init(&_mem);
            
            
            
            const static int f = 1024*1000*1.79; // 1.79Mhz
            const static double t = 1.0/f; // 时钟周期 0.000000545565642
            const static double cpu_cyles = 1 * t; // 0.000000572067039 上面差不多
            
            const static double ns = 1.0 / pow(10, 9); // 0.000000001
            //        double t = ns * 1000 / 1.79;
            
            auto t0=std::chrono::system_clock::now();
            
            std::thread cpu_thread(cpu_working, &_cpu, [this, &t0](int cyles){
                
                double p_t = cyles * cpu_cyles;  // 执行指令所需要花费时间
                
                auto t1=std::chrono::system_clock::now();
                auto d=std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0); // 实际花费实际，纳秒
                double d_t = d.count() * ns;
                
                double dd = p_t - d_t; // 纳秒差
                
//                            printf("实际时间差 %f\n", dd);
                if (this->debug)
                {
                    dd = this->cmd_interval * 1000 * 1000 * 1000; // 将秒转换成纳秒个数
                }
                if (dd > 0)
                {
                    usleep(dd / 1000);
                }
                
                t0 = t1; // 记录当前时间
                
                return cpu_callback(&_cpu) && !_stoped;
            });
            
            std::thread ppu_thread(ppu_working, &_ppu, [this](){
                
                return ppu_callback(&_ppu) && !_stoped;
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
            
            if (_stopedCallback != 0)
            {
                _stopedCallback();
            }
        }
        
        void setDebug(bool debug)
        {
            this->debug = debug;
            
            setLogEnabled(debug);
            
            log("设置debug模式: %d\n", debug);
        }

        bool debug = false;
        float cmd_interval = 0;
        

        CPU* cpu()
        {
            return &_cpu;
        }
        
        PPU* ppu()
        {
            return &_ppu;
        }
        
        Memory* mem()
        {
            return &_mem;
        }
        
        
        
        std::function<bool(CPU*)> cpu_callback;
        std::function<bool(PPU*)> ppu_callback;
        
    private:
        
        CPU _cpu;
        PPU _ppu;
        Memory _mem;
        
        bool _stoped;
        std::function<void()> _stopedCallback;
    };
    
    
}
