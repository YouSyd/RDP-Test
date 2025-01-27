#include "netio.h"
#include <algorithm>
#include <assert.h>

QUIC_API_TABLE* quic_api=NULL;

char PROTOCOL_TRANSCODE_DESC[100][256]={
#define enum_names(enum_code,enum_desc,enum_name) \
enum_desc "(" enum_name ")" ,
    ENUM_NAMES_PROTOCOL_TRANSCODE
#undef enum_names
};

QUIC_API_TABLE* NetIo_InitialAPI() {
    if(QUIC_STATUS_SUCCESS!=MsQuicOpen2((const QUIC_API_TABLE**)&quic_api)) {
        return NULL;
    }
    return quic_api;
}

QUIC_API_TABLE* NetIo_GetAPI() {
    return quic_api;
}

void NetIo_ClearAPI(QUIC_API_TABLE* api) {
    if(api) {
        MsQuicClose(api);
    }
}

pnet_io NetIo_Initial() {
    int errcode=0;
    pnet_io n=(pnet_io)calloc(sizeof(net_io),1);
    if(!n) return NULL;

    //防止内存异常
    new(&n->conns) std::list<pconn_io>();

    QUIC_REGISTRATION_CONFIG RegConfig={ 
        "quicsample", 
        QUIC_EXECUTION_PROFILE_LOW_LATENCY 
    };

    QUIC_API_TABLE* api=NetIo_GetAPI();
    if(!api) {
        errcode=-1;
        printf("Access QUIC API Table failed.\n");
        goto NETIO_INITIAL_EXIT;
    }

    if(QUIC_STATUS_SUCCESS!=api->RegistrationOpen(&RegConfig,&n->registration)) {
        errcode=-1;
        printf("%s failed.\n","RegistrationOpen");
        goto NETIO_INITIAL_EXIT;
    }
NETIO_INITIAL_EXIT:
    if(errcode!=0) {
        if(n) {
            //if(n->msquic) MsQuicClose(n->msquic);
            //n->msquic=NULL;

            if(n->registration&&api) {
                api->RegistrationClose(n->registration);
            }

            free(n);
        }
    }

    return (errcode==0)?n:NULL;
}

void NetIo_Clear(pnet_io net) {
    /**
     * net 分为 server 和 client
     * 对于server , 需要停掉 listener / conns 
     */
    // QUIC_API_TABLE* api=NetIo_GetAPI();
    // if(!api) return ;
    QUIC_API_TABLE* api=NetIo_GetAPI();
    if(!api) return;

    if(net->listener) {
        api->ListenerStop(net->listener);
        api->ListenerClose(net->listener);
        net->listener=NULL;
    }

    if(net->conns.size()>0) {
        for(auto it=net->conns.begin();it!=net->conns.end();) {
            ConnIo_Clear(*it);
            it=net->conns.erase(it);
        }
    }
}

int NetIo_SetAlpn(pnet_io net,char* alpn) {
    QUIC_BUFFER* buff=(QUIC_BUFFER*)net->alpn_buff;
    buff->Length=strlen(alpn);
    buff->Buffer=net->alpn_buff+sizeof(QUIC_BUFFER);
    strcpy((char*)buff->Buffer,alpn);

    return 0;
}

int NetIo_SetListenPort(pnet_io net,int port) {
    if(net&&net->mode==0) {
        net->port=port;
    }
    return 0;
}

int NetIo_SetConnectAddr(pnet_io net,char* ip,int port) {
    if(net&&net->mode==1) {
        strcpy(net->svr_ip,ip);
        net->port=port;
    }
    return 0;
}

int NetIo_SetContext(pnet_io net,void* context) {
    if(!net||!context) return -1;

    net->context=context;
    return 0;
}

int NetIo_SetHelloCallback(pnet_io net,stream_connect_first_hello_callback first_hello_callback) {
    if(!net||net->mode!=1) return -1;

    net->client_hello_callback=first_hello_callback;
    return 0;
}

int NetIo_LaunchServerListener(pnet_io net,int port) {
    int errcode=0;
    QUIC_API_TABLE* api=NetIo_GetAPI();

    if(!api||!net) {
        printf("Parameters Invalid.");
        return -1;
    }

    net->listener_callback=server_listener_callback;

    QUIC_ADDR addr={0};
    QuicAddrSetFamily(&addr, QUIC_ADDRESS_FAMILY_UNSPEC);
    QuicAddrSetPort(&addr,net->port);

    if(QUIC_STATUS_SUCCESS!=api->ListenerOpen(net->registration,net->listener_callback,net,&net->listener)) {
        errcode=-1;
        printf("ListenerOpen Failed.\n");
        goto NETIO_LAUNCHSVR_LISTENER_EXIT;
    }

    if(QUIC_STATUS_SUCCESS!=api->ListenerStart(net->listener,(const QUIC_BUFFER*)net->alpn_buff,1,&addr)) {
        errcode=-1;
        printf("ListenerStart failed.\n");
        goto NETIO_LAUNCHSVR_LISTENER_EXIT;
    }

NETIO_LAUNCHSVR_LISTENER_EXIT:
    if(errcode!=0) {
        if(net->listener) {
            api->ListenerClose(net->listener);
            net->listener=NULL;
        }
    }

    return errcode;
}

/**
 * NetIo_LaunchServer 调用前，需要设置 alpn_buff 、listener_callback 、port 等等等
 */
