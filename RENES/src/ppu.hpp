
#include "mem.hpp"
#include "vram.hpp"
#include "mem.hpp"

namespace ReNes {
    
    /*
     未完成：
     1、VBlank 逐行发送模拟，目前是绘完最后发送
     2、0x2000第5bit开启8x16精灵
     
     */

    // 2C02
    class PPU {
        
    public:
        
        bit8* io_regs; // I/O 寄存器, 8 x 8bit
        
//        uint8_t VRAM[1024*16]; // 16kb
        
        uint8_t* sprram()
        {
            return _sprram;
        }
        
        VRAM* vram()
        {
            return &_vram;
        }
        
        PPU()
        {
            // 检查数据尺寸
            assert(4 == sizeof(Sprite));
            
            // RGB 数据缓冲区
            _buffer = (uint8_t*)malloc(width()*height() * bpp());
            
            reset();
        }
        
        ~PPU()
        {
            if (_buffer != 0)
                free(_buffer);
        }
        
        void init(Memory* mem)
        {
            _mem = mem;
            io_regs = (bit8*)mem->getIORegsAddr(); // 直接映射地址，不走mem请求，因为mem读写请求模拟了内存访问限制，那部分是留给cpu访问的
            
            std::function<void(uint16_t, uint8_t)> writtingObserver = [this](uint16_t addr, uint8_t value){
                
                
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

                
                // 接收来自2007的数据
                switch (addr) {
                        
                    case 0x2003:
                    {
                        dstAddrWriting(_dstWrite2003, _dstAddr2004tmp, _dstAddr2004);
                        
                        if (_dstWrite2003 == 1)
                            _sprramWritingEnabled = true;
                        break;
                    }
                    case 0x2004:
                    {
                        if (_sprramWritingEnabled)
                        {
                            _sprram[_dstAddr2004++] = value;
//                            _dstWrite2003 = 0;
                        }
                        
                        break;
                    }
                    
                    case 0x2006:
                    {
//                        if (value == 0x20)
//                            log("fuck\n");
                        
                        dstAddrWriting(_dstWrite2006, _dstAddr2007tmp, _dstAddr2007);
                        
                        break;
                    }
                    case 0x2007:
                    {
                        log("向 %X 写数据 %d!\n", _dstAddr2007, value);
                        
                        _vram.write8bitData(_dstAddr2007, value);
                        
                        // 在每一次向$2007写数据后，地址会根据$2000的第二个bit位增加1或者32
                        _dstAddr2007 += io_regs[0].get(2) == 0 ? 1 : 32;
                        
//                        _dstWrite2006 = 0; // 每次写入2007之后，移动2006写入高位地址（可能是这样，也有可能没有这么处理）
                        
                        break;
                    }
                    case 0x4014:
                    {
                        memcpy(_sprram, &_mem->masterData()[value << 8], 256);
                        break;
                    }
                    default:
                        break;
                }
            };
            
            // 精灵读写监听
            mem->addWritingObserver(0x2003, writtingObserver);
            mem->addWritingObserver(0x2004, writtingObserver);
            
            // vram读写监听
            mem->addWritingObserver(0x2006, writtingObserver);
            mem->addWritingObserver(0x2007, writtingObserver);
            
            // DMA拷贝监听
            mem->addWritingObserver(0x4014, writtingObserver);
        }
        
