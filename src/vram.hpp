#pragma once

#include <map>
#include "type.hpp"


namespace ReNes {
    
    struct VRAM{
        
        VRAM()
        {
            // 申请内存
            _data = (uint8_t*)malloc(1024*16);
            memset(_data, 0, 1024*16);
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
            uint8_t data = *const_cast<VRAM*>(this)->_getRealAddr(addr, READ);
            return data;
        }
        
        // 写入数据
        inline
        void write8bitData(uint16_t addr, uint8_t value)
        {
            *_getRealAddr(addr, WRITE) = value;
        }

        bool error = false;
        
    private:
        
        enum ACCESS{
            READ,
            WRITE
        };
        
        // 得到实际内存地址
        inline
        uint8_t* _getRealAddr(uint16_t addr, ACCESS access)
        {
            // 处理调色板镜像
            if (addr >= 0x3F20 && addr <= 0x3FFF)
            {
                addr = 0x3F00 + ((addr-0x3F20) % 0x20);
            }
            else if (addr >= 0x3000 && addr <= 0x3EFF)
            {
                addr = 0x2000 + (addr - 0x3000);
            }
            else if (addr > 0x3FFF)
            {
                addr = addr % 0x4000;
//                printf("fix %x -> %x\n", addr, fixedAddr);
            }
            
            // 处理调色板第一位的镜像
            if (access == WRITE)
            {
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

            return _data + addr;
        }
        
        uint8_t* _data = 0;
        bool _updateBkColor = false;
    };
}
