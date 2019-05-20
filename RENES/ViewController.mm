//
//  ViewController.m
//  RENES
//
//  Created by rexq57 on 2017/6/25.
//  Copyright © 2017年 com.rexq57. All rights reserved.
//

#import "ViewController.h"
#include <src/renes.hpp>
#include <string>
#import "MyOpenGLView.h"
#import <BlocksKit/BlocksKit.h>
#import "PetternTableView.h"
#import <ReactiveObjC/ReactiveObjC.h>

using namespace ReNes;

@interface ViewController()<NSTabViewDelegate>
{
    Nes* _nes;
    
    dispatch_semaphore_t _nextSem;
    
    BOOL _keepNext;
    
    std::string _logStringCache;
    
    int _stopedCmdAddr;
    long _stopedCmdLine;
    
    
}

@property (nonatomic) NSString* filePath;

@property (nonatomic) IBOutlet MyOpenGLView* displayView;
@property (nonatomic) IBOutlet NSTabView* memTabView;
@property (nonatomic) IBOutlet NSTextView* memView;
@property (nonatomic) IBOutlet NSTextView* vramView;
@property (nonatomic) IBOutlet NSTextView* sprramView;
@property (nonatomic) IBOutlet PetternTableView* petternTableView;
@property (nonatomic) IBOutlet MyOpenGLView* scrollMirroringView;

@property (nonatomic) IBOutlet NSTextField* stopedCmdAddrField;
@property (nonatomic) IBOutlet NSTextField* stopedCmdLineField;


@property (nonatomic) IBOutlet NSTextView* logView;
@property (nonatomic) IBOutlet NSTextField* registersView;



@end

@implementation ViewController

- (void) log:(NSString*) buffer
{
    if (buffer == nil)
        return;
    
//    dispatch_async(dispatch_get_main_queue(), ^{
//        
//        @autoreleasepool {
//            
//            //        std::string str = std::string([_logView.string UTF8String]) + std::string([buffer UTF8String]);
//            
//            //        _logView.string = [NSString stringWithUTF8String:str.c_str()];
//            [[_logView textStorage] appendAttributedString:[[NSAttributedString alloc] initWithString:buffer]];
//            
//            [_logView scrollRangeToVisible: NSMakeRange(_logView.string.length, 0)];
//        }
//        
//
//    });
}

- (void) startNes:(NSString*) filePath
{
    // 停止，才重新启动
    if (_nes)
    {
        _nes->stop();
        _keepNext = false; // 拦截在第一次cmd之后
        dispatch_semaphore_signal(_nextSem); // 不拦截下一句指令，顺利让cpu进入stop判断
    }
    
    // 异步
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        
//        @autoreleasepool {
        
            NSData* data = [NSData dataWithContentsOfFile:filePath];
            
            if (_nes != 0)
            {
                delete _nes;
            }
            _nes = new Nes();
            
            _nes->willRunning = [self](){
                
                // 反汇编
                [self disassembly];
            };
            
            _nes->cpu_callback = [self](CPU* cpu){
                
                // 手动中断
                //            @synchronized ((__bridge id)_nes)
                {
                    if (cpu == _nes->cpu())
                    {
                        if (!_keepNext)
                        {
                            dispatch_semaphore_wait(_nextSem, DISPATCH_TIME_FOREVER);
                        }
                        else if ((cpu->regs.PC == _stopedCmdAddr || cpu->execCmdLine == _stopedCmdLine))
                        {
                            _nes->setDebug(true);
                            log("自定义地址中断\n");
                            dispatch_semaphore_wait(_nextSem, DISPATCH_TIME_FOREVER);
                        }
                    }
                }
                
                return true;
            };
            
            _nes->ppu_displayCallback = [self](PPU* ppu){
                
                if (ppu == _nes->ppu())
                {
                    {
                        // 显示图片
                        int width  = ppu->width();
                        int height = ppu->height();
                        uint8_t* srcBuffer = ppu->buffer();
                        
                        [self.displayView updateRGBData:srcBuffer size:CGSizeMake(width, height)];
                        
                    }
                    
                    {
                        if ([_memTabView.selectedTabViewItem.identifier isEqualToString:@"scroll"])
                        {
                            ppu->dumpScrollToBuffer();
                            
                            // 显示图片
                            int width  = _nes->ppu()->width()*2;
                            int height = _nes->ppu()->height()*2;
                            uint8_t* srcBuffer = ppu->scrollBufferRGB();
                            
                            [_scrollMirroringView updateRGBData:srcBuffer size:CGSizeMake(width, height)];
                        }
                    }
                }
                
                return true;
            };
        
            // for test
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
            
            NSLog(@"模拟器开始运行");
            
//        }
    });
}