int NetIo_LaunchServer(pnet_io net,int port,char* certfile,char* keyfile) {
    int errcode=0;
    QUIC_API_TABLE* api=NetIo_GetAPI();
    if(!net||net->mode!=0||!api) {
        printf("Parameters Invalid.");
        return -1;
    }

    net->settings.IdleTimeoutMs=15000;
    net->settings.IsSet.IdleTimeoutMs=TRUE;
    net->settings.ServerResumptionLevel=QUIC_SERVER_RESUME_AND_ZERORTT;
    net->settings.IsSet.ServerResumptionLevel=TRUE;
    net->settings.PeerBidiStreamCount=10;
    net->settings.IsSet.PeerBidiStreamCount=TRUE;

    const char* Cert=certfile;
    const char* KeyFile=keyfile;
    QUIC_CREDENTIAL_CONFIG crt_cfg;
    memset(&crt_cfg, 0, sizeof(crt_cfg));
    crt_cfg.Flags = QUIC_CREDENTIAL_FLAG_NONE;
    crt_cfg.Type=QUIC_CREDENTIAL_TYPE_CERTIFICATE_FILE;
    QUIC_CERTIFICATE_FILE cf={KeyFile,Cert};
    crt_cfg.CertificateFile=&cf;

    if(net->alpn_buff[0]=='\0') {
        //default
        NetIo_SetAlpn(net,"h2");
    }

    if(QUIC_STATUS_SUCCESS!=api->ConfigurationOpen(net->registration,(QUIC_BUFFER*)net->alpn_buff,1,&net->settings,sizeof(net->settings),net,&net->cfg)) {
        errcode=-1;
        printf("ConfigurationOpen failed.\n");
        goto NETIO_LAUNCHSVR_EXIT;
    }

    if(QUIC_STATUS_SUCCESS!=api->ConfigurationLoadCredential(net->cfg,&crt_cfg)) {
        errcode=-1;
        printf("ConfigurationLoadCredential failed.\n");
        goto NETIO_LAUNCHSVR_EXIT;
    }

    NetIo_SetListenPort(net,port);
    if(0!=NetIo_LaunchServerListener(net,net->port)) {
        errcode=-1;
        printf("NetIo_LaunchServerListener failed.\n");
        goto NETIO_LAUNCHSVR_EXIT;
    }

    printf("Launch server on port %d...\n",port);

NETIO_LAUNCHSVR_EXIT: 
    if(errcode!=0) {
        if(net->listener) {
            api->ListenerStop(net->listener);
            api->ListenerClose(net->listener);
            net->listener=NULL;
        }

        if(net->cfg) {
            api->ConfigurationClose(net->cfg);
            net->cfg=NULL;
        }
    }

    return errcode;
}

int NetIo_LaunchClient(pnet_io net,char* svr_ip,int port,QUIC_CONNECTION_CALLBACK_HANDLER conn_callback) {
    int errcode=0;
    
    QUIC_API_TABLE* api=NetIo_GetAPI();
    if(!net||!api) {
        printf("Parameters Invalid.");
        return -1;
    }

    net->mode=1;
    NetIo_SetConnectAddr(net,svr_ip,port);

    net->settings.IdleTimeoutMs=15000;//5秒超时
    net->settings.IsSet.IdleTimeoutMs=TRUE;

    if(net->alpn_buff[0]=='\0') {
        //default
        NetIo_SetAlpn(net,"h2");
    }

    if(QUIC_STATUS_SUCCESS!=api->ConfigurationOpen(net->registration,(const QUIC_BUFFER*)net->alpn_buff,1,&net->settings,sizeof(net->settings),net,&net->cfg)) {
        errcode=-1;
        printf("ConfigurationOpen failed.\n");
        goto NETIO_LAUNCHCLT_EXIT;
    }

    QUIC_CREDENTIAL_CONFIG crt_cfg;
    memset(&crt_cfg,0x00,sizeof(crt_cfg));
    crt_cfg.Type=QUIC_CREDENTIAL_TYPE_NONE;
    crt_cfg.Flags=QUIC_CREDENTIAL_FLAG_CLIENT;
    crt_cfg.Flags|=QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION;
    if(QUIC_STATUS_SUCCESS!=api->ConfigurationLoadCredential(net->cfg,&crt_cfg)) {
        errcode=-1;
        printf("ConfigurationLoadCredential failed.\n");
        goto NETIO_LAUNCHCLT_EXIT;
    }

    //启动一条连接线路
    pconn_io c=ConnIo_Initial(net,conn_callback);
    errcode=NetIo_AddConn(net,c);

NETIO_LAUNCHCLT_EXIT:
    if(errcode!=0) {
        if(net->cfg) {
            api->ConfigurationClose(net->cfg);
            net->cfg=NULL;
        }

        if(net->conns.size()>0) {
            /**
             * 释放连接
             */
            if(c) api->ConnectionClose(c->conn);

            auto it=std::find(net->conns.begin(),net->conns.end(),c);
            if(it!=net->conns.end()) {
                ConnIo_Clear(c);
                net->conns.erase(it);
            }
        }
    }

    return errcode;
}

int NetIo_AddConn(pnet_io net,pconn_io conn) {
    /*
     * 检查连接是否已经存在
     */
    if(!net||!conn) return -1;

    auto it=std::find(net->conns.begin(),net->conns.end(),conn);
    if(it==net->conns.end()) {
        net->conns.push_back(conn);
        if(conn->net!=net) conn->net=net;
        return 0;
    } else {
        return -1;
    }
}

