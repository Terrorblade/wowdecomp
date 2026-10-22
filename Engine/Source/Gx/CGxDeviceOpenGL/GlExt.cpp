#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "Engine/Source/Gx/CGxDeviceOpenGL/GlExt.h"
#include "Engine/Source/Storm/H/storm.h"

#if defined(WOWDECOMP_PLATFORM_POSIX)
#include <SDL.h>

// Entry points come from the SDL GL context (GLX/EGL) instead of WGL.
#define wglGetProcAddress(name) SDL_GL_GetProcAddress(name)
#endif

#if defined(_WIN32)
// STATIC: 0x00C6061C
static const char* s_wglExtensions;
#endif

// STATIC: 0x00C60620
static const char* s_glExtensions;

// STATIC: 0x00C6044C
int32_t glExtGPUProgramParameters;

// STATIC: 0x00C60450
PFNGLPROGRAMLOCALPARAMETERS4FVEXTPROC glProgramLocalParameters4fvEXT;

// STATIC: 0x00C60454
PFNGLPROGRAMENVPARAMETERS4FVEXTPROC glProgramEnvParameters4fvEXT;

// STATIC: 0x00C60458
int32_t wglExtSwapControl;

// STATIC: 0x00C6045C
int32_t wglArbPbuffer;

// STATIC: 0x00C60460
int32_t wglArbPixelFormat;

// STATIC: 0x00C60464
int32_t glATIEnvmapBumpmap;

// STATIC: 0x00C60468
int32_t glATIFragmentShader;

// STATIC: 0x00C6046C
int32_t glNVMultisampleFilterHint;

// STATIC: 0x00C6047C
int32_t glNVTextureShader3;

// STATIC: 0x00C60480
int32_t glNVTextureShader2;

// STATIC: 0x00C60484
int32_t glNVTextureShader;

// STATIC: 0x00C60488
int32_t glNVRegisterCombiners2;

// STATIC: 0x00C6048C
int32_t glNVRegisterCombiners;

// STATIC: 0x00C60498
int32_t glUnkC60498;

// STATIC: 0x00C6049C
int32_t glSGISGenerateMipmap;

// STATIC: 0x00C604A0
int32_t glExtBlendColor;

// STATIC: 0x00C604A4
int32_t glExtFramebufferMultisample;

// STATIC: 0x00C604A8
int32_t glExtFramebufferObject;

// STATIC: 0x00C604AC
int32_t glExtDrawRangeElements;

// STATIC: 0x00C604B0
int32_t glExtTextureFilterAnisotropic;

// STATIC: 0x00C604B4
int32_t glExtTextureCompressionS3tc;

// STATIC: 0x00C604B8
int32_t glExtTextureCompression;

// STATIC: 0x00C604BC
int32_t glExtTextureLodBias;

// STATIC: 0x00C604C0
int32_t glExtClampToEdge;

// STATIC: 0x00C604C4
int32_t glExtBgra;

// STATIC: 0x00C604C8
int32_t glARBPointSprite;

// STATIC: 0x00C604CC
int32_t glARBPointParameters;

// STATIC: 0x00C604D0
int32_t glARBOcclusionQuery;

// STATIC: 0x00C604D4
int32_t glARBMultisample;

// STATIC: 0x00C604D8
int32_t glARBTextureCubeMap;

// STATIC: 0x00C604DC
int32_t glARBTextureRectangle;

// STATIC: 0x00C604E0
int32_t glARBTextureEnvCombine;

// STATIC: 0x00C604E4
int32_t glARBVertexBufferObject;

// STATIC: 0x00C604E8
int32_t glARBFragmentProgram;

// STATIC: 0x00C604EC
int32_t glARBVertexProgram;

#if defined(_WIN32)
// STATIC: 0x00C604F0
PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;

// STATIC: 0x00C604F4
PFNWGLQUERYPBUFFERARBPROC wglQueryPbufferARB;

// STATIC: 0x00C604F8
PFNWGLDESTROYPBUFFERARBPROC wglDestroyPbufferARB;

// STATIC: 0x00C604FC
PFNWGLRELEASEPBUFFERDCARBPROC wglReleasePbufferDCARB;