- (instancetype) initWithCoder:(NSCoder *)coder
{
    if ((self = [super initWithCoder:coder]))
    {
        // 相应tab切换
//        [[self rac_signalForSelector:@selector(tabView:didSelectTabViewItem:) fromProtocol:@protocol(NSTabViewDelegate)] subscribeNext:^(RACTuple* tuple) {
//            
//            if (tuple.first == _memTabView){
//                
//                NSString* identifier = [tuple.second identifier];
//                
//                if (_nes)
//                {
//                    _nes->dumpScrollBuffer = [identifier isEqualToString:@"scroll"];
//                }
//                
////                _thread = std::thread(working);
//                
//                
////                NSLog(@"%@", [tuple.second identifier]);
//            }
//        }];
    }
    
    return self;
}

- (void)viewDidLoad {
    [super viewDidLoad];

    // Do any additional setup after loading the view.
    

    
    
    
    
//    setLogCallback([self](const char* buffer){
//        
//        // 收集debug字符串
//        @synchronized (self) {
//            _logStringCache += buffer;
//        }
//    });
    
    _logView.editable = NO;
    _logView.font = [NSFont fontWithName:@"Courier" size:12];
    _memView.font = [NSFont fontWithName:@"Courier" size:12];
    _vramView.font = [NSFont fontWithName:@"Courier" size:12];
    _sprramView.font = [NSFont fontWithName:@"Courier" size:12];
    
    
//    [self startNes];
    
    _nextSem = dispatch_semaphore_create(0);
    
    
    
    NSTimer *timer = [NSTimer scheduledTimerWithTimeInterval:0.1 target:self selector:@selector(updateView) userInfo:nil repeats:YES];
    [[NSRunLoop currentRunLoop] addTimer:timer forMode:NSRunLoopCommonModes];
}

- (void) viewDidAppear
{
    [super viewDidAppear];
    
    
}

- (void)setRepresentedObject:(id)representedObject {
    [super setRepresentedObject:representedObject];
    
    // Update the view, if already loaded.
}

- (void) dumpMemToView
{
    dispatch_async(dispatch_get_main_queue(), ^{
        
        @autoreleasepool {
        
            std::string str;
            int count;
            NSTextView* dstView;
            const uint8_t* srcData;
            
            switch ([[_memTabView.selectedTabViewItem identifier] integerValue]) {
                case 0:
                    dstView = _memView;
                    count = 0x10000;
                    srcData = _nes->mem()->masterData();
                    break;
                case 1:
                    dstView = _vramView;
                    count = 1024*16;
                    srcData = const_cast<VRAM*>(_nes->ppu()->vram())->masterData();
                    break;
                    
                case 2:
                    dstView = _sprramView;
                    count = 0x100;
                    srcData = _nes->ppu()->sprram();
                    break;
                case 3:
                {
                    uint8_t p[32];
                    
                    _nes->ppu()->petternTables(p);
                    NSMutableArray<NSColor*>* arr = [NSMutableArray array];
                    for (auto i : p)
                    {
                        const uint8_t* rgb = &DEFAULT_PALETTE[i*3];
                        NSColor* color = [NSColor colorWithRed:rgb[0]/255.0 green:rgb[1]/255.0 blue:rgb[2]/255.0 alpha:1];
                        
                        [arr addObject:color];
                    }
                    _petternTableView.colors = arr;
                    return;
                    break;
                }
                case 4:
                {
                    
                    return;
                    break;
                }
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
            
        }
    });
}


- (void) disassembly
{
    int pc = 0x8000;
    uint8_t cmd;
    
    std::string dis;
    do {
        
        Memory* mem = _nes->mem();
        
        // 从内存里面取出一条8bit指令，将PC移动到下一个内存地址
        cmd = mem->read8bitData(pc);
        
        //    log("[%ld][%04X] cmd: %x => ", execCmdLine, pc, cmd);
        
        if (!RENES_SET_FIND(CMD_LIST, cmd))
        {
//            assert(!"未知的指令！");
//            log("[%04X] cmd: %x => ", pc, cmd);
//            break;
            
            std::string str = std::to_string(cmd);
            str = "[" + int_to_hex(pc) + "]\t\t" + str + "\n";
            dis += str;
            pc ++;
        }
        else
        {
            auto info = CMD_LIST.at(cmd);
            std::string str = cmd_str(info, pc, mem);
            str = "[" + int_to_hex(pc) + "]\t\t" + str + "\n";
            //        printf("%s", str.c_str());
            
            dis += str;
            pc += info.bytes;
        }
        
        if (cmd == 0) // brk 下一句地址+1
        {
            pc ++;
        }
        
        
    }while (pc < 0x8000 + 1024*32);
    
    
    __block NSString* buffer = [NSString stringWithUTF8String:dis.c_str()];
    
    dispatch_async(dispatch_get_main_queue(), ^{
        
        @autoreleasepool {
            
            //        std::string str = std::string([_logView.string UTF8String]) + std::string([buffer UTF8String]);
            
            _logView.string = buffer;
//            [[_logView textStorage] appendAttributedString:[[NSAttributedString alloc] initWithString:buffer]];
//            
//            [_logView scrollRangeToVisible: NSMakeRange(_logView.string.length, 0)];
        }
        
        
    });
    
//    printf("%s\n", dis.c_str());
}

- (void) updateView
{
    if (_nes == 0 || !_nes->isRunning())
        return;
    
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
        
        @autoreleasepool {
        
            const CPU::__registers& regs = _nes->cpu()->regs;
            
            const bool* statues = _nes->ctr()->statues();
            
            double perFrameTime = (double)_nes->perFrameTime() / 1e9; // 每帧花费时间: 秒
            
            _registersView.stringValue = [NSString stringWithFormat:@"PC: 0x%04X SP: 0x%04X\n\
C:%d Z:%d I:%d D:%d B:%d _:%d V:%d N:%d\n\
A:%d X:%d Y:%d - %.9lf(%.9lf), %lf - fps %d\n\
%d %d %d %d %d %d %d %d\n\
%s", regs.PC, regs.SP,
                                          regs.P.get(CPU::__registers::C),
                                          regs.P.get(CPU::__registers::Z),
                                          regs.P.get(CPU::__registers::I),
                                          regs.P.get(CPU::__registers::D),
                                          regs.P.get(CPU::__registers::B),
                                          regs.P.get(CPU::__registers::_),
                                          regs.P.get(CPU::__registers::V),
                                          regs.P.get(CPU::__registers::N),
                                          regs.A,regs.X,regs.Y,
                                          (double)_nes->cpuCycleTime() / 1e9, (double)572 / 1e9, perFrameTime, (int)(1.0 / perFrameTime),
                                          statues[0], statues[1], statues[2], statues[3], statues[4], statues[5], statues[6], statues[7],
                                          _nes->ppu()->testLog.c_str()];
        
        }
        
    });
}

