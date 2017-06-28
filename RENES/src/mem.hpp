
#pragma once

#include <map>
#include "type.hpp"

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
        
        uint8_t* getIORegsAddr()
        {
            return &_data[0x2000];
        }
        
        uint8_t* masterData()
        {
            return _data;
        }
        
        // 读取数据
        uint8_t read8bitData(uint16_t addr)
        {
            auto data = *getRealAddr(addr, READ);
            
            // 处理2002读取，每次读取之后重置第7位
            if (addr == 0x2002)
            {
                ((bit8*)&_data[0x2002])->set(7, 0);
            }
            
            return data;
        }
        
        uint16_t read16bitData(uint16_t addr) 
        {
            return *(uint16_t*)getRealAddr(addr, READ);
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
        
        void write8bitData(uint16_t addr, int value)
        {
            *getRealAddr(addr, WRITE) = value;
        }
        
        // 添加写入监听者
        void addWritingObserver(uint16_t addr, std::function<void(uint16_t)> callback)
        {
            addrObserver[addr] = callback;
        }
        
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
            
            // 处理镜像
            if (addr >= 0x2008 && addr <= 0x3FFF)
            {
                fixedAddr = 0x2000 + (addr % 8);
            }
            
            // 2000 2001 只写
            // 2002 只读
            
            // 非法读取
            if (access == READ && (addr == 0x2000 || addr == 0x2001))
            {
                log("该内存只能写!\n");
                error = true;
                return (uint8_t*)0;
            }
            
            // 非法写入
            if (access == WRITE && (addr == 0x2002))
            {
                log("该内存只能读!\n");
                error = true;
                return (uint8_t*)0;
            }
            
            
            return _data + fixedAddr;
        }
        
        uint8_t* _data = 0;
        
        
        std::map<uint16_t, std::function<void(uint16_t)>> addrObserver;
    };
}
