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

#include "gpufilter.h"

static const GLfloat vertices[] = {
    -1.0f, -1.0f,
    1.0f, -1.0f,
    -1.0f,  1.0f,
    1.0f,  1.0f,
};

static const GLfloat UVs[] = {
    0.0f, 0.0f,
    1.0f, 0.0f,
    0.0f, 1.0f,
    1.0f, 1.0f,
};

#ifdef TARGET_OS_MAC
const char *kGPUDefaultVertexShaderCode = SHADER_STRING
(
 attribute vec4 inputPosition;
 attribute vec4 inputUV;
 
 varying vec2 uv;
 
 void main() {
     gl_Position = inputPosition;
     uv = inputUV.xy;
 }
 );

const char *kGPUDefaultFragmentShaderCode = SHADER_STRING
(
 varying vec2 uv;
 
 uniform sampler2D texture;
 
 void main() {
     gl_FragColor = texture2D(texture, uv);
     gl_FragColor = gl_FragColor.rgba;
 }
 );
#elif defined(TARGET_OS_IPHONE)
const char *kGPUDefaultVertexShaderCode = SHADER_STRING
(
 attribute vec4 inputPosition;
 attribute vec4 inputUV;
 
 varying vec2 uv;
 
 void main() {
     gl_Position = inputPosition;
     uv = inputUV.xy;
 }
 );

const char *kGPUDefaultFragmentShaderCode = SHADER_STRING
(
 varying highp vec2 uv;
 
 uniform sampler2D texture;
 
 void main() {
     gl_FragColor = texture2D(texture, uv);
     gl_FragColor = gl_FragColor.rgba;
 }
 );
#else
#error This file is only meant for OS X or iOS.
#endif

#pragma mark Forward Declarations

static int gpuCompileShader(GLuint *shader, GLenum type, const char *sourceCode, void (*logFunc)(const char *log));
static int gpuLinkProgram(GLuint program);
static GLenum gpuColorFormatToGLFormat(GPUColorFormat colorFormat);

#pragma mark - Render Image

GPUStatus gpuConfigureRenderingPipeline(void)
{
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    
    GLuint positionAttribute = 0;
    glVertexAttribPointer(positionAttribute, 2, GL_FLOAT, 0, 0, vertices);
    glEnableVertexAttribArray(positionAttribute);
    
    GLuint uvAttribute = 1;
	glVertexAttribPointer(uvAttribute, 2, GL_FLOAT, 0, 0, UVs);
    glEnableVertexAttribArray(uvAttribute);
    
    return GPUStatusOK;
}

GPUStatus gpuRenderTextureToFramebufferUsingProgram(GPUTexture *texture,
                                                    GPUFramebuffer *framebuffer,
                                                    GPUProgram *program)
{
    if (!texture->valid) {
        return GPUStatusInvalidTexture;
    }
    if (!framebuffer->valid) {
        return GPUStatusInvalidFramebuffer;
    }
    if (!program->valid) {
        return GPUStatusInvalidProgram;
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->framebufferId);
    glViewport(0, 0, framebuffer->texture.width, framebuffer->texture.height);
    
    glUseProgram(program->programId);
    glUniform1i(program->textureUniformLocation, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture->textureId);
    
    for (int i = 0; i < 7; i++) {
        if (program->additionalTextures[i].textureShouldBeUsed) {
            glUniform1i(program->additionalTextures[i].uniformLocation, i + 1);
            glActiveTexture(GL_TEXTURE0 + i + 1);
            glBindTexture(GL_TEXTURE_2D, program->additionalTextures[i].texture.textureId);
        }
    }
    
    glActiveTexture(GL_TEXTURE0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    return GPUStatusOK;
}

GPUStatus gpuRenderFramebufferToFramebufferUsingProgram(GPUFramebuffer *source,
                                                        GPUFramebuffer *target,
                                                        GPUProgram *program)
{
    return gpuRenderTextureToFramebufferUsingProgram(&source->texture, target, program);
}

#pragma mark - Framebuffer

GPUStatus gpuCreateFramebuffer(uint32_t width, uint32_t height, GPUFramebuffer *framebuffer)
{
    memset(framebuffer, 0, sizeof(GPUFramebuffer));
    
    framebuffer->texture.width = width;
    framebuffer->texture.height = height;
    
    glGenFramebuffers(1, &framebuffer->framebufferId);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->framebufferId);
    
    glGenTextures(1, &framebuffer->texture.textureId);
    glBindTexture(GL_TEXTURE_2D, framebuffer->texture.textureId);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, NULL);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, framebuffer->texture.textureId, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "Failed to make complete framebuffer object %x\n", glCheckFramebufferStatus(GL_FRAMEBUFFER));
        return GPUStatusFailedToMakeFramebufferObjectError;
    }
    
    framebuffer->valid = 1;
    framebuffer->texture.valid = 1;
    
    return GPUStatusOK;
}

