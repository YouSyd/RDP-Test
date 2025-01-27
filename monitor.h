#ifndef MONITOR_UNIT
#define MONITOR_UNIT

#include "netio.h"
#include "bmpdata.h"
#include "ydark.h"
// #include <winsock2.h>
// #include <windows.h>
/*
 * 来好好分析一下微软拉的这坨大屎
 * windows.h 包含 winsock.h
 * msquic.h 包含 winsock2.h
 * 然后大水冲了龙王庙，他们一家人干起来了，MLGB
 * 不得已之下，你要梳理你的项目，排好座次
 * 记住桌上的敬酒规则：
 * 如果有项目引用了winsock2.h，
 * 该项目的头文件引用或者包含 winsock2.h，
 * 则该项目头文件是大哥哥，排首位，坐头把交椅，上座，
 * windows.h必须 末席，面朝大门。
 * 
 */ 

#define CTRL_MONITOR "MonitorView"
#define SCROLLBAR_PIXLS 13
#define THUMB_MIN_PIXLS 50

typedef struct monitor {
    void* dibits;//DIBits 数据
    int src_width;
    int src_height;

    /**
     * 流程：
     * 1、采集dibits
     * 2、分块，计算每一块的摘要
     * 3、检查 predibits是否存在，如果不存在，压缩、发送
     * 4、如果存在，将每一块的 hashcodes 与 prehashcodes 比较
     *      如果不相同，记录下块号、块的dibits
     * 5、将所有不相同的块号索引记录，块的dibits压缩，一并发送,定义新的报文类型  
     * 6、将predibits 中发生变化的块 用 dibits中对应块的数据更新，hashcodes 一并更新
     * 
     * 7、收到数据后，对于 分割块的报文，还原出块索引，将压缩块数据还原出块的dibits
     * 8、找到predibits，用还原的dibits,块号，predibits 还原出当前的 完整 dibits
     * 9、绘制
     * 10、将当前的predibits 更新为 还原出来的新的完整的 dibits
     */
    void* hashcodes;
    void* predibits; 
    void* prehashcodes;
    /**
     * segs中存储的是按块存放的位图数据，每一块
     * 都是按照 clip_width × clip_height × depth 码放
     * 顺序按照（row_idx*row_size+col_idx) 确定索引号 
     */
    void* predibits_segs;
    void* dibits_segs;

    int mode;//视图模式 0-缩略模式 1-原图模式
    POINT offset;//滚动条
    POINT pt;
    POINT prept;

    void* filtered;
    int filter_cx;
    int filter_cy;

    void* compressed; //是个指针
    size_t compressed_size;
    int compress_quality;//压缩质量

    float compress_rate;
    
    float kbytes;
    long long stamp;
    float trans_rate;//kb/s

    void* decompressed; //使用WebPDecode后的数据是没有四字节行对齐的
    size_t decompressed_len;
    int dec_width;
    int dec_height;

    void* context;
}RMonitor,*pMonitor;
/***
 * Monitor Zone Code Defination.
 */
#define IMG_VIEW        20
#define CTRL_PANEL      21
#define CTRL_EXPEND     22
#define CTRL_COLLAPSE   23
#define CTRL_CFG        24

struct timer_settings;
typedef struct monitor_exchange {
    uint8_t* dibits;
    size_t* pdibits_len;
    uint8_t* compressed;
    size_t* pcompressed_len;
    uint8_t* decompressed;
    size_t* pdecompressed_len;
    uint8_t* netio_buffer;

    int predibits_flag;
    void* hashcodes;
    void* predibits; 
    void* prehashcodes;
    /**
     * segs中存储的是按块存放的位图数据，每一块
     * 都是按照 clip_width × clip_height × depth 码放
     * 顺序按照（row_idx*row_size+col_idx) 确定索引号 
     */
    void* predibits_segs;
    void* dibits_segs;

    int mode;
    int frequency;
    int* pquality;
    timer_settings* timer;
    UINT_PTR timerid;

    int width;
    int height;

    pnet_io net;
    pstream_io stream;

    //需要一个包处理函数
    /**
     * 当收到包之后，解压，写入decompressed , 写入 dibits ，并触发重绘等
     */
    stream_recv_data_protocol_process_callback protocol_process;
    HWND hwnd;//用于展示的窗口句柄

}RMonExg,*pMonExg;

pMonitor Monitor_New();
void Monitor_Delete(pMonitor m);
int Monitor_InitialSettings(HWND hwnd);
pMonitor Monitor_GetSettings(HWND hwnd);
void Monitor_ClearSettings(HWND hwnd);
int Monitor_SetSrcSize(HWND hwnd,int width,int height);
int Monitor_SetFilter(HWND hwnd,int cx,int cy);
int Monitor_SetCompressQuality(HWND hwnd, int quality_factor);
int Monitor_HitTest(HWND hwnd,POINT pt);

int MonitorView_GetZoneRect(HWND hwnd,int poscode,LPRECT prc);
int MonitorView_GetContentXY(HWND hwnd,POINT* lp);
int MonitorView_PaintScrollbar(HWND hwnd,pMonitor monitor,HDC hdc);
int Monitor_PaintPanel(HWND hwnd,pMonitor m,HDC hdc);

int Monitor_InitReportStamp(HWND hwnd);

LRESULT CALLBACK MonitorViewProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
int MointorViewPaint(HWND hwnd,WPARAM wParam,LPARAM lParam);

int pack_protocol_process(void* out_context,pNIHeader head,uint8_t** plts,int* lens,int count,void* inner_context);

extern pMonExg MONEXG;
pMonExg GetMonExg();

int TimerProc_SVR(timer_settings* timer,void* param);
int TimerProc_SVR2(timer_settings* timer,void* param);
int TimerProc_CLIENT(timer_settings* timer,void* param);

int Monitor_SetNetIo(pMonitor m,pnet_io net);


/**
 * 定时器相关
 * 创建一个定时器，用于完成数据采集
 */
typedef int (*timer_callback)(timer_settings*,void*);
typedef struct timer_settings {
    HANDLE timer;
    HANDLE thread;
    DWORD threadID;
    timer_callback timer_proc;
    LPTHREAD_START_ROUTINE thread_proc;
    void* param;
    int thread_status;//0-初始化 1-运行中 2-已退出 3-指令退出
}TimerSettings,*pTimerSettings;

pTimerSettings TimerSettings_Initial(LPTHREAD_START_ROUTINE thread_proc,timer_callback timer_proc,void* param);
void TimerSettings_Clear(pTimerSettings ts);
int TimerSettings_Start(pTimerSettings ts);
int TimerSettings_Stop(pTimerSettings ts);
int TimerSettings_SetTimer(pTimerSettings ts,int wait_millsec,int millsec);
DWORD WINAPI TimerSettings_DefaultThreadProc(LPVOID param);


#endif
