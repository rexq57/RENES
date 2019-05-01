#pragma once

#include "mem.hpp"
#include "vram.hpp"
#include "mem.hpp"

/*
 NTSC（2C02）
 主时钟周期是 21.477272 MHz±40 Hz
 CPU时钟周期 21.47 MHz÷12 = 1.789773 MHz
 APU帧计数器速率 60Hz
 PPU时钟速度 21.477272 MHz÷4 = 5.369318 MHz

 一个CPU时钟周期内，PPU点数是 5.369318 / 1.789773 = 3
 PPU时钟速度是CPU时钟速度的3倍
 
 【VBlank时间】
 图片尺寸是256x240，NTSC标称可见高度是224 (只是在显示前240条扫描线的时候，Y轴居中，从第8条开始显示，此操作由UI逻辑处理，数值224不影响计算)
 实际上，NTSC每帧尺寸是341x262 (见 https://wiki.nesdev.com/w/index.php/File:Ntsc_timing.png)
 VBlank我理解为PPU跑 [240,261] 这段扫描线的时间（假设261过后，下一条扫描线就是下一帧的第一条扫描线，如下图:）
 [239]...... (当前帧最后一条扫描线)
 [240]...... (VBlank开始)
 [...]......
 [261]...... (VBlank结束)
   [0]...... (下一帧的第一条扫描线)
 
 HBlank我理解为PPU跑每条扫描线的 [256,340] 点的时间
 
 【预渲染线】
 最后一条扫描线，每隔一帧短一个点，例如0帧341，1帧340，2帧341 ...
 
 */

class Timer {
    
public:
    
    Timer& timerStart()
    {
        mCurrentTimeMillis=clock();
        return *this;
    }
    
    Timer& timerEnd(const char* log)
    {
        return timerEnd(log, true);
    }
    
