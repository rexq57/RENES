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
    
    NSLog(@"%d", 1 & (-1));

    return NSApplicationMain(argc, argv);
}
