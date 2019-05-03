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

namespace ReNes {
    
    /*
     未完成：
     1、0x2000第5bit开启8x16精灵
     
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

    // 2C02
    class PPU {
        
    public:
        
        bit8* io_regs; // I/O 寄存器, 8 x 8bit
        bit8* mask_regs;    // PPU屏蔽寄存器
        bit8* status_regs;  // PPU状态寄存器
        
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
        
        PPU()
        {
            // 检查数据尺寸
            assert(4 == sizeof(Sprite));
            
            // RGB 数据缓冲区
            _buffer = (uint8_t*)malloc(RGB_BUFFER_LENGTH);
            
            // 精灵缓冲区
            _spr_buffer = (uint8_t*)malloc(SPR_BUFFER_LENGTH);
            
            // 卷轴缓冲区
            _scrollBuffer = (uint8_t*)malloc(RGB_BUFFER_LENGTH*4);
            _scrollBufferRGB = (uint8_t*)malloc(RGB_BUFFER_LENGTH*4);
            
            _vram = new VRAM();
            
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
            
            if (_scrollBufferRGB != 0)
                free(_scrollBufferRGB);
            
            if (_vram)
                delete _vram;
        }
        
        void init(Memory* mem)
        {
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
                        _vramDidUpdated(_v);
                        
                        // 自动增加
                        _v += io_regs[0].get(2) == 0 ? 1 : 32;
                        
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
                        // 据说第一次读取的值是无效的，会缓冲到下一次读取才返回
                        if (_2007ReadingStep == 0)
                        {
                        }
                        else
                        {
                            *value = _2007ReadingCache;
                        }
                        _2007ReadingCache = _vram->read8bitData(_v);
                        _v += io_regs[0].get(2) == 0 ? 1 : 32;
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
            
            // 绘制背景
            const uint8_t* sprPetternTableAddr = _sprPetternTableAddress(); // 第3bit决定精灵图案表地址 0x0000或0x1000
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
                
                const uint8_t* tileAddr = &sprPetternTableAddr[spr->tileIndex * 16];
                
                int high2 = spr->info.get(0) | (spr->info.get(1) << 1);
                bool sprFront = spr->info.get(5); // 优先级，0 - 在背景上 1 - 在背景下
                bool flipH = spr->info.get(6); // 水平翻转
                bool flipV = spr->info.get(7); // 竖直翻转
                
                drawSprBuffer(_spr_buffer, spr->x + 1, spr->y + 1, high2, tileAddr, sprPaletteAddr, flipH, flipV, i == 0, sprFront);
            }
            
            // test 暂时全部绘制            
//            static bool inited = false;
//            if (!inited)
            if (false)
            {
//                inited = true;
                const uint8_t* bkPetternTableAddr = _bkPetternTableAddress(); // 第4bit决定背景图案表地址 0x0000或0x1000
                const uint8_t* bkPaletteAddr = _bkPaletteAddress();   // 背景调色板地址
                
                // 上下镜像模式，水平复制
                for (int i=0; i<4; i++)
                {
                    //                size_t size = RGB_BUFFER_LENGTH;
                    //                size_t lineLength = BUFFER_PIXEL_WIDTH*BUFFER_PIXEL_BPP;
                    //                const static int offset[] = {
                    //                    0, BUFFER_PIXEL_WIDTH*BUFFER_PIXEL_BPP,
                    //                    2*RGB_BUFFER_LENGTH, 2*RGB_BUFFER_LENGTH+BUFFER_PIXEL_WIDTH*BUFFER_PIXEL_BPP
                    //                };
                    //
                    //                drawBackground(_scrollBuffer + offset[i], lineLength*2, 0, 32, 0, 30, i, bkPetternTableAddr, bkPaletteAddr);
                    
                    int stride = BUFFER_PIXEL_WIDTH * 2;
                    const static int offset[] = {
                        0, BUFFER_PIXEL_WIDTH,
                        2*BUFFER_PIXEL_CONUT, 2*BUFFER_PIXEL_CONUT+BUFFER_PIXEL_WIDTH
                    };
                    
                    drawBackground(_scrollBuffer + offset[i], stride, 0, 0, 32, 30, i, bkPetternTableAddr, bkPaletteAddr);
                }
            }
            
            _scanline_y = 0;
        }
        
        
        // 模拟扫描线 ? 但是应该是每条指令执行完成后，绘制这一行上的一定数量的点
        void drawScanline2(bool* vblankEvent)
        {
            *vblankEvent = false;

            // 绘制背景
            const uint8_t* bkPetternTableAddr = _bkPetternTableAddress(); // 第4bit决定背景图案表地址 0x0000或0x1000
            const uint8_t* bkPaletteAddr = _bkPaletteAddress();   // 背景调色板地址
            const uint8_t* sprPaletteAddr = _sprPaletteAddress();  // 精灵调色板地址
            
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
                const static int LINE_X_MAX = 33*8;
                
                bool fuck = false;
                
                for (int line_x=0; line_x<LINE_X_MAX; line_x++)
                {
                    struct Test {
                        int x;
                        int tx;
                    };
#ifdef CMP_PARAM
                    static std::map<int, Test> A_map = {{0, {0, 1}}, {1, {0, 2}}, {2, {0, 3}}, {3, {0, 4}}, {4, {0, 5}}, {5, {0, 6}}, {6, {0, 7}}, {7, {1, 0}},{8, {1, 1}},{9, {1, 2}},{10, {1, 3}},{11, {1, 4}},{12, {1, 5}},{13, {1, 6}},{14, {1, 7}},{15, {2, 0}},{16, {2, 1}},{17, {2, 2}},{18, {2, 3}},{19, {2, 4}},{20, {2, 5}},{21, {2, 6}},{22, {2, 7}},{23, {3, 0}},{24, {3, 1}},{25, {3, 2}},{26, {3, 3}},{27, {3, 4}},{28, {3, 5}},{29, {3, 6}},{30, {3, 7}},{31, {4, 0}},{32, {4, 1}},{33, {4, 2}},{34, {4, 3}},{35, {4, 4}},{36, {4, 5}},{37, {4, 6}},{38, {4, 7}},{39, {5, 0}},{40, {5, 1}},{41, {5, 2}},{42, {5, 3}},{43, {5, 4}},{44, {5, 5}},{45, {5, 6}},{46, {5, 7}},{47, {6, 0}},{48, {6, 1}},{49, {6, 2}},{50, {6, 3}},{51, {6, 4}},{52, {6, 5}},{53, {6, 6}},{54, {6, 7}},{55, {7, 0}},{56, {7, 1}},{57, {7, 2}},{58, {7, 3}},{59, {7, 4}},{60, {7, 5}},{61, {7, 6}},{62, {7, 7}},{63, {8, 0}},{64, {8, 1}},{65, {8, 2}},{66, {8, 3}},{67, {8, 4}},{68, {8, 5}},{69, {8, 6}},{70, {8, 7}},{71, {9, 0}},{72, {9, 1}},{73, {9, 2}},{74, {9, 3}},{75, {9, 4}},{76, {9, 5}},{77, {9, 6}},{78, {9, 7}},{79, {10, 0}},{80, {10, 1}},{81, {10, 2}},{82, {10, 3}},{83, {10, 4}},{84, {10, 5}},{85, {10, 6}},{86, {10, 7}},{87, {11, 0}},{88, {11, 1}},{89, {11, 2}},{90, {11, 3}},{91, {11, 4}},{92, {11, 5}},{93, {11, 6}},{94, {11, 7}},{95, {12, 0}},{96, {12, 1}},{97, {12, 2}},{98, {12, 3}},{99, {12, 4}},{100, {12, 5}},{101, {12, 6}},{102, {12, 7}},{103, {13, 0}},{104, {13, 1}},{105, {13, 2}},{106, {13, 3}},{107, {13, 4}},{108, {13, 5}},{109, {13, 6}},{110, {13, 7}},{111, {14, 0}},{112, {14, 1}},{113, {14, 2}},{114, {14, 3}},{115, {14, 4}},{116, {14, 5}},{117, {14, 6}},{118, {14, 7}},{119, {15, 0}},{120, {15, 1}},{121, {15, 2}},{122, {15, 3}},{123, {15, 4}},{124, {15, 5}},{125, {15, 6}},{126, {15, 7}},{127, {16, 0}},{128, {16, 1}},{129, {16, 2}},{130, {16, 3}},{131, {16, 4}},{132, {16, 5}},{133, {16, 6}},{134, {16, 7}},{135, {17, 0}},{136, {17, 1}},{137, {17, 2}},{138, {17, 3}},{139, {17, 4}},{140, {17, 5}},{141, {17, 6}},{142, {17, 7}},{143, {18, 0}},{144, {18, 1}},{145, {18, 2}},{146, {18, 3}},{147, {18, 4}},{148, {18, 5}},{149, {18, 6}},{150, {18, 7}},{151, {19, 0}},{152, {19, 1}},{153, {19, 2}},{154, {19, 3}},{155, {19, 4}},{156, {19, 5}},{157, {19, 6}},{158, {19, 7}},{159, {20, 0}},{160, {20, 1}},{161, {20, 2}},{162, {20, 3}},{163, {20, 4}},{164, {20, 5}},{165, {20, 6}},{166, {20, 7}},{167, {21, 0}},{168, {21, 1}},{169, {21, 2}},{170, {21, 3}},{171, {21, 4}},{172, {21, 5}},{173, {21, 6}},{174, {21, 7}},{175, {22, 0}},{176, {22, 1}},{177, {22, 2}},{178, {22, 3}},{179, {22, 4}},{180, {22, 5}},{181, {22, 6}},{182, {22, 7}},{183, {23, 0}},{184, {23, 1}},{185, {23, 2}},{186, {23, 3}},{187, {23, 4}},{188, {23, 5}},{189, {23, 6}},{190, {23, 7}},{191, {24, 0}},{192, {24, 1}},{193, {24, 2}},{194, {24, 3}},{195, {24, 4}},{196, {24, 5}},{197, {24, 6}},{198, {24, 7}},{199, {25, 0}},{200, {25, 1}},{201, {25, 2}},{202, {25, 3}},{203, {25, 4}},{204, {25, 5}},{205, {25, 6}},{206, {25, 7}},{207, {26, 0}},{208, {26, 1}},{209, {26, 2}},{210, {26, 3}},{211, {26, 4}},{212, {26, 5}},{213, {26, 6}},{214, {26, 7}},{215, {27, 0}},{216, {27, 1}},{217, {27, 2}},{218, {27, 3}},{219, {27, 4}},{220, {27, 5}},{221, {27, 6}},{222, {27, 7}},{223, {28, 0}},{224, {28, 1}},{225, {28, 2}},{226, {28, 3}},{227, {28, 4}},{228, {28, 5}},{229, {28, 6}},{230, {28, 7}},{231, {29, 0}},{232, {29, 1}},{233, {29, 2}},{234, {29, 3}},{235, {29, 4}},{236, {29, 5}},{237, {29, 6}},{238, {29, 7}},{239, {30, 0}},{240, {30, 1}},{241, {30, 2}},{242, {30, 3}},{243, {30, 4}},{244, {30, 5}},{245, {30, 6}},{246, {30, 7}},{247, {31, 0}},{248, {31, 1}},{249, {31, 2}},{250, {31, 3}},{251, {31, 4}},{252, {31, 5}},{253, {31, 6}},{254, {31, 7}},{255, {0, 0}},};
#endif
//                    int tx = (line_x+bg_t_x)%8; // 对应当前瓦片上的index
//                    int tx = (line_x+bg_t_x)%8; // 对应当前瓦片上的index
                    int tx = line_x%8; // 对应当前瓦片上的index
                    int draw_line_x = line_x/8*8-bg_t_x; // 背景绘制的整体起始点
                    
                    
                    // tx 1
                    // draw_line_x 7 = 8-1
//                    if (line_x < 8)
//                    {
//                        printf("tx: %d line_x: %d bg_t_x: %d draw_line_x: %d\n", tx, line_x, bg_t_x, draw_line_x);
//                    }
                    int x;
//                    int x2;
                    
                    if (draw_line_x + tx > 255)
                        continue;
//
//                    if (draw_line_x + tx< 0)
//                    {
//                        fuck = true;
//                        continue;
//                    }
                    if (draw_line_x + tx< 0)
                    {
                        fuck = true;
                        continue;
                    }
//                    if (line_x == 7 && draw_line_x == -1)
//                        line_x = 7;
                
                    const uint8_t* paletteAddr = bkPaletteAddr;
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
                    
//                    if (line_x % 8 == 0) // 每次跨界才计算新的瓦片地址
                    {
//                        int bg_x = line_x/8;
//                        int s_x = (bg_x+bg_offset_x); // 目标tile
//                        x = s_x % 32; // 32个tile 水平循环
                        
//                        int bg_x = (line_x + bg_t_x)/8;
                        int bg_x = line_x/8;
                        int s_x = (bg_x+bg_offset_x); // 目标tile
                        x = s_x % 32; // 32个tile 水平循环
                        
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
                            const uint8_t* nameTableAddr = _nameTableAddress(nameTableIndex);
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
                             */
                            const uint8_t* attributeTableAddr = _attributeTableAddress(nameTableIndex);
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
                    
#ifdef CMP_PARAM
                    if (line_y == 31 && fuck)
                    {
//                        A_map[draw_line_x + tx] = {x, tx};
                        
//                        Test& A = A_map.at(line_x);
                        Test& A = A_map.at(draw_line_x + tx);
                        if (A.x != x || A.tx != tx)
                            A.x = A.x;
                    }
                    else if (line_y == 32 && fuck)
                    {
                        printf("{");
                        for (auto it : A_map)
                        {
//                            printf("[%d] %d %d\n", it.first, it.second.tx, it.second.x);
                            printf("{%d, {%d, %d}},", it.first, it.second.x, it.second.tx);
                        }
                        printf("}");
                        printf("\n");
                    }
#endif
                    // line_x/8*8 为当前tile的标准坐标，bg_t_x 为偏移位置
                    
