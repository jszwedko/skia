
/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <GLES/gl.h> 

#include "GLWindowContext_android.h"

#include <android/native_window_jni.h>

namespace sk_app {

// Most of the following 3 functions (GLWindowContext::Create, constructor, desctructor)
// are copied from Unix/Win platform with unix/win changed to android

// platform-dependent create
GLWindowContext* GLWindowContext::Create(void* platformData, const DisplayParams& params) {
    GLWindowContext_android* ctx = new GLWindowContext_android(platformData, params);
    if (!ctx->isValid()) {
        delete ctx;
        return nullptr;
    }
    return ctx;
}

GLWindowContext_android::GLWindowContext_android(void* platformData, const DisplayParams& params)
    : GLWindowContext(platformData, params)
    , fDisplay(EGL_NO_DISPLAY)
    , fEGLContext(EGL_NO_CONTEXT)
    , fSurface(EGL_NO_SURFACE) {

    // any config code here (particularly for msaa)?

    this->initializeContext(platformData, params);
}

GLWindowContext_android::~GLWindowContext_android() {
    this->destroyContext();
}

void GLWindowContext_android::onInitializeContext(void* platformData, const DisplayParams& params) {
    ContextPlatformData_android* androidPlatformData = 
            reinterpret_cast<ContextPlatformData_android*>(platformData);

    fWidth = ANativeWindow_getWidth(androidPlatformData->fNativeWindow);
    fHeight = ANativeWindow_getHeight(androidPlatformData->fNativeWindow);

    fDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    EGLint majorVersion;
    EGLint minorVersion;
    eglInitialize(fDisplay, &majorVersion, &minorVersion);

    SkAssertResult(eglBindAPI(EGL_OPENGL_ES_API));

    EGLint numConfigs = 0;
    const EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };

    EGLConfig surfaceConfig;
    SkAssertResult(eglChooseConfig(fDisplay, configAttribs, &surfaceConfig, 1, &numConfigs));
    SkASSERT(numConfigs > 0);

    static const EGLint kEGLContextAttribsForOpenGLES[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    fEGLContext = eglCreateContext(
            fDisplay, surfaceConfig, nullptr, kEGLContextAttribsForOpenGLES);
    SkASSERT(EGL_NO_CONTEXT != fEGLContext);

    fSurface = eglCreateWindowSurface(
            fDisplay, surfaceConfig, androidPlatformData->fNativeWindow, nullptr);
    SkASSERT(EGL_NO_SURFACE != fSurface);

    SkAssertResult(eglMakeCurrent(fDisplay, fSurface, fSurface, fEGLContext));
    // GLWindowContext::initializeContext will call GrGLCreateNativeInterface so we
    // won't call it here.

    glClearStencil(0);
    glClearColor(0, 0, 0, 0);
    glStencilMask(0xffffffff);
    glClear(GL_STENCIL_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

    int redBits, greenBits, blueBits;
    eglGetConfigAttrib(fDisplay, surfaceConfig, EGL_RED_SIZE, &redBits);
    eglGetConfigAttrib(fDisplay, surfaceConfig, EGL_GREEN_SIZE, &greenBits);
    eglGetConfigAttrib(fDisplay, surfaceConfig, EGL_BLUE_SIZE, &blueBits);
    fColorBits = redBits + greenBits + blueBits;
    eglGetConfigAttrib(fDisplay, surfaceConfig, EGL_STENCIL_SIZE, &fStencilBits);
    eglGetConfigAttrib(fDisplay, surfaceConfig, EGL_SAMPLES, &fSampleCount);
}

void GLWindowContext_android::onDestroyContext() {
    if (!fDisplay || !fEGLContext || !fSurface) {
        return;
    }
    eglMakeCurrent(fDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    SkAssertResult(eglDestroySurface(fDisplay, fSurface));
    SkAssertResult(eglDestroyContext(fDisplay, fEGLContext));
    fEGLContext = EGL_NO_CONTEXT;
    fSurface = EGL_NO_SURFACE;
}

void GLWindowContext_android::onSwapBuffers() {
    if (fDisplay && fEGLContext && fSurface) {
        eglSwapBuffers(fDisplay, fSurface);
    }
}

}
