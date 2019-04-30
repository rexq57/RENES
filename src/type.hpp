#pragma once

#include <stdio.h>
#include <functional>
#include <chrono>
#include <mutex>

namespace ReNes {
    
#ifdef DEBUG
#define RENESAssert(...) assert(__VA_ARGS__)
#else
#define RENESAssert(...)
#endif
    
    //--------------------------------------
    // log打印
    
    static char g_buffer[1024];
    
    static std::function<void(const char*)> g_callback;
    static bool g_logEnabled = false;
    
    bool _log(const char* format, ...)
    {
        if (!g_logEnabled)
            return true;
        
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
        
        return true;
    }
    
#define NES_DEBUG
    
#ifdef NES_DEBUG
    #define log(...) _log(__VA_ARGS__)
#else
   #define log(...)
#endif
    
    void setLogCallback(std::function<void(const char*)> callback)
    {
        g_callback = callback;
    }
    
    void setLogEnabled(bool enabled)
    {
        g_logEnabled = enabled;
    }
    
    //--------------------------------------
    // 数据类型
    
    struct bit8 {
        
        inline void set(uint8_t offset, int value)
        {
            RENESAssert(value == 0 || value == 1);
            _data &= ~(0x1 << offset); // set 0
            _data |= ((value % 2) << offset);
        }
        
        inline uint8_t get(uint8_t offset) const
        {
            return (_data >> offset) & 0x1;
        }
        
    private:
        uint8_t _data; // 8bit
    };
    
    //--------------------------------------
    // 内置函数
    
    template <typename T>
    inline T const& __max (T const& a, T const& b)
    {
        // if a < b then use b else use a
        return  a < b ? b : a;
    }
    
    template <typename T>
    inline T const& __min (T const& a, T const& b)
    {
        // if a < b then use b else use a
        return  a > b ? b : a;
    }
    
    template <typename T>
    inline T const& __clamp (T const& x, T const& l, T const& u)
    {
        // if a < b then use b else use a
        return (x)<(l)?(l):((x)>(u)?(u):(x));
    }
    
#ifndef NES_MAX
#define NES_MAX(a, b) __max(a, b)
#endif
    
#ifndef NES_MIN
#define NES_MIN(a, b) __min(a, b)
#endif
    
#ifndef NES_CLAMP
#define NES_CLAMP(x,l,u) __clamp(x,l,u)
#endif
    
    template <typename T, typename V>
    bool array_find(const T arr, size_t count, const V& val)
    {
        for (int i=0; i<count; i++)
        {
            if (arr[i] == val)
                return true;
        }
        return false;
    }
    
#define ARRAY_FIND(s, v) array_find(s, sizeof(s)/sizeof(*s), v)
//#define VECTOR_FIND(s, v) (std::find(s.begin(), s.end(), v) != s.end())
#define SET_FIND(s, v) (s.find(v) != s.end())
    
    //--------------------------------------
    // 计时器
    
    // 日志打印
    #ifdef __ANDROID__
    #   include <android/log.h>
    #   define  LOG_TAG    "RENES"
    #   define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
    #   define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
    #else
    #   define  LOG_TAG    "RENES"
    #   define LOGD(...) printf("[%s] %s: ", LOG_TAG, __FUNCTION__);printf(__VA_ARGS__)
    #   define LOGE(...) printf("[%s] %s: ", LOG_TAG, __FUNCTION__);printf(__VA_ARGS__)
    #endif
    
    class Timer {
        
    public:
        
        Timer()
        {
            _m = new std::mutex();
            mLog = (char*)malloc(512);
        }

        ~Timer()
        {
            delete _m;
            free(mLog);
        }
        
        Timer& start()
        {
            _m->lock();
            mCurrentTimeMillis = std::chrono::steady_clock::now();
            _m->unlock();
            return *this;
        }
        
        Timer& stop(const char* log="")
        {
            return stop(log, false);
        }
        
        Timer& stop(const char* log, bool display)
        {
            _m->lock();
            
            long dif_ns = (std::chrono::steady_clock::now() - mCurrentTimeMillis).count(); // 纳秒
            _dif = dif_ns;
            
            float time = dif_ns / 1.e9;
            mFpsCount ++;
            mFpsTime += time;
            float fps = mFpsCount/mFpsTime;
            if (mFpsCount > 1000)
            {
                mFpsCount = 0;
                mFpsTime = 0;
            }
            
            sprintf(mLog, "%s %f fps %fs %fs\n", log , fps , 1.0f/fps , time);
            if (display)
            {
                LOGD("%s", mLog);
            }
            
            _m->unlock();
            
            return *this;
        }
        
        void reset()
        {
            _m->lock();
            mFpsTime = 0;
            mFpsCount = 0;
            _m->unlock();
        }
        
        const char* log() const
        {
            return mLog;
        }
        
        float fps()
        {
            return mFpsCount/mFpsTime;
        }
        
        // 返回秒差
        long dif() const
        {
            return _dif;
        }
        
    private:
        
        std::mutex* _m;
        
//        char mLog[512];
        char* mLog = 0;
        float mFpsTime = 0;
        long mFpsCount = 0;
        long _dif = 0;
        
        std::chrono::steady_clock::time_point mCurrentTimeMillis;
    };
}
