
#include "mem.hpp"
#include "semaphore.hpp"
#include "vram.hpp"

namespace ReNes {

    // 2C02
    class PPU {
        
    public:
        
        bit8* io_regs; // I/O 寄存器, 8 x 8bit
        
//        uint8_t VRAM[1024*16]; // 16kb
        VRAM vram;
        
        
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
            io_regs = (bit8*)mem->getIORegsAddr(); // 直接映射地址，不走mem请求，因为mem读写请求模拟了内存访问限制，那部分是留给cpu访问的
            
            std::function<void(uint16_t, uint8_t)> writtingObserver = [this](uint16_t addr, uint8_t value){
                
                // 接收来自2007的数据
                switch (addr) {
                    case 0x2006:
                    {
//                        int a7 = *(uint8_t*)&io_regs[6];
                        
//                        if (value == 0x20)
//                            log("fuck");
                        
                        

                        int writeBitIndex = _2006I;
                        _2006I = (_2006I+1) % 2;
                        
                        
                        _dstAddr2007 &= (0xFF << _2006I*8); // 清理高/低位
                        _dstAddr2007 |= (value << writeBitIndex*8); // 设置对应位
                        
                        
                        break;
                    }
                    case 0x2007:
                    {
//                        uint8_t data = *(uint8_t*)&io_regs[7];
                        
                        log("向 %X 写数据 %d!\n", _dstAddr2007, value);
                        
//                        VRAM[_dstAddr2007] = value;
                        vram.write8bitData(_dstAddr2007, value);
                        
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
            
            // 绘制背景
            // 从名称表里取当前绘制的tile（1字节）
            int nameTableIndex = 0;
            uint8_t* nameTableAddr = &vram.masterData()[0x2000 + nameTableIndex*0x0400];
            uint8_t* attributeTableAddr = &vram.masterData()[0x23C0 + nameTableIndex*0x0400];
            uint8_t* petternTableAddr = &vram.masterData()[0];
            
            uint8_t* paletteAddr = &vram.masterData()[0x3F00]; // 背景调色板地址
            
            
            for (int y=1; y<29; y++) // 只显示 28/30 行tile，首尾两行不显示
            {
                for (int x=0; x<32; x++)
                {
                    // 一个属性4字节，作用在8x8 tile里，所以需要将tiles分成4x4个单元
                    uint8_t* attributeAddr = &attributeTableAddr[(y % 8) * 4*4 + (x % 8)*4]; /* 一个属性行是4个属性，每个4字节，所以是4 x 4的字节数 */
                    // 再细分到 4x4 tile内 (1/4 属性 1字节)
                    uint8_t* attributeAddrFor4x4Tile = &attributeAddr[((y % 8) % 2) * 2*4  + ((x % 8) % 2)*1];/* 半个属性行只有 2 * 4 字节数 */
                    
                    // 再细分到属性内部决定该tile的bit位
                    int bit = (((y%8)%2)%2) * 4 + (((x%8)%2)%2) * 2;
                    // 在 bit 位取 2bit 作为该tile颜色的高2位
                    int high2 = (*attributeAddrFor4x4Tile >> bit) & 0x3;
                    
                    
                    uint8_t tileIndex = nameTableAddr[y*32 + x]; // 得到 bkg tile index
                    
                    
                    // 确定在图案表里的tile地址，每个tile是8x8像素，占用16字节。
                    // 前8字节(8x8) + 后8字节(8x8)
                    uint8_t* tileAddr = &petternTableAddr[tileIndex * 16];
                    
                    for (int ty=0; ty<8; ty++)
                    {
                        for (int tx=0; tx<8; tx++)
                        {
                            // tile 第一字节与第八字节对应的bit位，组成这一像素颜色的低2位（最后构成一个[0,63]的数，索引到系统默认的64种颜色）
                            int low2 = ((bit8*)&tileAddr[ty])->get(tx) | ((bit8*)&tileAddr[ty+8])->get(tx);
                            
                            int paletteUnitIndex = (high2 << 2) + low2;
                            int systemPaletteUnitIndex = paletteAddr[paletteUnitIndex]; // 系统默认调色板颜色索引 [0,63]
                            
                            int pixelIndex = (y*8 + ty) * 32*8*3 + (x*8+tx) *3;
                            
                            RGB* rgb = (RGB*)&defaultPalette[systemPaletteUnitIndex*3];
                            
                            *(RGB*)&_buffer[pixelIndex] = *rgb;
//                            log("colorIndex %d\n", systemPaletteUnitIndex);
                        }
                        
                        // 根据当前
                        
                    }
                }
            }
            
            
            
//            for (int y=0; y<height(); y++)
//            {
//                for (int x=0; x<width(); x++)
//                {
//                    int i = (y*width()+x)*bpp();
//                    _buffer[i] = 0xff;
//                    _buffer[i+1] = 0;
//                    _buffer[i+2] = 0;
//                }
//            }
            
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
        
        struct RGB {
            uint8_t r;
            uint8_t g;
            uint8_t b;
        };

        uint8_t defaultPalette[192] = {
//            84,84,84,0,30,116,8,16,144,48,0,136,68,0,100,92,0,48,84,4,0,60,24,0,32,42,0,8,58,0,0,64,0,0,60,0,0,50,60,0,0,0,
//            152,150,152,8,76,196,48,50,236,92,30,228,136,20,176,160,20,100,152,34,32,120,60,0,84,90,0,40,114,0,8,124,0,0,118,40,0,102,120,0,0,0,
//            236,238,236,76,154,236,120,124,236,176,98,236,228,84,236,236,88,180,236,106,100,212,136,32,160,170,0,116,196,0,76,208,32,56,204,108,56,180,204,60,60,60,
//            236,238,236,168,204,236,188,188,236,212,178,236,236,174,236,236,174,212,236,180,176,228,196,144,204,210,120,180,222,120,168,226,144,152,226,180,160,214,228,160,162,160
            84,84,84,
            0,30,116,
            8,16,144,
            48,0,136,
            68,0,100,
            92,0,48,
            84,4,0,
            60,24,0,
            32,42,0,
            8,58,0,
            0,64,0,
            0,60,0,
            0,50,60,
            0,0,0,
            152,150,152,
            8,76,196,
            48,50,236,
            92,30,228,
            136,20,176,
            160,20,100,
            152,34,32,
            120,60,0,
            84,90,0,
            40,114,0,
            8,124,0,
            0,118,40,
            0,102,120,
            0,0,0,
            236,238,236,
            76,154,236,
            120,124,236,
            176,98,236,
            228,84,236,
            236,88,180,
            236,106,100,
            212,136,32,
            160,170,0,
            116,196,0,
            76,208,32,
            56,204,108,
            56,180,204,
            60,60,60,
            236,238,236,
            168,204,236,
            188,188,236,
            212,178,236,
            236,174,236,
            236,174,212,
            236,180,176,
            228,196,144,
            204,210,120,
            180,222,120,
            168,226,144,
            152,226,180,
            160,214,228,
            160,162,160
        };
        
        
        int _2006I = 1;
        uint16_t _dstAddr2007;
        
        uint8_t* _buffer = 0;
        
        uint8_t _sprram[256]; // 精灵内存
        Semaphore _sem;
        
    };
}
