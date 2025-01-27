/**
 * 例子代码：
 * https://github.com/microsoft/msquic/blob/main/src/tools/sample/sample.c#L323
 */

#include <msquic.h>
#include <stdio.h>
#include <stdlib.h>

#include "netio.h"
#include "monitor.h"

#pragma comment(lib,"msquic.lib")
#pragma comment(lib,"ws2_32.lib")
//#pragma comment(lib,"netio.lib")


const QUIC_API_TABLE* MsQuic = NULL;
HQUIC Registration = NULL;
HQUIC Session = NULL;
HQUIC Listener = NULL;
HQUIC svr_cfg=NULL;

_IRQL_requires_max_(DISPATCH_LEVEL)
_Function_class_(QUIC_STREAM_CALLBACK)
QUIC_STATUS QUIC_API ServerStreamCallback(
    _In_ HQUIC Stream,
    _In_opt_ void* Context,
    _In_ QUIC_STREAM_EVENT* Event
    ) {
    switch (Event->Type) {
        case QUIC_STREAM_EVENT_SEND_COMPLETE: {
            free(Event->SEND_COMPLETE.ClientContext);
        } break;
        case QUIC_STREAM_EVENT_RECEIVE:
            printf("Server received data: %.*s\n", (int)Event->RECEIVE.Buffers[0].Length, Event->RECEIVE.Buffers[0].Buffer);
            break;
        case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN: {
            //表示对端结束了数据发送，意味着服务端可以开始发送数据了
            char svr_msg[256]="Hello from server!";
            int SEND_BUFFER_LENGTH=100;
            void* sendbuffer=malloc(sizeof(QUIC_BUFFER)+SEND_BUFFER_LENGTH);

            QUIC_BUFFER* svr_send_buff=(QUIC_BUFFER*)sendbuffer;
            svr_send_buff->Buffer=(uint8_t*)sendbuffer+sizeof(QUIC_BUFFER);
            strcpy((char*)svr_send_buff->Buffer,svr_msg);
            svr_send_buff->Length=strlen((char*)svr_send_buff->Buffer);
            MsQuic->StreamSend(Stream,svr_send_buff,1,QUIC_SEND_FLAG_FIN,sendbuffer);
        } break;
        case QUIC_STREAM_EVENT_PEER_SEND_ABORTED: {
            MsQuic->StreamShutdown(Stream, QUIC_STREAM_SHUTDOWN_FLAG_ABORT, 0);
        } break;
        case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
            MsQuic->StreamClose(Stream);
            break;
        default:
            break;
    }
    return QUIC_STATUS_SUCCESS;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Function_class_(QUIC_CONNECTION_CALLBACK)
QUIC_STATUS QUIC_API ServerConnectionCallback(
    _In_ HQUIC Connection,
    _In_opt_ void* Context,
    _In_ QUIC_CONNECTION_EVENT* Event
    ) {
    switch (Event->Type) {
        case QUIC_CONNECTION_EVENT_CONNECTED: {
            //MsQuic->ConnectionSendResumptionTicket(Connection,QUIC_SEND_RESUMPTION_FLAG_NONE,0,NULL);
            break;
        }
        case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
            printf("Connection shutdown complete.\n");
            MsQuic->ConnectionClose(Connection);
            break;
        case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED: {
            MsQuic->SetCallbackHandler(Event->PEER_STREAM_STARTED.Stream,ServerStreamCallback,NULL);
        } break;
        case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT: {
            //客户端超时或其他异常
            printf("Lost connection , PEER timeout.\n");
        } break;
        default:
            break;
    }
    return QUIC_STATUS_SUCCESS;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_Function_class_(QUIC_LISTENER_CALLBACK)
QUIC_STATUS QUIC_API ServerListenerCallback (
    _In_ HQUIC Listener,
    _In_opt_ void* Context,
    _Inout_ QUIC_LISTENER_EVENT* Event
    ) {
    switch(Event->Type) {
    case QUIC_LISTENER_EVENT_NEW_CONNECTION: {
        //有新连接等待接入
        HQUIC conn=Event->NEW_CONNECTION.Connection;
        printf("new connection accessed...\n");

        MsQuic->SetCallbackHandler(Event->NEW_CONNECTION.Connection,ServerConnectionCallback,NULL);
        MsQuic->ConnectionSetConfiguration(Event->NEW_CONNECTION.Connection,svr_cfg);
    } break;
    case QUIC_LISTENER_EVENT_STOP_COMPLETE: {

    } break;
    }

    return QUIC_STATUS_SUCCESS;

}

void CleanupServer() {
    if (Listener) {
        MsQuic->ListenerClose(Listener);
    }
}

// int main() {
//     if(QUIC_STATUS_SUCCESS!=MsQuicOpen2(&MsQuic)) {
//         printf("MsQuicOpenVersion failed.\n");
//     }

//     QUIC_REGISTRATION_CONFIG RegConfig = { "quicsample", QUIC_EXECUTION_PROFILE_LOW_LATENCY };
//     if(QUIC_STATUS_SUCCESS!=MsQuic->RegistrationOpen(&RegConfig, &Registration)) {
//         printf("RegistrationOpen failed.\n");
//     }
    
//     QUIC_ADDR Address={0};
//     QuicAddrSetFamily(&Address, QUIC_ADDRESS_FAMILY_UNSPEC);
//     QuicAddrSetPort(&Address,9537);

//     QUIC_SETTINGS settings={0};
//     settings.IdleTimeoutMs=30000;
//     settings.IsSet.IdleTimeoutMs=TRUE;
//     settings.ServerResumptionLevel = QUIC_SERVER_RESUME_AND_ZERORTT;
//     settings.IsSet.ServerResumptionLevel = TRUE;
//     settings.PeerBidiStreamCount = 1;
//     settings.IsSet.PeerBidiStreamCount = TRUE;

//     const char* Cert="cert.crt";
//     const char* KeyFile="private.key";
//     QUIC_CREDENTIAL_CONFIG Config;
//     memset(&Config, 0, sizeof(Config));
//     Config.Flags = QUIC_CREDENTIAL_FLAG_NONE;
//     Config.Type=QUIC_CREDENTIAL_TYPE_CERTIFICATE_FILE;
//     QUIC_CERTIFICATE_FILE cf={KeyFile,Cert};
//     Config.CertificateFile=&cf;
//     //Config.CertificateFile->CertificateFile=Cert;
//     //Config.CertificateFile->PrivateKeyFile=KeyFile;

//     char albuffer[256]="h2";//HTTP/2 
//     QUIC_BUFFER AlBuf={
//         (uint32_t)strlen(albuffer),
//         (uint8_t*)albuffer
//     };

//     //HQUIC svr_cfg;
//     if(QUIC_STATUS_SUCCESS!=MsQuic->ConfigurationOpen(Registration,&AlBuf,1,&settings,sizeof(settings),NULL,&svr_cfg)) {
//         printf("ConfigurationOpen failed.\n");
//     }

//     if(QUIC_STATUS_SUCCESS!=MsQuic->ConfigurationLoadCredential(svr_cfg,&Config)) {
//         printf("ConfigurationLoadCredential failed.\n");
//     }

//     if(QUIC_STATUS_SUCCESS!=MsQuic->ListenerOpen(Registration,(QUIC_LISTENER_CALLBACK_HANDLER)ServerListenerCallback,NULL,&Listener)) {
//         printf("ListenerOpen failed.\n");
//     }
    
//     if(QUIC_STATUS_SUCCESS!=MsQuic->ListenerStart(Listener,&AlBuf,1,&Address)) {
//         printf("ListenerStart failed.\n");
//     }
//     printf("Server listening on port 9537...\n");

//     // 保持运行等待客户端连接
//     getchar();

//     CleanupServer();
//     MsQuic->ListenerStop(Listener);
//     MsQuic->RegistrationClose(Registration);
//     MsQuicClose(MsQuic);
//     return 0;
// }

void main() {
    char* buffer=(char*)malloc(256);
    if(buffer) {
        pNIHeader head=(pNIHeader)buffer;

        head->mode=0;
        head->type=REQ_CTRL_PARAM;
        head->seq=9527;
        head->len=1987;

        printf("buffer:0x%08x, head:0x%08x, head->mode:0x%08x\n",buffer,head,&head->mode);

        free(buffer);
    }


    QUIC_API_TABLE* api=NetIo_InitialAPI();

    pMonitor monitor=Monitor_New();
    pTimerSettings timer=TimerSettings_Initial(NULL,TimerProc_SVR,NULL);
    pnet_io net=NetIo_Initial();
    net->timer=timer;
    //NetIo_SetListenPort(9527);
    NetIo_LaunchServer(net,9537,"cert.crt","private.key");
    net->protocol_process=pack_protocol_process;
    Monitor_SetNetIo(monitor,net);

    getchar();

    NetIo_Clear(net);
    Monitor_Delete(monitor);
    
    NetIo_ClearAPI(api);
}

