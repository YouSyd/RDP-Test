#include "bmpdata.h"
#include "webp/encode.h"
#include "webp/decode.h"

/**
 * 将HDC中的位图数据取出写入内存
 */
int capturebmpdata(HDC hdc,int width,int height,void* data,size_t* psize) {
    int result=0;
    HDC memdc=CreateCompatibleDC(hdc);
    HBITMAP hBitmap=CreateCompatibleBitmap(hdc,width,height);
    HBITMAP hBitmapPre=(HBITMAP)SelectObject(memdc,hBitmap);
    BitBlt(memdc,0,0,width,height,hdc,0,0,SRCCOPY);
    
    //保存处理图像数据
    BITMAP bmp;
    GetObject(hBitmap,sizeof(BITMAP),&bmp);
    BITMAPINFOHEADER infoHeader={0};

    infoHeader.biSize=sizeof(BITMAPINFOHEADER);
    infoHeader.biWidth=bmp.bmWidth;
    infoHeader.biHeight=bmp.bmHeight;
    infoHeader.biPlanes=1;
    infoHeader.biBitCount=24;
    infoHeader.biCompression=BI_RGB;
    infoHeader.biSizeImage=bmp.bmWidthBytes*bmp.bmHeight;

    //获取DIBits数据
    //24位图，DIB_RGB_COLORS模式
    int lineSize=((width*24+31)/32)*4;//bmp每行占据的字节数
    *psize=lineSize*height;
    
    if(0==GetDIBits(memdc,hBitmap,0,height,data,(BITMAPINFO*)&infoHeader,DIB_RGB_COLORS)) {
        //出问题了
        result=-1;
        goto CAPTUREBMPDATA_EXIT;
    }

CAPTUREBMPDATA_EXIT:
    SelectObject(memdc,hBitmapPre);
    DeleteObject(hBitmap);
    DeleteDC(memdc);

    return result;
}

int capturebmpdata(void* data,size_t* psize) {
    HDC hdcScreen=GetDC(NULL);
    int screenWidth=GetDeviceCaps(hdcScreen,HORZRES);
    int screenHeight=GetDeviceCaps(hdcScreen,VERTRES);

    int result=capturebmpdata(hdcScreen,screenWidth,screenHeight,data,psize);

    ReleaseDC(NULL, hdcScreen);
    return result;
}

/**
 * 从bmp数据中截取指定位置的数据（clip_x,clip_y) clip_width × clip_height
 * 这个效率高不了...
 * 咋弄?
 * 先验证数据结构
 * ---------------------------------------------------------------------
 * DIBits数据结构
 * 1、像素排列按照 从左到右，从下到上的模式 ， 第一个数据对应的是 图像左下角 row-1 ---- 0
 * 2、对于24位数据，按照 BGR 排列
 *    对于32位数据，按照 BGRA 排列
 * 3、行对齐规则，每行4字节对齐，不够补齐，每行数据的字节数为 
 *    (宽度×每像素字节数+3)/4*4
 * 我们的数据假定不需要α通道，即24位3字节数据
 */
int clipbmpdata(int width,int height,const void* data,int clip_x,int clip_y,int clip_width,int clip_height,void* clip_data,size_t* pclipsize) {
    int width_bytes=((width*3+3)/4)*4;
    int clip_width_bytes=((clip_width*3+3)/4)*4;
    if(clip_width%4!=0) {
        ZeroMemory(clip_data,clip_width_bytes*clip_height);
    }
    /**
     * 数据是       从 clip_y --> (clip_y+clip_height - 1)
     * 对应 offset  从 (clip_y + clip_height - 1) / ---> clip_y
     */
    for(int row=height-1-(clip_y+clip_height-1);row<=height-1-clip_y;row++) {
        int offset=width_bytes*row+clip_x*3;
        memcpy((BYTE*)clip_data+clip_width_bytes*(row-(height-1-(clip_y+clip_height-1))),(BYTE*)data+offset,clip_width*3);
    }
    *pclipsize=clip_width_bytes*clip_height;

    return 0;
}

/*
 * 参数 compressed : WebPMemoryWriter* 类型,
 * 数据存储在 其成员 ::mem中， 大小为::size
 * 
 */
int encodebmp2webp(void* data,int width,int height,size_t size,int quality_factor,void* compressed,size_t* pcompressedsize) {
    int result=0;
    int stride=((width*3+3)/4)*4;
    int compressquility=quality_factor;
    uint8_t* webp_data=NULL;

    WebPMemoryWriter* w=(WebPMemoryWriter*)compressed;
    if(w) {
        WebPMemoryWriterClear(w);

        WebPConfig cfg;
        WebPPicture pic;

        if(!WebPConfigInit(&cfg)||!WebPPictureInit(&pic)) return -1;
        cfg.quality=compressquility;
        pic.width=width;
        pic.height=height;
        pic.use_argb=0; //无α通道
        pic.custom_ptr=w;
        pic.writer=WebPMemoryWrite;

        if(!WebPPictureImportBGR(&pic,(uint8_t*)data,stride)) {
            result=-1;
        }

        if(result!=0||!WebPEncode(&cfg,&pic)) {
            result=-1;
        }
        *pcompressedsize=w->size;

        WebPPictureFree(&pic);
    }

    return result;
}