void gpuDestroyFramebuffer(GPUFramebuffer *framebuffer)
{
    if (framebuffer->valid) {
        framebuffer->valid = 0;
        framebuffer->texture.valid = 0;
        glDeleteFramebuffers(1, &framebuffer->framebufferId);
        glDeleteTextures(1, &framebuffer->texture.textureId);
    }
}

uint32_t gpuGetFramebufferSizeInBytes(GPUFramebuffer *framebuffer)
{
    return 4 * framebuffer->texture.width * framebuffer->texture.height;
}

GPUStatus gpuGetFramebufferContents(GPUFramebuffer *framebuffer,
                                    uint8_t *rgbaData,
                                    GPUColorFormat colorFormat)
{
    if (!framebuffer->valid) {
        return GPUStatusInvalidFramebuffer;
    }
    
    GLenum pixelFormat = gpuColorFormatToGLFormat(colorFormat);
    
    glFlush();
    glFinish();
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->framebufferId);
    glReadPixels(0, 0, framebuffer->texture.width, framebuffer->texture.height, pixelFormat, GL_UNSIGNED_BYTE, rgbaData);
    if (glGetError() != GL_NO_ERROR) {
        return GPUStatusUnknownError;
    }
    
    return GPUStatusOK;
}

#pragma mark - Texture

GPUStatus gpuCreateTexture(GPUTexture *texture)
{
    memset(texture, 0, sizeof(GPUTexture));
    
    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &texture->textureId);
    glBindTexture(GL_TEXTURE_2D, texture->textureId);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    texture->valid = 1;
    texture->width = 0;
    texture->height = 0;
    
    return GPUStatusOK;
}

void gpuDestroyTexture(GPUTexture *texture)
{
    if (texture->valid) {
        texture->valid = 0;
        glDeleteTextures(1, &texture->textureId);
    }
}


GPUStatus gpuUploadImageToTexture(uint32_t width, uint32_t height, GPUColorFormat colorFormat, uint8_t *pixelData, GPUTexture *texture)
{
    if (!texture->valid) {
        return GPUStatusInvalidTexture;
    }
    
    texture->width = width;
    texture->height = height;
    
    GLenum pixelFormat = gpuColorFormatToGLFormat(colorFormat);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture->textureId);
    glTexImage2D(GL_TEXTURE_2D, 0, pixelFormat, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelData);
    if (glGetError() != GL_NO_ERROR) {
        return GPUStatusUnknownError;
    }
    
    return GPUStatusOK;
}

GPUStatus gpuCreateTextureFromImage(uint32_t width, uint32_t height, GPUColorFormat colorFormat, uint8_t *pixelData, GPUTexture *texture)
{
    GPUStatus status = GPUStatusOK;
    
    status = gpuCreateTexture(texture);
    
    if (status == GPUStatusOK) {
        status = gpuUploadImageToTexture(width, height, colorFormat, pixelData, texture);
    }
    
    return status;
}

GPUStatus gpuCreateBlankTexture(uint32_t width, uint32_t height, GPUTexture *texture)
{
    uint8_t *dummyBuffer = malloc(width * height * 4);
    if (dummyBuffer == NULL) {
        return GPUStatusOutOfMemory;
    }
    memset(dummyBuffer, 0xff, width * height * 4);
    GPUStatus status = gpuCreateTextureFromImage(width, height, GPUColorFormatRGBA, dummyBuffer, texture);
    free(dummyBuffer);
    return status;
}

static GLenum gpuColorFormatToGLFormat(GPUColorFormat colorFormat)
{
    switch (colorFormat) {
        case GPUColorFormatRGB: return GL_RGB;
        case GPUColorFormatRGBA: return GL_RGBA;
        case GPUColorFormatBGRA: return GL_BGRA;
        default: return GL_RGBA;
    };
}