// STATIC: 0x00C60500
PFNWGLGETPBUFFERDCARBPROC wglGetPbufferDCARB;

// STATIC: 0x00C60504
PFNWGLCREATEPBUFFERARBPROC wglCreatePbufferARB;

// STATIC: 0x00C60508
PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB;

// STATIC: 0x00C60510
PFNWGLGETPIXELFORMATATTRIBIVARBPROC wglGetPixelFormatAttribivARB;

// STATIC: 0x00C60514
PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB;
#endif

// STATIC: 0x00C60518
PFNGLBLENDCOLOREXTPROC glBlendColorEXT;

// STATIC: 0x00C6051C
PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC glFramebufferRenderbufferEXT;

// STATIC: 0x00C60520
PFNGLFRAMEBUFFERTEXTURE3DEXTPROC glFramebufferTexture3DEXT;

// STATIC: 0x00C60524
PFNGLFRAMEBUFFERTEXTURE2DEXTPROC glFramebufferTexture2DEXT;

// STATIC: 0x00C60528
PFNGLFRAMEBUFFERTEXTURE1DEXTPROC glFramebufferTexture1DEXT;

// STATIC: 0x00C6052C
PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC glCheckFramebufferStatusEXT;

// STATIC: 0x00C60530
PFNGLGENFRAMEBUFFERSEXTPROC glGenFramebuffersEXT;

// STATIC: 0x00C60534
PFNGLDELETEFRAMEBUFFERSEXTPROC glDeleteFramebuffersEXT;

// STATIC: 0x00C60538
PFNGLBINDFRAMEBUFFEREXTPROC glBindFramebufferEXT;

// STATIC: 0x00C6053C
PFNGLRENDERBUFFERSTORAGEEXTPROC glRenderbufferStorageEXT;

// STATIC: 0x00C60540
PFNGLGENRENDERBUFFERSEXTPROC glGenRenderbuffersEXT;

// STATIC: 0x00C60544
PFNGLDELETERENDERBUFFERSEXTPROC glDeleteRenderbuffersEXT;

// STATIC: 0x00C60548
PFNGLBINDRENDERBUFFEREXTPROC glBindRenderbufferEXT;

// STATIC: 0x00C6054C
PFNGLPOINTPARAMETERFVARBPROC glPointParameterfvARB;

// STATIC: 0x00C60550
PFNGLPOINTPARAMETERFARBPROC glPointParameterfARB;

// STATIC: 0x00C60554
PFNGLGETQUERYOBJECTUIVARBPROC glGetQueryObjectuivARB;

// STATIC: 0x00C60558
PFNGLGETQUERYOBJECTIVARBPROC glGetQueryObjectivARB;

// STATIC: 0x00C6055C
PFNGLGETQUERYIVARBPROC glGetQueryivARB;

// STATIC: 0x00C60560
PFNGLENDQUERYARBPROC glEndQueryARB;

// STATIC: 0x00C60564
PFNGLBEGINQUERYARBPROC glBeginQueryARB;

// STATIC: 0x00C60568
PFNGLDELETEQUERIESARBPROC glDeleteQueriesARB;

// STATIC: 0x00C6056C
PFNGLGENQUERIESARBPROC glGenQueriesARB;

// STATIC: 0x00C60570
PFNGLUNMAPBUFFERARBPROC glUnmapBufferARB;

// STATIC: 0x00C60574
PFNGLMAPBUFFERARBPROC glMapBufferARB;

// STATIC: 0x00C60578
PFNGLBUFFERSUBDATAARBPROC glBufferSubDataARB;

// STATIC: 0x00C6057C
PFNGLBUFFERDATAARBPROC glBufferDataARB;

// STATIC: 0x00C60580
PFNGLGENBUFFERSARBPROC glGenBuffersARB;

// STATIC: 0x00C60584
PFNGLDELETEBUFFERSARBPROC glDeleteBuffersARB;

// STATIC: 0x00C60588
PFNGLBINDBUFFERARBPROC glBindBufferARB;

// STATIC: 0x00C60598
PFNGLGENPROGRAMSNVPROC glGenProgramsNV;

// STATIC: 0x00C6059C
PFNGLDELETEPROGRAMSNVPROC glDeleteProgramsNV;

