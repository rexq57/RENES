
#include "mem.hpp"
#include "vram.hpp"
#include "mem.hpp"

namespace ReNes {
    
    /*
     未完成：
     1、0x2000第5bit开启8x16精灵
     
     */
    
    // 系统调色板
    const static uint8_t DEFAULT_PALETTE[192] = {
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

    // 2C02
    class PPU {
        
    public:
        
        bit8* io_regs; // I/O 寄存器, 8 x 8bit
        bit8* mask_regs;    // PPU屏蔽寄存器
        bit8* status_regs;  // PPU状态寄存器
        
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
            printf("%x %x\n", this, &_mem);
            
            _mem = mem;
            io_regs = (bit8*)mem->getIORegsAddr(); // 直接映射地址，不走mem请求，因为mem读写请求模拟了内存访问限制，那部分是留给cpu访问的
            mask_regs = (bit8*)&io_regs[1];
            status_regs = (bit8*)&io_regs[2];
            
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
                        
                    case 0x2000:
                    {
                        _t &= ~(0x3 << 10);
                        _t |= ((value & 0x3) << 10);
                        break;
                    }
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
                            _sprram[_dstAddr2004++ % 256] = value;
//                            _dstWrite2003 = 0;
                        }
                        
                        break;
                    }
                    case 0x2005:
                    {
                        if (_w == 0)
                        {
                            // 低3位写入 _x
                            // 高5位写入 _t 低5位
                            
                            _t &= ~0x1F;
                            _t |= value >> 3;
                            
                            _x = value & 7; // 取低3位
                            
//                            printf("2005 0 %d\n", value);
                        }
                        else
                        {
                            // 低3位写入 _t 12~14位
                            // 高5位写入 _t 位5~9
                            
//                            printf("2005 %d -> %d\n", value, _t);
                            
                            _t &= ~0x73E0; // 清理 111 0011 1110 0000
                            _t |= ((value & 0x7) << 12) | ((value >> 3) << 5);
                            
                            
                            
//                            auto&v = _t;
//                            int bg_offset_x = v & 0x1F;
//                            int bg_offset_y = (v >> 5) & 0x1F;
//                            int bg_t_x = _x;
//                            int bg_t_y = (v >> 12) & 0x7;
//                            int bg_base = (v >> 10) & 0x3;
//                            printf("2005 %d %d %d - %d,%d (%d)\n", _t, bg_offset_x, bg_offset_y, bg_t_x, bg_t_y, bg_base);
                        }
                        
                        
                        
                        _w = 1 - _w;
                        
                        break;
                    }
                    case 0x2006:
                    {
//                        if (value == 0x20)
//                            log("fuck\n");
                        
//                        dstAddrWriting(_dstWrite2006, _dstAddr2007tmp, _dstAddr2007);
                        
                        if (_w == 0)
                        {
                            // 低6位写入 _t 的8~13位 并将14位置零
                            _t &= 0xFF;
                            _t |= (value & 0x3F) << 8;
                        }
                        else
                        {
                            // 将8位全部写入 _t 的低8位
                            _t &= ~0xFF;
                            _t |= (value & 0xFF);
                            
                            _v = _t; // 第二次写入会覆盖 _v
                            
//                            printf("%x\n", _v);
                            _firstRead2007 = true; // 每次_v生效，忽略从2007读取的第一个字节
                        }
                        _w = 1 - _w;
                        

                        break;
                    }
                    case 0x2007:
                    {
                        // 在每一次向$2007写数据后，地址会根据$2000的2bit位增加1或者32
                        _vram.write8bitData(_v, value);
                        _v += io_regs[0].get(2) == 0 ? 1 : 32;
                        
//                        if (value == 0x39)
//                            printf("fuck\n");
//                        printf("2007 %x <= %d\n", _v, value);
//                        printf("2007 %x == %d\n", _v, _vram.read8bitData(_v));
                        
                        break;
                    }
                    case 0x4014:
                    {
                        memcpy(_sprram, &_mem->masterData()[value << 8], 256);
                        break;
                    }
                    default:
                        assert(!"error!");
                        break;
                }
            };
            
            std::function<void(uint16_t, uint8_t*)> readingObserver = [this](uint16_t addr, uint8_t* value)
            {
                switch (addr) {
                    case 0x2002: // 读取 0x2002会重置 _w 状态
                        _w = 0;
                        status_regs->set(7, 0);
                        break;
                    case 0x2007:
                        // 据说第一次读取的值是无效的，会缓冲到下一次读取才返回
                        if (!_firstRead2007)
                        {
                            *value = _vram.read8bitData(_v);
//                            printf("读取 2007 %x - %d\n", _v, *value);
                            _v += io_regs[0].get(2) == 0 ? 1 : 32;
                        }
                        _firstRead2007 = false;
                        
                        break;
                    default:
                        assert(!"error!");
                        break;
                }
            };
            
            /*
             
             Status ($2002) < read
             
             7  bit  0
             ---- ----
             VSO. ....
             |||| ||||
             |||+-++++- Least significant bits previously written into a PPU register
             |||        (due to register not being updated for this address)
             ||+------- Sprite overflow. The intent was for this flag to be set
             ||         whenever more than eight sprites appear on a scanline, but a
             ||         hardware bug causes the actual behavior to be more complicated
             ||         and generate false positives as well as false negatives; see
             ||         PPU sprite evaluation. This flag is set during sprite
             ||         evaluation and cleared at dot 1 (the second dot) of the
             ||         pre-render line.
             |+-------- Sprite 0 Hit.  Set when a nonzero pixel of sprite 0 overlaps
             |          a nonzero background pixel; cleared at dot 1 of the pre-render
             |          line.  Used for raster timing.
             +--------- Vertical blank has started (0: not in vblank; 1: in vblank).
             Set at dot 1 of line 241 (the line *after* the post-render
             line); cleared after reading $2002 and at dot 1 of the
             pre-render line.
             
             */
            
            mem->addWritingObserver(0x2000, writtingObserver); // 写
            mem->addReadingObserver(0x2002, readingObserver); // 读
            
            mem->addReadingObserver(0x2007, readingObserver);
            
            // 精灵读写监听
            mem->addWritingObserver(0x2003, writtingObserver);
            mem->addWritingObserver(0x2004, writtingObserver);
            
            mem->addWritingObserver(0x2005, writtingObserver);
            
            // vram读写监听
            mem->addWritingObserver(0x2006, writtingObserver);
            mem->addWritingObserver(0x2007, writtingObserver);
            
            // DMA拷贝监听
            mem->addWritingObserver(0x4014, writtingObserver);
        }
        
        void draw()
        {
            // clear
            memset(_buffer, 0, width()*height()*3);
            
            status_regs->set(7, 0); // 清除 vblank
            
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
            
            
            const static RGB* pTRANSPARENT_RGB = (RGB*)&DEFAULT_PALETTE[bkPaletteAddr[0]*3];

            
            std::function<void(uint8_t*, int, int, int, uint8_t*, uint8_t*, bool, bool, bool)> drawTile = [this, bkPaletteAddr, sprPaletteAddr](uint8_t* buffer, int x, int y, int high2, uint8_t* tileAddr, uint8_t* paletteAddr, bool flipH, bool flipV, bool hitChecking)
            {
                bool isSprite = paletteAddr == sprPaletteAddr;
                
                for (int ty=0; ty<8; ty++)
                {
                    if (y + ty < 0)
                        continue;
                    
                    int ty_ = flipV ? 7-ty : ty;
                    
                    for (int tx=0; tx<8; tx++)
                    {
                        if (x + tx < 0)
                            continue;
                     
                        int tx_ = flipH ? tx : 7-tx;
                        
                        // tile 第一字节与第八字节对应的bit位，组成这一像素颜色的低2位（最后构成一个[0,63]的数，索引到系统默认的64种颜色）
                        int low0 = ((bit8*)&tileAddr[ty_])->get(tx_);
                        int low1 = ((bit8*)&tileAddr[ty_+8])->get(tx_);
                        int low2 = low0 | (low1 << 1);
                        
                        int paletteUnitIndex = (high2 << 2) + low2; // 背景调色板单元索引号
                        
                        int systemPaletteUnitIndex = paletteAddr[paletteUnitIndex]; // 系统默认调色板颜色索引 [0,63]
                        
                        // 如果绘制精灵，而且当前颜色引用到背景透明色（背景调色板第一位），就不绘制
                        if (isSprite && systemPaletteUnitIndex == bkPaletteAddr[0])
                        {
                            continue;
                        }
                        
                        int pixelIndex = (NES_MIN(y + ty, 239) * 32*8 + NES_MIN(x+tx, 255)) * 3; // 最低位是右边第一像素，所以渲染顺序要从右往左
                        
                        RGB* rgb = (RGB*)&DEFAULT_PALETTE[systemPaletteUnitIndex*3];
                        
                        auto* pb = (RGB*)&buffer[pixelIndex];
                        
                        // 当前背景色不是透明色，就发生碰撞！
                        if (hitChecking && x+tx_ != 255 && pb != pTRANSPARENT_RGB)
                        {
                            status_regs->set(6, 1);
                        }
                        
                        *pb = *rgb;
                    }
                }
            };
            
            
            /*
             0x2001 > write
             7  bit  0
             ---- ----
             BGRs bMmG
             |||| ||||
             |||| |||+- Greyscale (0: normal color, 1: produce a greyscale display)
             |||| ||+-- 1: Show background in leftmost 8 pixels of screen, 0: Hide
             |||| |+--- 1: Show sprites in leftmost 8 pixels of screen, 0: Hide
             |||| +---- 1: Show background
             |||+------ 1: Show sprites
             ||+------- Emphasize red*
             |+-------- Emphasize green*
             +--------- Emphasize blue*
             
             */
            bool showBg = mask_regs->get(3) == 1;
            bool showSp = mask_regs->get(4) == 1;
            

            /*
             
             VRAM
             
             yyy NN YYYYY XXXXX
             ||| || ||||| +++++-- coarse X scroll
             ||| || +++++-------- coarse Y scroll
             ||| ++-------------- nametable select
             +++----------------- fine Y scroll
             
             */

            const auto& v = _t;
            
            // 从 _t 中取出tile坐标偏移
            int bg_offset_x = v & 0x1F;
            int bg_offset_y = (v >> 5) & 0x1F;
            int bg_t_x = _x;
            int bg_t_y = (v >> 12) & 0x7;
            int bg_base = (v >> 10) & 0x3;
            
//            printf("%d\n", bg_base);
            
            // 绘制背景
            int tiles_y = bg_t_y == 0 ? 30 : 31;
            int tiles_x = bg_t_x == 0 ? 32 : 33;

            for (int bg_y=0; bg_y<tiles_y; bg_y++) // 只显示 30/30 行tile
            {
                int y = (bg_y+bg_offset_y) % 30;
                
                for (int bg_x=0; bg_x<tiles_x; bg_x++)
                {
                    int x = (bg_x+bg_offset_x) % 32;
                    
                    // 一个字节表示4x4的tile组，先确定当前(x,y)所在字节
                    uint8_t attributeAddrFor4x4Tile = attributeTableAddr[(y / 4 * (32/4) + x / 4)];
                    int bit = (y % 4) / 2 * 4 + (x % 4) / 2 * 2;
                    int high2 = (attributeAddrFor4x4Tile >> bit) & 0x3;
                    
                    int tileIndex = nameTableAddr[y*32 + x]; // 得到 bkg tile index
                    
                    // 确定在图案表里的tile地址，每个tile是8x8像素，占用16字节。
                    // 前8字节(8x8) + 后8字节(8x8)
                    uint8_t* tileAddr = &bkPetternTableAddr[tileIndex * 16];
                    
                    if (showBg)
                    {
                        drawTile(_buffer, bg_x*8-bg_t_x, bg_y*8-bg_t_y, high2, tileAddr, bkPaletteAddr, false, false, false);
                    }
                }
            }

            
            
            
            // 绘制精灵
            for (int i=0; i<64; i++)
            {
                Sprite* spr = (Sprite*)&_sprram[i*4];
                if (*(int*)spr == 0) // 没有精灵数据
                    continue;
                
                uint8_t* tileAddr = &sprPetternTableAddr[spr->tileIndex * 16];
                
                int high2 = spr->info.get(0) | (spr->info.get(1) << 1);
                bool flipH = spr->info.get(6);
                bool flipV = spr->info.get(7);
                
                if (showSp)
                    drawTile(_buffer, spr->x + 1, spr->y + 1, high2, tileAddr, sprPaletteAddr, flipH, flipV, i == 0);
            }
            

            
            // 绘制完成，等于发生了 VBlank，需要设置 2002 第7位
            status_regs->set(7, 1);
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
        
        void petternTables(uint8_t p[32])
        {
            for (int i=0; i<32; i++)
            {
                p[i] = _vram.masterData()[0x3F00 + i];
            }
        }
        
    private:
        
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

        
        
        void reset()
        {
            _w = 0;
            _t = 0;
            _v = 0;
            _x = 0;
            
            _dstWrite2003 = 0;
            _dstAddr2004tmp = 0;
            _dstAddr2004 = 0;
            _sprramWritingEnabled = false;
            
            memset(_sprram, 0, 256);
        }
        
        uint16_t _t; // 临时 VRAM 地址
        uint16_t _v; // 当前 VRAM 地址
        int _w = 0;
        int _x; // 3bit 精细 x 滚动
        
        
        bool _firstRead2007 = true;
        
        int _dstWrite2003 = 0;
        uint16_t _dstAddr2004tmp = 0;
        uint16_t _dstAddr2004 = 0;
        
        bool _sprramWritingEnabled = false;
        

        
        uint8_t* _buffer = 0;
        
        uint8_t _sprram[256]; // 精灵内存, 64 个，每个4字节
        VRAM _vram;
        Memory* _mem;
    };
}
