//
//  main.m
//  RENES
//
//  Created by rexq57 on 2017/6/25.
//  Copyright © 2017年 com.rexq57. All rights reserved.
//

#import <Cocoa/Cocoa.h>

size_t highBit(uint8_t a) {
    size_t bits=0;
    while (a!=0) {
        ++bits;
        a>>=1;
    };
    return bits;
}




int main(int argc, const char * argv[]) {
    

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