                    {
                        {
                            int bg_systemPaletteUnitIndex = paletteAddr[peletteIndex.merge()]; // 系统默认调色板颜色索引 [0,63]
                            
//                            int pixelIndex = (NES_MIN(draw_line_y + ty, 239) * 32*8 + NES_MIN(draw_line_x + tx, 255)); // 最低位是右边第一像素，所以渲染顺序要从右往左
//                            int pixelIndex = (NES_MIN(draw_line_y + ty, 239) * 32*8 + NES_MIN(line_x, 255));
                            int pixelIndex = (NES_MIN(draw_line_y + ty, 239) * 32*8 + NES_MIN(draw_line_x + tx, 255)); // 最低位是右边第一像素，所以渲染顺序要从右往左
                            
                            
                            // 获取当前像素的精灵数据，并检测碰撞
                            int spr_systemPaletteUnitIndex = 0; // 取低6位[0,63]
                            bool sprFront = true;
                            {
                                for (int i=0; i<8; i++)
                                {
                                    uint8_t& spr_data = _spr_buffer[pixelIndex + i*BUFFER_PIXEL_CONUT];
                                    
                                    if (spr_data == 0)
                                    {
                                        break;
                                    }
                                    
                                    //                                        spr_systemPaletteUnitIndex = spr_data & 63;
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
        
        void drawScanline(bool* vblankEvent)
        {
            *vblankEvent = false;

            // 绘制背景
            const uint8_t* bkPetternTableAddr = _bkPetternTableAddress();
            const uint8_t* sprPetternTableAddr = _sprPetternTableAddress();
            
            const uint8_t* bkPaletteAddr = _bkPaletteAddress();
            const uint8_t* sprPaletteAddr = _sprPaletteAddress();
            
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
//                int bg_y = line_y/8 + bg_offset_y; [瓦片扫描]
                int bg_tile_y = (line_y + bg_t_y)/8 + bg_offset_y; // [屏幕扫描]
                int tile_y = bg_tile_y % 30; // 30个tile 垂直循环
                
//                int ty = line_y%8; // [瓦片扫描]
                int ty = (line_y+bg_t_y)%8; // [屏幕扫描]
                int draw_line_y = line_y/8*8-bg_t_y;
                int dst_y = draw_line_y + ty;
                
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
                const static int LINE_X_MAX = 32*8; // [屏幕扫描]

                for (int line_x=0; line_x<LINE_X_MAX; line_x++)
                {
                    int tx = (line_x+bg_t_x)%8; // [屏幕扫描] 对应当前瓦片上的index
//                    int tx = line_x%8; // [瓦片扫描] 对应当前瓦片上的index
                    int draw_line_x = line_x/8*8-bg_t_x; // 背景绘制的整体起始点
                    int dst_x = draw_line_x + tx; // 屏幕上的坐标
                    
                    // [瓦片扫描]
//                    if (dst_x >= 256)
//                        continue;
//
//                    if (dst_x < 0)
//                    {
//                        continue;
//                    }
                    
                    // 背景不支持tile翻转，精灵才支持，这里作差别提示
                    bool flipV = false;
                    bool flipH = false;
                    int tx_ = flipH ? tx : 7-tx;
                    int ty_ = flipV ? 7-ty : ty;
                    
                    int first_tile_x = (line_x + bg_t_x)/8 + bg_offset_x; // [屏幕扫描]
                    
                    // 从基础名称表开始绘制，并根据tile偏移+瓦片索引
                    int nameTableIndex = ((firstNameTableIndex + first_tile_x/32) % 2);
                    
                    // int bg_tile_x = line_x/8 + bg_offset_x; // [瓦片扫描] 当前位置的tile
                    // 32个tile 水平循环在屏幕上的对应瓦片坐标，用来定位在对应（当前/下一个）名称表里的位置，确定tileIndex
                    int tile_x = first_tile_x % 32;
                    
                    int bg_systemPaletteUnitIndex;
                    
                    int bg_peletteIndex;
//                    #define RENES_BG_MODE_0
                    #ifdef RENES_BG_MODE_0
                    {
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
                        bg_peletteIndex = peletteIndex.merge();
                    }
                    #else
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
                        
                        bg_peletteIndex = _scrollBuffer[map_pos_y*512 + map_pos_x];
                    }
                    #endif
                    
                    bg_systemPaletteUnitIndex = bkPaletteAddr[bg_peletteIndex]; // 系统默认调色板颜色索引 [0,63]
                    {
                        //                            int pixelIndex = (dst_y * 32*8 + dst_x); // [瓦片扫描] 最低位是右边第一像素，所以渲染顺序要从右往左
                        int pixelIndex = (line_y * 32*8 + line_x); // [屏幕扫描] 最低位是右边第一像素，所以渲染顺序要从右往左
                        
                        // 获取当前像素的精灵数据，并检测碰撞
                        int spr_systemPaletteUnitIndex = 0; // 取低6位[0,63]
                        bool sprFront = true;
                        {
                            for (int i=0; i<1; i++)
                            {
                                uint8_t& spr_data = _spr_buffer[pixelIndex + i*BUFFER_PIXEL_CONUT];
                                
                                
                                if (spr_data == 0)
                                {
                                    break;
                                }
                                
                                //                                        spr_systemPaletteUnitIndex = spr_data & 63;
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
                                if (showBg && !sprFront && bg_peletteIndex != 0)
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
        
        // dump数据，给外部逻辑使用。index -> color
        void dumpScrollToBuffer()
        {
            //const uint8_t* bkPetternTableAddr = _bkPetternTableAddress();
            const uint8_t* bkPaletteAddr = _bkPaletteAddress();
            
            // convert to RGB buffer
            int size = BUFFER_PIXEL_WIDTH*BUFFER_PIXEL_HEIGHT*4;
            for (int i=0; i<size; i++)
            {
                int systemPaletteUnitIndex = bkPaletteAddr[_scrollBuffer[i]]; // 系统默认调色板颜色索引 [0,63]
                RGB* rgb = (RGB*)&DEFAULT_PALETTE[systemPaletteUnitIndex*3];
                ((RGB*)_scrollBufferRGB)[i] = *rgb;
            }
        }
        
        // 缓冲区信息
        inline int width() const { return BUFFER_PIXEL_WIDTH; }
        inline int height() const { return BUFFER_PIXEL_HEIGHT; }
        inline int bpp() const { return BUFFER_PIXEL_BPP; }
        inline uint8_t* buffer() const { return _buffer; }
        
        // 调试缓冲区，用于外部显示卷轴
        inline uint8_t* scrollBufferRGB() const { return _scrollBufferRGB; }
        
        // 输出调色板
        void petternTables(uint8_t p[32]) const
        {
            for (int i=0; i<32; i++)
            {
                p[i] = _vram->bkPaletteAddress()[i];
            }
        }
        
    private:
        
        void _vramDidUpdated(uint16_t addr)
        {
            // VRAM数据与内存的交互，通过2007最后来确定。在这里，可以确定哪部分VRAM内存被修改，进行tile更新
            int nameTableIndex;
            int tile_x, tile_y;
            if (addr < 0x2000) // 图案表更新 [0x0000, 0x1FFF]
            {
                // 单个图案是8x8 * 2的空间，可以根据地址得到影响的图案index in [256]
                // 遍历名称表中使用了该图案index的tile，进行更新
                // 只更新图案表，先确定图案表地址
                int i = io_regs[0].get(4);
                
                int offset = addr - i*0x1000;
                int index = offset / 16; // 得到下标[0, 255]
                bool updatedNameTableIndex[4] = {false};
                for (int i=0; i<4; i++)
                {
                    if (updatedNameTableIndex[i])
                        continue;
                    
                    // 遍历里面32x30字节，更新其中 == index 的项目
                    updateBackgroundTile(i, index);
                    updatedNameTableIndex[i] = true;
                    
                    // 检查其镜像，有，则加入
                    int mirroringIndex = _vram->nameTableMirroring(i);
                    if (mirroringIndex != i)
                    {
                        updateBackgroundTile(mirroringIndex, index);
                        updatedNameTableIndex[mirroringIndex] = true;
                    }
                }
            }
            //else if (addr < 0x3000) // 名称表更新 [0x2000, 0x2FFF]
            else if (addr < 0x3F00) // [0x2000, 0x2EFF] 镜像，长度0x0F00
            {
                // 名称表 32x20 的空间
                int offset = addr - 0x2000;
                
                if (addr < 0x3000)
                    offset = addr - 0x2000;
                else
                    offset = addr - 0x3000; // 镜像
                
                nameTableIndex = offset / 0x0400;
                
                if (offset % 0x400 < 0x400 - 64) // 名称表范围 [0, 959]
                {
                    tile_y = (offset % 0x400) / 32;
                    tile_x = (offset % 0x400) % 32;
                    
                    {
                        updateBackgroundTile(nameTableIndex, tile_x, tile_y);
                        
                        // 更新镜像
                        int mirroringIndex = _vram->nameTableMirroring(nameTableIndex);
                        if (mirroringIndex != nameTableIndex)
                        {
                            updateBackgroundTile(mirroringIndex, tile_x, tile_y);
                        }
                    }
                }
                else
                {
                    // 得到[64]偏移
                    int offset = addr % 0x400 - (0x400-64);
                    int tile_group_y = offset / 8;
                    int tile_group_x = offset % 8;
                    int tile_x = tile_group_x * 4;
                    int tile_y = tile_group_y * 4;
                    
                    updateBackgroundTile(nameTableIndex, tile_x, tile_y, 4, 4);
                    
                    // 更新镜像
                    int mirroringIndex = _vram->nameTableMirroring(nameTableIndex);
                    if (mirroringIndex != nameTableIndex)
                    {
                        updateBackgroundTile(mirroringIndex, tile_x, tile_y, 4, 4);
                    }
                }
            }
            else if (addr < 0x3F00) // [0x2000, 0x2EFF] 镜像
            {
                // 不知道会不会处理
            }
            else if (addr < 0x3F20) // 调色板 0,1
            {
                if (addr < 0x3F10) // 背景调色板
                {
                    // 遍历使用了该调色板下标的瓦片
                    int offset = addr - 0x3F00; // [16]
                    int paletteIndex = offset; // 得到下标[0, 15]
                    bool updatedNameTableIndex[4] = {false};
                    
                    for (int i=0; i<4; i++)
                    {
                        if (updatedNameTableIndex[i])
                            continue;
                        
                        // 遍历里面32x30字节，更新其中 == index 的项目
                        updateBackgroundTile(i, paletteIndex);
                        updatedNameTableIndex[i] = true;
                        
                        // 检查其镜像，有，则加入
                        int mirroringIndex = _vram->nameTableMirroring(i);
                        if (mirroringIndex != i)
                        {
                            updateBackgroundTile(mirroringIndex, paletteIndex);
                            updatedNameTableIndex[mirroringIndex] = true;
                        }
                    }
                }
            }
//            else if (addr < 0x4000) // 调色板镜像
//            {
//
//            }
//            else
//            {
//
//            }
            
            
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
            int stride = BUFFER_PIXEL_WIDTH * 2;
            const static int offset[] = {
                0, BUFFER_PIXEL_WIDTH,
                2*BUFFER_PIXEL_CONUT, 2*BUFFER_PIXEL_CONUT+BUFFER_PIXEL_WIDTH
            };
            
            drawBackground(_scrollBuffer + offset[nameTableIndex], stride, tile_x, tile_y, tile_x_count, tile_y_count, nameTableIndex, _bkPetternTableAddress(), _bkPaletteAddress());
        }
        
        // 绘制瓦片到缓冲区
        void drawTile(uint8_t* buffer, int stride, int x, int y, int high2, const uint8_t* tileAddr, const uint8_t* paletteAddr)
        {
            for (int ty=0; ty<8; ty++)
            {
                int dst_y = y + ty;
                
                if (dst_y < 0)
                    continue;
                if (dst_y >= 240)
                    continue;
                
                //                    int ty_ = flipV ? 7-ty : ty;
                int ty_ = ty;
                
                for (int tx=0; tx<8; tx++)
                {
                    int dst_x = x + tx;
                    
                    if (dst_x < 0)
                        continue;
                    
                    if (dst_x >= 256)
                        continue;
                    
                    //                        int tx_ = flipH ? tx : 7-tx;
                    int tx_ = 7-tx;
                    
                    // tile 第一字节与第八字节对应的bit位，组成这一像素颜色的低2位（构成调色板下标[16]）
                    int low0 = ((bit8*)&tileAddr[ty_])->get(tx_);
                    int low1 = ((bit8*)&tileAddr[ty_+8])->get(tx_);
                    int low2 = low0 | (low1 << 1);
                    
                    // 4bit
                    int paletteUnitIndex = (high2 << 2) + low2; // 背景调色板单元索引号
                    
                    // 缓冲区内的数据写入位置
                    
                    // old: RGB
//                    int bpp = 3;
//                    int pixelIndex = dst_y * stride + dst_x * bpp;
//                    int systemPaletteUnitIndex = paletteAddr[paletteUnitIndex]; // 系统默认调色板颜色索引 [0,63]
//
//                    RGB* rgb = (RGB*)&DEFAULT_PALETTE[systemPaletteUnitIndex*3];
//
//                    auto* pb = (RGB*)&buffer[pixelIndex];
//
//                    *pb = *rgb;
                    
                    // new: 调色表下标
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
                    int low0 = ((const bit8*)&tileAddr[ty_])->get(tx_);
                    int low1 = ((const bit8*)&tileAddr[ty_+8])->get(tx_);
                    int low2 = low0 | (low1 << 1);
                    
                    int paletteUnitIndex = (high2 << 2) + low2; // 背景调色板单元索引号 [0, 15]
                    
                    // 如果绘制精灵，而且当前颜色引用到背景透明色（背景调色板第一位），就不绘制
                    //                        if (systemPaletteUnitIndex == bkPaletteAddr[0])
                    if (paletteUnitIndex % 4 == 0)
                    {
                        continue;
                    }
                    
                    int pixelIndex = (NES_MIN(y + ty, 239) * 32*8 + NES_MIN(x+tx, 255)); // 最低位是右边第一像素，所以渲染顺序要从右往左
                    // 11 11 1111
                    // 高2bit: 第一精灵标记
                    // 中2bit: 前置精灵标记
                    // 低4bit: [16]调色板下标
                    int spr_data = (isFirstSprite ? (3 << 6) : 0) | (priority << 5) | paletteUnitIndex;
                    
                    for (int i=0; i<1; i++)
                    {
                        auto& dst = buffer[pixelIndex + BUFFER_PIXEL_CONUT*i];
                        
                        // 防止精灵的覆盖
                        if (dst != 0)
                            continue;
                        
                        dst = spr_data;
                    }
                }
            }
        };
        
        // 背景图案表地址
        // 图案表：256*16(4KB)的空间
        const uint8_t* _bkPetternTableAddress() const
        {
            // 第4bit决定背景图案表地址 0x0000或0x1000
            return _vram->petternTableAddress(io_regs[0].get(4));
        }
        
        // 精灵图案表地址
        // 图案表：256*16(4KB)的空间
        const uint8_t* _sprPetternTableAddress() const
        {
            // 第3bit决定精灵图案表地址 0x0000或0x1000
            return _vram->petternTableAddress(io_regs[0].get(3));
        }
        
        // 背景调色板地址
        const uint8_t* _bkPaletteAddress() const
        {
            return _vram->bkPaletteAddress();
        }
        
        // 精灵调色板地址
        const uint8_t* _sprPaletteAddress() const
        {
            return _vram->sprPaletteAddress();
        }
        
        // 名称表地址
        const uint8_t* _nameTableAddress(int index) const
        {
            return _vram->nameTableAddress(index);
        }
        
        // 属性表地址
        const uint8_t* _attributeTableAddress(int index) const
        {
            return _vram->attributeTableAddress(index);
        }
        
        // 从第nameTableIndex个名称表开始绘制（竖直镜像）
        void drawBackground(uint8_t* buffer, int stride, int tile_x_start, int tile_y_start, int tile_x_count, int tile_y_count, int tableIndex, const uint8_t* bkPetternTableAddr, const uint8_t* bkPaletteAddr){
            
            // 背景的tile偏移（图案表中的tile偏移）
            int bg_offset_x = 0;
            int bg_offset_y = 0;
            
            int tile_x_max = tile_x_start + tile_x_count;
            int tile_y_max = tile_y_start + tile_y_count;
            
            for (int bg_y=tile_y_start; bg_y<tile_y_max; bg_y++) // 只显示 30/30 行tile
            {
                int s_y = bg_y+bg_offset_y;
                int tile_y = s_y % 30; // 竖直第_y个tile，30个tile 垂直循环
                
                for (int bg_x=tile_x_start; bg_x<tile_x_max; bg_x++)
                {
                    int s_x = (bg_x+bg_offset_x);
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
                    
                    drawTile(buffer, stride, bg_x*8, bg_y*8, high2, tileAddr, bkPaletteAddr);
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
        uint8_t* _scrollBufferRGB = 0;
        
        uint8_t _sprram[256]; // 精灵内存, 64 个，每个4字节
        uint8_t _OAM[256];    // 每帧的OAM
        
        VRAM* _vram;
        Memory* _mem;
    };
}
