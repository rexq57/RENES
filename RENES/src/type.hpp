
#include <stdio.h>
#include <functional>

#pragma once

namespace ReNes {
    
    static char g_buffer[1024];
    
    static std::function<void(const char*)> g_callback;
    static bool g_logEnabled = false;
    
    void log(const char* format, ...)
    {
        if (!g_logEnabled)
            return;
        
        va_list args;
        va_start(args, format);
        vsprintf(g_buffer, format, args);
        va_end(args);
        
        if (g_callback != 0)
        {
            g_callback(g_buffer);
        }
        else
        {
            printf("%s", g_buffer);
        }
    }
    
    
    void setLogCallback(std::function<void(const char*)> callback)
    {
        g_callback = callback;
    }
    
    void setLogEnabled(bool enabled)
    {
        g_logEnabled = enabled;
    }
    
    struct bit8 {
        
        void set(uint8_t offset, int value)
        {
            assert(value == 0 || value == 1);
            
            _data &= ~(0x1 << offset); // set 0
            _data |= ((value % 2) << offset);
        }
        
        uint8_t get(uint8_t offset) const
        {
            return (_data >> offset) & 0x1;
        }
        
    private:
        uint8_t _data; // 8bit
    };
}
