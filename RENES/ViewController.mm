//
//  ViewController.m
//  RENES
//
//  Created by rexq57 on 2017/6/25.
//  Copyright © 2017年 com.rexq57. All rights reserved.
//

#import "ViewController.h"
#include "src/renes.hpp"
#include <string>
#import "MyOpenGLView.h"
#import <BlocksKit/BlocksKit.h>

@interface ViewController()
{
    ReNes::Nes _nes;
    
    dispatch_semaphore_t _nextSem;
    
    BOOL _keepNext;
    
    std::string _logStringCache;
}

@property (nonatomic) IBOutlet MyOpenGLView* displayView;
@property (nonatomic) IBOutlet NSTabView* memTabView;
@property (nonatomic) IBOutlet NSTextView* memView;
@property (nonatomic) IBOutlet NSTextView* vramView;


@property (nonatomic) IBOutlet NSTextView* logView;
@property (nonatomic) IBOutlet NSTextField* registersView;

@end

@implementation ViewController

- (void) log:(NSString*) buffer
{
    dispatch_async(dispatch_get_main_queue(), ^{
        
//        std::string str = std::string([_logView.string UTF8String]) + std::string([buffer UTF8String]);
        
//        _logView.string = [NSString stringWithUTF8String:str.c_str()];
        [[_logView textStorage] appendAttributedString:[[NSAttributedString alloc] initWithString:buffer]];
        
        [_logView scrollRangeToVisible: NSMakeRange(_logView.string.length, 0)];
    });
}

- (void)viewDidLoad {
    [super viewDidLoad];

    // Do any additional setup after loading the view.
    
    
    
    ReNes::setLogCallback([self](const char* buffer){
        
        // 收集debug字符串
        @synchronized (self) {
            _logStringCache += buffer;
        }
    });
    
    
    _nextSem = dispatch_semaphore_create(0);
    
    // 异步
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        
        NSString* filePath = [[NSBundle mainBundle] pathForResource:@"OUR.NES" ofType:@""];
        NSData* data = [NSData dataWithContentsOfFile:filePath];
        
        _nes.cpu_callback = [self](){
//            [self updateView]; // 不能在回调里执行
            
            if (!_keepNext)
                dispatch_semaphore_wait(_nextSem, DISPATCH_TIME_FOREVER);
            
            return true;
        };
        
        _nes.ppu_callback = [self](){
            
            // 显示图片
            int width  = _nes.ppu()->width();
            int height = _nes.ppu()->height();
            uint8_t* srcBuffer = _nes.ppu()->buffer();
            
            [self.displayView updateRGBData:srcBuffer size:CGSizeMake(width, height)];
            
            return true;
        };
        
        _nes.loadRom((const uint8_t*)[data bytes], [data length]);
        
        _nes.run();
    });
    
    NSTimer *timer = [NSTimer scheduledTimerWithTimeInterval:0.1 target:self selector:@selector(updateView) userInfo:nil repeats:YES];
    [[NSRunLoop currentRunLoop] addTimer:timer forMode:NSRunLoopCommonModes];
}

- (void) dumpMemToView
{
    dispatch_async(dispatch_get_main_queue(), ^{
        
        std::string str;
        int count;
        NSTextView* dstView;
        uint8_t* srcData;
        if ([[_memTabView.selectedTabViewItem identifier] integerValue] == 1)
        {
            dstView = _memView;
            count = 0x10000;
            srcData = _nes.mem()->masterData();
        }
        else
        {
            dstView = _vramView;
            count = 1024*16;
            srcData = _nes.ppu()->vram.masterData();
        }
        
        
        bool printLineNum = true;
        
        char buffer[10];
        
        for (int i=0; i<count; i++)
        {
            if (i > 1 && i % 0x10 == 0)
            {
                str += "\r";
                printLineNum = true;
            }
            
            if (printLineNum)
            {
                sprintf(buffer, "0x%04X  ", i);
                str += buffer;
                printLineNum = false;
            }
            
            uint8_t data = srcData[i];
            
            sprintf(buffer, "%02X", data);
            
            str += buffer;
            
            
            if (i < count-1)
            {
                {
                    str += " ";
                }
            }
        }
        dstView.string = [NSString stringWithUTF8String:str.c_str()];
    });
}

- (void) updateView
{
    [self updateRegisters];
    
    // 打印内存
    [self dumpMemToView];
    
    // 输出log
    @synchronized (self) {
        NSString* sbuffer = [NSString stringWithUTF8String:_logStringCache.c_str()];
        [self log:sbuffer];
        
        _logStringCache = "";
    }
}

- (void) updateRegisters
{
    dispatch_async(dispatch_get_main_queue(), ^{
        
        const ReNes::CPU::__registers& regs = _nes.cpu()->regs;
        
        _registersView.stringValue = [NSString stringWithFormat:@"PC: 0x%04X SP: 0x%04X\n\
C:%d Z:%d I:%d D:%d B:%d _:%d V:%d N:%d\n\
A:%d X:%d Y:%d", regs.PC, regs.SP,
                                      regs.P.get(ReNes::CPU::__registers::C),
                                      regs.P.get(ReNes::CPU::__registers::Z),
                                      regs.P.get(ReNes::CPU::__registers::I),
                                      regs.P.get(ReNes::CPU::__registers::D),
                                      regs.P.get(ReNes::CPU::__registers::B),
                                      regs.P.get(ReNes::CPU::__registers::_),
                                      regs.P.get(ReNes::CPU::__registers::V),
                                      regs.P.get(ReNes::CPU::__registers::N),
                                      regs.A,regs.X,regs.Y];
        
        
    });
}

- (IBAction) next:(id)sender
{
    dispatch_semaphore_signal(_nextSem);
}

- (IBAction) nextKeep:(id)sender
{
    _keepNext = !_keepNext;
    
    dispatch_semaphore_signal(_nextSem);
}

- (IBAction) MNI:(id) sender
{
    _nes.cpu()->interrupts(ReNes::CPU::InterruptTypeNMI);
}

- (IBAction) debug:(NSButton*) sender
{
    _nes.setDebug(!_nes.debug);
    _nes.cmd_interval = 0.001;
}

- (void)setRepresentedObject:(id)representedObject {
    [super setRepresentedObject:representedObject];

    // Update the view, if already loaded.
}


@end
