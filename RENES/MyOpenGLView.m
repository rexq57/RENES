//
//  MyOpenGLView.m
//  RENES
//
//  Created by Viktor Pih on 2017/6/29.
//  Copyright © 2017年 com.rexq. All rights reserved.
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
        
    }
    return self;
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
    
    
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glUseProgram(_program);
    
    if (_uniformTexture != -1)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(_uniformTexture, 0);
    }
    
    const GLfloat vertex[] = {
//        0.0f, 1.0f,
//        1.0f, 1.0f,
//        0.0f, 0.0f,
//        1.0f, 0.0f,
        
        -1.0f, 1.0f,
        1.0f, 1.0f,
        -1.0f, -1.0f,
        1.0f, -1.0f
    };
    
    const GLfloat textureCoordinate[] = {
//        -1.0f, 1.0f,
//        1.0f, 1.0f,
//        -1.0f, -1.0f,
//        1.0f, -1.0f,
                0.0f, 1.0f,
                1.0f, 1.0f,
                0.0f, 0.0f,
                1.0f, 0.0f,
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
        glGenTextures(1, &_textureId);
        glBindTexture(GL_TEXTURE_2D, _textureId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
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
        NSData* dt = [NSData dataWithBytes:data length:(int)size.width*(int)size.height*3];
        
        __weak MyOpenGLView* unsafe_self = self;
        self.updateData = ^() {
            
            [unsafe_self _updateRGBData:(uint8_t*)[dt bytes] size:size];
        };
    }
    
    [self setNeedsDisplay:YES];

}

@end
