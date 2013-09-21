/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2013 Martin Johannesson
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef TARGET_OS_MAC
#define GL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED
#import <OpenGL/gl3.h>
#elif defined(TARGET_OS_IPHONE)
#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES2/glext.h>
#else
#error This file is only meant for OS X or iOS.
#endif

/* Convenience macro for writing shaders in C code files. */
#define SHADER_STRING(x) #x

typedef enum GPUStatus {
    GPUStatusOK = 0,
    GPUStatusUnknownError = 1,
    GPUStatusFailedToMakeFramebufferObjectError = 2,
    GPUStatusNoSuchParameter = 3,
    GPUStatusOutOfMemory = 4,
    GPUStatusInvalidTexture = 5,
    GPUStatusInvalidFramebuffer = 6,
    GPUStatusInvalidProgram = 7
} GPUStatus;

typedef enum GPUColorFormat {
    GPUColorFormatRGB = 0,
    GPUColorFormatRGBA = 1,
    GPUColorFormatBGRA = 2
} GPUColorFormat;

typedef struct GPUTexture {
    uint32_t valid;
    uint32_t textureId;
    uint32_t width;
    uint32_t height;
} GPUTexture;

typedef struct GPUFramebuffer {
    uint32_t valid;
    uint32_t framebufferId;
    GPUTexture texture;
} GPUFramebuffer;

typedef struct GPUProgram {
    uint32_t valid;
    uint32_t programId;
    int32_t textureUniformLocation;
    struct {
        int textureShouldBeUsed;
        GPUTexture texture;
        int32_t uniformLocation;
    } additionalTextures[7];
} GPUProgram;

#pragma mark - Render Image

/* Configures the rendering pipeline. Call before rendering the first time. */
GPUStatus gpuConfigureRenderingPipeline(void);

/* Renders the texture image to the framebuffer using the specified program. */
GPUStatus gpuRenderTextureToFramebufferUsingProgram(GPUTexture *texture,
                                                    GPUFramebuffer *framebuffer,
                                                    GPUProgram *program);

/* Renders the framebuffer texture to the framebuffer using the program. */
GPUStatus gpuRenderFramebufferToFramebufferUsingProgram(GPUFramebuffer *source,
                                                        GPUFramebuffer *target,
                                                        GPUProgram *program);

#pragma mark - Framebuffer

/* Creates a texture backed frame buffer. */
GPUStatus gpuCreateFramebuffer(uint32_t width, uint32_t height,
                               GPUFramebuffer *framebuffer);

void gpuDestroyFramebuffer(GPUFramebuffer *framebuffer);

uint32_t gpuGetFramebufferSizeInBytes(GPUFramebuffer *framebuffer);

GPUStatus gpuGetFramebufferContents(GPUFramebuffer *framebuffer,
                                    uint8_t *pixelData,
                                    GPUColorFormat colorFormat);

#pragma mark - Texture

GPUStatus gpuCreateTexture(GPUTexture *texture);

void gpuDestroyTexture(GPUTexture *texture);

GPUStatus gpuUploadImageToTexture(uint32_t width, uint32_t height,
                                  GPUColorFormat colorFormat,
                                  uint8_t *pixelData, GPUTexture *texture);

/* Calls gpuCreateTexture() and gpuUploadImageToTexture(). */
GPUStatus gpuCreateTextureFromImage(uint32_t width, uint32_t height,
                                    GPUColorFormat colorFormat,
                                    uint8_t *pixelData, GPUTexture *texture);

/* Calls gpuCreateTextureFromImage() with all white pixel data. */
GPUStatus gpuCreateBlankTexture(uint32_t width, uint32_t height,
                                GPUTexture *texture);


#pragma mark - Shader Program

/* A pass-through vertex shader. Useful for most cases. */
extern const char *kGPUDefaultVertexShaderCode;

/* A pass-through fragment shader. Custom fragment shaders are more useful. */
extern const char *kGPUDefaultFragmentShaderCode;

/* Compiles a shader program from a vertex shader and a fragment shader. */
GPUStatus gpuCompileProgram(const char *vertexShaderCode,
                            const char *fragmentShaderCode,
                            GPUProgram *program,
                            void (*logFunc)(const char *log));

