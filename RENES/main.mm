//
//  main.m
//  RENES
//
//  Created by rexq57 on 2017/6/25.
//  Copyright © 2017年 com.rexq57. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#include <vector>
#include <map>
#include <chrono>

size_t highBit(uint8_t a) {
    size_t bits=0;
    while (a!=0) {
        ++bits;
        a>>=1;
    };
    return bits;
}

const std::vector<uint16_t> ARRAY = {
    0x2000, 0x2001, 0x2003, 0x2005, 0x2006, 0x4014
};

const uint16_t ARRAY2[] = {
    0x2000, 0x2001, 0x2003, 0x2005, 0x2006, 0x4014
};

template <typename T, typename V>
bool find(const T arr, size_t count, const V& val)
{
    for (int i=0; i<count; i++)
    {
        if (arr[i] == val)
            return true;
    }
    return false;
}



#define RENES_ARRAY_FIND(s, v) find(s, sizeof(s)/sizeof(*s), v)
#define VECTOR_FIND(s, v) (std::find(s.begin(), s.end(), v) != s.end())

#define TIME_START auto t0=std::chrono::system_clock::now();
#define TIME_END auto t1=std::chrono::system_clock::now();auto d=std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0);double d_t = d.count() * ns; printf("%f\n", d_t);

int main(int argc, const char * argv[]) {
    
//    printf("%x\n", 0b01);
    
    const static double ns = 1.0 / pow(10, 9); // 0.000000001
    
    {
        TIME_START
        
        for (int i=0; i<10000; i++)
        {
            if (VECTOR_FIND(ARRAY, 0x4014))
            {
                
            }
        }
        
        TIME_END
    }
    
    
    {
        TIME_START
        
        for (int i=0; i<10000; i++)
        {
            if (RENES_ARRAY_FIND(ARRAY2, 0x4014))
            {
                
            }
        }
        
        TIME_END
    }
    
    {
        std::map<uint16_t, std::function<void()>> addr8bitReadingObserver;
        
        addr8bitReadingObserver[0x2004] = [](){
            
        };
        
        TIME_START
        
        for (int i=0; i<10000; i++)
        {
            addr8bitReadingObserver.at(0x2004)();
        }
        
        TIME_END
    }
    
    {
        std::function<void()> func = [](){
            
        };
        
        std::function<void()> addr8bitReadingObserver[] = {
            func
        };
        
        
        
        TIME_START
        
        for (int i=0; i<10000; i++)
        {
            addr8bitReadingObserver[0]();
        }
        
        TIME_END
    }
    
    {
        TIME_START
        
        std::vector<int> arr;
        for (int i=0; i<10000; i++)
        {
            arr.push_back(i);
        }
        
        TIME_END
    }
    
    {
        TIME_START
        
        std::vector<int> arr(10000);
        for (int i=0; i<10000; i++)
        {
            arr[i] = i;
        }
        
        TIME_END
    }

    // 测试shader
//    CGPoint size = {480, 640};
//
//    for (double y=0.0; y<=0.25; y+=0.01)
//    {
//        for (double x=0.0; x<=1.0; x+=0.01)
//        {
//            CGPoint pos = {x, y};
//            
//            double index = floor(pos.x * 4.0 * size.x /* 一行的个数 */
//                                + (pos.y * 4.0 * size.y) * size.x);
//            printf(" %.2f(%.2f,%.2f)", index, x, y);
////            float index = pos.x + ;
//            
////            printf("%.2f, %.2f", pos.x, (pos.y * 4.0));
//        }
//        printf("\n");
//    }
    
    
    
    
    
    

    return NSApplicationMain(argc, argv);
}
