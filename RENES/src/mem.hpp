
#pragma once


namespace ReNes {

    struct Memory{

        Memory(size_t bytes)
        {
            // 申请 64kb 内存
            _data = (uint8_t*)malloc(bytes);
            memset(_data, 0, bytes);
        }
        
        ~Memory()
        {
            if (_data)
                free(_data);
        }
        
        // 得到实际内存地址
        uint8_t* getAddr(int offset)
        {
            return _data + offset;
        }
        
        // 读取数据
        uint8_t read8bitData(uint16_t addr) const
        {
            return *(_data + addr);
        }
        
        uint16_t read16bitData(uint16_t addr) const
        {
            return *(uint16_t*)(_data + addr);
        }
        
        // 写入数据
        void writeData(uint16_t addr, const uint8_t* data_addr, size_t length)
        {
            memcpy(_data + addr, data_addr, length);
        }

    private:

        uint8_t* _data = 0;
    };
}