int NetIo_RemoveConn(pnet_io net,pconn_io conn) {
    if(!net||!conn) return -1;
    auto it=std::find(net->conns.begin(),net->conns.end(),conn);
    if(it==net->conns.end()) {
        return -1;
    } else {
        net->conns.erase(it);
        return 0;
    }
}

pconn_io NetIo_GetConn(pnet_io net,HQUIC conn) {
    if(!net||!conn) return NULL;

    //这个效率有问题，需要优化
    for(auto it=net->conns.begin();it!=net->conns.end();it++) {
        if((*it)->conn==conn) return *it;
    }

    return NULL;
}

/**
 * 
 * 对于client端的conn,需要配置Open/Start
 * 对于server的conn,是msquic自己处理的，在 listener的 QUIC_LISTENER_EVENT_NEW_CONNECTION 事件中，
 * 会返回一个已经包装好的 conn, 不需要 Open/Start ， 只需要设置 conn的 cfg
 * 所以该函数只在client中调用
 */
pconn_io ConnIo_Initial(pnet_io net,QUIC_CONNECTION_CALLBACK_HANDLER conn_callback) {
    int errcode=0;
    
    QUIC_API_TABLE* api=NetIo_GetAPI();
    if(!api) return NULL;
    pconn_io c=(pconn_io)calloc(sizeof(conn_io),1);
    if(!c) return NULL;

    new(&c->streams) std::list<pstream_io>();

    c->conn_callback=conn_callback;
    
    //客户端模式
    if(net->mode==1) {
        //创建一个conn
        if(QUIC_STATUS_SUCCESS!=api->ConnectionOpen(net->registration,c->conn_callback,net,&c->conn)) {
            errcode=-1;
            printf("ConnectionOpen failed.\n");

            goto CONN_INITIAL_EXIT;
        }

        QUIC_STATUS status=api->ConnectionStart(c->conn,net->cfg,QUIC_ADDRESS_FAMILY_UNSPEC,net->svr_ip,net->port);
        if(QUIC_STATUS_SUCCESS!=status&&QUIC_STATUS_PENDING!=status) {
            errcode=-1;
            printf("ConnectionStart(%s:%d) failed.\n",net->svr_ip,net->port);

            goto CONN_INITIAL_EXIT;
        }

    }

CONN_INITIAL_EXIT:
    if(errcode!=0) {
        if(c->conn) {
            //net->msquic->ConnectionShutdown(...)
            api->ConnectionClose(c->conn);
            c->conn=NULL;
        }
    }

    return (errcode==0)?c:NULL;
}

pconn_io ConnIo_Initial(pnet_io net,HQUIC conn,QUIC_CONNECTION_CALLBACK_HANDLER conn_callback) {
    int errcode=0;
    
    QUIC_API_TABLE* api=NetIo_GetAPI();
    if(!api) return NULL;
    pconn_io c=(pconn_io)calloc(sizeof(conn_io),1);
    if(!c) return NULL;

    new(&c->streams) std::list<pstream_io>();
    c->conn=conn;
    c->conn_callback=conn_callback;
    c->net=net;
    
    api->SetCallbackHandler(c->conn,c->conn_callback,net);
    
    return c;
}
int ConnIo_Clear(pconn_io conn) {
    QUIC_API_TABLE* api=NetIo_GetAPI();
    if(!api) return -1;
    if(conn->streams.size()>0) {
        for(auto it=conn->streams.begin();it!=conn->streams.end();) {
            StreamIo_Clear(*it);
            it=conn->streams.erase(it);
        }
    }
    api->ConnectionClose(conn->conn);
    api->ConfigurationClose(conn->conn_cfg);

    return 0;
}

int ConnIo_AddStream(pconn_io conn,pstream_io stream) {
    /*
     * 检查流是否已经存在
     */
    if(!stream||!conn) return -1;

    auto it=std::find(conn->streams.begin(),conn->streams.end(),stream);
    if(it==conn->streams.end()) {
        conn->streams.push_back(stream);
        if(stream->conn!=conn) stream->conn=conn;

        if(stream->net!=conn->net) stream->net=conn->net;

        stream->context=stream->net->context;
        return 0;
    } else {
        return -1;
    }
}

int ConnIo_RemoveStream(pconn_io conn,pstream_io stream) {
    if(!stream||!conn) return -1;
    auto it=std::find(conn->streams.begin(),conn->streams.end(),stream);
    if(it==conn->streams.end()) {
        return -1;
    } else {
        conn->streams.erase(it);
        return 0;
    }
}

pstream_io ConnIo_GetStream(pconn_io conn,HQUIC stream) {
    if(!conn||!stream) return NULL;

    for(auto it=conn->streams.begin();it!=conn->streams.end();it++) {
        if(stream==(*it)->stream) {
            return *it;
        }
    }

    return NULL;
}

int ConnIo_SetCfg(pconn_io conn,int mode) {
    QUIC_API_TABLE* api=NetIo_GetAPI();
    if(mode==0) api->ConnectionSetConfiguration(conn->conn,conn->net->cfg);
    return 0;
}

/**
 * 该函数主动创建一个流
 */
