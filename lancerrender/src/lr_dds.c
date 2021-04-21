#include <lancerrender_img.h>
#include <stdint.h>
#include "lr_errors.h"
typedef enum {
    DXT1 = 0x31545844,
    DXT2 = 0x32545844,
    DXT3 = 0x33545844,
    DXT4 = 0x34545844,
    DXT5 = 0x35545844
} FourCC;

typedef struct DDS_PIXELFORMAT
{
    uint32_t dwSize;
    uint32_t dwFlags;
    FourCC dwFourCC;
    uint32_t dwRGBBitCount;
    uint32_t dwRBitMask;
    uint32_t dwGBitMask;
    uint32_t dwBBitMask;
    uint32_t dwABitMask;
} DDS_PIXELFORMAT;

typedef struct 
{
    uint32_t dwSize;
    uint32_t dwFlags;
    uint32_t dwHeight;
    uint32_t dwWidth;
    uint32_t dwPitchOrLinearSize;
    uint32_t dwDepth;
    uint32_t dwMipMapCount;
    uint32_t dwReserved1[11];
    DDS_PIXELFORMAT ddspf;
    uint32_t dwCaps;
    uint32_t dwCaps2;
    uint32_t dwCaps3;
    uint32_t dwCaps4;
    uint32_t dwReserved2;
} DDS_HEADER;

#define DDSD_CAPS 0x1
#define DDSD_HEIGHT 0x2
#define DDSD_WIDTH 0x4
#define DDSD_PITCH 0x8
#define DDSD_PIXELFORMAT 0x1000
#define DDSD_MIPMAPCOUNT 0x20000
#define DDSD_LINEARSIZE 0x80000
#define DDSD_DEPTH 0x800000

#define DDS_MAGIC 0x20534444
#define HEADER_SIZE 24
#define PFORMAT_SIZE 32

#define DDSCAPS_TEXTURE 0x1000
#define DDSCAPS_MIPMAP 0x400000
#define DDSCAPS_COMPLEX 0x8

#define DDSCAPS2_CUBEMAP 0x200
#define DDSCAPS2_VOLUME 0x200000

#define DX10 0x30315844

#define MAX_MIPLEVELS (64)

static LRTEXFORMAT GetFormat(DDS_HEADER header)
{
    switch(header.ddspf.dwFourCC) {
        case DXT1:
            return LRTEXFORMAT_DXT1;
        case DXT3:
            return LRTEXFORMAT_DXT3;
        case DXT5:
            return LRTEXFORMAT_DXT5;
    }
    //Uncompressed formats
    if (header.ddspf.dwFlags == 0x41 &&
        header.ddspf.dwRGBBitCount == 0x10 &&
        header.ddspf.dwFourCC == 0 &&
        header.ddspf.dwRBitMask == 0x7c00 &&
        header.ddspf.dwGBitMask == 0x3e0 &&
        header.ddspf.dwBBitMask == 0x1f &&
        header.ddspf.dwABitMask == 0x8000)
        return LRTEXFORMAT_ARGB1555;
    if (header.ddspf.dwFlags == 0x40 &&
        header.ddspf.dwRGBBitCount == 0x10 &&
        header.ddspf.dwFourCC == 0 &&
        header.ddspf.dwRBitMask == 0xf800 &&
        header.ddspf.dwGBitMask == 0x7e0 &&
        header.ddspf.dwBBitMask == 0x1f &&
        header.ddspf.dwABitMask == 0)
        return LRTEXFORMAT_BGR565;

    if (header.ddspf.dwFlags == 0x41 &&
        header.ddspf.dwRGBBitCount == 0x10 &&
        header.ddspf.dwFourCC == 0 &&
        header.ddspf.dwRBitMask == 0xf00 &&
        header.ddspf.dwGBitMask == 240 &&
        header.ddspf.dwBBitMask == 15 &&
        header.ddspf.dwABitMask == 0xf000)
        return LRTEXFORMAT_BGRA4444;
    
    //Uncompressed 32-bit and 24-bit formats
    if ((header.ddspf.dwFlags == 0x41 || header.ddspf.dwFlags == 0x40) &&
        (header.ddspf.dwRGBBitCount == 0x20 || header.ddspf.dwRGBBitCount == 0x18) &&
         header.ddspf.dwFourCC == 0 &&
         header.ddspf.dwABitMask != 0xc0000000)
         return LRTEXFORMAT_BGRA8888;
    return -1;
}

typedef struct {
    int x;
    int y;
} MipInfo;

typedef struct {
    int loaded;
    int mipCount;
    MipInfo mipInfo[MAX_MIPLEVELS];
    unsigned char* mipData[MAX_MIPLEVELS];
} DDS_SURFACE;

static void DestroySurface(DDS_SURFACE *sfc)
{
    for(int i = 0;i < sfc->mipCount; i++)
        free(sfc->mipData[i]);
    sfc->mipCount = 0;
    sfc->loaded = 0;
}

