
#include "cpu.hpp"
#include "ppu.hpp"
#include "control.hpp"

#include <functional>
#include <stdio.h>
#include "type.hpp"
#include <thread>
#include <unistd.h>
#include <chrono>

#include <mutex>
#include <condition_variable>

class Semaphore {
public:
    Semaphore (int count_ = 0)
    : count(count_) {}
    
    inline void notify()
    {
        std::unique_lock<std::mutex> lock(mtx);
        count++;
        cv.notify_one();
    }
    
    inline void wait()
    {
        std::unique_lock<std::mutex> lock(mtx);
        
        while(count == 0){
            cv.wait(lock);
        }
        count--;
    }
    
private:
    std::mutex mtx;
    std::condition_variable cv;
    int count;
};


namespace ReNes {

    class Nes {
        
    public:
        
        Nes()
        {
            setDebug(false);
        }
        
        ~Nes()
        {
            // 等待线程退出
            //            _stopMutex.lock();
            
            stop();
            
            _runningThread.join();
            
            printf("Nes即将析构\n");
        }
        
        void stop()
        {
            _stoped = true;
            
            //            _stopedCallback = stopedCallback;
        }
        
        // 加载rom
        void loadRom(const uint8_t* rom, size_t length)
        {
            // 解析头文件
            int rom16kB_count = rom[4];
            int vrom8kB_count = rom[5];
            
            bit8 b8_6 = *(bit8*)&rom[6];
            bit8 b8_7 = *(bit8*)&rom[7];
            
            printf("文件长度 %d\n", length);
            
            printf("[4] 16kB ROM: %d\n\
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
            
            _runningThread = std::thread(runWrapper, this);
        }
        
        void setDebug(bool debug)
        {
            this->debug = debug;
            
            _cpu.debug = debug;
            setLogEnabled(debug);
            
            log("设置debug模式: %d\n", debug);
        }
        
        void setFps(int fps)
        {
            _ppu.fps = fps;
        }
        
        //        void setDumpScrollBuffer(bool enabled)
        //        {
        //            _ppu.dumpScrollBuffer = enabled;
        //        }
        
        bool dumpScrollBuffer = false;
        
        int fps()
        {
            return _ppu.fps;
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
        
        long cmdTime() const
        {
            return _cmdTime;
        }
        
        long renderTime() const
        {
            return _renderTime;
        }
        
        // 回调函数
        std::function<bool(CPU*)> cpu_callback;
        std::function<bool(PPU*)> ppu_callback;
        std::function<void()> willRunning;
        
        bool isRunning() const {return _isRunning;};
        
    private:
        
        static
        void runWrapper(Nes* nes) {
            
            nes->_run();
        }
        
        void _run() {
            
            //            _stopMutex.lock();
            
            _stoped = false;
            
            // 初始化cpu
            _cpu.init(&_mem);
            _ppu.init(&_mem);
            
            _isRunning = true;
            if (willRunning)
                willRunning();
            
            //-----------------------------------
            // 控制器处理
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
            //-----------------------------------
            
//            const static double CPUFrequency = 1.789773; // 1.79Mhz
//            const static uint32_t cpu_cyles = (1.0/(1024*1000*CPUFrequency)) * 1e9 ; // 572 ns
            
            // CPU周期线程
            std::thread cpu_thread = std::thread([this](){
                
                bool isFirstCPUCycleForFrame = true;
                std::chrono::steady_clock::time_point firstTime;
                
                uint32_t cpuCyclesCountForFrame = 0;
                uint32_t ppuCyclesCountForScanline = 0;
                const uint32_t NumCyclesPerScanline = 341 / 3;
                const uint32_t TimePerFrame = 1.0 / fps() * 1e9; // 每帧需要的时间(纳秒)
                
                // NTSC
                const uint32_t NumScanline = 262;
                
                // 主循环
                do {
                    
                    // 第一帧开始进行初始化
                    if (isFirstCPUCycleForFrame)
                    {
                        firstTime = std::chrono::steady_clock::now();
                        _ppu.readyOnFirstLine();
                        isFirstCPUCycleForFrame = false;
                    }
                    
                    // 执行指令
                    int cyles = _cpu.exec();
                    
                    if (_cpu.error)
                        break;
                    
                    cpuCyclesCountForFrame += cyles;
                    ppuCyclesCountForScanline += cyles;
                    
                    if (ppuCyclesCountForScanline >= NumCyclesPerScanline)
                    {
                        ppuCyclesCountForScanline -= NumCyclesPerScanline;
                        bool vblankEvent = _ppu.drawScanline();
                        if (vblankEvent)
                            _cpu.interrupts(CPU::InterruptTypeNMI);
                        
                        if (_ppu.isOverScanline())
                        {
                            _displaySem.unlock(); // 显示图像
                            
                            // 每帧等待
                            auto dif_ns = (std::chrono::steady_clock::now() - firstTime).count(); // 纳秒
                            if ( dif_ns < TimePerFrame )
                                usleep( (uint32_t)(TimePerFrame - dif_ns) / 1000 );
                            
                            // 计数器重置
                            cpuCyclesCountForFrame -= NumCyclesPerScanline * NumScanline;
                            isFirstCPUCycleForFrame = true;
                        }
                    }
                    
                }while(cpu_callback(&_cpu) && !_stoped);
                
            });
            
            std::thread display_thread = std::thread([this](){
                
                do {
                    
                    _displaySem.lock();
                    
                    if (dumpScrollBuffer)
                        _ppu.dumpScrollToBuffer();
                    
                    ppu_callback(&_ppu);
                    
                }while(!_stoped);
            });
            
            // 等待线程结束
            cpu_thread.join();
            _displaySem.unlock(); // ppu线程结束时再次通知
            display_thread.join();
            
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
        
        
        bool _isRunning = false;
        
        // 硬件
        CPU _cpu;
        PPU _ppu;
        Memory _mem;
        Control _ctr;
        
        long _cmdTime;
        long _renderTime;
        
        bool _stoped;
        std::function<void()> _stopedCallback;
        
        std::thread _runningThread;
        
        std::mutex _displaySem;
    };
    
    
}
