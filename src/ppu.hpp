#pragma once

#include "mem.hpp"
#include "vram.hpp"
#include "mem.hpp"

// 优化的显示模式
#define RENES_BK_MODE_OPT

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

#define RENES_FRAME_VISIBLE_W 256
#define RENES_FRAME_VISIBLE_H 240

namespace ReNes {
    
    /*
     未解决问题:
     [问题1]、在渲染过程中完全忽略写入 ？？？？ https://wiki.nesdev.com/w/index.php/PPU_registers#Controller_.28.242000.29_.3E_write
     
     不确定的解决方案:
     1、在doPreRenderLine里清除了0x2000的低2bit来防止《超级玛丽》移动到第二名称表的时候，精灵0碰撞前间歇性名称表为1的情况，导致顶部无法显示第一名称里的状态栏的情况
     
     未知问题：
    1、《超级玛丽》运动到一段画面后，精灵0出现错位闪烁
    2、《超级玛丽》死完之后，无法重新开始，按键无效
    3、《超级玛丽》选关下管子的时候不显示动画
    4、精灵可以到屏幕上的任何地方。然而，他们并不擅长平滑地离开屏幕的左侧。有一个选项（PPU掩码2001位XXXX X11X），如果为零，则关闭屏幕的左8个像素，然后您可以顺利地离开屏幕的左侧。
     */
    
    // 系统调色板，网上有多种颜色风格
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

    // 一个提供给外部使用的显示缓冲区
    class RGB_Buffer {
    public:
        RGB_Buffer(int width, int height) {
            data = (uint8_t*)malloc(width * height * 3);
            this->width = width;
            this->height = height;
        }
        ~RGB_Buffer() {
            free(data);
        }
        uint8_t* data;
        int width;
        int height;
    };

    // 2C02
    class PPU {
        
    public:
        
        enum MIRRORING_MODE{
            MIRRORING_MODE_HORIZONTAL = VRAM::MIRRORING_MODE_HORIZONTAL,
            MIRRORING_MODE_VERTICAL = VRAM::MIRRORING_MODE_HORIZONTAL
        };
        
        std::string testLog;
        
        const uint8_t* sprram() const
        {
            return _sprram;
        }
        
        const VRAM* vram() const
        {
            return _vram;
        }
        
        void loadPetternTable(const uint8_t* addr)
        {
            _vram->loadPetternTable(addr);
        }
        
        // 设置系统信息
        void setSystemInfo(int w, int h)
        {
            _frame_w = w;
            _frame_h = h;
            
            // 设置为预渲染线
            _scanline_y = _frame_h-1;
        }
        
        PPU()
        {
            // 检查数据尺寸
            RENES_ASSERT(4 == sizeof(Sprite));
            
            // RGB 数据缓冲区
            _display_buffer = (uint8_t*)malloc(DISPLAY_BUFFER_LENGTH);
            
            // 精灵缓冲区
            _spr_buffer = (uint8_t*)malloc(SPR_BUFFER_LENGTH);
            _spr0buffer = (uint8_t*)malloc(SPR_BUFFER_LENGTH);
            _spr_bufferRGB = new RGB_Buffer(DISPLAY_BUFFER_PIXEL_WIDTH, DISPLAY_BUFFER_PIXEL_HEIGHT); // 像素单位是3字节，前3字节存储RGB
            
            // 卷轴缓冲区
            _scrollBuffer = (uint8_t*)malloc(DISPLAY_BUFFER_PIXEL_CONUT*4); // 4个表，像素单位是1字节，存储4bit的调色板下标
            _scrollBufferRGB = new RGB_Buffer(DISPLAY_BUFFER_PIXEL_WIDTH*2, DISPLAY_BUFFER_PIXEL_HEIGHT*2); // 4个表，像素单位是3字节，前3字节存储RGB
            
            
            _vram = new VRAM();
            
            reset();
        }
        
        ~PPU()
        {
            free(_display_buffer);
            
            free(_spr_buffer);
            
            free(_spr0buffer);
            
            free(_scrollBuffer);
            
            delete _spr_bufferRGB;
            delete _scrollBufferRGB;
            
            delete _vram;
        }
        