        void draw()
        {
            // 等待数据准备好才开始绘图
//            _sem.wait();
            
            /*
             0x2000 > write
             7  bit  0
             ---- ----
             VPHB SINN
             |||| ||||
             |||| ||++- Base nametable address                                          (ok)
             |||| ||    (0 = $2000; 1 = $2400; 2 = $2800; 3 = $2C00)
             |||| |+--- VRAM address increment per CPU read/write of PPUDATA            (ok)
             |||| |     (0: add 1, going across; 1: add 32, going down)
             |||| +---- Sprite pattern table address for 8x8 sprites                    (ok)
             ||||       (0: $0000; 1: $1000; ignored in 8x16 mode)
             |||+------ Background pattern table address (0: $0000; 1: $1000)           (ok)
             ||+------- Sprite size (0: 8x8; 1: 8x16)
             |+-------- PPU master/slave select
             |          (0: read backdrop from EXT pins; 1: output color on EXT pins)
             +--------- Generate an NMI at the start of the                             (ok)
             vertical blanking interval (0: off; 1: on)
             
             */
            
            // 绘制背景
            // 从名称表里取当前绘制的tile（1字节）
            int nameTableIndex = io_regs[0].get(0) | (io_regs[0].get(1) << 1); // 前2bit决定基础名称表地址
            uint8_t* nameTableAddr = &_vram.masterData()[0x2000 + nameTableIndex*0x0400];
            uint8_t* attributeTableAddr = &_vram.masterData()[0x23C0 + nameTableIndex*0x0400];
            uint8_t* bkPetternTableAddr = &_vram.masterData()[0x1000 * io_regs[0].get(4)]; // 第4bit决定背景图案表地址
            uint8_t* sprPetternTableAddr = &_vram.masterData()[0x1000 * io_regs[0].get(3)]; // 第3bit决定精灵图案表地址
            
            uint8_t* bkPaletteAddr = &_vram.masterData()[0x3F00];   // 背景调色板地址
            uint8_t* sprPaletteAddr = &_vram.masterData()[0x3F10]; // 精灵调色板地址
            
            
            
            
            std::function<void(int, int, int, uint8_t*, uint8_t*, bool, bool)> drawTile = [this, bkPaletteAddr, sprPaletteAddr](int x, int y, int high2, uint8_t* tileAddr, uint8_t* paletteAddr, bool flipH, bool flipV)
            {
                
                for (int ty=0; ty<8; ty++)
                {
                    for (int tx=0; tx<8; tx++)
                    {
                        // tile 第一字节与第八字节对应的bit位，组成这一像素颜色的低2位（最后构成一个[0,63]的数，索引到系统默认的64种颜色）
                        int low0 = ((bit8*)&tileAddr[ty])->get(tx);
                        int low1 = ((bit8*)&tileAddr[ty+8])->get(tx);
                        int low2 = low0 | (low1 << 1);
                        
                        int paletteUnitIndex = (high2 << 2) + low2; // 背景调色板单元索引号
                        int systemPaletteUnitIndex = paletteAddr[paletteUnitIndex]; // 系统默认调色板颜色索引 [0,63]
                        
                        // 如果绘制精灵，而且当前颜色引用到背景透明色（背景调色板第一位），就不绘制
                        if (paletteAddr == sprPaletteAddr && systemPaletteUnitIndex == bkPaletteAddr[0])
                        {
                            continue;
                        }
                        
                        int tx_ = flipH ? tx : 7-tx;
                        int ty_ = flipV ? 7-ty : ty;
                        
                        int pixelIndex = NES_MIN(y + ty_, 239) * 32*8*3 + NES_MIN(x+tx_, 255) *3; // 最低位是右边第一像素，所以渲染顺序要从右往左
                        
                        RGB* rgb = (RGB*)&defaultPalette[systemPaletteUnitIndex*3];
                        
                        *(RGB*)&_buffer[pixelIndex] = *rgb;
                        
                        //                            log("colorIndex %d\n", systemPaletteUnitIndex);
                    }
                }
            };
            

            
            for (int y=0; y<30; y++) // 只显示 30/30 行tile
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
                    
                    
                    int tileIndex = nameTableAddr[y*32 + x]; // 得到 bkg tile index
//                    tileIndex = 0;
                    
                    // 确定在图案表里的tile地址，每个tile是8x8像素，占用16字节。
                    // 前8字节(8x8) + 后8字节(8x8)
                    uint8_t* tileAddr = &bkPetternTableAddr[tileIndex * 16];

                    drawTile(x*8, y*8, high2, tileAddr, bkPaletteAddr, false, false);
                    
                }
            }
            
