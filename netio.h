#ifndef NETIO_TEST_UNIT
#define NETIO_TEST_UNIT

#include <stdio.h>
#include <stdlib.h>
#include <list>
#define _WINSOCKAPI_
#include <msquic.h>

#pragma comment(lib,"msquic.lib")
#pragma comment(lib,"ws2_32.lib")

struct netio_header;
struct io_stream;
typedef int (*stream_recv_data_callback)(uint8_t* buffer,size_t size,void* context);
typedef int (*stream_recv_data_protocol_process_callback)(void* out_context,netio_header* head,uint8_t** plts,int* lens,int count,void* inner_context);
typedef int (*stream_connect_first_hello_callback)(io_stream* stream,void* context);

struct io_connect;
struct io_net;
typedef struct io_stream {
    HQUIC stream;
    QUIC_STREAM_CALLBACK_HANDLER stream_callback;
    uint8_t* recv_buffer;
    size_t recv_buffer_size;
    size_t recv_size;
    size_t recv_offset;
    /**
     * 由于一个流上面存在切包情况 ，recv_buffer 就是一个缓冲区
     * StreamIo_CopyData 会从write_offset 写入新数据，并移动 write_offset
     * 上层应用会从read_offset 读取数据，并移动 read_offset
     */
    uint32_t write_offset;
    uint32_t read_offset;

    uint8_t* send_buffer;
    size_t send_size;
    size_t send_buffer_size;

    uint8_t* quic_buffer_list;
    uint8_t* quic_buffer_flags;
    uint8_t  quic_buffer_size;

    int recv_pending;//0-接收处理中，1-已经接收完毕 QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN
    int send_pending;//0-发送处理中，1-发送结束 QUIC_STREAM_EVENT_SEND_COMPLETE

    /**
     * 这里有一个问题，如何将 recv_callback 传递进去呢？
     * 外面只操作 netio , 怎么设置呢？
     * 从接收报文来分解，报文中展示响应类型？
     */
    stream_recv_data_callback recv_callback; //流收到数据后的回调（包括客户端首次与服务端建立连接的处理回调）

    //假设一个连接有多个流，每个流有自己的context
    void* context;

    io_connect* conn;
    io_net* net;
}stream_io,*pstream_io;

typedef struct io_connect {
    HQUIC conn;
    HQUIC conn_cfg;
    QUIC_CREDENTIAL_CONFIG conn_cred_cfg;

    QUIC_CONNECTION_CALLBACK_HANDLER conn_callback;

    /**
     * 比如一个连接有两个流，一个传输数据
     * 一个传输控制命令，两个流有自己的 流处理函数，
     * 流有自己独立的接收缓冲
     */
    std::list<pstream_io> streams;

    io_net* net;
}conn_io,*pconn_io;

typedef struct io_net {
    //QUIC_API_TABLE* msquic;
    HQUIC registration;
    QUIC_SETTINGS settings;
    HQUIC cfg;
    //QUIC_REGISTRATION_CONFIG reg_cfg;

    unsigned char alpn_buff[256];

    int mode;//server-0,client-1;
    char svr_ip[256];
    int port;

    HQUIC listener;
    QUIC_LISTENER_CALLBACK_HANDLER listener_callback;

    //对于客户端来讲，可能只有一个连接
    //对于服务端，可能有一堆连接
    std::list<pconn_io> conns;

    stream_recv_data_protocol_process_callback protocol_process;

    /**
     * 建立连接后的第一声啼哭
     */
    stream_connect_first_hello_callback client_hello_callback; 

    void* context;//参数

    void* timer;
}net_io,*pnet_io;

QUIC_API_TABLE* NetIo_InitialAPI();
QUIC_API_TABLE* NetIo_GetAPI();
void NetIo_ClearAPI(QUIC_API_TABLE* api);

pnet_io NetIo_Initial();
void NetIo_Clear(pnet_io);
int NetIo_SetAlpn(pnet_io net,char* alpn);
int NetIo_SetListenPort(pnet_io net,int port);

int NetIo_SetConnectAddr(pnet_io net,char* ip,int port);

int NetIo_SetHelloCallback(pnet_io net,stream_connect_first_hello_callback first_hello_callback);

