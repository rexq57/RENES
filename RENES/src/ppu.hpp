
#include "mem.hpp"
#include "semaphore.hpp"

namespace ReNes {

    // 2C02
    class PPU {
        
    public:
        
        bit8* io_regs; // I/O 寄存器, 8 x 8bit
        
        struct {
            
            
            
        private:
            uint8_t _data[1024*16]; // 16kb
            
        }VRAM;
        
        
        PPU():_sem(0)
        {
            
        }
        
        void init(Memory* mem)
        {
            io_regs = (bit8*)mem->getIORegsAddr();
            
            std::function<void(uint16_t)> writtingObserver = [](uint16_t addr){
                
                
            };
            
            
            // 设置内存写入监听器
            mem->addWritingObserver(0x2000, writtingObserver);
            mem->addWritingObserver(0x2001, writtingObserver);
            mem->addWritingObserver(0x4014, writtingObserver);
        }
        
        void draw()
        {
            // 等待数据准备好才开始绘图
//            _sem.wait();
            for (int i=0; i<100; i++)
            {
                
                
            }
            
            // 绘制完成，等于发生了 VBlank，需要设置 2002 第7位
            io_regs[2].set(7, 1);
        }
        

        
        
    private:
        
        
        uint8_t _sprram[256]; // 精灵内存
        Semaphore _sem;
        
    };
}