pstream_io StreamIo_Initial(pconn_io conn, QUIC_STREAM_CALLBACK_HANDLER stream_callback) {
    int errcode=0;
    pstream_io s=(pstream_io)calloc(sizeof(stream_io),1);
    if(!s) return NULL;

    QUIC_API_TABLE* api=NetIo_GetAPI();
    if(!api) return NULL;

    s->stream_callback=stream_callback;
    s->recv_callback=stream_recv_pack_dispath;
    s->recv_offset=0;
    
    s->send_buffer_size=1024*5;
    s->send_buffer=(uint8_t*)malloc(s->send_buffer_size);
    //为流设置收发缓冲
    //s->recv_buffer? //当收到数据的时候，如果接收缓冲为空，会指定收发缓冲
    //s->send_buffer?

    s->quic_buffer_size=100;
    s->quic_buffer_list=(uint8_t*)malloc(s->quic_buffer_size*sizeof(QUIC_BUFFER));
    s->quic_buffer_flags=(uint8_t*)calloc(sizeof(uint8_t)*s->quic_buffer_size,1);
    
    /**
     * 对于server,对于被动型的B/S模式（B请求-S响应），stream 似乎不需要创建，在conn_callback中有一个 
     * QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED 事件，会传入对端流，此时为
     * 该流设置stream_callback,
     * 在stream_callback的参数中获取
     * 即有对端的数据进来，获取其stream，在该 stream上SendStream
     * 
     * 对于client，在连接创立的时候QUIC_CONNECTION_EVENT_CONNECTED，
     * 手工创建一个流，主动发送消息
     * 
     * 理论上server 应该也可以主动创建流，并向client 推送 信息
     * 逻辑似乎是这样：
     * 一端主动创建流，发送消息，peer收到消息，自动包装出stream，送入peer的 stream_callback中，
     * 以参数传入。
     * 所以任意一端创建出流，另一端会自动生成流
     * 
     * 流可以主动创建，也可以由msquic封装生成，但都需要主动释放，且都在其生命周期内保持同一个句柄
     * 另外，当连接释放时，连接上的所有未释放的流会被自动释放
     * 也就是说msquic 会 维护 连接及 流的映射关系，但并不公开，fuck.
     */
    if(QUIC_STATUS_SUCCESS!=api->StreamOpen(conn->conn,QUIC_STREAM_OPEN_FLAG_NONE,stream_callback,conn,&s->stream)) {
        errcode=-1;
        goto STREAMIO_INITIAL_EXIT;
    }

    QUIC_STATUS ret=api->StreamStart(s->stream,QUIC_STREAM_START_FLAG_NONE);
    if(QUIC_STATUS_SUCCESS!=ret&&QUIC_STATUS_PENDING!=ret) {
        errcode=-1;
        goto STREAMIO_INITIAL_EXIT;
    }

STREAMIO_INITIAL_EXIT:
    if(errcode!=0) {
        
        if(s) free(s);
    }
    return (errcode==0)?s:NULL;
}

pstream_io StreamIo_Initial(pconn_io conn,HQUIC stream,QUIC_STREAM_CALLBACK_HANDLER stream_callback) {
    int errcode=0;
    pstream_io s=(pstream_io)calloc(sizeof(stream_io),1);
    if(!s) return NULL;

    QUIC_API_TABLE* api=NetIo_GetAPI();
    if(!api) return NULL;

    s->send_buffer_size=1024*5;
    s->send_buffer=(uint8_t*)malloc(s->send_buffer_size);

    s->quic_buffer_size=100;
    s->quic_buffer_list=(uint8_t*)malloc(s->quic_buffer_size*sizeof(QUIC_BUFFER));
    s->quic_buffer_flags=(uint8_t*)calloc(sizeof(uint8_t)*s->quic_buffer_size,1);
    
    s->stream=stream;
    s->stream_callback=stream_callback;
    api->SetCallbackHandler(stream,s->stream_callback,conn);
    s->recv_offset=0;
    s->conn=conn;
    return s;
}

/**
 * 所有stream 需要主动释放
 * 注意：无论内部分配的内存还是外部的指定内存，流释放会自动释放 recv_buffer，切勿重复释放
 * 为了方便使用，你只管分配就是了
 */
int StreamIo_Clear(pstream_io stream) {
    if(!stream) return 0;

    QUIC_API_TABLE* api=NetIo_GetAPI();
    if(!api) return -1;

    if(stream->send_buffer) {
        free(stream->send_buffer);
        stream->send_buffer=NULL;
    }

    if(stream->recv_buffer) {
        free(stream->recv_buffer);
        stream->recv_buffer=NULL;
    }

    if(stream->quic_buffer_flags) {
        free(stream->quic_buffer_flags);
        stream->quic_buffer_flags=NULL;
    }

    if(stream->quic_buffer_list) {
        free(stream->quic_buffer_list);
        stream->quic_buffer_list=NULL;
    }

    api->StreamClose(stream->stream);
    
    free(stream);
    return 0;
}

/**
 * void* recv 为对接msquic的数据结构，拷贝其 流中事件 QUIC_STREAM_EVENT_RECEIVE的数据
 * 具体格式如下：
 * // Event->RECEIVE: {
 *       // Event->RECEIVE.AbsoluteOffset;
 *       // Event->RECEIVE.BufferCount;
 *       // Event->RECEIVE.Buffers;
 *       // Event->RECEIVE.Flags;
 *       // Event->RECEIVE.TotalBufferLength
 * // }
 * 这里只更新 write_offset
 */