#pragma mark - Shader Program

GPUStatus gpuCompileProgram(const char *vertexShaderCode, const char *fragmentShaderCode, GPUProgram *program, void (*logFunc)(const char *log))
{
    memset(program, 0, sizeof(GPUProgram));
    
    // Create shader program.
    GLuint programId = glCreateProgram();
    
    // Create and compile vertex shader.
    GLuint vertexShader;
    int success = gpuCompileShader(&vertexShader, GL_VERTEX_SHADER, vertexShaderCode, logFunc);
    
    if (!success) {
        glDeleteProgram(programId);
        fprintf(stderr, "Failed to compile vertex shader.\n");
        return GPUStatusUnknownError;
    }
    
    // Create and compile fragment shader.
    GLuint fragmentShader;
    success = gpuCompileShader(&fragmentShader, GL_FRAGMENT_SHADER, fragmentShaderCode, logFunc);
    if (!success) {
        glDeleteShader(vertexShader);
        glDeleteProgram(programId);
        fprintf(stderr, "Failed to compile fragment shader.\n");
        return GPUStatusUnknownError;
    }
    
    // Attach vertex shader to program.
    glAttachShader(programId, vertexShader);
    
    // Attach fragment shader to program.
    glAttachShader(programId, fragmentShader);
    
    // Bind attribute locations.
    // This needs to be done prior to linking.
    glBindAttribLocation(programId, 0, "inputPosition");
    glBindAttribLocation(programId, 1, "inputUV");
    
    // Link program.
    success = gpuLinkProgram(programId);
    if (!success) {
        glDetachShader(programId, vertexShader);
        glDeleteShader(vertexShader);
        glDetachShader(programId, fragmentShader);
        glDeleteShader(fragmentShader);
        glDeleteProgram(programId);
        fprintf(stderr, "Failed to compile shader program.\n");
        return GPUStatusUnknownError;
    }
    
    // Release vertex and fragment shaders.
    glDetachShader(programId, vertexShader);
    glDeleteShader(vertexShader);
    glDetachShader(programId, fragmentShader);
    glDeleteShader(fragmentShader);
    
    program->programId = programId;
    program->textureUniformLocation = glGetUniformLocation(program->programId, "texture");
    program->additionalTextures[0].uniformLocation = glGetUniformLocation(program->programId, "texture2");
    program->additionalTextures[1].uniformLocation = glGetUniformLocation(program->programId, "texture3");
    program->additionalTextures[2].uniformLocation = glGetUniformLocation(program->programId, "texture4");
    program->additionalTextures[3].uniformLocation = glGetUniformLocation(program->programId, "texture5");
    program->additionalTextures[4].uniformLocation = glGetUniformLocation(program->programId, "texture6");
    program->additionalTextures[5].uniformLocation = glGetUniformLocation(program->programId, "texture7");
    program->additionalTextures[6].uniformLocation = glGetUniformLocation(program->programId, "texture8");
    
    program->valid = 1;
    
    return GPUStatusOK;
}

void gpuDestroyProgram(GPUProgram *program)
{
    if (program->valid) {
        program->valid = 0;
        glDeleteProgram(program->programId);
    }
}

static int gpuCompileShader(GLuint *shader, GLenum type, const char *sourceCode, void (*logFunc)(const char *log))
{
    GLint status;
    
    *shader = glCreateShader(type);
    glShaderSource(*shader, 1, &sourceCode, NULL);
    glCompileShader(*shader);
    
    glGetShaderiv(*shader, GL_COMPILE_STATUS, &status);
    if (status == 0) {
        GLint logLength;
        glGetShaderiv(*shader, GL_INFO_LOG_LENGTH, &logLength);
        if (logLength > 0) {
            GLchar *log = (GLchar *)malloc(logLength);
            glGetShaderInfoLog(*shader, logLength, &logLength, log);
            printf("Shader compile log:\n%s\n", log);
            if (logFunc != NULL) {
                logFunc(log);
            }
            free(log);
        }
        
        glDeleteShader(*shader);
        return 0;
    }
    
    return 1;
}