            // 绘制精灵
            for (int i=0; i<64; i++)
            {
                Sprite* spr = (Sprite*)&_sprram[i*4];
                if (*(int*)spr == 0) // 没有精灵数据
                    continue;
                
//                spr->y = 0;
                
                // 目前只处理8x8的精灵
//                for (int j=0; j<10; j++)
//                {
//                    uint8_t* tileAddr = &sprPetternTableAddr[j * 16];;//&petternTableAddr[spr->tileIndex * 16];
//                    
//                    int high2 = spr->info.get(0) | (spr->info.get(1) << 1);
//                    drawTile(spr->x + 1 + j*10, spr->y + 1, high2, tileAddr, sprPaletteAddr);
//                }
                
                uint8_t* tileAddr = &sprPetternTableAddr[spr->tileIndex * 16];
                
                int high2 = spr->info.get(0) | (spr->info.get(1) << 1);
                bool flipH = spr->info.get(6);
                bool flipV = spr->info.get(7);
                
//                int high2 = (spr->info.get(0) << 1) | spr->info.get(1);
                drawTile(spr->x + 1, spr->y + 1, high2, tileAddr, sprPaletteAddr, flipH, flipV);
            }

            
            // 绘制完成，等于发生了 VBlank，需要设置 2002 第7位（其实是每一行扫描线完成就会触发）
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
        
//        void drawTile(int x, int y, int high2, uint8_t* tileAddr, uint8_t* paletteAddr)
//        {
//            
//            for (int ty=0; ty<8; ty++)
//            {
//                for (int tx=0; tx<8; tx++)
//                {
//                    // tile 第一字节与第八字节对应的bit位，组成这一像素颜色的低2位（最后构成一个[0,63]的数，索引到系统默认的64种颜色）
//                    int low0 = ((bit8*)&tileAddr[ty])->get(tx);
//                    int low1 = ((bit8*)&tileAddr[ty+8])->get(tx);
//                    int low2 = low0 | (low1 << 1);
//                    
//                    int paletteUnitIndex = (high2 << 2) + low2; // 背景调色板单元索引号
//                    int systemPaletteUnitIndex = paletteAddr[paletteUnitIndex]; // 系统默认调色板颜色索引 [0,63]
//                    
//                    int pixelIndex = NES_MIN(y + ty, 239) * 32*8*3 + NES_MIN(x+(7-tx), 255) *3; // 最低位是右边第一像素，所以渲染顺序要从右往左
//                    
//                    RGB* rgb = (RGB*)&defaultPalette[systemPaletteUnitIndex*3];
//                    
//                    *(RGB*)&_buffer[pixelIndex] = *rgb;
//                    
//                    //                            log("colorIndex %d\n", systemPaletteUnitIndex);
//                }
//            }
//        }
        
        struct Sprite {
            
            uint8_t y;
            uint8_t tileIndex;  // 精灵在图案表里的tile索引
            bit8 info; // 精灵的信息
            uint8_t x;
        };
        
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
            0,0,0, 0,0,0, 0,0,0,
            
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
            0,0,0, 0,0,0, 0,0,0,
            
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
            60,60,60, 0,0,0, 0,0,0,
            
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
            160,162,160, 0,0,0, 0,0,0,
        };
        
        void reset()
        {
            _dstWrite2003 = 0;
            _dstAddr2004tmp = 0;
            _dstAddr2004 = 0;
            _sprramWritingEnabled = false;
            _dstWrite2006 = 0;
            _dstAddr2007 = 0;
            
            memset(_sprram, 0, 256);
        }
        
        int _dstWrite2003 = 0;
        uint16_t _dstAddr2004tmp = 0;
        uint16_t _dstAddr2004 = 0;
        bool _sprramWritingEnabled = false;
        
        int _dstWrite2006 = 0;
        uint16_t _dstAddr2007tmp = 0;
        uint16_t _dstAddr2007 = 0;
        
        uint8_t* _buffer = 0;
        
        uint8_t _sprram[256]; // 精灵内存, 64 个，每个4字节
        VRAM _vram;
        Memory* _mem;
    };
}
