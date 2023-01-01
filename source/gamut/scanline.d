/**
Scanline conversion public API. This is used both internally and externally, because converting
a whole row of pixels at once is a rather common operations.

Copyright: Copyright Guillaume Piolat 2023
License:   $(LINK2 http://www.boost.org/LICENSE_1_0.txt, Boost License 1.0)
*/
module gamut.scanline;


import core.stdc.string: memcpy;



@system:
nothrow:
@nogc:

/// All scanlines conversion functions follow this signature:
///   inScan pointer to first pixel of input scanline
///   outScan pointer to first pixel of output scanline
///
/// Type information, user data information, must be given by context.
/// Such a function assumes no overlap in memory between input and output scanlines.
alias scanlineConversionFunction_t = void function(const(ubyte)* inScan, ubyte* outScan, int width, void* userData);


/// Convert a row of pixel from RGBA 32-bit float (0 to 1.0) float to L 8-bit (0 to 255).
void scanline_convert_rgbaf32_to_l8(const(ubyte)* inScan, ubyte* outScan, int width, void* userData = null)
{
    const(float)* inp = cast(const(float)*)inScan; 
    ubyte* s = outScan;
    for (int x = 0; x < width; ++x)
    {
        ubyte b = cast(ubyte)(0.5f + (inp[4*x+0] + inp[4*x+1] + inp[4*x+2]) * 255.0f / 3.0f);
        *s++ = b;
    }
}

/// Convert a row of pixel from RGBA 32-bit float (0 to 1.0) float to L 16-bit (0 to 65535).
void scanline_convert_rgbaf32_to_l16(const(ubyte)* inScan, ubyte* outScan, int width, void* userData = null)
{
    const(float)* inp = cast(const(float)*)inScan; 
    ushort* s = cast(ushort*) outScan;
    for (int x = 0; x < width; ++x)
    {
        ushort b = cast(ushort)(0.5f + (inp[4*x+0] + inp[4*x+1] + inp[4*x+2]) * 65535.0f / 3.0f);
        *s++ = b;
    }
}


/// Convert a row of pixel from RGBA 32-bit float (0 to 1.0) float to L 32-bit float (0 to 1.0).
void scanline_convert_rgbaf32_to_lf32(const(ubyte)* inScan, ubyte* outScan, int width, void* userData = null)
{
    const(float)* inp = cast(const(float)*)inScan; 
    float* s = cast(float*) outScan;
    for (int x = 0; x < width; ++x)
    {
        float b = (inp[4*x+0] + inp[4*x+1] + inp[4*x+2]) / 3.0f;
        *s++ = b;
    }
}

/// Convert a row of pixel from RGBA 32-bit float (0 to 1.0) to LA 16-bit (0 to 65535).
void scanline_convert_rgbaf32_to_la8(const(ubyte)* inScan, ubyte* outScan, int width, void* userData = null)
{
    // Issue #21, workaround for DMD optimizer.
    version(DigitalMars) pragma(inline, false);

    const(float)* inp = cast(const(float)*)inScan; 
    ubyte* s = outScan;
    for (int x = 0; x < width; ++x)
    {
        ubyte b = cast(ubyte)(0.5f + (inp[4*x+0] + inp[4*x+1] + inp[4*x+2]) * 255.0f / 3.0f);
        ubyte a = cast(ubyte)(0.5f + inp[4*x+3] * 255.0f);
        *s++ = b;
        *s++ = a;
    }
}

/// Convert a row of pixel from RGBA 32-bit float (0 to 1.0) to LA 16-bit (0 to 65535).
void scanline_convert_rgbaf32_to_la16(const(ubyte)* inScan, ubyte* outScan, int width, void* userData = null)
{
    const(float)* inp = cast(const(float)*) inScan;
    ushort* s = cast(ushort*) outScan;    
    for (int x = 0; x < width; ++x)
    {
        ushort b = cast(ushort)(0.5f + (inp[4*x+0] + inp[4*x+1] + inp[4*x+2]) * 65535.0f / 3.0f);
        ushort a = cast(ushort)(0.5f + inp[4*x+3] * 65535.0f);
        *s++ = b;
        *s++ = a;
    }
}

/// Convert a row of pixel from RGBA 32-bit float (0 to 1) to LA 32-bit float (0 to 1).
void scanline_convert_rgbaf32_to_laf32(const(ubyte)* inScan, ubyte* outScan, int width, void* userData = null)
{
    const(float)* inp = cast(const(float)*) inScan;
    float* s = cast(float*) outScan;
    for (int x = 0; x < width; ++x)
    {
        float b = (inp[4*x+0] + inp[4*x+1] + inp[4*x+2]) / 3.0f;
        float a = inp[4*x+3];
        *s++ = b;
        *s++ = a;
    }
}

