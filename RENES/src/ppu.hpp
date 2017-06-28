
#include "mem.hpp"

namespace ReNes {

    // 2C02
    class PPU {
        
    public:
        
        uint8_t* io_regs; // I/O 寄存器, 8 x 8bit
        
        struct {
            
            
            
        private:
            uint8_t _data[1024*16]; // 16kb
            
        }VRAM;
        
        
        PPU()
        {
            
        }
        
        void init(Memory* mem)
        {
            io_regs = mem->getRealAddr(0x2000);
            
            std::function<void(uint16_t)> writtingObserver = [](uint16_t addr){
                
                
            };
            
            
            
            mem->addWritingObserver(0x4014, writtingObserver);
        }
        
        
        
    private:
        
        
        uint8_t _sprram[256]; // 精灵内存
        
        
    };
}
