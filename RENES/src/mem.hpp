
#pragma once

#include <map>

#define SET_FIND(s, v) (s.find(v) != s.end())

namespace ReNes {

    struct Memory{

        Memory()
        {
            // 申请内存
            _data = (uint8_t*)malloc(0x10000);
            memset(_data, 0, 0x10000);
        }
        
        ~Memory()
        {
            if (_data)
                free(_data);
        }
        
        // 得到实际内存地址
        uint8_t* getRealAddr(uint16_t addr) const
        {
            uint16_t fixedAddr = addr;
            
            // 处理镜像
            if (addr >= 0x2008 && addr <= 0x3FFF)
            {
                fixedAddr = 0x2000 + (addr % 8);
            }
            
            return _data + fixedAddr;
        }
        
        // 读取数据
        uint8_t read8bitData(uint16_t addr) const
        {
            return *getRealAddr(addr);
        }
        
        uint16_t read16bitData(uint16_t addr) const
        {
            return *(uint16_t*)getRealAddr(addr);
        }
        
        // 写入数据
        void writeData(uint16_t addr, const uint8_t* data_addr, size_t length)
        {
            memcpy(_data + addr, data_addr, length);
            
            // 检查地址监听
            if (SET_FIND(addrObserver, addr))
            {
                addrObserver.at(addr)(addr);
            }
        }
        
        // 添加写入监听者
        void addWritingObserver(uint16_t addr, std::function<void(uint16_t)> callback)
        {
            addrObserver[addr] = callback;
        }

    private:
        
        

        uint8_t* _data = 0;
        
        std::map<uint16_t, std::function<void(uint16_t)>> addrObserver;
    };
}
