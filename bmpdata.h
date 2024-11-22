#ifndef BMPDATA_UNIT
#define BMPDATA_UNIT


#include <stdio.h>
#include <windows.h>
#include "xxhash.h"

int capturebmpdata(void* data,size_t* psize);
int capturebmpdata(HDC hdc,int width,int height,void* data,size_t* psize);

int loadbmpdata(HDC hdc,HBITMAP hBmp,int width,int height,void* data,size_t size);

int loadclipbmpdata(void* dibits,const int width,const int height,const void* clip_dibits,const int x, const int y,const int clip_width,const int clip_height,const size_t clip_data_size);

int clipbmpdata(int width,int height,const void* data,int clip_x,int clip_y,int clip_width,int clip_height,void* clip_data,size_t* pclipsize);

int encodebmp2webp(void* data,int width,int height,size_t size,int quality_factor,void* compressed,size_t* pcompressedsize);
int decodewebp2bmp(void* compressed,size_t compressedsize,void* data,int* pwidth,int* pheight,size_t* psize);

int encodebmp2webp2(void* data,int width,int height,size_t size,int quality_factor,void* compressed,size_t* pcompressedsize);
int decodewebp2bmp2(void* compressed,size_t compressedsize,void* data,int* pwidth,int* pheight,size_t* psize);

int hashbmpdata(int width,int height,const void* data,int clip_x,int clip_y,int clip_width,int clip_height,void* clip_data,size_t* pclip_size,void* hash_context,void* clip_hashcodes,size_t* pclip_hashcodes_size);
#endif
