#ifndef _S3TC_H_
#define _S3TC_H_
#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
// void BlockDecompressImageDXTn(): Decompresses all the blocks of a DXTn compressed texture and stores the resulting pixels in 'image'.
//
// uint32_t width:					Texture width.
// uint32_t height:				Texture height.
// const unsigned char *blockStorage:	pointer to compressed DXTn blocks.
// uint32_t *image:				pointer to the image where the decompressed pixels will be stored.
 
void BlockDecompressImageDXT1(uint32_t width, uint32_t height, const unsigned char *blockStorage, void *image);
void BlockDecompressImageDXT3(uint32_t width, uint32_t height, const unsigned char *blockStorage, void *image);
void BlockDecompressImageDXT5(uint32_t width, uint32_t height, const unsigned char *blockStorage, void *image);

#ifdef __cplusplus
}
#endif
#endif