// STATIC: 0x00C605A0
PFNGLPROGRAMPARAMETERS4FVNVPROC glProgramParameters4fvNV;

// STATIC: 0x00C605A4
PFNGLBINDPROGRAMNVPROC glBindProgramNV;

// STATIC: 0x00C605A8
PFNGLLOADPROGRAMNVPROC glLoadProgramNV;

// STATIC: 0x00C605AC
PFNGLDISABLEVERTEXATTRIBARRAYARBPROC glDisableVertexAttribArrayARB;

// STATIC: 0x00C605B0
PFNGLENABLEVERTEXATTRIBARRAYARBPROC glEnableVertexAttribArrayARB;

// STATIC: 0x00C605B4
PFNGLVERTEXATTRIBPOINTERARBPROC glVertexAttribPointerARB;

// STATIC: 0x00C605B8
PFNGLGETPROGRAMIVARBPROC glGetProgramivARB;

// STATIC: 0x00C605BC
PFNGLISPROGRAMARBPROC glIsProgramARB;

// STATIC: 0x00C605C0
PFNGLPROGRAMLOCALPARAMETER4FVARBPROC glProgramLocalParameter4fvARB;

// STATIC: 0x00C605C4
PFNGLPROGRAMENVPARAMETER4FVARBPROC glProgramEnvParameter4fvARB;

// STATIC: 0x00C605C8
PFNGLGENPROGRAMSARBPROC glGenProgramsARB;

// STATIC: 0x00C605CC
PFNGLDELETEPROGRAMSARBPROC glDeleteProgramsARB;

// STATIC: 0x00C605D0
PFNGLBINDPROGRAMARBPROC glBindProgramARB;

// STATIC: 0x00C605D4
PFNGLPROGRAMSTRINGARBPROC glProgramStringARB;

// STATIC: 0x00C605D8
PFNGLCOMBINERPARAMETERINVPROC glCombinerParameteriNV;

// STATIC: 0x00C605DC
PFNGLCOMBINERPARAMETERFVNVPROC glCombinerParameterfvNV;

// STATIC: 0x00C605E0
PFNGLFINALCOMBINERINPUTNVPROC glFinalCombinerInputNV;

// STATIC: 0x00C605E4
PFNGLCOMBINERSTAGEPARAMETERFVNVPROC glCombinerStageParameterfvNV;

// STATIC: 0x00C605E8
PFNGLCOMBINEROUTPUTNVPROC glCombinerOutputNV;

// STATIC: 0x00C605EC
PFNGLCOMBINERINPUTNVPROC glCombinerInputNV;

// STATIC: 0x00C605F0
PFNGLDRAWRANGEELEMENTSEXTPROC glDrawRangeElementsEXT;

// STATIC: 0x00C605F4
PFNGLCOMPRESSEDTEXSUBIMAGE2DARBPROC glCompressedTexSubImage2DARB;

// STATIC: 0x00C605F8
PFNGLCOMPRESSEDTEXIMAGE2DARBPROC glCompressedTexImage2DARB;

// STATIC: 0x00C605FC
PFNGLCLIENTACTIVETEXTUREARBPROC glClientActiveTextureARB;

// STATIC: 0x00C60600
PFNGLACTIVETEXTUREARBPROC glActiveTextureARB;

// STATIC: 0x00C60604
PFNGLUNLOCKARRAYSEXTPROC glUnlockArraysEXT;

// STATIC: 0x00C60608
PFNGLLOCKARRAYSEXTPROC glLockArraysEXT;

// STATIC: 0x00C6060C
GLint glMaxTextureImageUnitsARB;

// STATIC: 0x00C60610
GLint glMaxTextureUnits;

// STATIC: 0x00C60614
int32_t glExtCVA;

// STATIC: 0x00C60618
uint32_t glVersion;

// VERIFY: d2e39dac auto
// OFFSET: 0x006908A0
void UnbindGlExtensions() {
    s_glExtensions = nullptr;
#if defined(_WIN32)
    s_wglExtensions = nullptr;
#endif
}