int NetIo_LaunchServerListener(pnet_io net,int port);
int NetIo_LaunchServer(pnet_io net,int port,char* certfile,char* keyfile);
int NetIo_LaunchClient(pnet_io net,char* svr_ip,int port,QUIC_CONNECTION_CALLBACK_HANDLER conn_callback);
int NetIo_AddConn(pnet_io net,pconn_io conn);
int NetIo_RemoveConn(pnet_io net,pconn_io conn);
pconn_io NetIo_GetConn(pnet_io net,HQUIC conn);
int NetIo_SetContext(pnet_io net,void* context);

pconn_io ConnIo_Initial(pnet_io net,QUIC_CONNECTION_CALLBACK_HANDLER conn_callback);
pconn_io ConnIo_Initial(pnet_io net,HQUIC conn,QUIC_CONNECTION_CALLBACK_HANDLER conn_callback);//有 conn 句柄
int ConnIo_Clear(pconn_io conn);
int ConnIo_AddStream(pconn_io conn,pstream_io stream);
int ConnIo_RemoveStream(pconn_io conn,pstream_io stream);
pstream_io ConnIo_GetStream(pconn_io conn,HQUIC stream);
int ConnIo_SetCfg(pconn_io conn,int mode); // 设置 conn_cfg 、conn_cred_cfg

pstream_io StreamIo_Initial(pconn_io conn, QUIC_STREAM_CALLBACK_HANDLER stream_callback);
pstream_io StreamIo_Initial(pconn_io conn,HQUIC stream,QUIC_STREAM_CALLBACK_HANDLER stream_callback); // 有 stream 句柄
int StreamIo_Clear(pstream_io stream);
int StreamIo_CopyData(pstream_io stream,uint64_t total_size,const QUIC_BUFFER* buffers,uint32_t buffercount,QUIC_RECEIVE_FLAGS flags);//QUIC_STREAM_EVENT_RECEIVE 将收到的数据写入netio的缓冲
int StreamIo_SetRecvCallBack(pstream_io stream,stream_recv_data_callback recv_callback);
int StreamIo_SendData(pstream_io stream,uint8_t* buffer,size_t size);

/*
 * 设置外接缓存，用于将流Recv的数据拷贝出去
 */
int StreamIo_SetExternalBuffer(pstream_io stream,uint8_t* buffer,size_t size);

QUIC_STATUS server_listener_callback(
    _In_ HQUIC Listener,
    _In_opt_ void* Context,
    _Inout_ QUIC_LISTENER_EVENT* Event
    );

QUIC_STATUS server_connection_callback(
    _In_ HQUIC Connection,
    _In_opt_ void* Context,
    _In_ QUIC_CONNECTION_EVENT* Event
    );

QUIC_STATUS server_stream_callback(
    _In_ HQUIC Stream,
    _In_opt_ void* Context,
    _In_ QUIC_STREAM_EVENT* Event
    );

QUIC_STATUS client_connection_callback(
    _In_ HQUIC Connection,
    _In_opt_ void* Context,
    _In_ QUIC_CONNECTION_EVENT* Event
    );

QUIC_STATUS client_stream_callback(
    _In_ HQUIC Stream,
    _In_opt_ void* Context,
    _In_ QUIC_STREAM_EVENT* Event
    );

extern QUIC_API_TABLE* quic_api;

#define ENUM_NAMES(name,desc) enum_names(name,desc,#name)
#define ENUM_NAMES_PROTOCOL_TRANSCODE ENUM_NAMES(REQ_IMG_PARAM,"请求报文-图像参数") \
ENUM_NAMES(RSP_IMG_PARAM,"响应报文-图像参数") \
ENUM_NAMES(REQ_IMG_DATA,"请求报文-图像数据") \
ENUM_NAMES(RSP_IMG_DATA,"响应报文-图像数据") \
ENUM_NAMES(RSP_IMG_DATA_PARTITIONMODE,"响应报文-图像数据（分段模式）") \
ENUM_NAMES(REQ_CTRL_PARAM,"请求报文-控制参数") \
ENUM_NAMES(RSP_CTRL_PARAM,"响应报文-控制参数") \
ENUM_NAMES(REQ_TRANS_RESET,"请求报文-重置") \
ENUM_NAMES(RSP_TRANS_RESET,"响应报文-重置")

typedef enum {
#define enum_names(enum_code,enum_desc,enum_name) enum_code,
    ENUM_NAMES_PROTOCOL_TRANSCODE
#undef enum_names
} TRANSCODE;