static int gpuLinkProgram(GLuint program)
{
    GLint status;
    glLinkProgram(program);
    
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status == 0) {
        GLint logLength;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        if (logLength > 0) {
            GLchar *log = (GLchar *)malloc(logLength);
            glGetProgramInfoLog(program, logLength, &logLength, log);
            printf("Program link log:\n%s\n", log);
            free(log);
        }
        
        return 0;
    }
    
    return 1;
}

void gpuSetSecondTextureForProgram(GPUTexture *texture, GPUProgram *program)
{
    program->additionalTextures[0].textureShouldBeUsed = 1;
    program->additionalTextures[0].texture = *texture;
}

void gpuSetThirdTextureForProgram(GPUTexture *texture, GPUProgram *program)
{
    program->additionalTextures[1].textureShouldBeUsed = 1;
    program->additionalTextures[1].texture = *texture;
}

void gpuSetFourthTextureForProgram(GPUTexture *texture, GPUProgram *program)
{
    program->additionalTextures[2].textureShouldBeUsed = 1;
    program->additionalTextures[2].texture = *texture;
}

void gpuSsetFifthTextureForProgram(GPUTexture *texture, GPUProgram *program)
{
    program->additionalTextures[3].textureShouldBeUsed = 1;
    program->additionalTextures[3].texture = *texture;
}

void gpuSetSixthTextureForProgram(GPUTexture *texture, GPUProgram *program)
{
    program->additionalTextures[4].textureShouldBeUsed = 1;
    program->additionalTextures[4].texture = *texture;
}

void gpuSetSeventhTextureForProgram(GPUTexture *texture, GPUProgram *program)
{
    program->additionalTextures[5].textureShouldBeUsed = 1;
    program->additionalTextures[5].texture = *texture;
}

void gpuSetEigthTextureForProgram(GPUTexture *texture, GPUProgram *program)
{
    program->additionalTextures[6].textureShouldBeUsed = 1;
    program->additionalTextures[6].texture = *texture;
}

GPUStatus gpuSetFloatForProgram(const char *name, float value, GPUProgram *program)
{
    if (!program->valid) {
        return GPUStatusInvalidProgram;
    }
    
    int32_t location = glGetUniformLocation(program->programId, name);
    if (location == -1) {
        return GPUStatusNoSuchParameter;
    }
    glUniform1f(location, value);
    return GPUStatusOK;
}

GPUStatus gpuSset2FloatsForProgram(const char *name, float value0, float value1, GPUProgram *program)
{
    if (!program->valid) {
        return GPUStatusInvalidProgram;
    }
    
    int32_t location = glGetUniformLocation(program->programId, name);
    if (location == -1) {
        return GPUStatusNoSuchParameter;
    }
    glUniform2f(location, value0, value1);
    return GPUStatusOK;
}

GPUStatus gpuSet3FloatsForProgram(const char *name, float value0, float value1, float value2, GPUProgram *program)
{
    if (!program->valid) {
        return GPUStatusInvalidProgram;
    }
    
    int32_t location = glGetUniformLocation(program->programId, name);
    if (location == -1) {
        return GPUStatusNoSuchParameter;
    }
    glUniform3f(location, value0, value1, value2);
    return GPUStatusOK;
}

GPUStatus gpuSet4FloatsForProgram(const char *name, float value0, float value1, float value2, float value3, GPUProgram *program)
{
    if (!program->valid) {
        return GPUStatusInvalidProgram;
    }
    
    int32_t location = glGetUniformLocation(program->programId, name);
    if (location == -1) {
        return GPUStatusNoSuchParameter;
    }
    glUniform4f(location, value0, value1, value2, value3);
    return GPUStatusOK;
}

GPUStatus gpuSetIntForProgram(const char *name, int32_t value, GPUProgram *program)
{
    if (!program->valid) {
        return GPUStatusInvalidProgram;
    }
    
    int32_t location = glGetUniformLocation(program->programId, name);
    if (location == -1) {
        return GPUStatusNoSuchParameter;
    }
    glUniform1i(location, value);
    return GPUStatusOK;
}

GPUStatus gpuSet2IntsForProgram(const char *name, int value0, int value1, GPUProgram *program)
{
    if (!program->valid) {
        return GPUStatusInvalidProgram;
    }
    
    int32_t location = glGetUniformLocation(program->programId, name);
    if (location == -1) {
        return GPUStatusNoSuchParameter;
    }
    glUniform2i(location, value0, value1);
    return GPUStatusOK;
}

