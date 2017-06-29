//
//  MyOpenGLView.h
//  RENES
//
//  Created by Viktor Pih on 2017/6/29.
//  Copyright © 2017年 com.rexq. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface MyOpenGLView : NSOpenGLView

- (void) updateRGBData:(uint8_t*) data size:(CGSize) size;

@end