// VERIFY: f55df59e auto
// OFFSET: 0x006908B0
static bool ScanString(const char* extensions, const char* name) {
    size_t length = strlen(name);

    if (!*extensions) {
        return false;
    }

    while (SStrCmpI(extensions, name, length)) {
        while (*extensions && *extensions != ' ') {
            extensions++;
        }

        while (*extensions == ' ') {
            extensions++;
        }

        if (!*extensions) {
            return false;
        }
    }

    return true;
}

// VERIFY: 8898dd32 auto
// OFFSET: 0x00690910
bool FindGlExt(const char* name) {
    const char* value = getenv(name);

    if (value && !SStrToInt(value)) {
        return false;
    }

    if (!s_glExtensions) {
        s_glExtensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
    }

    return ScanString(s_glExtensions, name);
}

#if defined(_WIN32)
// VERIFY: 5f60079f auto
// OFFSET: 0x00690960
bool HasWGLExtension(const char* name) {
    if (!wglGetExtensionsStringARB) {
        return false;
    }

    if (!s_wglExtensions) {
        s_wglExtensions = wglGetExtensionsStringARB(wglGetCurrentDC());
    }

    return ScanString(s_wglExtensions, name);
}
#else
// There is no WGL; the window system extensions the device uses are covered by SDL.
bool HasWGLExtension(const char* name) {
    return false;
}
#endif