GPUStatus gpuSet3IntsForProgram(const char *name, int value0, int value1, int value2, GPUProgram *program)
{
    if (!program->valid) {
        return GPUStatusInvalidProgram;
    }
    
    int32_t location = glGetUniformLocation(program->programId, name);
    if (location == -1) {
        return GPUStatusNoSuchParameter;
    }
    glUniform3i(location, value0, value1, value2);
    return GPUStatusOK;
}

GPUStatus gpuSet4IntsForProgram(const char *name, int value0, int value1, int value2, int value3, GPUProgram *program)
{
    if (!program->valid) {
        return GPUStatusInvalidProgram;
    }
    
    int32_t location = glGetUniformLocation(program->programId, name);
    if (location == -1) {
        return GPUStatusNoSuchParameter;
    }
    glUniform4i(location, value0, value1, value2, value3);
    return GPUStatusOK;
}

GPUStatus gpuSetVector2ForProgram(const char *name, const float *values, GPUProgram *program)
{
    if (!program->valid) {
        return GPUStatusInvalidProgram;
    }
    
    int32_t location = glGetUniformLocation(program->programId, name);
    if (location == -1) {
        return GPUStatusNoSuchParameter;
    }
    glUniform2fv(location, 1, values);
    return GPUStatusOK;
}

GPUStatus gpuSetVector3ForProgram(const char *name, const float *values, GPUProgram *program)
{
    if (!program->valid) {
        return GPUStatusInvalidProgram;
    }
    
    int32_t location = glGetUniformLocation(program->programId, name);
    if (location == -1) {
        return GPUStatusNoSuchParameter;
    }
    glUniform3fv(location, 1, values);
    return GPUStatusOK;
}

GPUStatus gpuSetVector4ForProgram(const char *name, const float *values, GPUProgram *program)
{
    if (!program->valid) {
        return GPUStatusInvalidProgram;
    }
    
    int32_t location = glGetUniformLocation(program->programId, name);
    if (location == -1) {
        return GPUStatusNoSuchParameter;
    }
    glUniform4fv(location, 1, values);
    return GPUStatusOK;
}

GPUStatus gpuSetFloatArrayForProgram(const char *name, uint32_t count, const float *values, GPUProgram *program)
{
    if (!program->valid) {
        return GPUStatusInvalidProgram;
    }
    
    int32_t location = glGetUniformLocation(program->programId, name);
    if (location == -1) {
        return GPUStatusNoSuchParameter;
    }
    glUniform1fv(location, count, values);
    return GPUStatusOK;
}

GPUStatus gpuSetVector2ArrayForProgram(const char *name, uint32_t count, const float *values, GPUProgram *program)
{
    if (!program->valid) {
        return GPUStatusInvalidProgram;
    }
    
    int32_t location = glGetUniformLocation(program->programId, name);
    if (location == -1) {
        return GPUStatusNoSuchParameter;
    }
    glUniform2fv(location, count, values);
    return GPUStatusOK;
}

GPUStatus gpuSetVector3ArrayForProgram(const char *name, uint32_t count, const float *values, GPUProgram *program)
{
    if (!program->valid) {
        return GPUStatusInvalidProgram;
    }
    
    int32_t location = glGetUniformLocation(program->programId, name);
    if (location == -1) {
        return GPUStatusNoSuchParameter;
    }
    glUniform3fv(location, count, values);
    return GPUStatusOK;
}

GPUStatus gpuSetVector4ArrayForProgram(const char *name, uint32_t count, const float *values, GPUProgram *program)
{
    if (!program->valid) {
        return GPUStatusInvalidProgram;
    }
    
    int32_t location = glGetUniformLocation(program->programId, name);
    if (location == -1) {
        return GPUStatusNoSuchParameter;
    }
    glUniform4fv(location, count, values);
    return GPUStatusOK;
}

GPUStatus gpuSetIntArrayForProgram(const char *name, uint32_t count, const int32_t *values, GPUProgram *program)
{
    if (!program->valid) {
        return GPUStatusInvalidProgram;
    }
    
    int32_t location = glGetUniformLocation(program->programId, name);
    if (location == -1) {
        return GPUStatusNoSuchParameter;
    }
    glUniform1iv(location, count, values);
    return GPUStatusOK;
}