    Timer& timerEnd(const char* log, bool display)
    {
        float time = (clock() - mCurrentTimeMillis) / 1000000.0f;
        mFpsCount ++;
        mFpsTime += time;
        float fps = mFpsCount/mFpsTime;
        if (mFpsCount > 1000)
        {
            mFpsCount = 0;
            mFpsTime = 0;
        }
        if (display)
            printf("%s %f fps %fs %fs\n", log , fps , 1.0f/fps , time);
        
        return *this;
    }
    
private:
    float mFpsTime = 0;
    long mFpsCount = 0;
    long mCurrentTimeMillis;
};

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
        
        std::string testLog;
        
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
            _buffer = (uint8_t*)malloc(RGB_BUFFER_LENGTH);
            
            _spr_buffer = (uint8_t*)malloc(SPR_BUFFER_LENGTH);
            
            // 卷轴缓冲区
            _scrollBuffer = (uint8_t*)malloc(RGB_BUFFER_LENGTH*4);
            _scrollBufferTmp = (uint8_t*)malloc(RGB_BUFFER_LENGTH);
            
            reset();
        }
        
        ~PPU()
        {
            if (_buffer != 0)
                free(_buffer);
            
            if (_spr_buffer != 0)
                free(_spr_buffer);
            
            if (_scrollBuffer != 0)
                free(_scrollBuffer);
            
            if (_scrollBufferTmp != 0)
                free(_scrollBufferTmp);
        }
        
        void init(Memory* mem)
        {
//            printf("%x %x\n", this, &_mem);
            
            _mem = mem;
            io_regs = (bit8*)mem->getIORegsAddr(); // 直接映射地址，不走mem请求，因为mem读写请求模拟了内存访问限制，那部分是留给cpu访问的
            mask_regs = (bit8*)&io_regs[1];
            status_regs = (bit8*)&io_regs[2];
            
            std::function<void(uint16_t, uint8_t)> writtingObserver = [this](uint16_t addr, uint8_t value){

                
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
                        _dstAddr2004 = value;
                        break;
                    }
                    case 0x2004:
                    {
                        _sprram[_dstAddr2004++ % 256] = value;
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
                            _2007ReadingStep = 0; // 每次_v生效，忽略从2007读取的第一个字节
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
                        
//                        _2007ReadingStep = 2;
                        
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
                    case 0x2004:
                        *value = _sprram[_dstAddr2004++ % 256];
                        break;
                    case 0x2007:
                        
//                        printf("读取 2007 %x - ", _v);
                        
                        // 据说第一次读取的值是无效的，会缓冲到下一次读取才返回
                        if (_2007ReadingStep == 0)
                        {
                        }
                        else
                        {
                            *value = _2007ReadingCache;
                        }
                        
                        _2007ReadingCache = _vram.read8bitData(_v);
                        _v += io_regs[0].get(2) == 0 ? 1 : 32;
                        
//                        printf("%d\n", *value);
                        
                        _2007ReadingStep = 1;
                        
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
            
            mem->addWritingObserver(0x2000, writtingObserver);
            mem->addReadingObserver(0x2002, readingObserver);
            mem->addReadingObserver(0x2004, readingObserver);
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
        
#define line_y_max 31*8
        
        void readyOnFirstLine()
        {
            status_regs->set(7, 0); // 清除 vblank
            status_regs->set(6, 0);
            
            // 准备当前帧的OAM
            memcpy(_OAM, _sprram, 256);
            
            // 设置背景色
            
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
            
            auto* VRAM = _vram.masterData();
            
            // 绘制背景
            uint8_t* bkPetternTableAddr = &VRAM[0x1000 * io_regs[0].get(4)]; // 第4bit决定背景图案表地址 0x0000或0x1000
            uint8_t* sprPetternTableAddr = &VRAM[0x1000 * io_regs[0].get(3)]; // 第3bit决定精灵图案表地址 0x0000或0x1000
            
            uint8_t* bkPaletteAddr = &VRAM[0x3F00];   // 背景调色板地址
            uint8_t* sprPaletteAddr = &VRAM[0x3F10];  // 精灵调色板地址
            
            //            const static RGB* pTRANSPARENT_RGB = (RGB*)&DEFAULT_PALETTE[bkPaletteAddr[0]*3];
            
            
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
            bool showSpr = mask_regs->get(4) == 1;
            
            this->_showBg = showBg;
            this->_showSpr = showSpr;
            /*
             
             VRAM
             
             yyy NN YYYYY XXXXX
             ||| || ||||| +++++-- coarse X scroll
             ||| || +++++-------- coarse Y scroll
             ||| ++-------------- nametable select
             +++----------------- fine Y scroll
             
             */
            
            
            
            /*
             
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
            
            
            // 绘制精灵到缓冲区，用来给扫描线使用
            // 一个字节表示一个点
            // 高2位 : 是否是第一精灵
            // 低6位 : 系统调色板里的下标
            
            memset(_spr_buffer, 0, SPR_BUFFER_LENGTH);
            for (int i=0; i<64; i++)
            {
                Sprite* spr = (Sprite*)&_OAM[i*4];
                if (*(int*)spr == 0) // 没有精灵数据
                    continue;
                
                uint8_t* tileAddr = &sprPetternTableAddr[spr->tileIndex * 16];
                
                int high2 = spr->info.get(0) | (spr->info.get(1) << 1);
                bool sprFront = spr->info.get(5); // 优先级，0 - 在背景上 1 - 在背景下
                bool flipH = spr->info.get(6); // 水平翻转
                bool flipV = spr->info.get(7); // 竖直翻转
                
                drawSprBuffer(_spr_buffer, spr->x + 1, spr->y + 1, high2, tileAddr, sprPaletteAddr, flipH, flipV, i == 0, sprFront);
            }
            
            _scanline_y = 0;
        }
        
        // 模拟扫描线 ? 但是应该是每条指令执行完成后，绘制这一行上的一定数量的点
        void drawScanline(bool* vblankEvent)
        {
            *vblankEvent = false;
            auto* VRAM = _vram.masterData();
            
            // 绘制背景
            uint8_t* bkPetternTableAddr = &VRAM[0x1000 * io_regs[0].get(4)]; // 第4bit决定背景图案表地址 0x0000或0x1000
            uint8_t* sprPetternTableAddr = &VRAM[0x1000 * io_regs[0].get(3)]; // 第3bit决定精灵图案表地址 0x0000或0x1000
            
            uint8_t* bkPaletteAddr = &VRAM[0x3F00];   // 背景调色板地址
            uint8_t* sprPaletteAddr = &VRAM[0x3F10];  // 精灵调色板地址
            
            bool showBg  = this->_showBg;
            bool showSpr = this->_showSpr;
            
            uint8_t* buffer = this->_buffer;
            
            const auto& v = _t;
            
            int line_y = this->_scanline_y ++; // 使用了再增加
            if (line_y <= 239) // 只绘制可见扫描线
            {
                // 从 _t 中取出tile坐标偏移，相当于表中的起始位置
                int bg_offset_x = v & 0x1F;         // tile整体偏移[0,31]
                int bg_offset_y = (v >> 5) & 0x1F;
                
                int bg_t_x = _x;                    // tile精细偏移[0,7]
                int bg_t_y = (v >> 12) & 0x7;
                //            int bg_base = (v >> 10) & 0x3;
                
                // 起始名称表索引
                int firstNameTableIndex = io_regs[0].get(0) | (io_regs[0].get(1) << 1); // 前2bit决定基础名称表地址
                
#ifdef DEBUG
                testLog = std::to_string(firstNameTableIndex) + ": " + std::to_string(bg_offset_x) + "-" + std::to_string(bg_t_x);
#endif
                
                
                // 计算当前扫描线所在瓦片，瓦片偏移发生在相对当前屏幕的tile上，而不是指图案表相对屏幕左上角发生的偏移
                int bg_y = line_y/8;
                int s_y = bg_y+bg_offset_y;         // 实际瓦片坐标，向上[0,31]
                int tile_y = s_y % 30; // 30个tile 垂直循环
                
                int ty = (line_y+bg_t_y)%8;
                int draw_line_y = line_y/8*8-bg_t_y;
                
                // 瓦片坐标位于不可见的扫描线
                if (draw_line_y + ty < 0)
                    return;
                
                // 宽度应该是256，刚好填满32个tile。但是如果发生错位的情况，是需要33个tile
                const static int LINE_X_MAX = 32*8;
                
                for (int line_x=0; line_x<LINE_X_MAX; line_x++)
                {
                    int tx = (line_x+bg_t_x)%8; // 对应当前瓦片上的index
                    int draw_line_x = line_x/8*8-bg_t_x; // 背景绘制的整体起始点
                
                    uint8_t* paletteAddr = bkPaletteAddr;
                    bool flipV = false;
                    bool flipH = false;
                    
                    int tx_ = flipH ? tx : 7-tx;
                    int ty_ = flipV ? 7-ty : ty;
                    
                    if (draw_line_x + tx < 0)
                        continue;
                    
                    /*
                     调色板
                     0x3F00 16字节
                     */
                    /* 调色板索引 [16]颜色
                     11 11
                     高2bit: 属性表
                     低2bit: 来自图案表 <- 名称表
                     */
                    struct PaletteIndex{
                        int high2bit;
                        int low2bit;
                        inline
                        int merge() const
                        {
                            return (high2bit << 2) + low2bit;
                        }
                    };
                    PaletteIndex peletteIndex;
                    
//                    if (line_x % 8 == 0) // 每次跨界才计算新的瓦片地址
                    {
//                        int bg_x = (line_x + bg_t_x)/8;
                        int bg_x = line_x/8;
                        int s_x = (bg_x+bg_offset_x); // 目标tile
                        int x = s_x % 32; // 32个tile 水平循环
                        
                        // 用于确定一个像素，必须输入参数 tileIndex
                        //                            if (line_y % 8 == 0)
                        {
                            int nameTableIndex = ((firstNameTableIndex + s_x/32) % 2);
                            // 未处理左右镜像，需要计算s_y
                            
                            /* 名称表
                             $2000-$23FF    $0400    Nametable 0
                             $2400-$27FF    $0400    Nametable 1
                             $2800-$2BFF    $0400    Nametable 2
                             $2C00-$2FFF    $0400    Nametable 3
                             */
                            uint8_t* nameTableAddr = &VRAM[0x2000 + nameTableIndex*0x0400];
                            {
                                // 名称表是32x30连续空间，960字节，每个字节是一个索引，表示[0,255]的数
                                // 可以定位256个瓦片的地址（瓦片：图案表中的单位）
                                int tileIndex = nameTableAddr[tile_y*32 + x]; // 得到 bkg tile index
                                
                                /* 图案表：一个4KB的空间，PPU有两个图案表，映射在VRAM里 供背景和精灵使用
                                $0000-$0FFF    $1000    Pattern table 0
                                $1000-$1FFF    $1000    Pattern table 1
                                 
                                确定在图案表里的tile地址，每个tile控制8x8像素（64个像素），8字节一组，共2组，16字节
                                两组8字节
                                64bit: 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111
                                64bit: 11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111
                                       1 (低)
                                       1 (高)
                                   构成2bit
                                */
                                
                                uint8_t* bkTileAddr = &bkPetternTableAddr[tileIndex * 16];
                                {
                                    // 图案表tile 第一字节与第八字节对应的bit位，组成这一像素颜色的低2位（最后构成一个[0,63]的数，索引到系统默认的64种颜色）
                                    int low0 = ((bit8*)&bkTileAddr[ty_])->get(tx_);
                                    int low1 = ((bit8*)&bkTileAddr[ty_+8])->get(tx_);
                                    peletteIndex.low2bit = low0 | (low1 << 1);
                                }
                            }
                            
                            /*
                             属性表 - 位于名称表未使用的64字节空间
                             64字节 = 名称表0x400(1024)-960(32x30)
                             */
                            uint8_t* attributeTableAddr = &VRAM[0x23C0 + nameTableIndex*0x0400];
                            {
                                // 一个字节表示4x4的tile组，先确定当前(x,y)所在字节（即属性）
                                // 每2bit用于2x2的tile，作为peletteIndex的高2位
                                uint8_t attributeAddrFor4x4Tile = attributeTableAddr[(tile_y / 4 * (32/4) + x / 4)];
                                // 换算tile(x,y)所使用的bit位
                                int bit = (tile_y % 4) / 2 * 4 + (x % 4) / 2 * 2;
                                peletteIndex.high2bit = (attributeAddrFor4x4Tile >> bit) & 0x3;
                            }
                        }
                    }
                    
                    // line_x/8*8 为当前tile的标准坐标，bg_t_x 为偏移位置
                    
                    {
                        {
                            int bg_systemPaletteUnitIndex = paletteAddr[peletteIndex.merge()]; // 系统默认调色板颜色索引 [0,63]
                            
                            int pixelIndex = (NES_MIN(draw_line_y + ty, 239) * 32*8 + NES_MIN(draw_line_x + tx, 255)); // 最低位是右边第一像素，所以渲染顺序要从右往左
//                            int pixelIndex = (NES_MIN(draw_line_y + ty, 239) * 32*8 + NES_MIN(line_x, 255));
                            
                            
                            
                            // 获取当前像素的精灵数据，并检测碰撞
                            int spr_systemPaletteUnitIndex = 0; // 取低6位[0,63]
                            bool sprFront = true;
                            {
                                for (int i=0; i<8; i++)
                                {
                                    uint8_t& spr_data = _spr_buffer[pixelIndex + i*BUFFER_PIXEL_CONUT];
                                    
                                    if (spr_data == 0) break;
                                    
                                    sprFront = (spr_data & 32) == 0;
                                    spr_systemPaletteUnitIndex = sprPaletteAddr[spr_data & 15];
                                    
                                    {
                                        bool isFirstSprite = spr_data > 63;
                                        
                                        // 碰撞条件，第一精灵 & 非0调色板第一位（透明色）& 允许碰撞
                                        if (isFirstSprite && spr_systemPaletteUnitIndex != sprPaletteAddr[0] && status_regs->get(6) == 0)
                                        {
                                            status_regs->set(6, 1);
                                        }
                                    }
                                }
                            }
                            
                            int systemPaletteUnitIndex = -1;
                            {
                                if (showSpr && spr_systemPaletteUnitIndex != 0)
                                {
                                    // 判断前后
                                    if (showBg && !sprFront && peletteIndex.merge() != 0)
                                    {
                                        systemPaletteUnitIndex = bg_systemPaletteUnitIndex;
                                    }
                                    else
                                    {
                                        systemPaletteUnitIndex = spr_systemPaletteUnitIndex;
                                    }
                                }
                                else if (showBg)
                                {
                                    systemPaletteUnitIndex = bg_systemPaletteUnitIndex;
                                }
                            }
                            
                            auto* pb = (RGB*)&buffer[pixelIndex*3];
                            if (systemPaletteUnitIndex != -1)
                            {
                                RGB* rgb = (RGB*)&DEFAULT_PALETTE[systemPaletteUnitIndex*3];
                                *pb = *rgb;
                            }
                            else
                            {
                                memset(pb, 0, 3);
                            }
                        }
                    }
                    
                }
                
                // 绘制了最后一条可见扫描线，则设置VBlank标记
                if (line_y == 239)
                {
                    status_regs->set(7, 1);
                    // 设置vblank事件
                    *vblankEvent = true;
                }
            }
        }
        
        // 当前扫描线
        inline
        int currentScanline() const
        {
            return _scanline_y;
        }
        
        // dump数据，给外部逻辑使用
        void dumpScrollToBuffer()
        {
            auto* VRAM = _vram.masterData();
            
            uint8_t* bkPetternTableAddr = &VRAM[0x1000 * io_regs[0].get(4)]; // 第4bit决定背景图案表地址 0x0000或0x1000
            uint8_t* bkPaletteAddr = &VRAM[0x3F00];   // 背景调色板地址
            
            // 上下镜像模式，水平复制
            for (int i=0; i<4; i++)
            {
                drawBackground(_scrollBufferTmp, i, bkPetternTableAddr, bkPaletteAddr);
                
                // copy to buffer
                size_t lineLength = BUFFER_PIXEL_WIDTH*BUFFER_PIXEL_BPP;
                
                uint8_t* dst = _scrollBuffer + (i%2)*lineLength + i/2*RGB_BUFFER_LENGTH*2;
                size_t dstStride = lineLength*2;
                
                for (int y=0; y<BUFFER_PIXEL_HEIGHT; y++)
                {
                    memcpy(dst+y*dstStride, _scrollBufferTmp+y*lineLength, lineLength);
                }
            }
        }
        
        inline
        int width() const
        {
            return BUFFER_PIXEL_WIDTH;
        }
        
        inline
        int height() const
        {
            return BUFFER_PIXEL_HEIGHT;
        }
        
        inline
        int bpp() const
        {
            return BUFFER_PIXEL_BPP;
        }
        
        inline
        uint8_t* buffer() const
        {
            return _buffer;
        }
        
        inline
        uint8_t* scrollBuffer() const
        {
            return _scrollBuffer;
        }
        
        void petternTables(uint8_t p[32])
        {
            for (int i=0; i<32; i++)
            {
                p[i] = _vram.masterData()[0x3F00 + i];
            }
        }
        
    private:
        
        void drawTile(uint8_t* buffer, int x, int y, int high2, uint8_t* tileAddr, uint8_t* paletteAddr)
        {
            for (int ty=0; ty<8; ty++)
            {
                if (y + ty < 0)
                    continue;
                
                //                    int ty_ = flipV ? 7-ty : ty;
                int ty_ = ty;
                
                for (int tx=0; tx<8; tx++)
                {
                    if (x + tx < 0)
                        continue;
                    
                    //                        int tx_ = flipH ? tx : 7-tx;
                    int tx_ = 7-tx;
                    
                    // tile 第一字节与第八字节对应的bit位，组成这一像素颜色的低2位（最后构成一个[0,63]的数，索引到系统默认的64种颜色）
                    int low0 = ((bit8*)&tileAddr[ty_])->get(tx_);
                    int low1 = ((bit8*)&tileAddr[ty_+8])->get(tx_);
                    int low2 = low0 | (low1 << 1);
                    
                    // 11 1111
                    // 来自名称表 来自图案表tile
                    // 3字节表示[0-63]
                    int paletteUnitIndex = (high2 << 2) + low2; // 背景调色板单元索引号
                    
                    int systemPaletteUnitIndex = paletteAddr[paletteUnitIndex]; // 系统默认调色板颜色索引 [0,63]
                    
                    int pixelIndex = (NES_MIN(y + ty, 239) * 32*8 + NES_MIN(x+tx, 255)) * 3; // 最低位是右边第一像素，所以渲染顺序要从右往左
                    
                    RGB* rgb = (RGB*)&DEFAULT_PALETTE[systemPaletteUnitIndex*3];
                    
                    auto* pb = (RGB*)&buffer[pixelIndex];
                    
                    *pb = *rgb;
                }
            }
        };
        
        void drawSprBuffer(uint8_t* buffer, int x, int y, int high2, uint8_t* tileAddr, uint8_t* paletteAddr, bool flipH, bool flipV, bool isFirstSprite, bool priority)
        {
            
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
                    
                    int paletteUnitIndex = (high2 << 2) + low2; // 背景调色板单元索引号 [0, 15]
                    
                    //                        int systemPaletteUnitIndex = paletteAddr[paletteUnitIndex]; // 系统默认调色板颜色索引 [0,63]
                    
                    // 如果绘制精灵，而且当前颜色引用到背景透明色（背景调色板第一位），就不绘制
                    //                        if (systemPaletteUnitIndex == bkPaletteAddr[0])
                    if (paletteUnitIndex % 4 == 0)
                    {
                        continue;
                    }
                    
                    int pixelIndex = (NES_MIN(y + ty, 239) * 32*8 + NES_MIN(x+tx, 255)); // 最低位是右边第一像素，所以渲染顺序要从右往左
                    //                        int spr_data = systemPaletteUnitIndex | (isFirstSprite ? (3 << 6) : 0);
                    int spr_data = (isFirstSprite ? (3 << 6) : 0) | (priority << 5) | paletteUnitIndex;
                    
                    for (int i=0; i<8; i++)
                    {
                        auto& dst = buffer[pixelIndex + BUFFER_PIXEL_CONUT*i];
                        
                        if (dst != 0)
                            continue;
                        
                        dst = spr_data; // 低6位 = [0,63] ， 高2位 = 填充标记
                    }
                }
            }
        };
        
        // 名称表长度是0x0400，
        
        // 名称表地址
        inline
        uint8_t* nameTableAddress(int index)
        {
            return &_vram.masterData()[0x2000 + index*0x0400];
        }
        
        // 属性表地址
        inline
        uint8_t* attributeTableAddress(int index)
        {
            return &_vram.masterData()[0x23C0 + index*0x0400];
        }
        
        // 从第nameTableIndex个名称表开始绘制（竖直镜像）
        void drawBackground(uint8_t* buffer, int tableIndex, uint8_t* bkPetternTableAddr, uint8_t* bkPaletteAddr){
            
            // 背景的tile偏移（图案表中的tile偏移）
            int bg_offset_x = 0;
            int bg_offset_y = 0;
            
            // 精细偏移
            int bg_t_x = 0;
            int bg_t_y = 0;
            
            // 总共需要绘制的tiles个数，水平|竖直
            int tiles_y = bg_t_y == 0 ? 30 : 31;
            int tiles_x = bg_t_x == 0 ? 32 : 33;
            
            //                uint8_t* nameTableAddr = &_vram.masterData()[0x2000 + nameTableIndex*0x0400];
            //                uint8_t* attributeTableAddr = &_vram.masterData()[0x23C0 + nameTableIndex*0x0400];
            
            for (int bg_y=0; bg_y<tiles_y; bg_y++) // 只显示 30/30 行tile
            {
                int s_y = bg_y+bg_offset_y;
                int y = s_y % 30; // 竖直第_y个tile，30个tile 垂直循环
                
                
                for (int bg_x=0; bg_x<tiles_x; bg_x++)
                {
                    int s_x = (bg_x+bg_offset_x);
                    int x = s_x % 32; // 水平第_x个tile，32个tile 水平循环
                    
                    // 实际
                    int nameTableIndex = ((tableIndex + s_x/32) % 2);
                    int attributeTableIndex = nameTableIndex;
                    // 未处理左右镜像，需要计算s_y ?
                    
                    uint8_t* nameTableAddr = nameTableAddress(nameTableIndex);
                    uint8_t* attributeTableAddr = attributeTableAddress(attributeTableIndex);
                    
                    // 一个字节表示4x4的tile组，先确定当前(x,y)所在字节
                    uint8_t attributeAddrFor4x4Tile = attributeTableAddr[(y / 4 * (32/4) + x / 4)];
                    int bit = (y % 4) / 2 * 4 + (x % 4) / 2 * 2;
                    int high2 = (attributeAddrFor4x4Tile >> bit) & 0x3;
                    
                    int tileIndex = nameTableAddr[y*32 + x]; // 得到 bkg tile index
                    
                    // 确定在图案表里的tile地址，每个tile是8x8像素，占用16字节。
                    // 前8字节(8x8) + 后8字节(8x8)
                    uint8_t* tileAddr = &bkPetternTableAddr[tileIndex * 16];
                    
                    drawTile(buffer, bg_x*8-bg_t_x, bg_y*8-bg_t_y, high2, tileAddr, bkPaletteAddr);
                }
            }
        };
        
        const static int BUFFER_PIXEL_WIDTH = 256;
        const static int BUFFER_PIXEL_HEIGHT = 240;
        const static int BUFFER_PIXEL_BPP = 3;
        const static int BUFFER_PIXEL_CONUT = BUFFER_PIXEL_WIDTH * BUFFER_PIXEL_HEIGHT;
        
        const static int RGB_BUFFER_LENGTH = BUFFER_PIXEL_CONUT * 3;
        
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
            
            _dstAddr2004 = 0;
            
            memset(_sprram, 0, 256);
        }
        
        uint16_t _t; // 临时 VRAM 地址
        uint16_t _v; // 当前 VRAM 地址
        int _w = 0;
        int _x; // 3bit 精细 x 滚动
        
        
        int _2007ReadingStep = 0;
        uint8_t _2007ReadingCache = 0;
        
        uint16_t _dstAddr2004 = 0;

        // 绘图
        int _scanline_y = 0;
        bool _showBg;
        bool _showSpr;
        
        // 建立 w * h * 8 的缓冲区，每像素8字节，用来存储8个精灵的数据。
        // 如果该像素没有精灵，甚至没有叠加，则该位第一字节为0
        const int SPR_BUFFER_LENGTH = BUFFER_PIXEL_CONUT*8;
        
        uint8_t* _spr_buffer = 0;   // 精灵绘制缓冲区
        uint8_t* _buffer = 0;       // 显示缓冲区
        uint8_t* _scrollBuffer = 0; // 卷轴缓冲区
        uint8_t* _scrollBufferTmp = 0;
        
        uint8_t _sprram[256]; // 精灵内存, 64 个，每个4字节
        uint8_t _OAM[256];    // 每帧的OAM
        
        VRAM _vram;
        Memory* _mem;
        
        Timer _timer;
    };
}