int StreamIo_CopyData(pstream_io stream,uint64_t total_size,const QUIC_BUFFER* buffers,uint32_t buffercount,QUIC_RECEIVE_FLAGS flags) {
    if(!stream||!buffers||total_size<=0) return -1;
    printf("recv %d bytes\n",total_size);
    if(!stream->recv_buffer)  {
        //StreamIo_SetExternalBuffer(stream,)
        stream->recv_buffer_size=1024*500;
        stream->recv_buffer=(uint8_t*)malloc(stream->recv_buffer_size);
        stream->recv_offset=0;
        stream->write_offset=stream->read_offset=0;
        /**
         * 从 从 [read_offset,write_offset)之间是可读的
         * 0. 约定缓冲区可用大小 为 (recv_buffer_size-1) ，即 recv_offset < recv_buffer_size. 否则就超了
         * 1. 当 (write_offset+1)%recv_buffer_size==read_offset ，表示写满了
         * 2. read_offset==write_offset 表示读完了
         * 3. 当前可读，取决于 read_offset 和 write_offset的值
         *      3.1 当 write_offset > read_offset 时， write_offset-read_offset 即为可读
         *      3.2 当 write_offset < read_offset 时， write_offset+recv_buffer_size-read_offset 即为可读
         *      3.3 当 write_offset = read_offset 时， 表示没有数据可读
         */
    }

    if(!stream->recv_buffer) return -1;

    size_t check_size=0;
    for(int idx=0;idx<buffercount;idx++) {
        //这里得改，会修改write_offset
        /**
         * 首先判断不会超
         * 当前写入的位置 需要修正为 write_offset
         * 如果 write_offset
         */
        if(stream->recv_size+buffers[idx].Length<stream->recv_buffer_size) {
            /**
             * 判断是否需要分段写入
             */
            if(stream->write_offset+buffers[idx].Length<stream->recv_buffer_size) {
                memcpy((uint8_t*)stream->recv_buffer+stream->write_offset,buffers[idx].Buffer,buffers[idx].Length);
                stream->write_offset+=buffers[idx].Length;
                stream->recv_size+=buffers[idx].Length;
            } else {
                /**
                 * 分段写入
                 */
                int p1_len=stream->recv_buffer_size-stream->write_offset;
                int p2_len=buffers[idx].Length-p1_len;

                memcpy((uint8_t*)stream->recv_buffer+stream->write_offset,buffers[idx].Buffer,p1_len);
                stream->write_offset=(stream->write_offset+p1_len)%stream->recv_buffer_size;//should be 0!
                memcpy((uint8_t*)stream->recv_buffer+stream->write_offset,buffers[idx].Buffer+p1_len,p2_len);
                stream->write_offset+=p2_len;
                stream->recv_size+=buffers[idx].Length;

                printf("触发分段写入 ， write_offset %d , QUIC_BUFFER::Length %d , 分割为 %d + %d , recv_size %d\n",
                    stream->write_offset,
                    buffers[idx].Length,p1_len,p2_len,stream->recv_size
                );
            }

            check_size+=buffers[idx].Length;
        } else {
            //缓冲区不足，出差错了.
            printf("buffer insufficient! recv_size %d , QUIC_BUFFER::Length %d (index %d)\n",stream->recv_size,buffers[idx].Length,idx);
            break;
        }
    }

    if(check_size!=total_size) {
        printf("StreamCopyData的 check size %d 与 total size %d 不一致\n",check_size,total_size);
    }

    if(QUIC_RECEIVE_FLAG_FIN&flags) { //stream->recv_pending=1;
        //需要关闭流
        QUIC_API_TABLE* api=NetIo_GetAPI();
        if(api) api->StreamShutdown(stream->stream,QUIC_STREAM_SHUTDOWN_FLAG_ABORT,0);
    }

    return 0;
}

/**
 * 设置外接的recv buffer.
 * 通常为上层应用的数据缓冲
 */
int StreamIo_SetExternalBuffer(pstream_io stream,uint8_t* buffer,size_t size) {
    if(!stream||!buffer) return -1;

    stream->recv_buffer=buffer;
    stream->recv_size=size;
    stream->recv_offset=0;
    stream->recv_pending=0;

    return 0;
}

/**
 * 无论同步异步，buffer必须保证在整个发送过程中有效，发送结束前不得销毁
 */
int StreamIo_SendData(pstream_io stream,uint8_t* buffer,size_t size) {
    if(!stream||!buffer&&size<=0) return -1;

    if(!stream->send_buffer) {
        stream->send_buffer_size=1024;
        stream->send_buffer=(uint8_t*)malloc(stream->send_buffer_size);
    }

    if(!stream->send_buffer) return -1;

    QUIC_BUFFER* buff=NULL;
    for(int idx=0;idx<stream->quic_buffer_size;idx++) {
        if(stream->quic_buffer_flags[idx]==0) {
            buff=(QUIC_BUFFER*)(stream->quic_buffer_list+sizeof(QUIC_BUFFER)*idx);
            stream->quic_buffer_flags[idx]=1;//占用
            buff->Buffer=buffer;
            buff->Length=size;

            QUIC_API_TABLE* api=NetIo_GetAPI();
            QUIC_STATUS status=api->StreamSend(stream->stream,(const QUIC_BUFFER*)buff,1,QUIC_SEND_FLAG_NONE,buff);
            if(status!=QUIC_STATUS_SUCCESS&&status!=QUIC_STATUS_PENDING) {
                printf("StreamSend失败.\n");
                return -1;
            }
            return 0;
        }
    }

    printf("StreamSend的发送缓冲QUIC_BUFFER list 已经耗尽,发送失败");
    return -1;
}