GPUStatus gpuSetIntVector2ArrayForProgram(const char *name, uint32_t count, const int32_t *values, GPUProgram *program)
{
    if (!program->valid) {
        return GPUStatusInvalidProgram;
    }
    
    int32_t location = glGetUniformLocation(program->programId, name);
    if (location == -1) {
        return GPUStatusNoSuchParameter;
    }
    glUniform2iv(location, count, values);
    return GPUStatusOK;
}

GPUStatus gpuSetIntVector3ArrayForProgram(const char *name, uint32_t count, const int32_t *values, GPUProgram *program)
{
    if (!program->valid) {
        return GPUStatusInvalidProgram;
    }
    
    int32_t location = glGetUniformLocation(program->programId, name);
    if (location == -1) {
        return GPUStatusNoSuchParameter;
    }
    glUniform3iv(location, count, values);
    return GPUStatusOK;
}

GPUStatus gpuSetIntVector4ArrayForProgram(const char *name, uint32_t count, const int32_t *values, GPUProgram *program)
{
    if (!program->valid) {
        return GPUStatusInvalidProgram;
    }
    
    int32_t location = glGetUniformLocation(program->programId, name);
    if (location == -1) {
        return GPUStatusNoSuchParameter;
    }
    glUniform4iv(location, count, values);
    return GPUStatusOK;
}

GPUStatus gpuSsetMatrix2x2ForProgram(const char *name, const float *values, GPUProgram *program)
{
    if (!program->valid) {
        return GPUStatusInvalidProgram;
    }
    
    int32_t location = glGetUniformLocation(program->programId, name);
    if (location == -1) {
        return GPUStatusNoSuchParameter;
    }
    glUniformMatrix2fv(location, 1, GL_FALSE, values);
    return GPUStatusOK;
}

GPUStatus gpuSetMatrix3x3ForProgram(const char *name, const float *values, GPUProgram *program)
{
    if (!program->valid) {
        return GPUStatusInvalidProgram;
    }
    
    int32_t location = glGetUniformLocation(program->programId, name);
    if (location == -1) {
        return GPUStatusNoSuchParameter;
    }
    glUniformMatrix3fv(location, 1, GL_FALSE, values);
    return GPUStatusOK;
}

GPUStatus gpuSetMatrix4x4ForProgram(const char *name, const float *values, GPUProgram *program)
{
    if (!program->valid) {
        return GPUStatusInvalidProgram;
    }
    
    int32_t location = glGetUniformLocation(program->programId, name);
    if (location == -1) {
        return GPUStatusNoSuchParameter;
    }
    glUniformMatrix4fv(location, 1, GL_FALSE, values);
    return GPUStatusOK;
}

GPUStatus gpuSetMatrix2x2ArrayForProgram(const char *name, uint32_t count, const float *values, GPUProgram *program)
{
    if (!program->valid) {
        return GPUStatusInvalidProgram;
    }
    
    int32_t location = glGetUniformLocation(program->programId, name);
    if (location == -1) {
        return GPUStatusNoSuchParameter;
    }
    glUniformMatrix2fv(location, count, GL_FALSE, values);
    return GPUStatusOK;
}

GPUStatus gpuSetMatrix3x3ArrayForProgram(const char *name, uint32_t count, const float *values, GPUProgram *program)
{
    if (!program->valid) {
        return GPUStatusInvalidProgram;
    }
    
    int32_t location = glGetUniformLocation(program->programId, name);
    if (location == -1) {
        return GPUStatusNoSuchParameter;
    }
    glUniformMatrix3fv(location, count, GL_FALSE, values);
    return GPUStatusOK;
}

GPUStatus gpuSetMatrix4x4ArrayForProgram(const char *name, uint32_t count, const float *values, GPUProgram *program)
{
    if (!program->valid) {
        return GPUStatusInvalidProgram;
    }
    
    int32_t location = glGetUniformLocation(program->programId, name);
    if (location == -1) {
        return GPUStatusNoSuchParameter;
    }
    glUniformMatrix4fv(location, count, GL_FALSE, values);
    return GPUStatusOK;
}

