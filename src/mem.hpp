#pragma once

#include <map>
#include "type.hpp"
#include <vector>

// RAM内存结构的偏移地址
#define PRG_ROM_LOWER_BANK_OFFSET 0x8000
// #define PRG_ROM_UPPER_BANK_OFFSET 0xC000 (0x8000 + 16kB)

namespace ReNes {
    
    struct Memory{
        
        const static int DEFUALT_SIZE = 0x10000;

        Memory()
        {
            // 申请内存
            _data = (uint8_t*)malloc(DEFUALT_SIZE);
            memset(_data, 0, DEFUALT_SIZE);
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
        
        void loadPRGRom(const uint8_t** romAddrs, int index)
        {
            const int bankSize = 1024*16;
            
            int offset = bankSize*index;
            memcpy(_data + PRG_ROM_LOWER_BANK_OFFSET + offset, romAddrs[index], bankSize);
        }
        
        // 直接获取8bit数据，不走读写监听
        inline
        uint8_t get8bitData(uint16_t addr) const
        {
            auto data = *const_cast<Memory*>(this)->_getRealAddr(addr);
            return data;
        }
        
        // 直接获取16bit数据，不走读写监听
        inline
        uint16_t get16bitData(uint16_t addr) const
        {
            uint16_t data = *(uint16_t*)const_cast<Memory*>(this)->_getRealAddr(addr);
            return data;
        }
        
        // 读取数据
        inline
        uint8_t read8bitData(uint16_t addr, bool* valid=0)
        {
            // 只在debug模式下检查内存错误，以提高release速度
#ifdef DEBUG
            // 检查非法读取
            {
                // 该内存只写
                const static uint16_t MEMORY_WRITE_ONLY[] = {
                    0x2000, 0x2001, 0x2003, 0x2005, 0x2006, 0x4014
                };
                
                if (RENES_ARRAY_FIND(MEMORY_WRITE_ONLY, addr))
                {
                    log("该内存只能写!\n");
                    error = true;
                    if (valid)
                        *valid = false;
                    return (uint8_t)0;
                }
            }
#endif
            
            auto data = *_getRealAddr(addr);
            bool tmpValid = true;

            // 检查地址监听
            if (RENES_SET_FIND(addr8bitReadingObserver, addr))
            {
                addr8bitReadingObserver.at(addr)(addr, &data, &tmpValid);
            }
            
            if (valid) *valid = tmpValid;

            return data;
        }
        
        inline
        void write8bitData(uint16_t addr, uint8_t value)
        {
            // 只在debug模式下检查内存错误，以提高release速度
#ifdef DEBUG
            // 检查非法写入
            {
                // 该内存只读
                const static uint16_t MEMORY_READ_ONLY[] = {
                    0x2002
                };
                
                if (RENES_ARRAY_FIND(MEMORY_READ_ONLY, addr))
                {
                    log("该内存只能读!\n");
                    error = true;
                    return;
                }
            }
#endif
            
            *_getRealAddr(addr) = value;
            
//            for test
//            if ((addr == 0x0100 + 0xfe || addr == 0x0100 + 0xff))
//            {
//                log("fuck\n");
//            }
            
            // 检查地址监听
            if (RENES_SET_FIND(addrWritingObserver, addr))
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
        void addReadingObserver(uint16_t addr, std::function<void(uint16_t, uint8_t*, bool*)> callback)
        {
            addr8bitReadingObserver[addr] = callback;
        }
        
        bool error = false;

    private:

        // 得到实际内存地址
        inline
        uint8_t* _getRealAddr(uint16_t addr)
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

//            return _data + addr;
            return &_data[addr % DEFUALT_SIZE];
        }
        
        
        uint8_t* _data = 0;
        
        
        std::map<uint16_t, std::function<void(uint16_t, uint8_t*, bool*)>> addr8bitReadingObserver;
        std::map<uint16_t, std::function<void(uint16_t, uint8_t)>> addrWritingObserver;
    };
}