        void init(Memory* mem)
        {
            _mem = mem;
            // 直接映射地址，不走mem请求，因为mem读写请求模拟了内存访问限制，那部分是留给cpu访问的
            _io_regs = (bit8*)mem->getIORegsAddr();
            _control_regs = (bit8*)&_io_regs[0];
            _mask_regs = (bit8*)&_io_regs[1];
            _status_regs = (bit8*)&_io_regs[2];
            
            std::function<void(uint16_t, uint8_t)> writtingObserver = [this](uint16_t addr, uint8_t value){
                switch (addr) {
                    case 0x2000:
                    {
                        // t: ...BA.. ........ = d: ......BA
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
                        // 问题1
//                        if (_status_regs->get(7) == 1)
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
                        }
                        else
                        {
                            // 低3位写入 _t 12~14位
                            // 高5位写入 _t 位5~9
                            _t &= ~0x73E0; // 清理 111 0011 1110 0000
                            _t |= ((value & 0x7) << 12) | ((value >> 3) << 5);
                        }

                        _w = 1 - _w;
                        break;
                    }
                    case 0x2006:
                    {
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
                            _2007ReadingStep = 0; // 每次_v生效，忽略从2007读取的第一个字节
                        }
                        _w = 1 - _w;
                        break;
                    }
                    case 0x2007:
                    {
                        // 在每一次向$2007写数据后，地址会根据$2000的2bit位增加1或者32
                        _vram->write8bitData(_v, value);
                        
                        // 通知
//                        _vramDidUpdasted(_v);
                        
                        // 自动增加
                        _v += _control_regs->get(2) == 0 ? 1 : 32;
                        
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
            
            std::function<void(uint16_t, uint8_t*, bool*)> readingObserver = [this](uint16_t addr, uint8_t* value, bool* valid)
            {
                switch (addr) {
                    case 0x2002: // 读取 0x2002会重置 _w 状态
                        _w = 0;
                        _status_regs->set(7, 0);
                        break;
                    case 0x2004:
                        *value = _sprram[_dstAddr2004++ % 256];
                        break;
                    case 0x2007:
                        // (从调色板之前的地址读取 [0, 3EFF] ) 第一次读取的值是无效的，会缓冲到下一次读取才返回
                        if (_v <= 0x3EFF)
                        {
                            if (_2007ReadingStep == 0)
                            {
                                *valid = false;
                            }
                            else
                            {
                                *value = _2007ReadingCache;
                            }
                            
                            _2007ReadingCache = _vram->read8bitData(_v);
                        }
                        else
                        {
                            _2007ReadingCache = _vram->read8bitData(_v);
//                            printf("addr: %x %d\n", _v, _2007ReadingCache);
                            *value = _2007ReadingCache;
                        }
                        
                        _v += _control_regs->get(2) == 0 ? 1 : 32;
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
        
        void initMirroring(MIRRORING_MODE mode)
        {
            _vram->initMirroring((VRAM::MIRRORING_MODE)mode);
        }
        
        // 预渲染
        void doPreRenderLine()
        {
            // 在预渲染线上的 dot 1 清理$2002的第5bit（精灵溢出）标记（但是不确定这个dot 1就是第一个像素，有个地方提到是第二个像素）
            _status_regs->set(5, 0);
            _status_regs->set(6, 0); // hit标记清零
            _status_regs->set(7, 0); // 清除 vblank
            
            // 准备当前帧的OAM
            memcpy(_OAM, _sprram, 256);
            _control_regs->set(0, 0);
            _control_regs->set(1, 0);
            
            // 更新调色板镜像
            _vram->updatePaletteMirror();
            
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
            
            // 绘制背景
//            const uint8_t* sprPetternTableAddr = _sprPetternTableAddress();
            const uint8_t* sprPaletteAddr = _sprPaletteAddress();  // 精灵调色板地址
            
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
            bool showBg = _mask_regs->get(3) == 1;
            bool showSpr = _mask_regs->get(4) == 1;
            
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
            memset(_spr_buffer, 0, SPR_BUFFER_LENGTH);
            memset(_spr0buffer, 0, SPR_BUFFER_LENGTH);
            
            // 精灵0必须在最上面以供碰撞检测
            for (int i=0; i<64; i++)
            {
                Sprite* spr = (Sprite*)&_OAM[i*4];
                if (*(int*)spr == 0) // 没有精灵数据
                    continue;
                
//                const uint8_t* tileAddr = &sprPetternTableAddr[spr->tileIndex * 16];
                const uint8_t* tileAddr = _sprPetternTableAddress(spr->tileIndex);
                
                int high2 = spr->info.get(0) | (spr->info.get(1) << 1);
                bool sprFront = spr->info.get(5); // 优先级，0 - 在背景上 1 - 在背景下
                bool flipH = spr->info.get(6); // 水平翻转
                bool flipV = spr->info.get(7); // 竖直翻转
                
//                printf("[%d] %d,%d\n", i, spr->x, spr->y);
                
                drawSprBuffer(_spr_buffer, spr->x, spr->y+1, high2, tileAddr, sprPaletteAddr, flipH, flipV, i == 0, sprFront);
                
                // 精灵0再额外存储到一个buffer，用于检测
                if (i == 0)
                    drawSprBuffer(_spr0buffer, spr->x, spr->y+1, high2, tileAddr, sprPaletteAddr, flipH, flipV, i == 0, sprFront);
            }
            
            // 绘制全部名称表到_scrollBuffer，可用于 to RGB buffer，以及优化的显示模式下的像素叠加
            {
                // 设置帧空白
                memset(_display_buffer, 0, DISPLAY_BUFFER_LENGTH);
                
                // 绘制每个名称表 -> scrollBuffer
//                for (int i=0; i<4; i++)
//                {
//                    updateBackgroundTile(i, 0, 0, 32, 30);
//                }
                
                // 绘制部分名称表，拷贝其镜像 -> scrollBuffer
                bool updatedNameTableIndex[4] = {false};
                
                for (int i=0; i<4; i++)
                {
                    if (updatedNameTableIndex[i])
                        continue;
                    
                    // 遍历里面32x30字节，更新其中 == index 的项目（这里等于全部刷新）
                    updateBackgroundTile(i, 0, 0, 32, 30);
                    updatedNameTableIndex[i] = true;
                    
                    // 检查其镜像，有，则进行数据拷贝
                    int mirroringIndex = _vram->nameTableMirroring(i);
                    if (mirroringIndex != i)
                    {
                        updatedNameTableIndex[mirroringIndex] = true;
                        // 复制src -> 镜像
                        const static int scrollBufferOffset[] = {
                            0, DISPLAY_BUFFER_PIXEL_WIDTH,
                            DISPLAY_BUFFER_PIXEL_CONUT*2, DISPLAY_BUFFER_PIXEL_CONUT*2+DISPLAY_BUFFER_PIXEL_WIDTH
                        };
                        
                        const uint8_t* srcAddr = _scrollBuffer + scrollBufferOffset[i];
                        const uint8_t* mirrAddr = _scrollBuffer + scrollBufferOffset[mirroringIndex];
                        int stride = 256*2;
                        for (int y=0; y<240; y++)
                        {
                            memcpy((void*)(mirrAddr+y*stride), (void*)(srcAddr+y*stride), 256);
                        }
                    }
                }
            }
        }
        
        // 在当前扫描线执行 pixelCount 次像素渲染
        void drawScanline(bool* vblankEvent, int pixelCount)
        {
            if (vblankEvent)
                *vblankEvent = false;
            _drawScanline(vblankEvent, pixelCount);
        }
        
        inline
        bool currentFrameOver() const
        {
            return _currentFrameOver;
        }
        
        // dump数据，给外部逻辑使用。index -> color
        void dumpScrollToBufferRGB()
        {
            const uint8_t* bkPaletteAddr = _bkPaletteAddress();
            
            // convert to RGB buffer
            int size = DISPLAY_BUFFER_PIXEL_WIDTH*DISPLAY_BUFFER_PIXEL_HEIGHT*4;
            for (int i=0; i<size; i++)
            {
                int systemPaletteUnitIndex = bkPaletteAddr[_scrollBuffer[i]]; // 系统默认调色板颜色索引 [0,63]
                RGB* rgb = (RGB*)&DEFAULT_PALETTE[systemPaletteUnitIndex*3];
                ((RGB*)_scrollBufferRGB->data)[i] = *rgb;
            }
        }
        
        void dumpSpriteToBufferRGB()
        {
            const uint8_t* sprPaletteAddr = _sprPaletteAddress();
            
            // convert to RGB buffer
            int size = DISPLAY_BUFFER_PIXEL_WIDTH*DISPLAY_BUFFER_PIXEL_HEIGHT;
            for (int i=0; i<size; i++)
            {
                int systemPaletteUnitIndex = sprPaletteAddr[_spr_buffer[i]]; // 系统默认调色板颜色索引 [0,63]
                RGB* rgb = (RGB*)&DEFAULT_PALETTE[systemPaletteUnitIndex*3];
                ((RGB*)_spr_bufferRGB->data)[i] = *rgb;
            }
        }
        
        // 缓冲区信息
        inline int width() const { return DISPLAY_BUFFER_PIXEL_WIDTH; }
        inline int height() const { return DISPLAY_BUFFER_PIXEL_HEIGHT; }
        inline int bpp() const { return DISPLAY_BUFFER_PIXEL_BPP; }
        inline uint8_t* buffer() const { return _display_buffer; }
        
        // 调试缓冲区，用于外部显示卷轴
        inline const RGB_Buffer* scrollBufferRGB() const { return _scrollBufferRGB; }
        inline const RGB_Buffer* spriteBufferRGB() const { return _spr_bufferRGB; }
        
        // 输出调色板
        void petternTables(uint8_t p[32]) const
        {
            for (int i=0; i<32; i++)
            {
                p[i] = _vram->bkPaletteAddress()[i];
            }
        }
        
    private:
        
        // 精灵溢出检测：当某条扫描线上出现8个以上精灵时，设置精灵溢出标记
        bool _spriteOverflow(int scanline)
        {
            int count = 0;
            int sprHeight = _control_regs->get(5) ? 15 : 7;
            
            // 扫描所有精灵
            for (int i=0; i<64; i++)
            {
                Sprite* spr = (Sprite*)&_OAM[i*4];
                if (*(int*)spr == 0) // 没有精灵数据
                    continue;
                
                // 需要判断范围
                const int start = spr->y-1;
                const int end   = start + sprHeight;
                if (scanline >= start && scanline <= end) {
                    if (count++ > 8)
                        return true;
                }
            }
            
            return false;
        }
        
        void _drawScanline(bool* vblankEvent, int pixelCount)
        {
            RENES_ASSERT(_scanline_y >= 0 && _scanline_y < _frame_h);
            
            bool isPreRenderLine = _scanline_y == _frame_h-1;
            if (isPreRenderLine)
            {
                if (_scanline_x == 0) {
                    doPreRenderLine();
                }
                else if (_scanline_x == 1) {
                    
                }
            }
            
            if (_scanline_y == 0)
                _currentFrameOver = false;
            
            if (_scanline_x == 0 && _status_regs->get(5) == 0)
            {
                // 检查精灵溢出
                if (_spriteOverflow(_scanline_y))
                {
//                    printf("精灵溢出\n");
                    _status_regs->set(5, 1);
                }
            }
            
            // 绘制背景
            const uint8_t* bkPaletteAddr = _bkPaletteAddress();
            const uint8_t* sprPaletteAddr = _sprPaletteAddress();
            
            bool showBg  = this->_showBg;
            bool showSpr = this->_showSpr;
            
            uint8_t* display_buffer = this->_display_buffer;
            
            const auto& v = _t;
            
            int line_y = this->_scanline_y; // 使用了再增加
            if (line_y <= 239) // 只绘制可见扫描线
            {
                // 从 _t 中取出tile坐标偏移，相当于表中的起始位置
                int bk_offset_x = v & 0x1F;         // tile整体偏移[0,31]
                int bk_offset_y = (v >> 5) & 0x1F;
                
                int bk_t_x = _x;                    // tile精细偏移[0,7]
                int bk_t_y = (v >> 12) & 0x7;
                //            int bk_base = (v >> 10) & 0x3;
                
                // 起始名称表索引
                int firstNameTableIndex = _control_regs->get(0) | (_control_regs->get(1) << 1); // 前2bit决定基础名称表地址
                
                
                
#ifdef DEBUG
                testLog = std::to_string(firstNameTableIndex) + ": " + std::to_string(bk_offset_x) + "-" + std::to_string(bk_t_x);
#endif
                
                
                // 计算当前扫描线所在瓦片，瓦片偏移发生在相对当前屏幕的tile上，而不是指图案表相对屏幕左上角发生的偏移
                //                int bk_y = line_y/8 + bk_offset_y; [瓦片扫描]
                int bk_tile_y = (line_y + bk_t_y)/8 + bk_offset_y; // [屏幕扫描]
                int tile_y = bk_tile_y % 30; // 30个tile 垂直循环
                
                //                int ty = line_y%8; // [瓦片扫描]
                int ty = (line_y+bk_t_y)%8; // [屏幕扫描]
                // int draw_line_y = line_y/8*8-bk_t_y;
                // int dst_y = draw_line_y + ty;
                
                // 瓦片坐标位于不可见的扫描线
                
                // [瓦片扫描]
                //                if (dst_y >= 240)
                //                    return;
                //
                //                if (dst_y < 0)
                //                    return;
                
                // 需要将计算模式设计为屏幕点扫描
                
                // 宽度应该是256，刚好填满32个tile。但是如果发生错位的情况，是需要33个tile
                //const static int LINE_X_MAX = 33*8; [瓦片扫描]
                int line_x_max = fminf(_scanline_x+pixelCount-1, RENES_FRAME_VISIBLE_W - 1);
                for (int line_x=_scanline_x; line_x <= line_x_max; line_x++)
                {
                    int tx = (line_x+bk_t_x)%8; // [屏幕扫描] 对应当前瓦片上的index
                    //                    int tx = line_x%8; // [瓦片扫描] 对应当前瓦片上的index
                    /// int draw_line_x = line_x/8*8-bk_t_x; // 背景绘制的整体起始点
                    // int dst_x = draw_line_x + tx; // 屏幕上的坐标
                    // [瓦片扫描]
                    //                    if (dst_x >= 256)
                    //                        continue;
                    //
                    //                    if (dst_x < 0)
                    //                    {
                    //                        continue;
                    //                    }
                    

                    
                    int first_tile_x = (line_x + bk_t_x)/8 + bk_offset_x; // [屏幕扫描]
                    
                    // 从基础名称表开始绘制，并根据tile偏移+瓦片索引
                    int nameTableIndex = ((firstNameTableIndex + first_tile_x/32) % 2);
                    
                    // int bk_tile_x = line_x/8 + bk_offset_x; // [瓦片扫描] 当前位置的tile
                    // 32个tile 水平循环在屏幕上的对应瓦片坐标，用来定位在对应（当前/下一个）名称表里的位置，确定tileIndex
                    int tile_x = first_tile_x % 32;
                    
                    int bk_systemPaletteUnitIndex;
                    
                    int bk_peletteIndex;
                    
#ifndef RENES_BK_MODE_OPT
                    {
                        const uint8_t* bkPetternTableAddr = _bkPetternTableAddress();
                        
                        // 背景不支持tile翻转，精灵才支持，这里作差别提示
                         bool flipV = false;
                         bool flipH = false;
                         int tx_ = flipH ? tx : 7-tx;
                         int ty_ = flipV ? 7-ty : ty;
                        
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
                        {
                            // 未处理左右镜像，需要计算s_y
                            
                            /* 名称表
                             $2000-$23FF    $0400    Nametable 0
                             $2400-$27FF    $0400    Nametable 1
                             $2800-$2BFF    $0400    Nametable 2
                             $2C00-$2FFF    $0400    Nametable 3
                             */
                            const uint8_t* nameTableAddr = _nameTableAddress(nameTableIndex);
                            {
                                // 名称表是32x30连续空间，960字节，每个字节是一个索引，表示[0,255]的数
                                // 可以定位256个瓦片的地址（瓦片：图案表中的单位）
                                int tileIndex = nameTableAddr[tile_y*32 + tile_x]; // 得到 bkg tile index
                                
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
                                
                                const uint8_t* bkTileAddr = &bkPetternTableAddr[tileIndex * 16];
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
                             影响tile组: 4x4(i8x8)
                             32x30 / (4x4) = 8x7.5
                             */
                            const uint8_t* attributeTableAddr = _attributeTableAddress(nameTableIndex);
                            {
                                // 一个字节表示4x4的tile组，先确定当前(x,y)所在字节（即属性）
                                // 每2bit用于2x2的tile，作为peletteIndex的高2位
                                uint8_t attributeAddrFor4x4Tile = attributeTableAddr[(tile_y / 4 * (32/4) + tile_x / 4)];
                                // 换算tile(x,y)所使用的bit位
                                int bit = (tile_y % 4) / 2 * 4 + (tile_x % 4) / 2 * 2;
                                peletteIndex.high2bit = (attributeAddrFor4x4Tile >> bit) & 0x3;
                            }
                        }
                        bk_peletteIndex = peletteIndex.merge();
                    }
#else
                    // 优化模式：直接使用_scrollBuffer已经计算好的调色表下标数据
                    {
                        // 瓦片绝对坐标 = 屏幕坐标 + 瓦片偏移(含精细偏移)
                        // 当前模式：名称表分屏
                        // 需要直接加名称表虚拟起始坐标即可: 名称表坐标 + 瓦片偏移(含精细偏移)
                        const static int NameTablePosition[] = {
                            0,0, 256,0,
                            0,240, 256,240
                        };
                        const int *map_table_pos = &NameTablePosition[nameTableIndex*2];
                        
                        int map_pos_x = map_table_pos[0] + tile_x*8 + tx;
                        int map_pos_y = map_table_pos[1] + tile_y*8 + ty;
                        
                        bk_peletteIndex = _scrollBuffer[map_pos_y*512 + map_pos_x];
                    }
#endif
                    
                    bk_systemPaletteUnitIndex = bkPaletteAddr[bk_peletteIndex]; // 系统默认调色板颜色索引 [0,63]
                    {
                        int pixelIndex = (line_y * 32*8 + line_x); // [屏幕扫描] 最低位是右边第一像素，所以渲染顺序要从右往左
                        
                        // 获取当前像素的精灵数据，并检测碰撞
                        int spr_systemPaletteUnitIndex = 0; // 取低6位[0,63]
                        bool sprFront = true;
                        {
                            uint8_t& spr_data = _spr_buffer[pixelIndex];
                            
                            if (spr_data != 0)
                            {
                                sprFront = (spr_data & 0x10) == 0; // 即第4bit为0
                                spr_systemPaletteUnitIndex = sprPaletteAddr[spr_data & 15];
                                
                                // 碰撞检测 (必须同时显示背景和精灵的情况下)
                                if (showSpr && showBg)
                                {
                                    // 精灵0单独使用一个全屏buffer
                                    uint8_t spr0data = _spr0buffer[pixelIndex];
                                    bool isFirstSprite = spr0data > 31; // 即第5bit非0
                                    int sprPaletteIndex = spr0data & 15;   // 前4bit
                                    
                                     // 使用精灵0来检测碰撞
                                    int sysPaletteIndex = sprPaletteAddr[sprPaletteIndex];
                                    
                                    // 碰撞条件，第一精灵 & 非0调色板第一位（透明色）& 允许碰撞
                                    if (isFirstSprite && sysPaletteIndex != sprPaletteAddr[0] && _status_regs->get(6) == 0)
                                    {
                                        _status_regs->set(6, 1);
                                    }
                                }
                            }
                        }
                        
                        int systemPaletteUnitIndex = -1;
                        {
                            if (showSpr && spr_systemPaletteUnitIndex != 0)
                            {
                                // 判断前后
                                if (showBg && !sprFront && bk_peletteIndex != 0)
                                {
                                    systemPaletteUnitIndex = bk_systemPaletteUnitIndex;
                                }
                                else
                                {
                                    systemPaletteUnitIndex = spr_systemPaletteUnitIndex;
                                }
                            }
                            else if (showBg)
                            {
                                systemPaletteUnitIndex = bk_systemPaletteUnitIndex;
                            }
                        }
                        
                        auto* pb = (RGB*)&display_buffer[pixelIndex*3];
                        if (systemPaletteUnitIndex != -1)
                        {
                            RGB* rgb = (RGB*)&DEFAULT_PALETTE[systemPaletteUnitIndex*3];
                            *pb = *rgb;
                        }
                    }
                }
            }
            
            _scanline_x += pixelCount;
            
            // 奇数帧，跳过最后一条扫描的最后一个点
//            if (_frameCount % 2 == 1 && _scanline_y == _frame_h-1 && _scanline_x >= _frame_w-1) _scanline_x ++;
            
            // 检查当前扫描线完成
            RENES_ASSERT(_frame_w > 0 && _frame_h > 0);
            if (_scanline_x >= _frame_w)
            {
                // 绘制了最后一条可见扫描线，则设置VBlank标记
                if (_scanline_y == RENES_FRAME_VISIBLE_H-1) // 239
                {
                    _status_regs->set(7, 1);
                    // 设置vblank事件
                    if (vblankEvent)
                        *vblankEvent = true;
                }
                
                _scanline_y ++;
                _scanline_y %= _frame_h;
                
                int count = _scanline_x - _frame_w + 1;
                _scanline_x = 0;
                _drawScanline(vblankEvent, count); // 这里执行后 _scanline_x = count
                
                // 发生了一次扫描线循环，设置当前帧完成标记（外部执行等待）
                // 由于PPU初始扫描线是从-1开始，我们设置为261，也就是最后一根扫描线纳入下一帧的范畴。当跳到最后一根扫描线的时候，标记当前帧结束
                if (_scanline_y == 0)
                    _currentFrameOver = true;
                
//                printf("%d\n", _scanline_y);
            }
        }
        
        // 检查图案是否使用了该调色板下标
        inline
        bool isUsedPaletteIndex(const uint8_t* bkTileAddr, int paletteIndex_low2bit) const
        {
            //const uint8_t* bkTileAddr = &bkPetternTableAddr[tileIndex * 16];
            {
                for (int i=0; i<64; i++)
                {
                    int ty_ = i/8;
                    int tx_ = i%8;
                    {
                        // 图案表tile 第一字节与第八字节对应的bit位，组成这一像素颜色的低2位（最后构成一个[0,63]的数，索引到系统默认的64种颜色）
                        int low0 = ((bit8*)&bkTileAddr[ty_])->get(tx_);
                        int low1 = ((bit8*)&bkTileAddr[ty_+8])->get(tx_);
                        int low2bit = low0 | (low1 << 1);
                        if (low2bit == paletteIndex_low2bit)
                            return true;
                    }
                }
            }
            return false;
        }
        
        // 更新所有使用了目标调色板下标的tile
        void updateBackgroundTile(int nameTableIndex, int paletteIndex)
        {
            const uint8_t* bkPetternTableAddr = _bkPetternTableAddress();
            const uint8_t* addr = _vram->nameTableAddress(nameTableIndex);
            int paletteIndex_low2bit = paletteIndex & 0x3;
            for (int ti = 0; ti < 32*30; ti++)
            {
                const uint8_t* bkTileAddr = &bkPetternTableAddr[addr[ti] * 16];
                if (isUsedPaletteIndex(bkTileAddr, paletteIndex_low2bit))
                {
                    updateBackgroundTile(nameTableIndex, ti%32, ti/32);
                }
            }
        }
        
        // 更新指定位置的tile
        void updateBackgroundTile(int nameTableIndex, int tile_x, int tile_y, int tile_x_count=1, int tile_y_count=1)
        {
            int stride = DISPLAY_BUFFER_PIXEL_WIDTH * 2; // 水平是两个缓冲区排列的，所以这里要 x2
            const static int offset[] = {
                0, DISPLAY_BUFFER_PIXEL_WIDTH,
                2*DISPLAY_BUFFER_PIXEL_CONUT, 2*DISPLAY_BUFFER_PIXEL_CONUT+DISPLAY_BUFFER_PIXEL_WIDTH
            };
            
            drawBackground(_scrollBuffer + offset[nameTableIndex], stride, tile_x, tile_y, tile_x_count, tile_y_count, nameTableIndex, _bkPetternTableAddress(), _bkPaletteAddress());
        }
        
        // 绘制瓦片到缓冲区
        void drawTile(uint8_t* buffer, int stride, int x, int y, int high2, const uint8_t* tileAddr, const uint8_t* paletteAddr)
        {
            for (int ty=0; ty<8; ty++)
            {
                int dst_y = y + ty;
                
                if (dst_y < 0 || dst_y >= RENES_FRAME_VISIBLE_H)
                    continue;
                
                // int ty_ = flipV ? 7-ty : ty;
                int ty_ = ty;
                
                for (int tx=0; tx<8; tx++)
                {
                    int dst_x = x + tx;
                    
                    if (dst_x < 0 || dst_x >= RENES_FRAME_VISIBLE_W)
                        continue;
                    
                    // int tx_ = flipH ? tx : 7-tx;
                    int tx_ = 7-tx;
                    
                    // tile 第一字节与第八字节对应的bit位，组成这一像素颜色的低2位（构成调色板下标[16]）
                    int low0 = ((bit8*)&tileAddr[ty_])->get(tx_);
                    int low1 = ((bit8*)&tileAddr[ty_+8])->get(tx_);
                    int low2 = low0 | (low1 << 1);
                    
                    // 4bit
                    int paletteUnitIndex = (high2 << 2) + low2; // 背景调色板单元索引号
                    
                    // 向缓冲区写入调色表下标
                    int bpp = 1;
                    int pixelIndex = dst_y * stride + dst_x * bpp;
                    auto* pb = (uint8_t*)&buffer[pixelIndex];
                    *pb = paletteUnitIndex;
                }
            }
        };
        
        // 绘制精灵到缓冲区
        void drawSprBuffer(uint8_t* buffer, int x, int y, int high2, const uint8_t* tileAddr, const uint8_t* paletteAddr, bool flipH, bool flipV, bool isFirstSprite, bool priority)
        {
            bool mode8x16 = _control_regs->get(5);
            
            if (!mode8x16)
            {
                for (int ty=0; ty<8; ty++)
                {
                    int dst_y = y + ty; // 屏幕上的坐标y
                    if (dst_y < 0 || dst_y >= RENES_FRAME_VISIBLE_H)
                        continue;
                    
                    int ty_ = flipV ? 7-ty : ty; // 内部数据坐标转换
                    
                    const bit8* low0_tileAddr = (const bit8*)&tileAddr[ty_];
                    const bit8* low1_tileAddr = low0_tileAddr+8;
                    
                    for (int tx=0; tx<8; tx++)
                    {
                        int dst_x = x + tx; // 屏幕上的坐标x
                        if (dst_x < 0 || dst_x >= RENES_FRAME_VISIBLE_W)
                            continue;
                        
                        /*
                         x (0-7)
                                 像素 0123 4567
                                 bit 7654 3210
                         8bit: (高位) 1111 1111 (低位)
                         8bit: (高位) 1111 1111 (低位)
                         最低位是右边第一像素，所以内部数据坐标要颠倒
                         */
                        int tx_ = flipH ? tx : 7-tx;
                        
                        // tile 第一字节与第八字节对应的bit位，组成这一像素颜色的低2位（最后构成一个[0,63]的数，索引到系统默认的64种颜色）
                        int low0 = low0_tileAddr->get(tx_);
                        int low1 = low1_tileAddr->get(tx_);
                        int low2 = low0 | (low1 << 1);
                        
                        int paletteUnitIndex = (high2 << 2) + low2; // 背景调色板单元索引号 [0, 15]
                        
                        // 如果绘制精灵，而且当前颜色引用到背景透明色（背景调色板第一位），就不绘制
                        if (paletteUnitIndex % 4 == 0)
                        {
                            continue;
                        }
                        
                        // 根据dst_y和dst_x来确定像素索引，缓冲区中表示的是 1字节/1像素
                        // 所以stride是256
                        int pixelIndex = dst_y*RENES_FRAME_VISIBLE_W + dst_x;
                        // 11 11 1111
                        // 6、7bit: 未使用
                        // 5 bit: 第一精灵标记
                        // 4 bit: 前置精灵标记
                        // 低4bit: [16]调色板下标
                        int spr_data = (isFirstSprite ? (3 << 5) : 0) | (priority << 4) | paletteUnitIndex;
                        
                        buffer[pixelIndex] = spr_data;
                    }
                }
            }
            else
            {
                
                for (int ty=0; ty<16; ty++)
                {
                    int dst_y = y + ty; // 屏幕上的坐标y
                    if (dst_y < 0 || dst_y >= RENES_FRAME_VISIBLE_H)
                        continue;
                    
                    int ty_ = flipV ? 15-(ty%8) : ty; // 内部数据坐标转换
                    int tileAddrOffset = ty_ >= 8 ? 16 : 0;
                    
                    const bit8* low0_tileAddr = (const bit8*)&tileAddr[tileAddrOffset + (ty_%8)];
                    const bit8* low1_tileAddr = low0_tileAddr+8;
                    
                    for (int tx=0; tx<8; tx++)
                    {
                        int dst_x = x + tx; // 屏幕上的坐标x
                        if (dst_x < 0 || dst_x >= RENES_FRAME_VISIBLE_W)
                            continue;
                        
                        /*
                         x (0-7)
                                 像素 0123 4567
                                 bit 7654 3210
                         8bit: (高位) 1111 1111 (低位)
                         8bit: (高位) 1111 1111 (低位)
                         最低位是右边第一像素，所以内部数据坐标要颠倒
                         */
                        int tx_ = flipH ? tx : 7-tx;
                        
                        // tile 第一字节与第八字节对应的bit位，组成这一像素颜色的低2位（最后构成一个[0,63]的数，索引到系统默认的64种颜色）
                        int low0 = low0_tileAddr->get(tx_);
                        int low1 = low1_tileAddr->get(tx_);
                        int low2 = low0 | (low1 << 1);
                        
                        int paletteUnitIndex = (high2 << 2) + low2; // 背景调色板单元索引号 [0, 15]
                        
                        // 如果绘制精灵，而且当前颜色引用到背景透明色（背景调色板第一位），就不绘制
                        if (paletteUnitIndex % 4 == 0)
                        {
                            continue;
                        }
                        
                        // 根据dst_y和dst_x来确定像素索引，缓冲区中表示的是 1字节/1像素
                        // 所以stride是256
                        int pixelIndex = dst_y*RENES_FRAME_VISIBLE_W + dst_x;
                        // 11 11 1111
                        // 6、7bit: 未使用
                        // 5 bit: 第一精灵标记
                        // 4 bit: 前置精灵标记
                        // 低4bit: [16]调色板下标
                        int spr_data = (isFirstSprite ? (3 << 5) : 0) | (priority << 4) | paletteUnitIndex;
                        
                        buffer[pixelIndex] = spr_data;
                    }
                }
            }
        };
        
        // 从第nameTableIndex个名称表开始绘制（竖直镜像）
        void drawBackground(uint8_t* buffer, int stride, int tile_x_start, int tile_y_start, int tile_x_count, int tile_y_count, int tableIndex, const uint8_t* bkPetternTableAddr, const uint8_t* bkPaletteAddr){
            
            // 背景的tile偏移（图案表中的tile偏移）
            int bk_offset_x = 0;
            int bk_offset_y = 0;
            
            int tile_x_max = tile_x_start + tile_x_count;
            int tile_y_max = tile_y_start + tile_y_count;
            
            for (int bk_y=tile_y_start; bk_y<tile_y_max; bk_y++) // 只显示 30/30 行tile
            {
                int s_y = bk_y+bk_offset_y;
                int tile_y = s_y % 30; // 竖直第_y个tile，30个tile 垂直循环
                
                for (int bk_x=tile_x_start; bk_x<tile_x_max; bk_x++)
                {
                    int s_x = (bk_x+bk_offset_x);
                    int tile_x = s_x % 32; // 水平第_x个tile，32个tile 水平循环
                    
                    // 实际名称表
                    int nameTableIndex = ((tableIndex + s_x/32) % 2);
                    int attributeTableIndex = nameTableIndex;
                    // 未处理左右镜像，需要计算s_y ?
                    
                    const uint8_t* nameTableAddr = _nameTableAddress(nameTableIndex);
                    const uint8_t* attributeTableAddr = _attributeTableAddress(attributeTableIndex);
                    
                    // 一个字节表示4x4的tile组，先确定当前(x,y)所在字节
                    uint8_t attributeAddrFor4x4Tile = attributeTableAddr[(tile_y / 4 * (32/4) + tile_x / 4)];
                    int bit = (tile_y % 4) / 2 * 4 + (tile_x % 4) / 2 * 2;
                    int high2 = (attributeAddrFor4x4Tile >> bit) & 0x3;
                    
                    int tileIndex = nameTableAddr[tile_y*32 + tile_x]; // 得到 bkg tile index
                    
                    // 确定在图案表里的tile地址，每个tile是8x8像素，占用16字节。
                    // 前8字节(8x8) + 后8字节(8x8)
                    const uint8_t* tileAddr = &bkPetternTableAddr[tileIndex * 16];
                    
                    drawTile(buffer, stride, bk_x*8, bk_y*8, high2, tileAddr, bkPaletteAddr);
                }
            }
        };
        
        // 背景图案表地址
        // 图案表：256*16(4KB)的空间
        const uint8_t* _bkPetternTableAddress() const
        {
            // 第4bit决定背景图案表地址 0x0000或0x1000
            return _vram->petternTableAddress(_control_regs->get(4));
        }
        
        // 精灵图案表地址
        // 图案表：256*16(4KB)的空间
        const uint8_t* _sprPetternTableAddress(int spriteID) const
        {
            // 第3bit决定精灵图案表地址 0x0000或0x1000, 8x16模式下忽略，直接用0x0000
            bool mode8x16 = _control_regs->get(5);
            if (!mode8x16)
            {
                return _vram->petternTableAddress(_control_regs->get(3)) + spriteID*16;
            }
            else
            {
                if (spriteID % 2 == 0)
                {
                    return _vram->petternTableAddress(0) + spriteID/2*32;
                }
                else
                {
                    return _vram->petternTableAddress(1) + (spriteID-1)/2*32;
                }
            }
        }
        const uint8_t* _bkPaletteAddress() const { return _vram->bkPaletteAddress(); }
        const uint8_t* _sprPaletteAddress() const { return _vram->sprPaletteAddress(); }
        const uint8_t* _nameTableAddress(int index) const { return _vram->nameTableAddress(index); }
        const uint8_t* _attributeTableAddress(int index) const { return _vram->attributeTableAddress(index); }
        
        const static int DISPLAY_BUFFER_PIXEL_WIDTH = RENES_FRAME_VISIBLE_W;
        const static int DISPLAY_BUFFER_PIXEL_HEIGHT = RENES_FRAME_VISIBLE_H;
        const static int DISPLAY_BUFFER_PIXEL_BPP = 3;
        const static int DISPLAY_BUFFER_PIXEL_CONUT = DISPLAY_BUFFER_PIXEL_WIDTH * DISPLAY_BUFFER_PIXEL_HEIGHT;
        const static int DISPLAY_BUFFER_LENGTH = DISPLAY_BUFFER_PIXEL_CONUT * 3;
        
        struct Sprite {
            
            uint8_t y;
            uint8_t tileIndex;  // 精灵在图案表里的tile索引
            bit8 info;          // 精灵的信息
            uint8_t x;
        };
        
        struct RGB {
            uint8_t r;
            uint8_t g;
            uint8_t b;
        };

        void reset()
        {
            _frameCount = 0;
            
            _w = 0;
            _t = 0;
            _v = 0;
            _x = 0;
            
            _dstAddr2004 = 0;
            
            memset(_sprram, 0, 256);
        }
        
        uint16_t _t; // 临时 VRAM 地址
        uint16_t _v; // 当前 VRAM 地址
        int _w = 0;  // 地址锁寄存器
        int _x; // 3bit 精细 x 滚动
        
        
        int _2007ReadingStep = 0;
        uint8_t _2007ReadingCache = 0;
        
        uint16_t _dstAddr2004 = 0;

        // 绘图
        int _scanline_x = 0; // 当前扫描线上的扫描点坐标
        int _scanline_y = 0;
        bool _showBg;
        bool _showSpr;
        
        // 建立 w * h * 8 的缓冲区，每像素8字节，用来存储8个精灵的数据。
        // 如果该像素没有精灵，甚至没有叠加，则该位第一字节为0
        const int SPR_BUFFER_LENGTH = DISPLAY_BUFFER_PIXEL_CONUT;
        
        uint8_t* _display_buffer = 0;   // 显示缓冲区
        uint8_t* _scrollBuffer = 0;     // 卷轴缓冲区，每个像素存储4bit数[0,15]，用来定位背景调色板。数据单位：每个像素1字节。
        RGB_Buffer* _scrollBufferRGB = 0;
        
        
        uint8_t _sprram[256]; // 精灵内存, 64 个，每个4字节
        uint8_t _OAM[256];    // 每帧的OAM
        uint8_t* _spr_buffer = 0;   // 精灵绘制缓冲区
        uint8_t* _spr0buffer;       // 0号精灵缓冲区
        RGB_Buffer* _spr_bufferRGB = 0;
        
        VRAM* _vram;
        Memory* _mem;
        
        bit8* _io_regs;      // I/O 寄存器, 8 x 8bit
        bit8* _control_regs; // 控制寄存器
        bit8* _mask_regs;    // PPU屏蔽寄存器
        bit8* _status_regs;  // PPU状态寄存器
        
        int _frame_w;
        int _frame_h;

        uint32_t _frameCount;
        bool _currentFrameOver;
    };
}
