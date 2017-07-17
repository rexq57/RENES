
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
        
        uint8_t* masterData()
        {
            return _data;
        }
        
        // 读取数据
        uint8_t read8bitData(uint16_t addr)
        {
            auto data = *getRealAddr(addr, READ);

            return data;
        }
        
//        uint16_t read16bitData(uint16_t addr)
//        {
//            uint16_t data = *(uint16_t*)getRealAddr(addr, READ);
//            return data;
//        }
        
        void write8bitData(uint16_t addr, uint8_t value)
        {
            *getRealAddr(addr, WRITE) = value;
            
            // 检查地址监听
//            if (SET_FIND(addrObserver, addr))
//            {
//                addrObserver.at(addr)(addr, value);
//            }
        }
        
        // 添加写入监听者
//        void addWritingObserver(uint16_t addr, std::function<void(uint16_t, uint8_t)> callback)
//        {
//            addrObserver[addr] = callback;
//        }
        
        bool error = false;
        
    private:
        
        enum ACCESS{
            READ,
            WRITE
        };
        
        // 得到实际内存地址
        uint8_t* getRealAddr(uint16_t addr, ACCESS access)
        {
            uint16_t fixedAddr = addr;
            
            // 处理调色板镜像
            if (addr >= 0x3F20 && addr <= 0x3FFF)
            {
                fixedAddr = 0x3F00 + (addr % 0x20);
            }

            return _data + fixedAddr;
        }
        
        uint8_t* _data = 0;
        
        
//        std::map<uint16_t, std::function<void(uint16_t, uint8_t)>> addrObserver;
    };
}
