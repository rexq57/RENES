//
//  DisplayView.m
//  RENES
//
//  Created by Viktor Pih on 2017/9/21.
//  Copyright © 2017年 com.rexq. All rights reserved.
//

#import "DisplayView.h"

void runSynchronouslyOnVideoProcessingQueue(void (^block)(void))
{
    dispatch_queue_t videoProcessingQueue = [GPUImageContext sharedContextQueue];

    if (dispatch_get_current_queue() == videoProcessingQueue)

        {
            block();
        }else
        {
            dispatch_sync(videoProcessingQueue, block);
        }
}

void runAsynchronouslyOnVideoProcessingQueue(void (^block)(void))
{
    dispatch_queue_t videoProcessingQueue = [GPUImageContext sharedContextQueue];
    

    if (dispatch_get_current_queue() == videoProcessingQueue)

        {
            block();
        }else
        {
            dispatch_async(videoProcessingQueue, block);
        }
}

#ifndef CSHADER_STRING
#define CSTRINGIZE(...) #__VA_ARGS__
//#define CSTRINGIZE2(x) CSTRINGIZE(x)
#define CSHADER_STRING(text) CSTRINGIZE(text)
#endif

const GLchar* const kVertexShaderString = CSHADER_STRING
(
 attribute vec4 position;
 attribute vec4 inputTextureCoordinate;
 
 varying vec2 textureCoordinate;
 
 void main()
 {
     gl_Position = position;
     textureCoordinate = inputTextureCoordinate.xy;
 }
 );

const GLchar* const kFragmentShaderString = CSHADER_STRING
(
 varying vec2 textureCoordinate;
 
 uniform sampler2D inputImageTexture;
 
 void main()
 {
     gl_FragColor = texture2D(inputImageTexture, textureCoordinate);
     //      gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
 }
 );


static
const char * GetGLErrorStr(GLenum err)
{
    switch (err)
    {
        case GL_NO_ERROR:          return "No error";
        case GL_INVALID_ENUM:      return "Invalid enum";
        case GL_INVALID_VALUE:     return "Invalid value";
        case GL_INVALID_OPERATION: return "Invalid operation";
            //            case GL_STACK_OVERFLOW:    return "Stack overflow";
            //            case GL_STACK_UNDERFLOW:   return "Stack underflow";
        case GL_OUT_OF_MEMORY:     return "Out of memory";
        default:                   return "Unknown error";
    }
}


void CheckGLError(const char* tag)
{
    //#ifdef DEBUG
    //        while (true)
    {
        const GLenum err = glGetError();
        if (GL_NO_ERROR != err)
        {
            //std::cout << index << "GL Error: " << GetGLErrorStr(err) << std::endl;
            printf("%s GL Error:%s\n", tag, GetGLErrorStr(err));
        }
    }
    //#endif
}

@interface GPUImageView(Private)

- (void)commonInit;

@end

@interface GPUImageFramebufferWrapper : GPUImageFramebuffer

@property (nonatomic) GLuint customTexture;

@end

@implementation GPUImageFramebufferWrapper

- (GLuint) texture
{
    return _customTexture;
}

@end

@interface DisplayView()
{
    GLuint _program;
    GLuint _textureId;
    
    GLint _uniformTexture;
    GLint _attribPosition;
    GLint _attribTextureCoordinate;
    
    CGSize _lastSize;
    uint8_t* _buffer;
}

@property (copy) void(^updateData)();

@end

@implementation DisplayView

- (void) commonInit
{
    [super commonInit];
    
    _textureId = 0;
}

- (void)dealloc
{
    runSynchronouslyOnVideoProcessingQueue(^{
        
        [GPUImageContext useImageProcessingContext];
        
        if (_textureId)
            glDeleteTextures(1, &_textureId);
    });
}

/*
// Only override drawRect: if you perform custom drawing.
// An empty implementation adversely affects performance during animation.
- (void)drawRect:(CGRect)rect {
    // Drawing code
}
*/

- (void) _updateRGBData:(uint8_t*) data size:(CGSize) size
{
    if (_textureId == 0)
    {
        glActiveTexture(GL_TEXTURE0);
        glGenTextures(1, &_textureId);
        glBindTexture(GL_TEXTURE_2D, _textureId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // This is necessary for non-power-of-two textures
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, (int)size.width, (int)size.height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        
        CheckGLError("create texture");
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, _textureId);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, (int)size.width, (int)size.height, GL_RGB, GL_UNSIGNED_BYTE, data);
        
        CheckGLError("update texture");
    }
}

- (void) updateRGBData:(uint8_t*) data size:(CGSize) size
{
    @synchronized (self) {
        
        @autoreleasepool {
            
            size_t len = (int)size.width*(int)size.height*3;
            
            if (!CGSizeEqualToSize(_lastSize, size))
            {
                if (_buffer)
                    free(_buffer);
                
                _buffer = (uint8_t*)malloc(len);
                
                _lastSize = size;
            }
            
            //            NSData* dt = [NSData dataWithBytes:data length:len];
            memcpy(_buffer, data, len);
            
            if (self.updateData == nil)
            {
                __weak DisplayView* unsafe_self = self;
                uint8_t* buffer = _buffer;
                self.updateData = ^() {
                    
                    //                [unsafe_self _updateRGBData:(uint8_t*)[dt bytes] size:size];
                    [unsafe_self _updateRGBData:buffer size:size];
                };
            }
            
            
//            dispatch_async(dispatch_get_main_queue(), ^{
//                
//                @autoreleasepool {
//                    //        [self setNeedsDisplay:YES];
//                    [self display];
//                }
//            });
            
            runSynchronouslyOnVideoProcessingQueue(^{
                
                [GPUImageContext useImageProcessingContext];
                
                [self render];
            });
        }
    }
    //    [[self openGLContext] flushBuffer];
    //    [self update];
    //    self.needsDisplay = YES;
    
}

#pragma mark -
#pragma mark GPUInput protocol

- (void)render
{
    @synchronized (self) {
        if (self.updateData)
            self.updateData();
        
        self.updateData = nil;
    }
    
    GPUImageFramebufferWrapper* buffer = [GPUImageFramebufferWrapper new];
    buffer.customTexture = _textureId;
    [buffer disableReferenceCounting];
    
    [self setInputFramebuffer:buffer atIndex:0];
    [self setInputSize:_lastSize atIndex:0];
    
    [self newFrameReadyAtTime:kCMTimeInvalid atIndex:0];
}

@end