// VERIFY: df2e8e3f auto
// OFFSET: 0x006909A0
void BindGlExtensions() {
    int32_t major = -1;
    int32_t minor = -1;
    sscanf(reinterpret_cast<const char*>(glGetString(GL_VERSION)), "%d.%d", &major, &minor);
    glVersion = minor | (major << 16);

    glExtCVA = FindGlExt("GL_EXT_compiled_vertex_array");

    if (glExtCVA) {
        glLockArraysEXT = reinterpret_cast<PFNGLLOCKARRAYSEXTPROC>(wglGetProcAddress("glLockArraysEXT"));
        glUnlockArraysEXT = reinterpret_cast<PFNGLUNLOCKARRAYSEXTPROC>(wglGetProcAddress("glUnlockArraysEXT"));
    }

    if (FindGlExt("GL_ARB_multitexture")) {
        glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &glMaxTextureUnits);
        glActiveTextureARB = reinterpret_cast<PFNGLACTIVETEXTUREARBPROC>(wglGetProcAddress("glActiveTextureARB"));
        glClientActiveTextureARB = reinterpret_cast<PFNGLCLIENTACTIVETEXTUREARBPROC>(wglGetProcAddress("glClientActiveTextureARB"));
    } else {
        glMaxTextureUnits = 1;
    }

    glExtBgra = FindGlExt("GL_EXT_bgra");

    if (FindGlExt("GL_SGIS_texture_edge_clamp") || FindGlExt("GL_EXT_texture_edge_clamp")) {
        glExtClampToEdge = 1;
    } else {
        glExtClampToEdge = 0;
    }

    glExtTextureLodBias = FindGlExt("GL_EXT_texture_lod_bias");

    const char* compressedTexImage2D = nullptr;

    if (glExtTextureLodBias) {
        GLfloat maxLodBias = 0.0f;
        glGetFloatv(GL_MAX_TEXTURE_LOD_BIAS_EXT, &maxLodBias);

        if (maxLodBias == 0.0f) {
            glExtTextureLodBias = 0;
        }
    }

    const char* compressedTexSubImage2D = nullptr;
    glExtTextureCompression = 0;

    if (FindGlExt("GL_ARB_texture_compression")) {
        compressedTexImage2D = "glCompressedTexImage2DARB";
        compressedTexSubImage2D = "glCompressedTexSubImage2DARB";
        glExtTextureCompression = 1;
    } else if (FindGlExt("GL_EXT_texture_compression")) {
        compressedTexImage2D = "glCompressedTexImage2DEXT";
        compressedTexSubImage2D = "glCompressedTexSubImage2DEXT";
        glExtTextureCompression = 1;
    }

    glExtTextureCompressionS3tc = FindGlExt("GL_EXT_texture_compression_s3tc");

    if (glExtTextureCompression && glExtTextureCompressionS3tc) {
        glCompressedTexImage2DARB = reinterpret_cast<PFNGLCOMPRESSEDTEXIMAGE2DARBPROC>(wglGetProcAddress(compressedTexImage2D));
        glCompressedTexSubImage2DARB = reinterpret_cast<PFNGLCOMPRESSEDTEXSUBIMAGE2DARBPROC>(wglGetProcAddress(compressedTexSubImage2D));
    }

    if (!glCompressedTexImage2DARB || !glCompressedTexSubImage2DARB) {
        glExtTextureCompression = 0;
        glExtTextureCompressionS3tc = 0;
    }

    glSGISGenerateMipmap = FindGlExt("GL_SGIS_generate_mipmap");
    glExtTextureFilterAnisotropic = FindGlExt("GL_EXT_texture_filter_anisotropic");
    glARBVertexBufferObject = FindGlExt("GL_ARB_vertex_buffer_object");

    if (glARBVertexBufferObject) {
        glBindBufferARB = reinterpret_cast<PFNGLBINDBUFFERARBPROC>(wglGetProcAddress("glBindBufferARB"));
        glDeleteBuffersARB = reinterpret_cast<PFNGLDELETEBUFFERSARBPROC>(wglGetProcAddress("glDeleteBuffersARB"));
        glGenBuffersARB = reinterpret_cast<PFNGLGENBUFFERSARBPROC>(wglGetProcAddress("glGenBuffersARB"));
        glBufferDataARB = reinterpret_cast<PFNGLBUFFERDATAARBPROC>(wglGetProcAddress("glBufferDataARB"));
        glBufferSubDataARB = reinterpret_cast<PFNGLBUFFERSUBDATAARBPROC>(wglGetProcAddress("glBufferSubDataARB"));
        glMapBufferARB = reinterpret_cast<PFNGLMAPBUFFERARBPROC>(wglGetProcAddress("glMapBufferARB"));
        glUnmapBufferARB = reinterpret_cast<PFNGLUNMAPBUFFERARBPROC>(wglGetProcAddress("glUnmapBufferARB"));
    }

    glExtDrawRangeElements = FindGlExt("GL_EXT_draw_range_elements");

    if (glExtDrawRangeElements) {
        glDrawRangeElementsEXT = reinterpret_cast<PFNGLDRAWRANGEELEMENTSEXTPROC>(wglGetProcAddress("glDrawRangeElementsEXT"));

        GLint maxVertices;
        GLint maxIndices;
        glGetIntegerv(GL_MAX_ELEMENTS_VERTICES_EXT, &maxVertices);
        glGetIntegerv(GL_MAX_ELEMENTS_INDICES_EXT, &maxIndices);
    }

    glUnkC60498 = 0;
    glARBTextureEnvCombine = FindGlExt("GL_ARB_texture_env_combine");
    glNVRegisterCombiners = FindGlExt("GL_NV_register_combiners");

    if (glNVRegisterCombiners) {
        glCombinerInputNV = reinterpret_cast<PFNGLCOMBINERINPUTNVPROC>(wglGetProcAddress("glCombinerInputNV"));
        glCombinerOutputNV = reinterpret_cast<PFNGLCOMBINEROUTPUTNVPROC>(wglGetProcAddress("glCombinerOutputNV"));
        glFinalCombinerInputNV = reinterpret_cast<PFNGLFINALCOMBINERINPUTNVPROC>(wglGetProcAddress("glFinalCombinerInputNV"));
        glCombinerParameterfvNV = reinterpret_cast<PFNGLCOMBINERPARAMETERFVNVPROC>(wglGetProcAddress("glCombinerParameterfvNV"));
        glCombinerParameteriNV = reinterpret_cast<PFNGLCOMBINERPARAMETERINVPROC>(wglGetProcAddress("glCombinerParameteriNV"));
    }

    glNVRegisterCombiners2 = FindGlExt("GL_NV_register_combiners2");

    if (glNVRegisterCombiners2) {
        glCombinerStageParameterfvNV = reinterpret_cast<PFNGLCOMBINERSTAGEPARAMETERFVNVPROC>(wglGetProcAddress("glCombinerStageParameterfvNV"));
    }

    glNVTextureShader = FindGlExt("GL_NV_texture_shader");
    glNVTextureShader2 = FindGlExt("GL_NV_texture_shader2");
    glNVTextureShader3 = FindGlExt("GL_NV_texture_shader3");
    glATIFragmentShader = FindGlExt("GL_ATI_fragment_shader");
    glATIEnvmapBumpmap = FindGlExt("GL_ATI_envmap_bumpmap");
    glARBFragmentProgram = FindGlExt("GL_ARB_fragment_program");

    if (glARBFragmentProgram) {
        glProgramStringARB = reinterpret_cast<PFNGLPROGRAMSTRINGARBPROC>(wglGetProcAddress("glProgramStringARB"));
        glBindProgramARB = reinterpret_cast<PFNGLBINDPROGRAMARBPROC>(wglGetProcAddress("glBindProgramARB"));
        glProgramLocalParameter4fvARB = reinterpret_cast<PFNGLPROGRAMLOCALPARAMETER4FVARBPROC>(wglGetProcAddress("glProgramLocalParameter4fvARB"));
        glDeleteProgramsARB = reinterpret_cast<PFNGLDELETEPROGRAMSARBPROC>(wglGetProcAddress("glDeleteProgramsARB"));
        glGenProgramsARB = reinterpret_cast<PFNGLGENPROGRAMSARBPROC>(wglGetProcAddress("glGenProgramsARB"));
        glIsProgramARB = reinterpret_cast<PFNGLISPROGRAMARBPROC>(wglGetProcAddress("glIsProgramARB"));
        glGetProgramivARB = reinterpret_cast<PFNGLGETPROGRAMIVARBPROC>(wglGetProcAddress("glGetProgramivARB"));
        glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS_ARB, &glMaxTextureImageUnitsARB);
    }

    glARBVertexProgram = FindGlExt("GL_ARB_vertex_program");

    if (glARBVertexProgram) {
        glVertexAttribPointerARB = reinterpret_cast<PFNGLVERTEXATTRIBPOINTERARBPROC>(wglGetProcAddress("glVertexAttribPointerARB"));
        glEnableVertexAttribArrayARB = reinterpret_cast<PFNGLENABLEVERTEXATTRIBARRAYARBPROC>(wglGetProcAddress("glEnableVertexAttribArrayARB"));
        glDisableVertexAttribArrayARB = reinterpret_cast<PFNGLDISABLEVERTEXATTRIBARRAYARBPROC>(wglGetProcAddress("glDisableVertexAttribArrayARB"));
    }

    if (glARBVertexProgram || glARBFragmentProgram) {
        glProgramStringARB = reinterpret_cast<PFNGLPROGRAMSTRINGARBPROC>(wglGetProcAddress("glProgramStringARB"));
        glBindProgramARB = reinterpret_cast<PFNGLBINDPROGRAMARBPROC>(wglGetProcAddress("glBindProgramARB"));
        glProgramEnvParameter4fvARB = reinterpret_cast<PFNGLPROGRAMENVPARAMETER4FVARBPROC>(wglGetProcAddress("glProgramEnvParameter4fvARB"));
        glProgramLocalParameter4fvARB = reinterpret_cast<PFNGLPROGRAMLOCALPARAMETER4FVARBPROC>(wglGetProcAddress("glProgramLocalParameter4fvARB"));
        glDeleteProgramsARB = reinterpret_cast<PFNGLDELETEPROGRAMSARBPROC>(wglGetProcAddress("glDeleteProgramsARB"));
        glGenProgramsARB = reinterpret_cast<PFNGLGENPROGRAMSARBPROC>(wglGetProcAddress("glGenProgramsARB"));
        glIsProgramARB = reinterpret_cast<PFNGLISPROGRAMARBPROC>(wglGetProcAddress("glIsProgramARB"));
        glGetProgramivARB = reinterpret_cast<PFNGLGETPROGRAMIVARBPROC>(wglGetProcAddress("glGetProgramivARB"));
        glExtGPUProgramParameters = FindGlExt("GL_EXT_gpu_program_parameters");

        if (glExtGPUProgramParameters) {
            glProgramEnvParameters4fvEXT = reinterpret_cast<PFNGLPROGRAMENVPARAMETERS4FVEXTPROC>(wglGetProcAddress("glProgramEnvParameters4fvEXT"));
            glProgramLocalParameters4fvEXT = reinterpret_cast<PFNGLPROGRAMLOCALPARAMETERS4FVEXTPROC>(wglGetProcAddress("glProgramLocalParameters4fvEXT"));
        }
    }

    if (glVersion >= 0x10003 || FindGlExt("GL_ARB_texture_cube_map")) {
        glARBTextureCubeMap = 1;
    } else {
        glARBTextureCubeMap = 0;
    }

    glARBMultisample = FindGlExt("GL_ARB_multisample");

    if (FindGlExt("GL_ARB_texture_rectangle") || FindGlExt("GL_EXT_texture_rectangle")) {
        glARBTextureRectangle = 1;
    } else {
        bool found = FindGlExt("GL_NV_texture_rectangle");
        glARBTextureRectangle = 0;

        if (found) {
            glARBTextureRectangle = 1;
        }
    }

    if (glVersion >= 0x10005 || FindGlExt("GL_ARB_occlusion_query")) {
        glARBOcclusionQuery = 1;
        glGenQueriesARB = reinterpret_cast<PFNGLGENQUERIESARBPROC>(wglGetProcAddress("glGenQueriesARB"));
        glDeleteQueriesARB = reinterpret_cast<PFNGLDELETEQUERIESARBPROC>(wglGetProcAddress("glDeleteQueriesARB"));
        glBeginQueryARB = reinterpret_cast<PFNGLBEGINQUERYARBPROC>(wglGetProcAddress("glBeginQueryARB"));
        glEndQueryARB = reinterpret_cast<PFNGLENDQUERYARBPROC>(wglGetProcAddress("glEndQueryARB"));
        glGetQueryivARB = reinterpret_cast<PFNGLGETQUERYIVARBPROC>(wglGetProcAddress("glGetQueryivARB"));
        glGetQueryObjectivARB = reinterpret_cast<PFNGLGETQUERYOBJECTIVARBPROC>(wglGetProcAddress("glGetQueryObjectivARB"));
        glGetQueryObjectuivARB = reinterpret_cast<PFNGLGETQUERYOBJECTUIVARBPROC>(wglGetProcAddress("glGetQueryObjectuivARB"));
    } else {
        glARBOcclusionQuery = 0;
    }

    if (glVersion >= 0x10004 || FindGlExt("GL_ARB_point_parameters")) {
        glARBPointParameters = 1;
        glPointParameterfARB = reinterpret_cast<PFNGLPOINTPARAMETERFARBPROC>(wglGetProcAddress("glPointParameterfARB"));
        glPointParameterfvARB = reinterpret_cast<PFNGLPOINTPARAMETERFVARBPROC>(wglGetProcAddress("glPointParameterfvARB"));
    } else {
        glARBPointParameters = 0;
    }

    if (glVersion >= 0x20000 || FindGlExt("GL_ARB_point_sprite")) {
        glARBPointSprite = 1;
    } else {
        glARBPointSprite = 0;
    }

    glExtFramebufferObject = FindGlExt("GL_EXT_framebuffer_object");
    glExtFramebufferMultisample = FindGlExt("GL_EXT_framebuffer_multisample");

    if (glExtFramebufferObject) {
        glBindRenderbufferEXT = reinterpret_cast<PFNGLBINDRENDERBUFFEREXTPROC>(wglGetProcAddress("glBindRenderbufferEXT"));
        glDeleteRenderbuffersEXT = reinterpret_cast<PFNGLDELETERENDERBUFFERSEXTPROC>(wglGetProcAddress("glDeleteRenderbuffersEXT"));
        glGenRenderbuffersEXT = reinterpret_cast<PFNGLGENRENDERBUFFERSEXTPROC>(wglGetProcAddress("glGenRenderbuffersEXT"));
        glRenderbufferStorageEXT = reinterpret_cast<PFNGLRENDERBUFFERSTORAGEEXTPROC>(wglGetProcAddress("glRenderbufferStorageEXT"));
        glBindFramebufferEXT = reinterpret_cast<PFNGLBINDFRAMEBUFFEREXTPROC>(wglGetProcAddress("glBindFramebufferEXT"));
        glDeleteFramebuffersEXT = reinterpret_cast<PFNGLDELETEFRAMEBUFFERSEXTPROC>(wglGetProcAddress("glDeleteFramebuffersEXT"));
        glGenFramebuffersEXT = reinterpret_cast<PFNGLGENFRAMEBUFFERSEXTPROC>(wglGetProcAddress("glGenFramebuffersEXT"));
        glCheckFramebufferStatusEXT = reinterpret_cast<PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC>(wglGetProcAddress("glCheckFramebufferStatusEXT"));
        glFramebufferTexture1DEXT = reinterpret_cast<PFNGLFRAMEBUFFERTEXTURE1DEXTPROC>(wglGetProcAddress("glFramebufferTexture1DEXT"));
        glFramebufferTexture2DEXT = reinterpret_cast<PFNGLFRAMEBUFFERTEXTURE2DEXTPROC>(wglGetProcAddress("glFramebufferTexture2DEXT"));
        glFramebufferTexture3DEXT = reinterpret_cast<PFNGLFRAMEBUFFERTEXTURE3DEXTPROC>(wglGetProcAddress("glFramebufferTexture3DEXT"));
        glFramebufferRenderbufferEXT = reinterpret_cast<PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC>(wglGetProcAddress("glFramebufferRenderbufferEXT"));
    }

    glExtBlendColor = FindGlExt("GL_EXT_blend_color");

    if (glExtBlendColor) {
        glBlendColorEXT = reinterpret_cast<PFNGLBLENDCOLOREXTPROC>(wglGetProcAddress("glBlendColorEXT"));
    }
}