extern char PROTOCOL_TRANSCODE_DESC[100][256];

/**
 * 遇到了诡异的内存对齐问题，似乎和我以前的认知不同
 typedef struct netio_header {
    uint8_t mode;//0-服务端，1-客户端
    uint8_t type;//报文类型 ，关于什么的报文
    uint64_t seq;
    uint32_t len;//报文长（netio_header)
}NIHeader,*pNIHeader;
 这个是OK的

 typedef struct netio_header {
    uint8_t mode;//0-服务端，1-客户端
    uint8_t type;//报文类型 ，关于什么的报文
    uint64_t seq;
    uint64_t len;//报文长（netio_header)
}NIHeader,*pNIHeader;
 这个就不OK了

使用 sizeof , 都是 24字节，但经过网络传输后，就出问题了
该问题耗时两天，已经查到原因，对于StreamSend中的 QUIC_BUFFER*参数
在某个不经意的角落，不小心修改了其Length成员，导致MsQuic发送异常，继而
数据解析异常
 * 
 */
typedef struct netio_header {
    uint8_t mode;//0-服务端，1-客户端
    uint32_t type;//报文类型 ，关于什么的报文
    uint64_t seq;
    uint64_t len;//内容的尺寸，不含头（netio_header)
}NIHeader,*pNIHeader;

/**
 * 做这样一种设想
 * 在前期模式，设计为REQUEST 和 RESPONSE 完成参数交互。
 * 对于抓屏，先请求参数，
 * client --> server : 请求 目标的分辨率，设置帧率
 * server --> client : ① 回复参数，设定定时器
 *                       
 *                     ② 屏幕压缩数据
 *                     ③ 屏幕压缩数据
 *                     .............
 * 
 * client --> server : 解析参数，分辨率，帧率
 *                     绑定数据交互缓冲 stream->recv_buffer / &recv_buffer_size / &write_offset / &read_offset 暴露给上层应用
 *                     根据帧率设置帧更新的定时器（上层应用）,上层应用 定期 从绑定的数据交互缓冲中读取数据，更新 read_offset
 *                     网络程序持续在RECV中写入数据交互缓冲，更新 write_offset
 *  
 * 客户端定时器到期检查当前是否有可读取的帧数据，如果有，读取，写入目标DIBits，并更新绘图，调整 read_offset
 * 服务端定时器：采集DIBits，压缩，使用建立的流发送 压缩数据                 
 * 
 * 服务端没有控件窗口句柄，客户端有用于现实的控件句柄
 * 
 * 服务端（源）有定时器，用于定时采集数据，压缩，并发送
 * 客户端有定时器，用于定时读取采集数据，解压，并显示
 * 
 * 服务端有 取数据的数据源，读写指针位置
 * 客户端有 写数据的数据源，读写指针位置
 * 
 * 服务端有压缩后的数据缓冲对接网络
 * 客户端有解压后的数据缓冲，对接DIBits 
 *
 * 
 */

#define RECV_PACK_PROCESS_PARAM_INVALID -1
#define RECV_PACK_PROCESS_SUCCESS 0
#define RECV_PACK_PROCESS_PENDING 1 //数据未到齐
#define RECV_PACK_PROCESS_NOPACKCALLBACK   2 //尚未挂载pack处理的回调函数
#define RECV_PACK_PROCESS_FAILED 3 //pack回调处理失败

typedef struct pack_ctrl_param {
    uint32_t width;
    uint32_t height;
    uint32_t quality;
    uint32_t frequency;

}CtrlParam,*pCtrlParam;

int stream_recv_pack_dispath(uint8_t* buffer,size_t size,void* context);
int client_connect_first_hello_frameproj(pstream_io stream,void* context);

/**
 * 差异分块传输模式下的报文，
 * 包含块的组头，记录差异的块的数量，差异块的索引
 * 规则如下：
 * 块的数量为 uint32_t
 * 紧接着是uint64_t的数组，记录每个索引号
 */
typedef struct pack_partition_segparam {
    uint64_t segcounts;
    uint64_t* seg_indexes;
}PartitionParam,pPartitionParam;

typedef struct pack_partition_data {
    uint64_t seg_index;
    uint64_t seg_size;
    uint8_t* seg_data;
}PartitionData,*pPartitionData;

#endif

