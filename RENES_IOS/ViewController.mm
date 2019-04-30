//
//  ViewController.m
//  RENES_IOS
//
//  Created by rexq57 on 2017/9/20.
//  Copyright © 2017年 com.rexq. All rights reserved.
//

#import "ViewController.h"
#import <GPUImage/GPUImage.h>
#import <Masonry/Masonry.h>
#import "DisplayView.h"

#include <src/renes.hpp>

using namespace ReNes;

@interface ViewController ()
{
    DisplayView* _displayView;
    
    Nes* _nes;
}

@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do any additional setup after loading the view, typically from a nib.
    
    _displayView = [DisplayView new];
    [self.view addSubview:_displayView];
    {
        [_displayView mas_updateConstraints:^(MASConstraintMaker *make) {
            make.left.equalTo(self.view.mas_left);
            make.right.equalTo(self.view.mas_right);
            make.top.equalTo(self.view.mas_top);
            make.bottom.equalTo(self.view.mas_bottom);
        }];
        
//        _displayView.backgroundColor = [UIColor redColor];
    }
    
    NSString* path = [[NSBundle mainBundle] pathForResource:@"Roms/超级玛莉.NES" ofType:@""];
    [self startNes:path];
}

- (void) viewDidAppear:(BOOL)animated
{
    [super viewDidAppear:animated];
    
//    uint8_t buffer[256*240*3];
//    memset(buffer, 0, 256*240*3);
//    for (int i=0; i<256*240; i++) {
//        buffer[i*3] = 0;
//        buffer[i*3+1] = 255;
//        buffer[i*3+2] = 0;
//    }
//    
//    [_displayView updateRGBData:buffer size:CGSizeMake(256, 240)];
}

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

// 隐藏状态栏
- (BOOL) prefersStatusBarHidden
{
    if (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPhone)
        return YES;
    
    return NO;
}

- (void) startNes:(NSString*) filePath
{
    // 停止，才重新启动
    if (_nes)
    {
        _nes->stop();
//        _keepNext = false; // 拦截在第一次cmd之后
//        dispatch_semaphore_signal(_nextSem); // 不拦截下一句指令，顺利让cpu进入stop判断
    }
    
    // 异步
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        
        @autoreleasepool {
            
            NSData* data = [NSData dataWithContentsOfFile:filePath];
            
            if (_nes != 0)
            {
                delete _nes;
            }
            _nes = new Nes();
            
            _nes->willRunning = [self](){
                
                // 反汇编
//                [self disassembly];
            };
            
            _nes->cpu_callback = [self](CPU* cpu){
                
                // 手动中断
                //            @synchronized ((__bridge id)_nes)
                {
//                    if (cpu == _nes->cpu())
//                    {
//                        if (!_keepNext)
//                        {
//                            dispatch_semaphore_wait(_nextSem, DISPATCH_TIME_FOREVER);
//                        }
//                        else if ((cpu->regs.PC == _stopedCmdAddr || cpu->execCmdLine == _stopedCmdLine))
//                        {
//                            _nes->setDebug(true);
//                            log("自定义地址中断\n");
//                            dispatch_semaphore_wait(_nextSem, DISPATCH_TIME_FOREVER);
//                        }
//                    }
                }
                
                
                return true;
            };
            
            _nes->ppu_displayCallback = [self](PPU* ppu){
                
                //            @synchronized ((__bridge id)_nes)
                {
                    if (ppu == _nes->ppu())
                    {
                        {
                            // 显示图片
                            int width  = ppu->width();
                            int height = ppu->height();
                            uint8_t* srcBuffer = ppu->buffer();
                            
                            [_displayView updateRGBData:srcBuffer size:CGSizeMake(width, height)];
                            
                        }
                        
                        {
                            // 显示图片
//                            int width  = _nes->ppu()->width()*2;
//                            int height = _nes->ppu()->height()*2;
//                            uint8_t* srcBuffer = _nes->ppu()->scrollBuffer();
//                            
//                            [_scrollMirroringView updateRGBData:srcBuffer size:CGSizeMake(width, height)];
                            
                        }
                    }
                }
                
                return true;
            };
            
            //        _nes->mem()->addWritingObserver(0x2000, [self](uint16_t addr, uint8_t value){
            //
            //            if (addr == 0x2000 && (*(bit8*)&value).get(7) == 1)
            //            {
            //                _keepNext = false;
            //                log("fuck\n");
            //            }
            //        });
            
            _nes->setDebug(false);
            _nes->loadRom((const uint8_t*)[data bytes], [data length]);
            
            _nes->run();
            
//            _nes->dumpScrollBuffer = [_memTabView.selectedTabViewItem.identifier isEqualToString:@"scroll"];
            
            NSLog(@"模拟器开始运行");
            
        }
    });
}


@end
