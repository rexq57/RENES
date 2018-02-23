//
//  DisplayView.h
//  RENES
//
//  Created by rexq57 on 2017/9/21.
//  Copyright © 2017年 com.rexq. All rights reserved.
//

#import <GPUImage/GPUImage.h>

@interface DisplayView : GPUImageView

- (void) updateRGBData:(uint8_t*) data size:(CGSize) size;

@end

