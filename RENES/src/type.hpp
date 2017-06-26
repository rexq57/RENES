
#include <stdio.h>
#include <functional>

#pragma once

namespace ReNes {
    
    static char g_buffer[1024];
    
    static std::function<void(const char*)> g_callback;
    
    void log(const char* format, ...)
    {
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
}
