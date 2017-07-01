
#include "mem.hpp"

namespace ReNes {

    class Control {
        
    public:
        
        enum KEY{
            KEY_A,
            KEY_B,
            KEY_SELECT,
            KEY_START,
            
            KEY_UP,
            KEY_DOWN,
            KEY_LEFT,
            KEY_RIGHT,
            
            
            
            __KEY_MAX
        };
        
        Control() {
            
            memset(_statues, 0, __KEY_MAX);
        }
        
        void init(Memory* mem)
        {
            _mem = mem;
        }
        
        void up(bool pressDown)
        {
            _statues[KEY_UP] = pressDown;
        }
        
        void down(bool pressDown)
        {
            _statues[KEY_DOWN] = pressDown;
        }
        
        void left(bool pressDown)
        {
            _statues[KEY_LEFT] = pressDown;
        }
        
        void right(bool pressDown)
        {
            _statues[KEY_RIGHT] = pressDown;
        }
        
        void setNextKey(KEY key)
        {
            _nextKey = key;
        }
        
        int nextKeyStatue() 
        {
            int statue = _statues[_nextKey];
            
            _nextKey++;
            _nextKey %= __KEY_MAX;
            
            return statue;
        }
        
        void reset()
        {
            setNextKey(KEY_A);
        }
        
    private:
        
        Memory* _mem = 0;
        
        int _nextKey = KEY_A;
        bool _statues[__KEY_MAX];
    };
}
