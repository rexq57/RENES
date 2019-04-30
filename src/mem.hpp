#pragma once

#include <map>
#include "type.hpp"
#include <vector>


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
        
        inline
        uint8_t* getIORegsAddr()
        {
            return &_data[0x2000];
        }
        
        inline
        uint8_t* masterData()
        {
            return _data;
        }
        
        // 读取数据
        inline
        uint8_t read8bitData(uint16_t addr, bool event=false)
        {
            auto data = *_getRealAddr(addr, READ);
            
            // 处理2005,2006读取，每次读取之后重置bit7 ?
//            switch (addr)
//            {
//                case 0x2005:
//                case 0x2006:
//                    ((bit8*)&_data[addr])->set(7, 0);
//            }
            
            if (event)
            {
                // 检查地址监听
                if (SET_FIND(addr8bitReadingObserver, addr))
                {
                    addr8bitReadingObserver.at(addr)(addr, &data);
                }
            }

            return data;
        }
        
        // 16bit读取访问属于直接访问，没有事件处理
        inline
        uint16_t read16bitData(uint16_t addr) const
        {
            uint16_t data = *(uint16_t*)const_cast<Memory*>(this)->_getRealAddr(addr, READ);
            return data;
        }
        
        inline
        void write8bitData(uint16_t addr, uint8_t value)
        {
            *_getRealAddr(addr, WRITE) = value;
            
//            for test
//            if ((addr == 0x0100 + 0xfe || addr == 0x0100 + 0xff))
//            {
//                log("fuck\n");
//            }
            
            // 检查地址监听
            if (SET_FIND(addrWritingObserver, addr))
            {
                addrWritingObserver.at(addr)(addr, value);
            }
        }
        
        // 添加写入监听者
        void addWritingObserver(uint16_t addr, std::function<void(uint16_t, uint8_t)> callback)
        {
            addrWritingObserver[addr] = callback;
        }
        
        // 添加读取监听者
        void addReadingObserver(uint16_t addr, std::function<void(uint16_t, uint8_t*)> callback)
        {
            addr8bitReadingObserver[addr] = callback;
        }
        
        bool error = false;

    private:
        
        enum ACCESS{
            READ,
            WRITE,
            MASTER
        };

        // 得到实际内存地址
        inline
        uint8_t* _getRealAddr(uint16_t addr, ACCESS access)
        {
            // 对访问地址做镜像修正
            if (addr >= 0x2008 && addr <= 0x3FFF)       // 处理I/O寄存器镜像
            {
                addr = 0x2000 + (addr % 8);
            }
            else if (addr >= 0x0800 && addr <= 0x1FFF)  // 2KB internal RAM 镜像
            {
                addr = addr % 0x0800;
            }
            
            // 只在debug模式下检查内存错误，以提高release速度
#ifdef DEBUG
            if (access == READ) // 检查非法读取
            {
                // 该内存只写
                const static uint16_t MEMORY_WRITE_ONLY[] = {
                    0x2000, 0x2001, 0x2003, 0x2005, 0x2006, 0x4014
                };
                
                if (ARRAY_FIND(MEMORY_WRITE_ONLY, addr))
                {
                    log("该内存只能写!\n");
                    error = true;
                    return (uint8_t*)0;
                }
            }
            else if (access == WRITE) // 检查非法写入
            {
                // 该内存只读
                const static uint16_t MEMORY_READ_ONLY[] = {
                    0x2002
                };
                
                if (ARRAY_FIND(MEMORY_READ_ONLY, addr))
                {
                    log("该内存只能读!\n");
                    error = true;
                    return (uint8_t*)0;
                }
            }
#endif

            return _data + addr;
        }
        
        
        uint8_t* _data = 0;
        
        
        std::map<uint16_t, std::function<void(uint16_t, uint8_t*)>> addr8bitReadingObserver;
        std::map<uint16_t, std::function<void(uint16_t, uint8_t)>> addrWritingObserver;
    };
}