#if defined(_WIN32)
// VERIFY: 31ee35e7 auto
// OFFSET: 0x0068C5A0
void BindWglExtensions() {
    wglGetExtensionsStringARB = reinterpret_cast<PFNWGLGETEXTENSIONSSTRINGARBPROC>(wglGetProcAddress("wglGetExtensionsStringARB"));

    if (HasWGLExtension("WGL_ARB_pixel_format")) {
        wglArbPixelFormat = 1;
        wglGetPixelFormatAttribivARB = reinterpret_cast<PFNWGLGETPIXELFORMATATTRIBIVARBPROC>(wglGetProcAddress("wglGetPixelFormatAttribivARB"));
        wglChoosePixelFormatARB = reinterpret_cast<PFNWGLCHOOSEPIXELFORMATARBPROC>(wglGetProcAddress("wglChoosePixelFormatARB"));
    }

    if (HasWGLExtension("WGL_ARB_pbuffer")) {
        wglArbPbuffer = 1;
        wglCreatePbufferARB = reinterpret_cast<PFNWGLCREATEPBUFFERARBPROC>(wglGetProcAddress("wglCreatePbufferARB"));
        wglGetPbufferDCARB = reinterpret_cast<PFNWGLGETPBUFFERDCARBPROC>(wglGetProcAddress("wglGetPbufferDCARB"));
        wglReleasePbufferDCARB = reinterpret_cast<PFNWGLRELEASEPBUFFERDCARBPROC>(wglGetProcAddress("wglReleasePbufferDCARB"));
        wglDestroyPbufferARB = reinterpret_cast<PFNWGLDESTROYPBUFFERARBPROC>(wglGetProcAddress("wglDestroyPbufferARB"));
        wglQueryPbufferARB = reinterpret_cast<PFNWGLQUERYPBUFFERARBPROC>(wglGetProcAddress("wglQueryPbufferARB"));
    }

    if (HasWGLExtension("WGL_EXT_swap_control")) {
        wglExtSwapControl = 1;
        wglSwapIntervalEXT = reinterpret_cast<PFNWGLSWAPINTERVALEXTPROC>(wglGetProcAddress("wglSwapIntervalEXT"));
    }
}
#else
// Pixel formats and pbuffers are WGL-only (SDL picks the visual, render targets use FBOs). Swap control is always
// available through SDL_GL_SetSwapInterval.
void BindWglExtensions() {
    wglArbPixelFormat = 0;
    wglArbPbuffer = 0;
    wglExtSwapControl = 1;
}
#endif