int StreamIo_SendData(pstream_io stream,uint8_t** buffers,size_t* sizes,int count) {
    if(!stream||!buffers&&!sizes||count<=0) return -1;

    if(!stream->send_buffer) {
        stream->send_buffer_size=1024;
        stream->send_buffer=(uint8_t*)malloc(stream->send_buffer_size);
    }

    if(!stream->send_buffer) return -1;

    QUIC_BUFFER* buff=NULL;
    for(int idx=count-1;idx<stream->quic_buffer_size;idx++) {
        int pre_idx=idx;
        for(int i=idx-count;i<=idx;i++) {
            if(stream->quic_buffer_flags[i]!=0) {
                idx=i+count;
                break;
            }
        }

        if(idx!=pre_idx) continue;
        else {
            int quic_start_idx=idx-(count-1);
            QUIC_BUFFER* qbuff=(QUIC_BUFFER*)(stream->quic_buffer_list+sizeof(QUIC_BUFFER)*quic_start_idx);
            for(int quic_idx=quic_start_idx;quic_idx<count+quic_start_idx;quic_idx++) {
                qbuff[quic_idx].Buffer=buffers[quic_idx-quic_start_idx];
                qbuff[quic_idx].Length=sizes[quic_idx-quic_start_idx];
            }

            QUIC_API_TABLE* api=NetIo_GetAPI();
            QUIC_STATUS status=api->StreamSend(stream->stream,(const QUIC_BUFFER*)qbuff,count,QUIC_SEND_FLAG_NONE,buff);
            if(status!=QUIC_STATUS_SUCCESS&&status!=QUIC_STATUS_PENDING) {
                printf("StreamSend失败.\n");
                return -1;
            }
            return 0;
        }
    }

    printf("StreamSend的发送缓冲QUIC_BUFFER list 已经耗尽,发送失败");
    return -1;
}

int StreamIo_SetRecvCallBack(pstream_io stream,stream_recv_data_callback recv_callback) {
    if(!stream) return -1;

    stream->recv_callback=recv_callback;
    return 0;
}

QUIC_STATUS client_stream_callback(
    _In_ HQUIC Stream,
    _In_opt_ void* Context,
    _In_ QUIC_STREAM_EVENT* Event
    ) {
    switch (Event->Type) {
        case QUIC_STREAM_EVENT_SEND_COMPLETE: {
            pconn_io conn=(pconn_io)Context;
            pstream_io stream=ConnIo_GetStream(conn,Stream);
            if(stream&&stream->quic_buffer_list&&stream->quic_buffer_flags) {
                stream->send_pending=1;
                int quic_buffer_idx=((uint8_t*)Event->SEND_COMPLETE.ClientContext-stream->quic_buffer_list)/sizeof(QUIC_BUFFER);
                if(quic_buffer_idx>=0&&quic_buffer_idx<stream->quic_buffer_size) {
                    stream->quic_buffer_flags[quic_buffer_idx]=0;
                }
            }
        } break;
        case QUIC_STREAM_EVENT_RECEIVE: {
            pconn_io conn=(pconn_io)Context;
            printf("client stream 0x%08x receive data ... Under conn 0x%08x\n",Stream,conn);
            pstream_io stream=ConnIo_GetStream(conn,Stream);
            if(stream) {
                StreamIo_CopyData(stream,Event->RECEIVE.TotalBufferLength,Event->RECEIVE.Buffers,Event->RECEIVE.BufferCount,Event->RECEIVE.Flags);
                if(stream->recv_callback) stream->recv_callback(stream->recv_buffer,stream->recv_size,stream);
                else stream_recv_pack_dispath(stream->recv_buffer,stream->recv_size,stream);
            }
        } break;
        case QUIC_STREAM_EVENT_PEER_SEND_ABORTED: {
            printf("peer 端已经丢失流的连接，本地流将关闭...\n");
            QUIC_API_TABLE* api=NetIo_GetAPI();

            api->StreamShutdown(Stream,QUIC_STREAM_SHUTDOWN_FLAG_ABORT,0);
        } break;
        case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN: {
            printf("peer send shutdown .\n");
            //PEER 发送了 FIN
            //对方发送完毕了，你可以动手了.
            //根据stream 查找其对应的 stream_io , 并调用 recv_callback.
            // context 为 stream 对应的 conn
            pconn_io conn=(pconn_io)Context;
            pstream_io stream=ConnIo_GetStream(conn,Stream);
            if(stream) {
                if(stream->recv_callback) stream->recv_callback(stream->recv_buffer,stream->recv_size,stream->context);
            }
        } break;
        case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE: {
            pconn_io conn=(pconn_io)Context;
            pstream_io stream=ConnIo_GetStream(conn,Stream);
            if(stream) {
                ConnIo_RemoveStream(conn,stream);
            }
        } break;
        default:
            break;
    }
    return QUIC_STATUS_SUCCESS;
}

