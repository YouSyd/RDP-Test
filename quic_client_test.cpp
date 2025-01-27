/**
 * MsQuic API 表
   └── Registration（注册实例）
         ├── Configuration（配置） 1  QUIC_REGISTRATION_CONFIG :主要包括 应用名称 和 执行模式（速度？延迟？系统资源？吞吐量？...)
         ├── Configuration（配置） 2
         └── Connection（连接实例）1
                ├── Configuration（特定连接的配置） 加密凭证、应用层协议、流控制、其它（超时时间、最大传输单元MTU,因为quic是基于UDP实现的)
                └── Stream（流实例）1, 2, ...
         └── Connection（连接实例）2
                ├── Configuration（特定连接的配置）
                └── Stream（流实例）1, 2, ...

 */

/**
 * 例子代码：
 * https://github.com/microsoft/msquic/blob/main/src/tools/sample/sample.c#L323
 */

/**
 * MsQuic Stream Recv 不支持指定用户的缓冲区，无法做到零拷贝
 */
#include "monitor.h"
#include "netio.h"
#include <stdio.h>
#include <stdlib.h>

#pragma comment(lib,"msquic.lib")
#pragma comment(lib,"netio.lib")

const QUIC_API_TABLE* MsQuic = NULL;
HQUIC Registration = NULL;
HQUIC Session = NULL;

_IRQL_requires_max_(DISPATCH_LEVEL)
_Function_class_(QUIC_STREAM_CALLBACK)
QUIC_STATUS QUIC_API ClientStreamCallback(
    _In_ HQUIC Stream,
    _In_opt_ void* Context,
    _In_ QUIC_STREAM_EVENT* Event
    ) {
    switch (Event->Type) {
        case QUIC_STREAM_EVENT_SEND_COMPLETE: {
            free(Event->SEND_COMPLETE.ClientContext);
        } break;
        case QUIC_STREAM_EVENT_RECEIVE:
            printf("Client received data: %.*s\n", (int)Event->RECEIVE.Buffers[0].Length, Event->RECEIVE.Buffers[0].Buffer);
            //Event->RECEIVE.BufferCount
            break;
        case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN: {
            //PEER 发送了 FIN
            //对方发送完毕了，你可以动手了.
        } break;
        case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
             if(!Event->SHUTDOWN_COMPLETE.AppCloseInProgress) MsQuic->StreamClose(Stream);
            break;
        default:
            break;
    }
    return QUIC_STATUS_SUCCESS;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Function_class_(QUIC_CONNECTION_CALLBACK)
QUIC_STATUS QUIC_API ClientConnectionCallback(
    _In_ HQUIC Connection,
    _In_opt_ void* Context,
    _In_ QUIC_CONNECTION_EVENT* Event
    ) {
    switch (Event->Type) {
        case QUIC_CONNECTION_EVENT_CONNECTED: {
            printf("Handshake done for the connection.\n");
            HQUIC Stream = NULL;
            MsQuic->StreamOpen(Connection, QUIC_STREAM_OPEN_FLAG_NONE, ClientStreamCallback, NULL, &Stream);
            MsQuic->StreamStart(Stream, QUIC_STREAM_START_FLAG_NONE);
            char Message[256]="Hello from client!";
            void* client_send_buffer=malloc(sizeof(QUIC_BUFFER)+strlen(Message));
            QUIC_BUFFER* CltSendBuff=(QUIC_BUFFER*)client_send_buffer;
            CltSendBuff->Buffer=(uint8_t*)client_send_buffer+sizeof(QUIC_BUFFER);
            CltSendBuff->Length=strlen(Message);
            strcpy((char*)CltSendBuff->Buffer,Message);

            MsQuic->StreamSend(Stream,CltSendBuff, 1, QUIC_SEND_FLAG_FIN,client_send_buffer);
            break;
        }
        case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
            printf("连接资源已经释放\n");
            MsQuic->ConnectionClose(Connection);
            break;
        case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT: {
            printf("心跳超时，连接中断或网络异常，连接已丢失\n");
        } break;
        default:
            break;
    }
    return QUIC_STATUS_SUCCESS;
}

int main() {
    QUIC_API_TABLE* api=NetIo_InitialAPI();
    pMonitor monitor=Monitor_New();
    pnet_io net=NetIo_Initial();
    NetIo_LaunchClient(net,"127.0.0.1",9537,client_connection_callback);
    net->protocol_process=pack_protocol_process;
    NetIo_SetHelloCallback(net,client_connect_first_hello_frameproj);
    Monitor_SetNetIo(monitor,net);

    getchar(); 
    Monitor_Delete(monitor);
    NetIo_ClearAPI(api);
}

// int main() {
//     MsQuicOpen2(&MsQuic);

//     QUIC_REGISTRATION_CONFIG RegConfig = { "quicsample", QUIC_EXECUTION_PROFILE_LOW_LATENCY };
//     MsQuic->RegistrationOpen(&RegConfig, &Registration);

//     HQUIC conn_cfg;
//     char alpn[256]="h2";
//     QUIC_BUFFER qbuff={
//         (uint32_t)strlen(alpn),
//         (uint8_t*)alpn
//     };
//     QUIC_SETTINGS settings={0};
//     settings.IdleTimeoutMs=30000;//3秒超时
//     settings.IsSet.IdleTimeoutMs=TRUE;
    
//     QUIC_CREDENTIAL_CONFIG CredConfig;
//     memset(&CredConfig, 0, sizeof(CredConfig));
//     CredConfig.Type=QUIC_CREDENTIAL_TYPE_NONE;
//     CredConfig.Flags=QUIC_CREDENTIAL_FLAG_CLIENT;
//     //if(Unsecure) {
//         CredConfig.Flags|=QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION;//不启用CA验证，自签名证书需要客户端配置，即将server端的cert 加入客户端本机的信任证书，会破坏安全环境
//     //}


//     MsQuic->ConfigurationOpen(Registration,(const QUIC_BUFFER *const)&qbuff,1,&settings,sizeof(settings),NULL,&conn_cfg);   
//     MsQuic->ConfigurationLoadCredential(conn_cfg, &CredConfig);

//     HQUIC connection;
//     MsQuic->ConnectionOpen(Registration,ClientConnectionCallback,NULL,&connection);
//     MsQuic->ConnectionStart(connection,conn_cfg,QUIC_ADDRESS_FAMILY_UNSPEC,"127.0.0.1",5656);
    

//     //StartClient();

//     getchar();  // 保持运行等待服务端响应

//     MsQuic->ConnectionClose(connection);
//     MsQuic->RegistrationClose(Registration);
//     MsQuicClose(MsQuic);
//     return 0;
// }
