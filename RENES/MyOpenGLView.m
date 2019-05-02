//
//  MyOpenGLView.m
//  RENES
//
//  Created by rexq57 on 2017/6/29.
//  Copyright © 2017年 com.rexq57. All rights reserved.
//

#import "MyOpenGLView.h"
#include <OpenGL/gl3.h>

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

@interface MyOpenGLView()
{
    GLuint _program;
    GLuint _textureId;
    
    GLint _uniformTexture;
    GLint _attribPosition;
    GLint _attribTextureCoordinate;
    
    CGSize _lastSize;
    uint8_t* _buffer;
    
    NSTimer* _displayTimer;
}

@property (copy) void(^updateData)();


@end

@implementation MyOpenGLView

- (void)prepareOpenGL {
//    init_gl();
    
    // this sets swap interval for double buffering
    GLint swapInt = 1;
    [[self openGLContext] setValues:&swapInt forParameter:NSOpenGLCPSwapInterval];
    
    // this enables alpha in the frame buffer (commented now)
    //  GLint opaque = 0;
    //  [[self openGLContext] setValues:&opaque forParameter:NSOpenGLCPSurfaceOpacity];
    
    _program = [self loadProgramWithVertexShader:kVertexShaderString fragmentShader:kFragmentShaderString];
    
    _uniformTexture = glGetUniformLocation(_program, "inputImageTexture");
    _attribPosition = glGetAttribLocation(_program, "position");
    _attribTextureCoordinate = glGetAttribLocation(_program, "inputTextureCoordinate");
    
    
    _textureId = 0;
    
}


- (instancetype) initWithCoder:(NSCoder *)coder
{
    if ((self = [super initWithCoder:coder]))
    {
        NSTimer* displayTimer = [NSTimer timerWithTimeInterval:0.02   //time interval
                                            target:self
                                          selector:@selector(timerFired:)
                                          userInfo:nil
                                           repeats:YES];
        
        [[NSRunLoop currentRunLoop] addTimer:displayTimer
                                     forMode:NSDefaultRunLoopMode];
        _displayTimer = displayTimer;
    }
    return self;
}

- (void)timerFired:(id)sender
{
    [self setNeedsDisplay:YES];
}

- (GLuint) loadProgramWithVertexShader:(const char*) strVSource fragmentShader:(const char*) strFSource
{
    GLuint iVShader;
    GLuint iFShader;
    int iProgId;
    //    GLint status;
    iVShader = [self loadShader:strVSource type:GL_VERTEX_SHADER];
    iFShader = [self loadShader:strFSource type:GL_FRAGMENT_SHADER];
    
    iProgId = glCreateProgram();
    
    glAttachShader(iProgId, iVShader);
    glAttachShader(iProgId, iFShader);
    
    glLinkProgram(iProgId);
    
    GLint status;
    glGetProgramiv(iProgId, GL_LINK_STATUS, &status);
    if (status <= 0) {
        printf("Load Program: Linking Faile\n");
        return 0;
    }
    glDeleteShader(iVShader);
    glDeleteShader(iFShader);
    
    return iProgId;
}

- (GLuint) loadShader:(const char*) shaderText type:(int) iType
{
    /** Initialization-time for shader **/
    GLuint shader, prog;

    
    // Create ID for shader
    shader = glCreateShader(iType);
    
    // Define shader text
    glShaderSource(shader, 1, &shaderText, NULL);
    
    // Compile shader
    glCompileShader(shader);
    
    // Associate shader with program
    glAttachShader(prog, shader);
    
    // Link program
    glLinkProgram(prog);
    
    // Validate program
    glValidateProgram(prog);
    
    // Check the status of the compile/link
    GLint logLen;
    glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &logLen);
    if(logLen > 0)
    {
        // Show any errors as appropriate
        GLchar* log = (GLchar*)malloc(logLen+1);
        glGetProgramInfoLog(prog, logLen, &logLen, log);
        fprintf(stderr, "Prog Info Log: %s\n", log);
        free(log);
    }
    
    return shader;
}


- (void)drawRect:(NSRect)dirtyRect {
    
    @synchronized (self) {
        if (self.updateData)
            self.updateData();
        
        self.updateData = nil;
    }
    
    // 必须设置，否则不能适应全视图显示（估计是参数没有更新，导致坐标点计算step不匹配）
    glViewport(0, 0, self.bounds.size.width, self.bounds.size.height);
    
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glUseProgram(_program);
    
    if (_uniformTexture != -1)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(_uniformTexture, _textureId);
    }
    
    const static GLfloat vertex[] = {
        -1.0f, 1.0f,
        1.0f, 1.0f,
        -1.0f, -1.0f,
        1.0f, -1.0f
    };
    
    const static GLfloat textureCoordinate[] = {

        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f
    };
    
    glVertexAttribPointer(_attribPosition, 2, GL_FLOAT, 0, 0, vertex);
    glEnableVertexAttribArray(_attribPosition);
    
    if (_attribTextureCoordinate != -1)
    {
        glVertexAttribPointer(_attribTextureCoordinate, 2, GL_FLOAT, 0, 0, textureCoordinate);
        glEnableVertexAttribArray(_attribTextureCoordinate);
    }
    
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    
    glFlush();
}

- (void) _updateRGBData:(uint8_t*) data size:(CGSize) size
{
    if (_textureId == 0)
    {
        glActiveTexture(GL_TEXTURE0);
        glGenTextures(1, &_textureId);
        glBindTexture(GL_TEXTURE_2D, _textureId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        // This is necessary for non-power-of-two textures
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (int)size.width, (int)size.height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, _textureId);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, (int)size.width, (int)size.height, GL_RGB, GL_UNSIGNED_BYTE, data);
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
        
            // 拷贝数据到显示缓冲区
            memcpy(_buffer, data, len);
            
            if (self.updateData == nil)
            {
                __weak MyOpenGLView* unsafe_self = self;
                uint8_t* buffer = _buffer;
                self.updateData = ^() {
                    [unsafe_self _updateRGBData:buffer size:size];
                };
            }
            
            // 在主线程调用视图渲染，会造成主线程卡顿，影响按键事件的检测
//            dispatch_async(dispatch_get_main_queue(), ^{
//                @autoreleasepool {
//                    [self display];
//                }
//            });
            // 异步主线程进行显示
//            dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
//                @autoreleasepool {
////                    [self setNeedsDisplay:YES]; // 只能在主线程调用
//                    [self display];
//                }
//            });
        }
    }
}

- (void) mouseDown:(NSEvent *)event
{
    [self.window makeFirstResponder:self.window.contentViewController];
}

@end