static int SurfaceByteCount(LRTEXFORMAT fmt, int width, int height)
{
    switch(fmt) {
        case LRTEXFORMAT_DXT1:
            return ((width + 3) / 4) * ((height + 3) / 4) * 8;
        case LRTEXFORMAT_DXT3:
        case LRTEXFORMAT_DXT5:
            return ((width + 3) / 4) * ((height + 3) / 4) * 16;
        case LRTEXFORMAT_ARGB1555:
        case LRTEXFORMAT_BGR565:
        case LRTEXFORMAT_BGRA4444:
            return width * height * 2;
        case LRTEXFORMAT_BGRA8888:
            return width * height * 4;
    }
    return 0;
}

static int GetMipCount(DDS_HEADER *header)
{
    int mipCount = 1;
    if(header->dwCaps & DDSCAPS_MIPMAP ||
        header->dwFlags & DDSD_MIPMAPCOUNT)
        mipCount = header->dwMipMapCount;
    if(mipCount > MAX_MIPLEVELS) return -1;
    if(mipCount <= 0) return -2;
    return mipCount;
}

static int LoadSurface(SDL_RWops *rw, DDS_SURFACE *sfc, LRTEXFORMAT fmt, DDS_HEADER *header)
{
    int w = header->dwWidth;
    int h = header->dwHeight;
    int mipCount = GetMipCount(header);
    sfc->loaded = 1;
    sfc->mipCount = mipCount;
    int i = 0; 
    for(i = 0; i < mipCount; i++) {
        int len = SurfaceByteCount(fmt, w, h);
        sfc->mipInfo[i].x = w;
        sfc->mipInfo[i].y = h;
        sfc->mipData[i] = malloc(len);
        if(fmt == LRTEXFORMAT_BGRA8888 && header->ddspf.dwRGBBitCount == 24) {
            for(int j = 0; j < len; j += 4) {
                if(!rw->read(rw, &sfc->mipData[i][j], 3, 1))
                    goto readerror;
                sfc->mipData[i][j + 3] = 255;
            }
        } else {
            if(!rw->read(rw, (void*)sfc->mipData[i], len, 1))
                goto readerror;
        }
        //if no alpha
        if(fmt == LRTEXFORMAT_BGRA8888 && header->ddspf.dwABitMask == 0) {
            for(int px = 0; px < len; px += 4) {
                sfc->mipData[i][px + 3] = 255;
            }
        }
        //swap channels if needed
        if(fmt == LRTEXFORMAT_BGRA8888 && header->ddspf.dwRBitMask == 0xff0000) {
            for(int px = 0; px < len; px += 4) {
                unsigned char r = sfc->mipData[i][px];
                unsigned char b = sfc->mipData[i][px + 2];
                sfc->mipData[i][px] = b;
                sfc->mipData[i][px + 2] = r;
            }
        }
        w /= 2;
        h /= 2;
        if (w < 1) w = 1;
        if (h < 1) h = 1;       
    }
    return 1;

    readerror:
    for(int k = 0; k <= i; k++) {
        free(sfc->mipData[k]);
        sfc->loaded = 0;
    }
    return 0;
}



LRDDSResult LR_DDS_Load(LR_Context *ctx, LR_Texture *tex, SDL_RWops *rw)
{
    uint32_t magic;
    DDS_HEADER header;
    if(!rw->read(rw, &magic, 4, 1)) return LRDDSResult_UNEXPECTEDEOF;
    if(magic != DDS_MAGIC) {
        return LRDDSResult_INVALIDORCORRUPT;
    }
    if(!rw->read(rw, &header, sizeof(DDS_HEADER), 1)) return LRDDSResult_UNEXPECTEDEOF;
    if(header.ddspf.dwFourCC == DX10)
        return LRDDSResult_UNSUPPORTEDFORMAT; //DX10-style DDS is not supported

    //Don't check header size - not all .dds files write it correctly.
    //3D textures not supported
    if((header.dwFlags & DDSD_DEPTH) || (header.dwCaps2 && DDSCAPS2_VOLUME))
        return LRDDSResult_UNSUPPORTEDTYPE; //we don't support 3D textures.
    
    if(header.dwCaps2 & DDSCAPS2_CUBEMAP) {
        return LRDDSResult_UNSUPPORTEDTYPE; //can't do cubemaps yet (TODO)
    }

    LRTEXFORMAT texFormat = GetFormat(header);
    if(texFormat == -1)
        return LRDDSResult_UNSUPPORTEDPIXELFORMAT;

    int mipCount = GetMipCount(&header);

    if(mipCount == -1) return LRDDSResult_TOOMANYLEVELS;
    if(mipCount == -2) return LRDDSResult_INVALIDORCORRUPT;

    DDS_SURFACE sfc;
    memset(&sfc, 0, sizeof(DDS_SURFACE));
    if(!LoadSurface(rw, &sfc, texFormat, &header)) {
        return LRDDSResult_UNEXPECTEDEOF;
    }

    LR_Texture_Allocate(ctx, tex, LRTEXTYPE_2D, texFormat, sfc.mipInfo[0].x, sfc.mipInfo[0].y);
    for(int i = 0; i < sfc.mipCount; i++) {
        LR_Texture_UploadMipLevel(ctx, tex, i, sfc.mipInfo[i].x, sfc.mipInfo[i].y, sfc.mipData[i]);
    }
    DestroySurface(&sfc);
    return LRDDSResult_OK;
}