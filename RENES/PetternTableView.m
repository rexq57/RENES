//
//  PetternTableView.m
//  RENES
//
//  Created by Viktor Pih on 2017/7/13.
//  Copyright © 2017年 com.rexq. All rights reserved.
//

#import "PetternTableView.h"

@implementation PetternTableView

- (void)drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];
    
    // Drawing code here.
    CGContextRef ctx = [[NSGraphicsContext currentContext] graphicsPort];
    
    int size = 20;
    for (int i=0; i<[_colors count]; i++)
    {
        NSColor* color = _colors[i];
        
        CGContextAddRect(ctx, CGRectMake((i%16)*size, i / 16 * size, size, size));
        [color set];
        //    CGContextStrokePath(ctx); // 空心
        CGContextFillPath(ctx);
    }
}

- (void) setColors:(NSArray<NSColor *> *)colors
{
    _colors = colors;
    
    dispatch_async(dispatch_get_main_queue(), ^{
        
        @autoreleasepool {
            
            [self display];
        }
    });
}

- (BOOL) isFlipped
{
    return YES;
}

@end