QUIC_STATUS client_connection_callback(
    _In_ HQUIC Connection,
    _In_opt_ void* Context,
    _In_ QUIC_CONNECTION_EVENT* Event
    ) {
    
    QUIC_API_TABLE* api=NetIo_GetAPI();
    switch (Event->Type) {
        case QUIC_CONNECTION_EVENT_CONNECTED: {
            printf("Handshake done for the connection.\n");
            pnet_io net=(pnet_io)Context;
            if(net) {
                pconn_io conn=NetIo_GetConn(net,Connection);
                if(conn) {
                    pstream_io stream=StreamIo_Initial(conn,client_stream_callback);
                    ConnIo_AddStream(conn,stream);

                    if(stream->recv_callback) stream->recv_callback(NULL,0,stream);

                    if(stream->net->client_hello_callback) stream->net->client_hello_callback(stream,stream->net);
                }
            }
        } break;
        case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE: {
            printf("连接资源已经释放\n");
            pnet_io net=(pnet_io)Context;
            if(net) {
                pconn_io conn=NetIo_GetConn(net,Connection);
                if(conn) {
                    NetIo_RemoveConn(net,conn);
                }
            }
        } break;
        case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT: {
            printf("心跳超时，连接中断或网络异常，连接已丢失\n");
            
            //根据Connection找到 netio
            pnet_io net=(pnet_io)Context;
            if(net) {
                pconn_io conn=NetIo_GetConn(net,Connection);
                if(conn) {
                    NetIo_RemoveConn(net,conn);
                }
            }
        } break;
        default: break;
    }
    return QUIC_STATUS_SUCCESS;
}

QUIC_STATUS server_stream_callback(
    _In_ HQUIC Stream,
    _In_opt_ void* Context,
    _In_ QUIC_STREAM_EVENT* Event
    ) {
    QUIC_API_TABLE* api=NetIo_GetAPI();

    switch (Event->Type) {
        case QUIC_STREAM_EVENT_SEND_COMPLETE: {
            pconn_io conn=(pconn_io)Context;
            pstream_io stream=ConnIo_GetStream(conn,Stream);
            if(stream&&stream->quic_buffer_list&&stream->quic_buffer_flags) {
                stream->send_pending=1;
                int quic_buffer_idx=(QUIC_BUFFER*)(Event->SEND_COMPLETE.ClientContext)-(QUIC_BUFFER*)stream->quic_buffer_list;
                if(quic_buffer_idx>=0&&quic_buffer_idx<stream->quic_buffer_size) {
                    stream->quic_buffer_flags[quic_buffer_idx]=0;
                }
            }
        } break;
        case QUIC_STREAM_EVENT_RECEIVE: {
            pconn_io conn=(pconn_io)Context;
            printf("server stream 0x%08x receive data... Under conn 0x%08x\n",Stream,conn);
            pstream_io stream=ConnIo_GetStream(conn,Stream);
            if(stream) {
                StreamIo_CopyData(stream,Event->RECEIVE.TotalBufferLength,Event->RECEIVE.Buffers,Event->RECEIVE.BufferCount,Event->RECEIVE.Flags);
            
                //收到数据需要处理,处理之后stream->recv_size 要修正
                if(stream->recv_callback) stream->recv_callback(stream->recv_buffer,stream->recv_size,stream);
                else stream_recv_pack_dispath(stream->recv_buffer,stream->recv_size,stream);
            }
        } break;
        case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN: {
            //表示对端结束了数据发送，意味着服务端可以开始发送数据了
            printf("peer send shutdown.\n");
            pconn_io conn=(pconn_io)Context;
            pstream_io stream=ConnIo_GetStream(conn,Stream);
            if(stream) {
                stream->recv_callback(stream->recv_buffer,stream->recv_size,stream);
            }
        } break;
        case QUIC_STREAM_EVENT_PEER_SEND_ABORTED: {
            printf("peer 已经断开连接，本地流将关闭...\n");
            api->StreamShutdown(Stream, QUIC_STREAM_SHUTDOWN_FLAG_ABORT, 0);
        } break;
        case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE: {
            pconn_io conn=(pconn_io)Context;
            pstream_io stream=ConnIo_GetStream(conn,Stream);
            if(stream) {
                ConnIo_RemoveStream(conn,stream);
            }
        } break;
        default:
            break;
    }
    return QUIC_STATUS_SUCCESS;
}

QUIC_STATUS server_connection_callback(
    _In_ HQUIC Connection,
    _In_opt_ void* Context,
    _In_ QUIC_CONNECTION_EVENT* Event
    ) {
    QUIC_API_TABLE* api=NetIo_GetAPI();
    switch (Event->Type) {
        case QUIC_CONNECTION_EVENT_CONNECTED: {
            api->ConnectionSendResumptionTicket(Connection,QUIC_SEND_RESUMPTION_FLAG_NONE,0,NULL);

        } break;
        case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE: {
            printf("Connection shutdown complete.\n");
            pnet_io net=(pnet_io)Context;
            pconn_io conn=NetIo_GetConn(net,Connection);
            if(conn) {
                NetIo_RemoveConn(net,conn);
            }
        } break;
        case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED: {
            pnet_io net=(pnet_io)Context;
            pconn_io conn=NetIo_GetConn(net,Connection);
            if(conn) {
                printf("new stream 0x%08x created. Under conn 0x%08x\n",Event->PEER_STREAM_STARTED.Stream,conn);
                pstream_io stream=StreamIo_Initial(conn,Event->PEER_STREAM_STARTED.Stream,server_stream_callback);
                if(0!=ConnIo_AddStream(conn,stream)) {
                    printf("Add stream failed.\n");
                }
            }
        } break;
        case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT: {
            //客户端超时或其他异常
            printf("Lost connection , PEER timeout.\n");
            pnet_io net=(pnet_io)Context;
            pconn_io conn=NetIo_GetConn(net,Connection);
            if(conn) {
                NetIo_RemoveConn(net,conn);
            }
        } break;
        default:
            break;
    }
    return QUIC_STATUS_SUCCESS;
}