- (IBAction) next:(id)sender
{
    _keepNext = false;
    dispatch_semaphore_signal(_nextSem);
}

- (IBAction) nextKeep:(id)sender
{
    _keepNext = !_keepNext;
    
    dispatch_semaphore_signal(_nextSem);
    
    int number = (int)strtol([[_stopedCmdAddrField stringValue] UTF8String], NULL, 16);
    _stopedCmdAddr = number;
    
    _stopedCmdLine = [[_stopedCmdLineField stringValue] integerValue];
}

- (IBAction) MNI:(id) sender
{
    _nes->cpu()->interrupts(CPU::InterruptTypeNMI);
}

- (IBAction) debug:(NSButton*) sender
{
    _nes->setDebug(!_nes->debug);
    _nes->cmd_interval = 0.001;
}

- (IBAction) resetButton:(id)sender
{
    [self startNes:self.filePath];
}

- (IBAction) openRom:(id)sender
{
    //create open panel...
    NSOpenPanel* openPanel = [NSOpenPanel openPanel];
    
    //set restrictions / allowances...
    [openPanel setAllowsMultipleSelection: NO];
    [openPanel setCanChooseDirectories:NO];
    [openPanel setCanCreateDirectories:NO];
    [openPanel setCanChooseFiles:YES];
    //only allow images...
    [openPanel setAllowedFileTypes:@[@"nes"]];
    
    //open panel as sheet on main window...
    [openPanel beginSheetModalForWindow:self.view.window completionHandler:^(NSInteger result) {
        
        if (result == NSFileHandlingPanelOKButton) {
            
            //get url (should only be one due to restrictions)...
            for( NSURL* URL in [openPanel URLs] ) {
                
//                NSError* err = nil;
                
                self.filePath = [URL path];
                
                [self startNes:self.filePath];
            }
        }
    }];
}



- (void) keyDown:(NSEvent *)event
{
//    NSLog(@"%d", [event keyCode]);
    
//    const static int keys[] = {
////        126, 125, 123, 124, 7, 6, 18, 19
//        13, 1, 0, 2, 38, 40, 18, 19
//    };
    
    switch ([event keyCode]) {
        case 13:
        {
            _nes->ctr()->up(true);
            break;
        }
        case 1:
        {
            _nes->ctr()->down(true);
            break;
        }
        case 0:
        {
            _nes->ctr()->left(true);
            break;
        }
        case 2:
        {
            _nes->ctr()->right(true);
            break;
        }
        case 40:
        {
            _nes->ctr()->A(true);
            break;
        }
        case 38:
        {
            _nes->ctr()->B(true);
            break;
        }
        case 18:
        {
            _nes->ctr()->select(true);
            break;
        }
        case 19:
        {
            _nes->ctr()->start(true);
            break;
        }
        default:
            break;
    }
}

- (void) keyUp:(NSEvent *)event
{
    switch ([event keyCode]) {
        case 13:
        {
            _nes->ctr()->up(false);
            break;
        }
        case 1:
        {
            _nes->ctr()->down(false);
            break;
        }
        case 0:
        {
            _nes->ctr()->left(false);
            break;
        }
        case 2:
        {
            _nes->ctr()->right(false);
            break;
        }
        case 40:
        {
            _nes->ctr()->A(false);
            break;
        }
        case 38:
        {
            _nes->ctr()->B(false);
            break;
        }
        case 18:
        {
            _nes->ctr()->select(false);
            break;
        }
        case 19:
        {
            _nes->ctr()->start(false);
            break;
        }
        default:
            break;
    }
}

@end
