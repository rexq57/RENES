
#include "cpu.hpp"
#include "ppu.hpp"
#include "control.hpp"

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
//            for (int i=0; i<rom16kB_count; i++)
//            {
//                const uint8_t* romAddr = &rom[16];
//                
//                memcpy(_mem.masterData() + PRG_ROM_LOWER_BANK_OFFSET, romAddr, 1024*16);
//                
//                // 如果只有1个16kB的bank，则需要再复制一份到0xC000处，让中断向量能够在0xFFFA出现
//                if (rom16kB_count == 1)
//                {
//                    memcpy(_mem.masterData() + PRG_ROM_UPPER_BANK_OFFSET, romAddr, 1024*16);
//                }
//            }
            if (rom16kB_count == 1)
            {
                const uint8_t* romAddr = &rom[16];
                
                memcpy(_mem.masterData() + PRG_ROM_LOWER_BANK_OFFSET, romAddr, 1024*16);
                
                // 如果只有1个16kB的bank，则需要再复制一份到0xC000处，让中断向量能够在0xFFFA出现
                memcpy(_mem.masterData() + PRG_ROM_UPPER_BANK_OFFSET, romAddr, 1024*16);
            }
            else
            {
                for (int i=0; i<rom16kB_count && i < 2; i++)
                {
                    const uint8_t* romAddr = &rom[16 + 1024*16*i];
                    
                    memcpy(_mem.masterData() + PRG_ROM_LOWER_BANK_OFFSET + 1024*16*i, romAddr, 1024*16);
                }
            }
            
            
            // 将图案表数据载入VRAM
            for (int i=0; i<vrom8kB_count; i++)
            {
                const uint8_t* vromAddr = &rom[16+1024*16*rom16kB_count + 1024*8*i];
                
                memcpy(_ppu.vram()->masterData(), vromAddr, 1024*8);
            }
        }

        
        // 执行当前指令
        void run() {
            
            _stoped = false;
            
            // 初始化cpu
            _cpu.init(&_mem);
            _ppu.init(&_mem);
            
            _isRunning = true;
            if (willRunning)
                willRunning();
            
            // 控制器处理
            {
                int dstWrite4016 = 0;
                uint16_t dstAddr4016tmp = 0;
                uint16_t dstAddr4016 = 0;
                
                _mem.addWritingObserver(0x4016, [this, &dstWrite4016, &dstAddr4016tmp,  &dstAddr4016](uint16_t addr, uint8_t value){
                    
                    // 写0x4016 2次，以设置从0x4016读取的硬件信息
                    std::function<void(int&,uint16_t&, uint16_t&)> dstAddrWriting = [value](int& dstWrite, uint16_t& dstAddrTmp, uint16_t& dstAddr){
                        
                        //                    int writeBitIndex = dstWrite;
                        //                    dstWrite = (dstWrite+1) % 2;
                        //
                        //
                        //                    dstAddr &= (0xFF << dstWrite*8); // 清理高/低位
                        //                    dstAddr |= (value << writeBitIndex*8); // 设置对应位
                        
                        int writeBitIndex = (dstWrite+1) % 2;
                        
                        dstAddrTmp &= (0xFF << dstWrite*8); // 清理相反的高/低位
                        dstAddrTmp |= (value << writeBitIndex*8); // 设置对应位
                        
                        dstWrite = (dstWrite+1) % 2;
                        
                        if (dstWrite == 0)
                            dstAddr = dstAddrTmp;
                    };
                    
                    dstAddrWriting(dstWrite4016, dstAddr4016tmp, dstAddr4016);
                    
                    // 每次重新请求控制器的时候，重置按键
//                    if (dstWrite4016 == 1 && dstAddr4016 == 0x100)
                    if (dstWrite4016 == 0 && dstAddr4016 == 0x100)
                    {
                        _ctr.reset();
                    }
                        
                });
                
                _mem.addReadingObserver(0x4016, [this, &dstWrite4016, &dstAddr4016](uint16_t addr, uint8_t* value, bool* cancel){
                    
                    if (dstAddr4016 == 0x100)
                    {
                        *value = _ctr.nextKeyStatue();
                        
//                        dstWrite4016 = 0;
                    }
                });
            }
            
            const static int f = 1024*1000*1.79; // 1.79Mhz
            const static double t = 1.0/f; // 时钟周期 0.000000545565642
            const static double cpu_cyles = 1 * t; // 0.000000572067039 上面差不多
            
            const static double ns = 1.0 / pow(10, 9); // 0.000000001
            
            // cpu线程
            std::thread cpu_thread;
            {
                
                //        double t = ns * 1000 / 1.79;
                
                auto t0=std::chrono::system_clock::now();
                
                cpu_thread = std::thread(cpu_working, &_cpu, [this, &t0](int cyles){
                    
                    double p_t = cyles * cpu_cyles;  // 执行指令所需要花费时间
                    
                    auto t1=std::chrono::system_clock::now();
                    auto d=std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0); // 实际花费实际，纳秒
                    double d_t = d.count() * ns;
                    
                    double dd = p_t - d_t; // 纳秒差
                    
                    //                            printf("实际时间差 %f\n", dd);
                    _cmdTime = dd;
                    if (this->debug)
                    {
                        dd = this->cmd_interval * 1000 * 1000 * 1000; // 将秒转换成纳秒个数
                    }
                    if (dd > 0)
                    {
                        usleep(dd * 1000000);
                        
//                        t0 = std::chrono::system_clock::now();
                    }
//                    else
                    {
                        t0 = t1; // 记录当前时间
                    }
                    
                    
                    return cpu_callback(&_cpu) && !_stoped;
                });
            }
            
            std::thread ppu_thread;
            {
                const double p_t = 1.0 / 60;
                
                auto t0=std::chrono::system_clock::now();
                
                ppu_thread = std::thread(ppu_working, &_ppu, [this, &t0, &p_t](){
                    
                    
                    
                    auto t1=std::chrono::system_clock::now();
                    auto d=std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0); // 实际花费时间，纳秒
                    double d_t = d.count() * ns;
                    
                    double dd = p_t - d_t; // 纳秒差
                    
                    _renderTime = d_t;
                    
                    if (dd > 0)
                    {
                        usleep(dd * 1000000);
                        
                        t0 = std::chrono::system_clock::now();
                    }
                    else
                    {
                        t0 = t1; // 记录当前时间
                    }
                    
                    bool res = ppu_callback(&_ppu); // 先进行更新，这里放到后面会闪屏，不知道什么原因
                    
                    _cpu.interrupts(CPU::InterruptTypeNMI); // 每次VBlank发生在最后一行，就是绘制完一帧的时候，通知CPU执行NMI中断
                    
                    return res && !_stoped;
                });
            }
            
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
            
            _cpu.debug = debug;
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
        
        Control* ctr()
        {
            return &_ctr;
        }
        
        double cmdTime()
        {
            return _cmdTime;
        }
        
        double renderTime()
        {
            return _renderTime;
        }
        
        std::function<bool(CPU*)> cpu_callback;
        std::function<bool(PPU*)> ppu_callback;
        std::function<void()> willRunning;
        
        bool isRunning() const {return _isRunning;};
        
    private:
        
        bool _isRunning = false;
        
        // 硬件
        CPU _cpu;
        PPU _ppu;
        Memory _mem;
        Control _ctr;
        
        double _cmdTime;
        double _renderTime;
        
        bool _stoped;
        std::function<void()> _stopedCallback;
    };
    
    
}
