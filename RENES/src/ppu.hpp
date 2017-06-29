
#include "mem.hpp"
#include "semaphore.hpp"

namespace ReNes {

    // 2C02
    class PPU {
        
    public:
        
        bit8* io_regs; // I/O 寄存器, 8 x 8bit
        
        uint8_t VRAM[1024*16]; // 16kb
        
        
        PPU():_sem(0)
        {
            // RGB 数据缓冲区
            _buffer = (uint8_t*)malloc(width()*height() * bpp());
        }
        
        ~PPU()
        {
            if (_buffer != 0)
                free(_buffer);
        }
        
        void init(Memory* mem)
        {
            io_regs = (bit8*)mem->getIORegsAddr();
            
            std::function<void(uint16_t)> writtingObserver = [this](uint16_t addr){
                
                // 接收来自2007的数据
                switch (addr) {
                    case 0x2006:
                    {
                        int a7 = *(uint8_t*)&io_regs[6];
                        
                        _dstAddr2007 |= (*((uint8_t*)&io_regs[6]) << _2006I*8);
                        
                        _2006I = (_2006I+1) % 2;
                        
                        break;
                    }
                    case 0x2007:
                    {
                        uint8_t data = *(uint8_t*)&io_regs[7];
                        
                        log("向 %X 写数据 %d!\n", _dstAddr2007, data);
                        
                        VRAM[_dstAddr2007] = data;
                        
                        // 在每一次向$2007写数据后，地址会根据$2000的第二个bit位增加1或者32
                        _dstAddr2007 += io_regs[0].get(1) == 0 ? 1 : 32;
                        
                        _2006I = 1; // 每次写入2007之后，移动2006写入高位地址（可能是这样，也有可能没有这么处理）
                        
                        break;
                    }
                    default:
                        break;
                }
            };
            
            
            // 设置内存写入监听器
            mem->addWritingObserver(0x2000, writtingObserver);
            mem->addWritingObserver(0x2001, writtingObserver);
            mem->addWritingObserver(0x2006, writtingObserver);
            mem->addWritingObserver(0x2007, writtingObserver);
            
            mem->addWritingObserver(0x4014, writtingObserver);
        }
        
        void draw()
        {
            // 等待数据准备好才开始绘图
//            _sem.wait();
            for (int y=0; y<height(); y++)
            {
                for (int x=0; x<width(); x++)
                {
                    int i = (y*width()+x)*bpp();
                    _buffer[i] = 0xff;
                    _buffer[i+1] = 0;
                    _buffer[i+2] = 0;
                }
            }
            
            // 绘制完成，等于发生了 VBlank，需要设置 2002 第7位
            io_regs[2].set(7, 1);
        }
        
        int width()
        {
            return 256;
        }
        
        int height()
        {
            return 240;
        }
        
        int bpp()
        {
            return 3;
        }
        
        uint8_t* buffer()
        {
            return _buffer;
        }
        
    private:
        
        int _2006I = 1;
        uint16_t _dstAddr2007;
        
        uint8_t* _buffer = 0;
        
        uint8_t _sprram[256]; // 精灵内存
        Semaphore _sem;
        
    };
}
