//
//  ViewController.m
//  RENES
//
//  Created by rexq on 2017/6/25.
//  Copyright © 2017年 com.rexq. All rights reserved.
//

#import "ViewController.h"
#include "src/renes.hpp"
#include <string>
#import "MyOpenGLView.h"

@interface ViewController()
{
    ReNes::Nes _nes;
    
    dispatch_semaphore_t _nextSem;
    
    BOOL _keepNext;
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
    });
}

- (void)viewDidLoad {
    [super viewDidLoad];

    // Do any additional setup after loading the view.
    
    ReNes::setLogCallback([self](const char* buffer){
        
        NSString* sbuffer = [NSString stringWithUTF8String:buffer];
        
        [self log:sbuffer];
    });
    
    
    _nextSem = dispatch_semaphore_create(0);
    
    // 异步
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        
        NSString* filePath = [[NSBundle mainBundle] pathForResource:@"OUR.NES" ofType:@""];
        NSData* data = [NSData dataWithContentsOfFile:filePath];
        
        _nes.cpu_callback = [self](){
            [self updateView];
            
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
            
//            uint8_t* buffer = new uint8_t[width*height*4];
//            {
//                int pixels = width*height;
//                for (int i=0; i<pixels; i++)
//                {
//                    buffer[i*4] = srcBuffer[i*3];
//                    buffer[i*4+1] = srcBuffer[i*3+1];
//                    buffer[i*4+2] = srcBuffer[i*3+2];
//                    buffer[i*4+3] = 255;
//                }
//            }
//
//            
//            size_t bufferLength = width * height * 4;
//            CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, buffer, bufferLength, NULL);
//            size_t bitsPerComponent = 8;
//            size_t bitsPerPixel = 32;
//            size_t bytesPerRow = 4 * width;
//            CGColorSpaceRef colorSpaceRef = CGColorSpaceCreateDeviceRGB();
//            CGBitmapInfo bitmapInfo = kCGBitmapByteOrderDefault | kCGImageAlphaPremultipliedLast;
//            CGColorRenderingIntent renderingIntent = kCGRenderingIntentDefault;
//            
//            CGImageRef iref = CGImageCreate(width,
//                                            height,
//                                            bitsPerComponent,
//                                            bitsPerPixel,
//                                            bytesPerRow,
//                                            colorSpaceRef,
//                                            bitmapInfo,
//                                            provider,   // data provider
//                                            NULL,       // decode
//                                            YES,        // should interpolate
//                                            renderingIntent);
//            
//            NSImage * image = [[NSImage alloc] initWithCGImage:iref size:NSMakeSize(width, height)];
//
//            CGDataProviderRelease(provider);
//            CGColorSpaceRelease(colorSpaceRef);
//            CGImageRelease(iref);
//            delete[] buffer;
//            
//            dispatch_async(dispatch_get_main_queue(), ^{
//                
////                self.displayView.image = image;
//                
//                
//                
//            });
            
            return true;
        };
        
        _nes.loadRom((const uint8_t*)[data bytes], [data length]);
        
        _nes.run();
    });
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
            srcData = _nes.ppu()->VRAM;
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

- (void)setRepresentedObject:(id)representedObject {
    [super setRepresentedObject:representedObject];

    // Update the view, if already loaded.
}


@end