/// Convert a row of pixel from RGBA 32-bit float (0 to 1) to RGB 8-bit (0 to 255).
void scanline_convert_rgbaf32_to_rgb8(const(ubyte)* inScan, ubyte* outScan, int width, void* userData = null)
{
    const(float)* inp = cast(const(float)*) inScan;
    ubyte* s = outScan;

    for (int x = 0; x < width; ++x)
    {
        ubyte r = cast(ubyte)(0.5f + inp[4*x+0] * 255.0f);
        ubyte g = cast(ubyte)(0.5f + inp[4*x+1] * 255.0f);
        ubyte b = cast(ubyte)(0.5f + inp[4*x+2] * 255.0f);
        *s++ = r;
        *s++ = g;
        *s++ = b;
    }
}

/// Convert a row of pixel from RGBA 32-bit float (0 to 1) to RGB 16-bit (0 to 65535).
void scanline_convert_rgbaf32_to_rgb16(const(ubyte)* inScan, ubyte* outScan, int width, void* userData = null)
{
    const(float)* inp = cast(const(float)*) inScan;
    ushort* s = cast(ushort*) outScan;
    for (int x = 0; x < width; ++x)
    {
        ushort r = cast(ushort)(0.5f + inp[4*x+0] * 65535.0f);
        ushort g = cast(ushort)(0.5f + inp[4*x+1] * 65535.0f);
        ushort b = cast(ushort)(0.5f + inp[4*x+2] * 65535.0f);
        *s++ = r;
        *s++ = g;
        *s++ = b;
    }
}

/// Convert a row of pixel from RGBA 32-bit float (0 to 1) to RGB 16-bit (0 to 65535).
void scanline_convert_rgbaf32_to_rgbf32(const(ubyte)* inScan, ubyte* outScan, int width, void* userData = null)
{
    const(float)* inp = cast(const(float)*) inScan;
    float* s = cast(float*) outScan;
    for (int x = 0; x < width; ++x)
    {
        *s++ = inp[4*x+0];
        *s++ = inp[4*x+1];
        *s++ = inp[4*x+2];
    }
}

/// Convert a row of pixel from RGBA 32-bit float (0 to 1) to RGBA 8-bit (0 to 255).
void scanline_convert_rgbaf32_to_rgba8(const(ubyte)* inScan, ubyte* outScan, int width, void* userData = null)
{
    const(float)* inp = cast(const(float)*) inScan;
    ubyte* s = outScan;
    for (int x = 0; x < width; ++x)
    {
        ubyte r = cast(ubyte)(0.5f + inp[4*x+0] * 255.0f);
        ubyte g = cast(ubyte)(0.5f + inp[4*x+1] * 255.0f);
        ubyte b = cast(ubyte)(0.5f + inp[4*x+2] * 255.0f);
        ubyte a = cast(ubyte)(0.5f + inp[4*x+3] * 255.0f);
        *s++ = r;
        *s++ = g;
        *s++ = b;
        *s++ = a;
    }
}

/// Convert a row of pixel from RGBA 32-bit float (0 to 1) to RGBA 16-bit (0 to 65535).
void scanline_convert_rgbaf32_to_rgba16(const(ubyte)* inScan, ubyte* outScan, int width, void* userData = null)
{
    const(float)* inp = cast(const(float)*) inScan;
    ushort* s = cast(ushort*)outScan;
    for (int x = 0; x < width; ++x)
    {
        ushort r = cast(ushort)(0.5f + inp[4*x+0] * 65535.0f);
        ushort g = cast(ushort)(0.5f + inp[4*x+1] * 65535.0f);
        ushort b = cast(ushort)(0.5f + inp[4*x+2] * 65535.0f);
        ushort a = cast(ushort)(0.5f + inp[4*x+3] * 65535.0f);
        *s++ = r;
        *s++ = g;
        *s++ = b;
        *s++ = a;
    }
}

/// Convert a row of pixel from RGBA 32-bit float (0 to 1) to RGBA 32-bit float.
void scanline_convert_rgbaf32_to_rgbaf32(const(ubyte)* inScan, ubyte* outScan, int width, void* userData = null)
{
    memcpy(outScan, inScan, width * 4 * float.sizeof);
}