QUIC_STATUS server_listener_callback(
    _In_ HQUIC Listener,
    _In_opt_ void* Context,
    _Inout_ QUIC_LISTENER_EVENT* Event
    ) {
    QUIC_API_TABLE* api=NetIo_GetAPI();
    switch(Event->Type) {
    case QUIC_LISTENER_EVENT_NEW_CONNECTION: {
        //有新连接等待接入
        printf("new connection accessed...\n");
        pnet_io net=(pnet_io)Context;
        pconn_io conn=ConnIo_Initial(net,Event->NEW_CONNECTION.Connection,server_connection_callback);
        if(conn) {
            if(0!=NetIo_AddConn(net,conn)) {
                printf("ERROR-SVR_LISTENER:(svr QUIC_LISTENER_EVENT_NEW_CONNECTION) record new connection failed.\n");
            }
            api->SetCallbackHandler(conn->conn,server_connection_callback,net);
            ConnIo_SetCfg(conn,net->mode);
        }
    } break;
    case QUIC_LISTENER_EVENT_STOP_COMPLETE: {

    } break;
    }

    return QUIC_STATUS_SUCCESS;
}

int stream_recv_pack_dispath(uint8_t* buffer,size_t size,void* context) {
    pstream_io stream=(pstream_io)context;
    if(!stream) return RECV_PACK_PROCESS_PARAM_INVALID;

    if(size<=0) return RECV_PACK_PROCESS_PENDING;//没有缓冲数据，无需处理
    if(((stream->write_offset+stream->recv_buffer_size-stream->read_offset)%stream->recv_buffer_size)<sizeof(NIHeader)) return RECV_PACK_PROCESS_PENDING;//数据不对
    
    //获取报文头
    pNIHeader head=(pNIHeader)(stream->recv_buffer+stream->read_offset);
    uint8_t head_buff[256]={0};
    if(stream->recv_buffer_size<=(stream->read_offset+sizeof(NIHeader))) {
        //拼凑head;
        size_t p1=stream->recv_buffer_size-stream->read_offset;
        size_t p2=sizeof(NIHeader)-p1;
        memcpy(head_buff,stream->recv_buffer+stream->read_offset,p1);
        memcpy(&head_buff[p1],stream->recv_buffer,p2);
        head=(pNIHeader)&head_buff[0];
    }

    //取出报文内容并处理
    //取出数据,将数据写入缓冲，这里只更新 read_offset
    uint8_t* plts[2]={NULL};
    int lens[2]={0};
    int plts_count=0;
    int pack_len=0;
    //这里还需要加一个判断，当前数据片段是否完整?
    int bytes_avaliable=0;
    if(stream->write_offset>stream->read_offset) bytes_avaliable=stream->write_offset-stream->read_offset;
    else bytes_avaliable=stream->write_offset+stream->recv_buffer_size-stream->read_offset;
    if(bytes_avaliable<head->len+sizeof(NIHeader)) {
        //一个完整的分片尚未获取
        // 需要等待流收到更多数据
        printf("PACK DATA PENDING:当前PACK计划携带数据 %d bytes, 目前收到 %d bytes，仍需 %d bytes\n",head->len,bytes_avaliable-sizeof(NIHeader),head->len-(bytes_avaliable-sizeof(NIHeader)));
        return RECV_PACK_PROCESS_PENDING;

    } else {
        if(stream->read_offset+sizeof(NIHeader)+head->len<stream->recv_buffer_size) {
            //一段
            plts[0]=stream->recv_buffer+(stream->read_offset+sizeof(NIHeader));
            lens[0]=head->len;
            plts_count=1;
            pack_len=lens[0];
        } else {
            //两段
            plts[0]=stream->recv_buffer+(stream->read_offset+sizeof(NIHeader));
            lens[0]=stream->recv_buffer_size-stream->read_offset-sizeof(NIHeader);
            plts[1]=stream->recv_buffer;
            lens[1]=head->len-lens[0];
            plts_count=2;
            pack_len=lens[0]+lens[1];
        }

        //调用流上面的处置函数
        /**
         * 由于流都是内部创建的，参数只能从外部带入
         * 流的处置只能挂在net上？
         * 其实我的工作只需要将stream上的数据暴露出来，给到外部程序访问就可以了
         * 遇到一个完整的包就处理
         */
        if(stream->net->protocol_process) {
            printf("当前读写指针 read_offset %d , write_offset %d , recv_size %d , pack_len %d , NIHeader %d\n",stream->read_offset,stream->write_offset,stream->recv_size,pack_len,sizeof(NIHeader));
            int ret=stream->net->protocol_process(stream->net->context,head,plts,lens,plts_count,stream);//外部接口数据交互及处理
            
            //外部处理完了之后，内部处理
            //更新read指针和 recv_size;
            int readbytes=sizeof(NIHeader)+pack_len;
            stream->read_offset=(stream->read_offset+readbytes)%stream->recv_buffer_size;
            stream->recv_size-=readbytes;

            return (ret==0)?RECV_PACK_PROCESS_SUCCESS:RECV_PACK_PROCESS_FAILED;
        } else {
            return RECV_PACK_PROCESS_NOPACKCALLBACK;
        }
    }
}

int client_connect_first_hello_frameproj(pstream_io stream,void* context) {
    pnet_io net=(pnet_io)context;
    pNIHeader head=(pNIHeader)stream->send_buffer;
    head->type=REQ_CTRL_PARAM;
    head->mode=1;
    head->seq=0;
    head->len=0;

    return StreamIo_SendData(stream,(uint8_t*)head,sizeof(NIHeader));
}