int encodebmp2webp2(void* data,int width,int height,size_t size,int quality_factor,void* compressed,size_t* pcompressedsize) {
    int stride=(width*3+3)/4*4;
    uint8_t* compressed_data=NULL;
    *pcompressedsize=WebPEncodeBGR((const uint8_t*)data,width,height,stride,quality_factor,&compressed_data);
    if(!(*pcompressedsize)) return -1;

    memcpy(compressed,compressed_data,*pcompressedsize);
    WebPFree(compressed_data);
    return 0;
}

int decodewebp2bmp2(void* compressed,size_t compressedsize,void* data,int* pwidth,int* pheight,size_t* psize) {
    int result=0;

    uint8_t* decompressed=NULL;
    if(result!=0||(NULL==(decompressed=WebPDecodeBGR((const uint8_t*)compressed,compressedsize,pwidth,pheight)))) {
        result=-1;
    } else {
        int stride=(*pwidth)*3;
        if(stride%4!=0) {
            stride=(((*pwidth)*3+3)/4)*4;
            //需要补齐4字节
            for(int idx=0;idx<*pheight;idx++) {
                memcpy((uint8_t*)data+idx*stride,(uint8_t*)decompressed+idx*(*pwidth)*3,(*pwidth)*3);
            }
        } else {
            memcpy((uint8_t*)data,(uint8_t*)decompressed,stride*(*pheight));
        }
        *psize=stride*(*pheight);

        WebPFree(decompressed);
    }

    return result;
}


int decodewebp2bmp(void* compressed,size_t compressedsize,void* data,int* pwidth,int* pheight,size_t* psize) {
    int result=0;

    WebPMemoryWriter* w=(WebPMemoryWriter*)compressed;
    uint8_t* decompressed=NULL;
    if(result!=0||(NULL==(decompressed=WebPDecodeBGR(w->mem,w->size,pwidth,pheight)))) {
        result=-1;
    } else {
        int stride=(*pwidth)*3;
        if(stride%4!=0) {
            stride=(((*pwidth)*3+3)/4)*4;
            //需要补齐4字节
            for(int idx=0;idx<*pheight;idx++) {
                memcpy((uint8_t*)data+idx*stride,(uint8_t*)decompressed+idx*(*pwidth)*3,(*pwidth)*3);
            }
        } else {
            memcpy((uint8_t*)data,(uint8_t*)decompressed,stride*(*pheight));
        }
        *psize=stride*(*pheight);
        WebPFree(decompressed);
    }

    return result;
}

/**
 * 将bmp数据导入HDC对象
 * 注意 hBmp必须是hdc的兼容位图，外部申请，外部释放
 */
int loadbmpdata(HDC hdc,HBITMAP hBmp,int width,int height,void* data,size_t size) {
    BITMAPINFO info;
    ZeroMemory(&info,sizeof(info));
    info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=width;
    info.bmiHeader.biHeight=height; // 正值表示自下而上，负值表示自上而下
    info.bmiHeader.biPlanes=1;
    info.bmiHeader.biBitCount=24;   // 24 位 RGB
    info.bmiHeader.biCompression=BI_RGB;

    // 将数据写入位图
    if(!SetDIBits(hdc,hBmp,0,height,data,&info,DIB_RGB_COLORS)) {
        return -1;
    }

    return 0;
}

/**
 * 将clipdata 写入dibits
 */
int loadclipbmpdata(void* dibits,const int width,const int height,const void* clip_dibits,const int x, const int y,const int clip_width,const int clip_height,const size_t clip_data_size) {
    /**
     * BITMAP 的DIBITS 数据格式
     * 1、按行存储，每行BGR，每行步长必须是4字节整数倍
     * 2、第一行存储的是图像的最后一行的数据
     */

    /**
     * 根据clip_y + idx 坐标 找到 dibits 中的 offset
     */
    int strip=(width*3+3)/4*4;
    int clip_strip=(clip_width*3+3)/4*4;
    for(int idx=y;idx<y+clip_height;idx++) {
        /**
         * 图像中的第y行，对应dibits中的第clip_height-1-y行
         */
        int dibits_offset=(height-1-idx)*strip+3*x;
        int clip_dibits_offset=(clip_height-1-(idx-y))*clip_strip;
        memcpy((uint8_t*)dibits+dibits_offset,(uint8_t*)clip_dibits+clip_dibits_offset,clip_strip);
    }

    return 0;
}

/**
 * 将 bmp的 dibits数据取出 (clip_x,clip_y) clip_width × clip_height 部分 ， 存入 clip_data
 * 并将裁剪的部分计算出hashcode
 */
int hashbmpdata(int width,int height,const void* data,int clip_x,int clip_y,int clip_width,int clip_height,void* clip_data,size_t* pclip_size,void* hash_context,void* clip_hashcodes,size_t* pclip_hashcodes_size) {
    if(0!=clipbmpdata(width,height,data,clip_x,clip_y,clip_width,clip_height,clip_data,pclip_size)){
        printf("获取裁剪区 pixels(%d,%d) %d×%d 的dibits数据失败\n",clip_x,clip_y,clip_width,clip_height);
        return -1;
    }

    XXH64_state_t* state=(XXH64_state_t*)hash_context;
    if(state==NULL) {
        fprintf(stderr,"Failed to create state\n");
        return -1;
    }

    // 初始化状态对象（使用种子 0）
    if(XXH64_reset(state,0)==XXH_ERROR) {
        fprintf(stderr, "Failed to reset state\n");
        XXH64_freeState(state);
        return -1;
    }

    // 分段更新状态
    XXH64_update(state,clip_data,*pclip_size);

    // 计算最终哈希值
    *((uint64_t*)clip_hashcodes)=XXH64_digest(state);

    return 0;
}