void gpuDestroyProgram(GPUProgram *program);

/* Bind textures to program, in addition to the input image to render. */
void gpuSetSecondTextureForProgram(GPUTexture *texture, GPUProgram *program);
void gpuSetThirdTextureForProgram(GPUTexture *texture, GPUProgram *program);
void gpuSetFourthTextureForProgram(GPUTexture *texture, GPUProgram *program);
void gpuSetFifthTextureForProgram(GPUTexture *texture, GPUProgram *program);
void gpuSetSixthTextureForProgram(GPUTexture *texture, GPUProgram *program);
void gpuSetSeventhTextureForProgram(GPUTexture *texture, GPUProgram *program);
void gpuSetEigthTextureForProgram(GPUTexture *texture, GPUProgram *program);

/* Bind parameters to program. */
GPUStatus gpuSetFloatForProgram(const char *name, float value, GPUProgram *program);
GPUStatus gpuSet2FloatsForProgram(const char *name, float value0, float value1, GPUProgram *program);
GPUStatus gpuSet3FloatsForProgram(const char *name, float value0, float value1, float value2, GPUProgram *program);
GPUStatus gpuSet4FloatsForProgram(const char *name, float value0, float value1, float value2, float value3, GPUProgram *program);
GPUStatus gpuSetIntForProgram(const char *name, int32_t value, GPUProgram *program);
GPUStatus gpuSet2IntsForProgram(const char *name, int value0, int value1, GPUProgram *program);
GPUStatus gpuSet3IntsForProgram(const char *name, int value0, int value1, int value2, GPUProgram *program);
GPUStatus gpuSet4IntsForProgram(const char *name, int value0, int value1, int value2, int value3, GPUProgram *program);
GPUStatus gpuSetVector2ForProgram(const char *name, const float *values, GPUProgram *program);
GPUStatus gpuSetVector3ForProgram(const char *name, const float *values, GPUProgram *program);
GPUStatus gpuSetVector4ForProgram(const char *name, const float *values, GPUProgram *program);
GPUStatus gpuSetVector3ForProgram(const char *name, const float *values, GPUProgram *program);
GPUStatus gpuSetVector4ForProgram(const char *name, const float *values, GPUProgram *program);
GPUStatus gpuSetFloatArrayForProgram(const char *name, uint32_t count, const float *values, GPUProgram *program);
GPUStatus gpuSetFloatArrayForProgram(const char *name, uint32_t count, const float *values, GPUProgram *program);
GPUStatus gpuSetVector3ArrayForProgram(const char *name, uint32_t count, const float *values, GPUProgram *program);
GPUStatus gpuSetVector4ArrayForProgram(const char *name, uint32_t count, const float *values, GPUProgram *program);
GPUStatus gpuSetIntArrayForProgram(const char *name, uint32_t count, const int32_t *values, GPUProgram *program);
GPUStatus gpuSetIntVector2ArrayForProgram(const char *name, uint32_t count, const int32_t *values, GPUProgram *program);
GPUStatus gpuSetIntVector3ArrayForProgram(const char *name, uint32_t count, const int32_t *values, GPUProgram *program);
GPUStatus gpuSetIntVector4ArrayForProgram(const char *name, uint32_t count, const int32_t *values, GPUProgram *program);
GPUStatus gpuSetMatrix2x2ForProgram(const char *name, const float *values, GPUProgram *program);
GPUStatus gpuSetMatrix3x3ForProgram(const char *name, const float *values, GPUProgram *program);
GPUStatus gpuSetMatrix4x4ForProgram(const char *name, const float *values, GPUProgram *program);
GPUStatus gpuSetMatrix2x2ArrayForProgram(const char *name, uint32_t count, const float *values, GPUProgram *program);
GPUStatus gpuSetMatrix3x3ArrayForProgram(const char *name, uint32_t count, const float *values, GPUProgram *program);
GPUStatus gpuSetMatrix4x4ArrayForProgram(const char *name, uint32_t count, const float *values, GPUProgram *program);

