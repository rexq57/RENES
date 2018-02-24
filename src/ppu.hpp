
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
 
 每帧需要的时间是 1.0 / 60 = 0.016666666666667s
 按照PPU是CPU时钟的3倍速度来算，一条扫描线绘制完成需要 256 / 3 = 85.333333333333333 个CPU周期，240条扫描线需要 20480 个CPU周期
 每个CPU周期是 1.0 / (1024*1000*1.789773) = 0.000000545634837s，所以一帧绘制时间是 0.011174601471807s
 
 【VBlank时间】
 图片尺寸是256x240，NTSC标称可见高度是224
 实际上，NTSC每帧尺寸是341x262 (见 https://wiki.nesdev.com/w/index.php/File:Ntsc_timing.png)
 VBlank我理解为PPU跑 [240,261] 这段扫描线的时间
 HBlank我理解为PPU跑每条扫描线的 [256,340] 点的时间
 
 【预渲染线】
 最后一条扫描线，每隔一帧短一个点，例如0帧341，1帧340，2帧341 ...
 
 【计算每帧等待时间】
 先根据帧率算出每一帧的CPU周期个数: CPUCyclesPerFrame = 1.0 / 60 / (1.0 / (1024*1000*1.789773)) = 30545.459227463146749
 当前帧花费的总时间: TimeForCurrentFrame
 当计数器达到这个值，检测这帧花费时间，当速度过快时，进行等待，usleep( 1.0 / 60 - TimeForCurrentFrame )
 
 【CPU 和 PPU 协作】
 
 do
 {
     检测中断
     cpu->checkInterrupts()
 
     执行一次指令，并得到当前帧的CPU周期计数
     cmdCycles += cpu->execCmd()
     cyclesCountForCPU += cmdCycles
     cyclesCountForPPU += cmdCycles
 
     每当CPU周期数达到满足一条扫描所需的时间（周期数），NumCyclesPerScanline = 341 / 3.0 = 113.666666666666667，则调用PPU渲染一条扫描线
     if (cyclesCountForPPU >= NumCyclesPerScanline)
     {
         cyclesCountForPPU -= NumCyclesPerScanline
         ppu->drawLine()
 
         通知是否进行画面呈现
         当这是最后一条扫描线时，再计算是否需要进行等待
         if (ppu->isLastScanline())
         {
             usleep( 1.0 / 60 - TimeForCurrentFrame )
         }
     }
 
 }while(true)
 
 
 
 
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
        
        int fps = 60; // default = 60
        
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
            
            std::function<void(uint16_t, uint8_t*, bool*)> readingObserver = [this](uint16_t addr, uint8_t* value, bool* cancel)
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
                            *cancel = true; // 取消这次操作
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
        
        void draw()
        {
            
            
            status_regs->set(7, 0); // 清除 vblank
            status_regs->set(6, 0);
            
            // 准备当前帧的OAM
            memcpy(_OAM, _sprram, 256);
            
            // clear
//            memset(_buffer, 0, RGB_BUFFER_LENGTH);
//            memset(_scrollBuffer, 0, bufferLength()*4);
            
            
            
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
            

//            uint8_t paletteBuffer[32];
//            {
//                memcpy(paletteBuffer, &VRAM[0x3F00], 32);
//                
//                // 第一组调色板的0号颜色被作为整个屏幕背景的默认颜色，其他每组的第0号颜色都会跟背景颜色一样，也可以说这些颜色是透明的，所以会显示背景颜色（图中也可以见到），因此实际上调色板最多可标示颜色25种。
//                paletteBuffer[0] = paletteBuffer[0x10];
//                
//                // 当写入精灵调色板第一位时，设置好其他镜像
//                for (int i=0; i<8; i++)
//                {
//                    if (i != 0 && i != 4)
//                        paletteBuffer[0x00 + i*4] = paletteBuffer[0x10];
//                }
//            }
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
                
                //                if (showSp)
                //                drawTile(_spr_buffer, spr->x + 1, spr->y + 1, high2, tileAddr, sprPaletteAddr, flipH, flipV, i == 0, showSp);
                
                drawSprBuffer(_spr_buffer, spr->x + 1, spr->y + 1, high2, tileAddr, sprPaletteAddr, flipH, flipV, i == 0, sprFront);
            }
            

            // 模拟扫描线
            std::function<void(uint8_t*, bool, bool)> scanLine = [this, sprPaletteAddr, bkPetternTableAddr, bkPaletteAddr](uint8_t* buffer, bool showBg, bool showSpr)
            {
                auto* VRAM = _vram.masterData();
                
//                int line_y_max = bg_t_y == 0 ? 30*8 : 31*8;
//                int line_x_max = bg_t_x == 0 ? 32*8 : 33*8;
                int line_y_max = 31*8;
                int line_x_max = 33*8;
                
                
                const auto& v = _t;
                
                
                uint8_t* tileAddr;
                int high2;

                // 获取时间点
                auto startTime=std::chrono::system_clock::now();
                
                for (int line_y=0; line_y<line_y_max; line_y++)
                {
                    
                    
                    // 从 _t 中取出tile坐标偏移
                    int bg_offset_x = v & 0x1F;         // tile整体偏移[0,31]
                    int bg_offset_y = (v >> 5) & 0x1F;
                    
                    int bg_t_x = _x;                    // tile精细偏移[0,7]
                    int bg_t_y = (v >> 12) & 0x7;
                    //            int bg_base = (v >> 10) & 0x3;
                    
                    // 需要实时获取
                    int nameTableIndex = io_regs[0].get(0) | (io_regs[0].get(1) << 1); // 前2bit决定基础名称表地址
                    
                    testLog = std::to_string(nameTableIndex) + ": " + std::to_string(bg_offset_x) + "-" + std::to_string(bg_t_x);
                    
                    
                    // 计算当前扫描线所在瓦片，瓦片偏移发生在相对当前屏幕的tile上，而不是指图案表相对屏幕左上角发生的偏移
                    int bg_y = line_y/8;
                    int s_y = bg_y+bg_offset_y;         // 实际瓦片坐标，向上[0,31]
                    int y = s_y % 30; // 30个tile 垂直循环
                    
                    int draw_line_y = line_y/8*8-bg_t_y;
                    int ty = (line_y+bg_t_y)%8;
                    
                    if (draw_line_y + ty < 0)
                        continue;
                    
                    for (int line_x=0; line_x<line_x_max; line_x++)
                    {
                        int draw_line_x = line_x/8*8-bg_t_x;
                        int tx = (line_x+bg_t_x)%8;
                        if (draw_line_x + tx < 0)
                            continue;
                        
                        if (line_x % 8 == 0) // 每次跨界才计算新的瓦片地址
                        {
                            int bg_x = line_x/8;
                            int s_x = (bg_x+bg_offset_x); // 目标tile
                            int x = s_x % 32; // 32个tile 水平循环
                            
//                            if (line_y % 8 == 0)
                            {
                                int realNameTableIndex = ((nameTableIndex + s_x/32) % 2);
                                // 未处理左右镜像，需要计算s_y
                                
                                uint8_t* nameTableAddr = &VRAM[0x2000 + realNameTableIndex*0x0400];
                                uint8_t* attributeTableAddr = &VRAM[0x23C0 + realNameTableIndex*0x0400];
                                
                                //                            NameTableInfo& info = infos[s_x];
                                //                            uint8_t* nameTableAddr = info.nameTableAddr;
                                //                            uint8_t* attributeTableAddr = info.attributeTableAddr;
                                
                                // 一个字节表示4x4的tile组，先确定当前(x,y)所在字节
                                uint8_t attributeAddrFor4x4Tile = attributeTableAddr[(y / 4 * (32/4) + x / 4)];
                                int bit = (y % 4) / 2 * 4 + (x % 4) / 2 * 2;
                                high2 = (attributeAddrFor4x4Tile >> bit) & 0x3;
                                
                                int tileIndex = nameTableAddr[y*32 + x]; // 得到 bkg tile index
                                
                                // 确定在图案表里的tile地址，每个tile是8x8像素，占用16字节。
                                // 前8字节(8x8) + 后8字节(8x8)
                                tileAddr = &bkPetternTableAddr[tileIndex * 16];
                                
                                // 记录当前tile信息
//                                infos[bg_x] = {tileAddr, high2};
                            }
//                            else
//                            {
//                                tileAddr = infos[bg_x].tileAddr;
//                                high2 = infos[bg_x].high2;
//                            }
                        }
                        
                        // line_x/8*8 为当前tile的标准坐标，bg_t_x 为偏移位置

                        {
                            uint8_t* paletteAddr = bkPaletteAddr;
                            bool flipV = false;
                            bool flipH = false;
                            
                            {
                                int tx_ = flipH ? tx : 7-tx;
                                int ty_ = flipV ? 7-ty : ty;
                                
                                int paletteUnitIndex;
                                {
                                    // tile 第一字节与第八字节对应的bit位，组成这一像素颜色的低2位（最后构成一个[0,63]的数，索引到系统默认的64种颜色）
                                    int low0 = ((bit8*)&tileAddr[ty_])->get(tx_);
                                    int low1 = ((bit8*)&tileAddr[ty_+8])->get(tx_);
                                    int low2 = low0 | (low1 << 1);
                                    
                                    paletteUnitIndex = (high2 << 2) + low2; // 背景调色板单元索引号
                                }

                                int bg_systemPaletteUnitIndex = paletteAddr[paletteUnitIndex]; // 系统默认调色板颜色索引 [0,63]
                                
                                int pixelIndex = (NES_MIN(draw_line_y + ty, 239) * 32*8 + NES_MIN(draw_line_x+tx, 255)); // 最低位是右边第一像素，所以渲染顺序要从右往左
                                
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
                                        if (showBg && !sprFront && paletteUnitIndex != 0)
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
                    
                    // 模拟60Hz扫描线，每帧1/60秒，每条扫描线 1/60/240 秒 的延迟
                    if (line_y<line_y_max-1)
                    {
                        const long p_t = 1.0 / fps / 240 * 1e9; // 纳秒
//                        printf("fuck %f", p_t);
                        
                        auto t1=std::chrono::system_clock::now();
                        auto d=std::chrono::duration_cast<std::chrono::nanoseconds>(t1-startTime); // 实际花费时间，纳秒
                        long d_t = d.count();
                        
                        long dd = p_t - d_t; // 纳秒差
                        
                        if (dd > 0)
                        {
//                            printf("sleep %f-%f = %f\n", p_t, d_t, dd * 1000000);
                            usleep((uint32_t)dd / 1000);
                            
                            startTime = std::chrono::system_clock::now();
                        }
                        else
                        {
                            startTime = t1; // 记录当前时间
                        }
                    }
                }

            };
            
//            _timer.timerStart();
            // 绘制扫描线
            scanLine(_buffer, showBg, showSpr);
            
//            _timer.timerEnd("draw");
            
            // 绘制完成，等于发生了 VBlank，需要设置 2002 第7位
            status_regs->set(7, 1);
            
            
        }
        
        void dumpScrollToBuffer()
        {
            auto* VRAM = _vram.masterData();
            
            uint8_t* bkPetternTableAddr = &VRAM[0x1000 * io_regs[0].get(4)]; // 第4bit决定背景图案表地址 0x0000或0x1000
            uint8_t* bkPaletteAddr = &VRAM[0x3F00];   // 背景调色板地址
            
            for (int i=0; i<4; i++)
            {
                drawBackground(_scrollBufferTmp, i, bkPetternTableAddr, bkPaletteAddr);
                
                //                    if (i<2) for test
                {
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
        }
        
        int width() const
        {
            return BUFFER_PIXEL_WIDTH;
        }
        
        int height() const
        {
            return BUFFER_PIXEL_HEIGHT;
        }
        
        int bpp() const
        {
            return BUFFER_PIXEL_BPP;
        }
        
//        size_t bufferLength() const
//        {
//            return width()*height() * bpp();
//        }
        
        uint8_t* buffer() const
        {
            return _buffer;
        }
        
        uint8_t* scrollBuffer() const
        {
//            assert(dumpScrollBuffer);
            
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
        
        void drawBackground(uint8_t* buffer, int nameTableIndex, uint8_t* bkPetternTableAddr, uint8_t* bkPaletteAddr){
            
            int bg_offset_x = 0;
            int bg_offset_y = 0;
            int bg_t_x = 0;
            int bg_t_y = 0;
            
            int tiles_y = bg_t_y == 0 ? 30 : 31;
            int tiles_x = bg_t_x == 0 ? 32 : 33;
            
            auto* VRAM = _vram.masterData();
            
            //                uint8_t* nameTableAddr = &_vram.masterData()[0x2000 + nameTableIndex*0x0400];
            //                uint8_t* attributeTableAddr = &_vram.masterData()[0x23C0 + nameTableIndex*0x0400];
            
            for (int bg_y=0; bg_y<tiles_y; bg_y++) // 只显示 30/30 行tile
            {
                int s_y = bg_y+bg_offset_y;
                int y = s_y % 30; // 30个tile 垂直循环
                
                
                for (int bg_x=0; bg_x<tiles_x; bg_x++)
                {
                    int s_x = (bg_x+bg_offset_x);
                    int x = s_x % 32; // 32个tile 水平循环
                    
                    int realNameTableIndex = ((nameTableIndex + s_x/32) % 2);
                    // 未处理左右镜像，需要计算s_y
                    
                    uint8_t* nameTableAddr = &VRAM[0x2000 + realNameTableIndex*0x0400];
                    uint8_t* attributeTableAddr = &VRAM[0x23C0 + realNameTableIndex*0x0400];
                    
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
        

        
        // 建立 w * h * 8 的缓冲区，每像素8字节，用来存储8个精灵的数据。
        // 如果该像素没有精灵，甚至没有叠加，则该位第一字节为0
        const int SPR_BUFFER_LENGTH = BUFFER_PIXEL_CONUT*8;
        
        uint8_t* _spr_buffer = 0;   // 精灵绘制缓冲区
        uint8_t* _buffer = 0;       // 显示缓冲区
        uint8_t* _scrollBuffer = 0; // 卷轴缓冲区
        uint8_t* _scrollBufferTmp = 0;
        
//        uint8_t* test7774;
        
        uint8_t _sprram[256]; // 精灵内存, 64 个，每个4字节
        uint8_t _OAM[256];    // 每帧的OAM
        
        VRAM _vram;
        Memory* _mem;
        
        Timer _timer;
    };
}
