
#include "mem.hpp"

namespace ReNes {

    // 2C02
    class PPU {
        
    public:
        
        PPU():_vram(1024*16)
        {
            
        }
        
    private:
        
        Memory _vram; // 16kb
    };
}
