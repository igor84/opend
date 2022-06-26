/**
Various public types.

Copyright: Copyright Guillaume Piolat 2022
License:   $(LINK2 http://www.boost.org/LICENSE_1_0.txt, Boost License 1.0)
*/
module gamut.types;

nothrow @nogc:

/// Image format.
enum ImageFormat
{
    unknown = -1, /// Unknown format (returned value only, never use it as input value)
    first   =  0,
    JPEG    =  0, /// Independent JPEG Group (*.JPG, *.JIF, *.JPEG, *.JPE)
    PNG     =  1, /// Portable Network Graphics (*.PNG)
    QOI     =  2  /// Quite OK Image format (*.QOI)
}

/// Image format.
enum ImageType
{
    unknown = -1, /// Unknown format (returned value only, never use it as input value)
    uint8 = 0,    /// Array of ushort: unsigned 8-bit
    uint16,       /// Array of ushort: unsigned 16-bit
    f32,          /// Array of float: 32-bit IEEE floating point
    
    la8,          /// 16-bit Luminance Alpha image: 2 x unsigned 8-bit
    la16,         /// 32-bit Luminance Alpha image: 2 x unsigned 16-bit
    laf32,        /// 64-bit Luminance Alpha image: 2 x 32-bit IEEE floating point

    rgb8,         /// 24-bit RGB image: 3 x unsigned 8-bit
    rgb16,        /// 48-bit RGB image: 3 x unsigned 16-bit
    rgbf32,       /// 96-bit RGB float image: 3 x 32-bit IEEE floating point

    rgba8,        /// 32-bit RGBA image: 4 x unsigned 8-bit
    rgba16,       /// 64-bit RGBA image: 4 x unsigned 16-bit    
    rgbaf32,      /// 128-bit RGBA float image: 4 x 32-bit IEEE floating point
}

// Size of one pixel for type
int bytesForImageType(ImageType type) pure @safe
{
    final switch(type)
    {
        case ImageType.uint8:   return 1;
        case ImageType.uint16:  return 2;
        case ImageType.f32:     return 4;
        case ImageType.la8:     return 2;
        case ImageType.la16:    return 4;
        case ImageType.laf32:   return 8;
        case ImageType.rgb8:    return 3;
        case ImageType.rgb16:   return 6;
        case ImageType.rgba8:   return 4;
        case ImageType.rgba16:  return 8;
        case ImageType.rgbf32:  return 12;
        case ImageType.rgbaf32: return 16;
        case ImageType.unknown: assert(false);
    }
}

// Limits


/// When images have an unknown width.
enum GAMUT_INVALID_IMAGE_WIDTH = -1;  

/// When images have an unknown height.
enum GAMUT_INVALID_IMAGE_HEIGHT = -1; 


/// No FIBITMAP can exceed this width in gamut.
enum GAMUT_MAX_IMAGE_WIDTH = 16384;  

/// No FIBITMAP can exceed this height in gamut.
enum GAMUT_MAX_IMAGE_HEIGHT = 16384; 



// Load flags

/// No loading options.
enum int LOAD_NORMAL = 0; 

/// Load the image in grayscale, faster than loading as RGB24 then converting to greyscale.
/// Can't be used with either `LOAD_RGB` or `LOAD_RGBA`.
enum int LOAD_GREYSCALE = 1;

/// Load the image in RGB8/RGB16, faster than loading as RGB8 then converting to greyscale.
/// Can't be used with either `LOAD_GREYSCALE` or `LOAD_RGBA`.
enum int LOAD_RGB = 2; 

/// Load the image in RGBA8/RGBA16, faster than loading as RGBA8 then converting to greyscale.
/// Can't be used with either `LOAD_GREYSCALE` or `LOAD_RGBA`.
enum int LOAD_RGBA = 4; 

