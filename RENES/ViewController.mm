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


using namespace ReNes;

@interface ViewController()
{
    ReNes::Nes* _nes;
    
    dispatch_semaphore_t _nextSem;
    
    BOOL _keepNext;
    
    std::string _logStringCache;
    
    int _stopedCmdAddr;
}

@property (nonatomic) IBOutlet MyOpenGLView* displayView;
@property (nonatomic) IBOutlet NSTabView* memTabView;
@property (nonatomic) IBOutlet NSTextView* memView;
@property (nonatomic) IBOutlet NSTextView* vramView;
@property (nonatomic) IBOutlet NSTextView* sprramView;

@property (nonatomic) IBOutlet NSTextField* stopedCmdAddrField;


@property (nonatomic) IBOutlet NSTextView* logView;
@property (nonatomic) IBOutlet NSTextField* registersView;



@end

@implementation ViewController

- (void) log:(NSString*) buffer
{
    if (buffer == nil)
        return;
    
    dispatch_async(dispatch_get_main_queue(), ^{
        
//        std::string str = std::string([_logView.string UTF8String]) + std::string([buffer UTF8String]);
        
//        _logView.string = [NSString stringWithUTF8String:str.c_str()];
        [[_logView textStorage] appendAttributedString:[[NSAttributedString alloc] initWithString:buffer]];
        
        [_logView scrollRangeToVisible: NSMakeRange(_logView.string.length, 0)];
    });
}

- (void) startNes
{
    // 异步
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        
        NSString* filePath = [[NSBundle mainBundle] pathForResource:@"OUR.NES" ofType:@""];
        NSData* data = [NSData dataWithContentsOfFile:filePath];
        
        if (_nes != 0)
        {
            delete _nes;
        }
        _nes = new ReNes::Nes();
        
        _nes->cpu_callback = [self](CPU* cpu){
            
            // 手动中断
            //            @synchronized ((__bridge id)_nes)
            {
                if (cpu == _nes->cpu())
                {
                    if (!_keepNext || cpu->regs.PC == _stopedCmdAddr)
                        dispatch_semaphore_wait(_nextSem, DISPATCH_TIME_FOREVER);
                }
            }
            
            
            return true;
        };
        
        _nes->ppu_callback = [self](PPU* ppu){
            
            //            @synchronized ((__bridge id)_nes)
            {
                if (ppu == _nes->ppu())
                {
                    // 显示图片
                    int width  = ppu->width();
                    int height = ppu->height();
                    uint8_t* srcBuffer = ppu->buffer();
                    
                    [self.displayView updateRGBData:srcBuffer size:CGSizeMake(width, height)];
                }
            }
            return true;
        };
        
        _nes->setDebug(true);
        _nes->loadRom((const uint8_t*)[data bytes], [data length]);
        
        _nes->run();
    });
}

- (void)viewDidLoad {
    [super viewDidLoad];

    // Do any additional setup after loading the view.
    
    
    
//    ReNes::setLogCallback([self](const char* buffer){
//        
//        // 收集debug字符串
//        @synchronized (self) {
//            _logStringCache += buffer;
//        }
//    });
    
    [self startNes];
    
    _nextSem = dispatch_semaphore_create(0);
    
    
    
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

        switch ([[_memTabView.selectedTabViewItem identifier] integerValue]) {
            case 0:
                dstView = _memView;
                count = 0x10000;
                srcData = _nes->mem()->masterData();
                break;
            case 1:
                dstView = _vramView;
                count = 1024*16;
                srcData = _nes->ppu()->vram()->masterData();
                break;
            
            case 2:
                dstView = _sprramView;
                count = 0x100;
                srcData = _nes->ppu()->sprram();
                break;
            default:
                assert(!"error!");
                break;
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
    // 打印寄存器
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
        
        const ReNes::CPU::__registers& regs = _nes->cpu()->regs;
        
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
    
    _stopedCmdAddr = [[_stopedCmdAddrField stringValue] intValue];
}

- (IBAction) MNI:(id) sender
{
    _nes->cpu()->interrupts(ReNes::CPU::InterruptTypeNMI);
}

- (IBAction) debug:(NSButton*) sender
{
    _nes->setDebug(!_nes->debug);
    _nes->cmd_interval = 0.001;
}

- (IBAction) resetButton:(id)sender
{
    _nes->stop([self](){

        [self startNes];
    });

    _keepNext = false; // 拦截在第一次cmd之后
    dispatch_semaphore_signal(_nextSem); // 不拦截下一句指令，顺利让cpu进入stop判断
}

- (void)setRepresentedObject:(id)representedObject {
    [super setRepresentedObject:representedObject];

    // Update the view, if already loaded.
}


@end
