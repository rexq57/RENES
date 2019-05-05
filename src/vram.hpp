#pragma once

#include <map>
#include "type.hpp"


namespace ReNes {
    
    /*
     内存映射
     
     Address range    Size    Description
     $0000-$0FFF    $1000    Pattern table 0
     $1000-$1FFF    $1000    Pattern table 1
     $2000-$23FF    $0400    Nametable 0
     $2400-$27FF    $0400    Nametable 1
     $2800-$2BFF    $0400    Nametable 2
     $2C00-$2FFF    $0400    Nametable 3
     $3000-$3EFF    $0F00    Mirrors of $2000-$2EFF
     $3F00-$3F1F    $0020    Palette RAM indexes
     $3F20-$3FFF    $00E0    Mirrors of $3F00-$3F1F
     
     为了模拟镜像，需要封装访问接口，指向同一个内存。并且在读写函数内，使用镜像地址跳转。
     */
    
    struct VRAM{
        
        VRAM()
        {
            // 申请内存 16KB
            _data = (uint8_t*)malloc(0x4000);
            memset(_data, 0, 0x4000);
            
            // 设置镜像
            addMirroring(&_data[0x2000], 0x3000, 0x3EFF); // 名称表镜像
            addMirroring(&_data[0x3F00], 0x3F20, 0x3FFF); // 调色板镜像
            
            // 名称表垂直镜像
            addMirroring(&_data[0x2000], 0x2800, 0x2BFF);
            addMirroring(&_data[0x2400], 0x2C00, 0x2FFF);
            
            // 水平镜像
//            addMirroring(&_data[0x2000], 0x2400, 0x27FF);
//            addMirroring(&_data[0x2800], 0x2C00, 0x2FFF);
        }
        
        ~VRAM()
        {
            if (_data)
                free(_data);
        }
        
        inline
        uint8_t* masterData()
        {
            return _data;
        }
        
        // 读取数据
        inline
        uint8_t read8bitData(uint16_t addr) const
        {
            uint8_t data = *const_cast<VRAM*>(this)->_getRealAddr(addr);
            return data;
        }
        
        // 写入数据
        inline
        void write8bitData(uint16_t addr, uint8_t value)
        {
            *_getRealAddr(addr) = value;
            
            // 处理调色板第一位的镜像
//            if (access == WRITE)
            {
                // 可能需要一个物理地址监控。设置镜像写入关联 -> 物理地址监控.写入事件
                if (addr >= 0x3F00 && addr < 0x3F20)
                {
                    // 第一组调色板的0号颜色被作为整个屏幕背景的默认颜色，其他每组的第0号颜色都会跟背景颜色一样，也可以说这些颜色是透明的，所以会显示背景颜色（图中也可以见到），因此实际上调色板最多可标示颜色25种。
                    //                    if (addr != 0x3F00 || addr != 0x3F10) ?
                    
                    if (addr == 0x3F10)
                        _updateBkColor = true;
                    
                    if (_updateBkColor)
                    {
                        _data[0x3F00] = _data[0x3F10];
                    }
                    
                    // 当写入精灵调色板第一位时，设置好其他镜像
                    for (int i=0; i<8; i++)
                    {
                        if (i != 0 && i != 4)
                            _data[0x3F00 + i*4] = _data[0x3F10];
                    }
                }
            }
        }
        
        void loadPetternTable(const uint8_t* addr)
        {
            // 8KB
            memcpy(_getRealAddr(0x0000), addr, 0x2000);
        }

        bool error = false;
        
        ////////////////////////////////////////////
        
        // 图案表地址
        // 图案表：256*16(4KB)的空间 index in [2]
        const uint8_t* petternTableAddress(int index) const
        {
            RENES_ASSERT(index == 0 || index == 1);
            return const_cast<VRAM*>(this)->_getRealAddr(0x1000 * index);
//            return &_data[0x1000 * index]; // 第3bit决定精灵图案表地址 0x0000或0x1000
        }
        
        // 背景调色板地址
        const uint8_t* bkPaletteAddress() const
        {
            return const_cast<VRAM*>(this)->_getRealAddr(0x3F00);
//            return &_data[0x3F00];
        }
        
        // 精灵调色板地址
        const uint8_t* sprPaletteAddress() const
        {
            return const_cast<VRAM*>(this)->_getRealAddr(0x3F10);
//            return &_data[0x3F10];
        }
        
        // 名称表地址
        const uint8_t* nameTableAddress(int index) const
        {
            return const_cast<VRAM*>(this)->_getRealAddr(0x2000 + index*0x0400);
//            return &_data[0x2000 + index*0x0400];
        }
        
        // 属性表地址
        const uint8_t* attributeTableAddress(int index) const
        {
            return const_cast<VRAM*>(this)->_getRealAddr(0x23C0 + index*0x0400);
//            return &_data[0x23C0 + index*0x0400];
        }
        
        // 得到名称表镜像index，支持检测互为镜像index
        const int nameTableMirroring(int index) const
        {
            // 通过判断内存地址来比较是否是镜像
            const uint8_t* addr = nameTableAddress(index);
            int i = index;
            do
            {
                i = (i + 1) % 4;
                if (nameTableAddress(i) == addr)
                {
                    return i;
                }
            }while (i != index);
            return index;
        }
        
    private:
        
        struct Mirroring {
            
            uint8_t  *data;
            uint16_t start;
            uint16_t end;
            
            Mirroring(uint8_t *data, uint16_t start, uint16_t end)
            {
                this->data = data;
                this->start = start;
                this->end = end;
            }
            
            inline bool hit(uint16_t addr) const
            {
                return addr >= start && addr <= end;
            }
            inline uint8_t* address(uint16_t addr)
            {
                RENES_ASSERT(hit(addr));
                return &data[addr - start];
            }
        };
        std::vector<Mirroring> _mirrorings;
        
        void addMirroring(uint8_t *data, uint16_t start, uint16_t end)
        {
            // 禁止镜像区域交叉
            for (auto& mirr : _mirrorings)
            {
                if (mirr.hit(start) || mirr.hit(end) || mirr.hit(data-_data))
                {
                    RENES_ASSERT(!"无效的镜像设置");
                    return;
                }
            }
            
            _mirrorings.push_back(Mirroring(data, start, end));
        }
        
        // 得到实际内存地址
        inline
        uint8_t* _getRealAddr(uint16_t addr)
        {
//#ifdef DEBUG
//            // 处理调色板镜像
//            uint16_t testAddr = addr;
//            if (addr >= 0x3F20 && addr <= 0x3FFF)
//            {
//                testAddr = 0x3F00 + ((addr-0x3F20) % 0x20);
//            }
//            else if (addr >= 0x3000 && addr <= 0x3EFF)
//            {
//                testAddr = 0x2000 + (addr - 0x3000);
//            }
//            else if (addr > 0x3FFF)
//            {
//                testAddr = addr % 0x4000;
////                printf("fix %x -> %x\n", addr, fixedAddr);
//            }
//#endif
            // 遍历镜像设置
            for (auto& mirr : _mirrorings)
            {
                if (mirr.hit(addr))
                {
                    uint8_t *retAddr = mirr.address(addr);
//#ifdef DEBUG
//                    // 检查默认镜像设置问题
//                    RENES_ASSERT(&_data[addr] == retAddr);
//#endif
                    return retAddr;
                }
            }

            return &_data[addr];
        }
        
        uint8_t* _data = 0;
        bool _updateBkColor = false;
    };
}
