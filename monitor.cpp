#include "monitor.h"
#include "webp/encode.h"
#include "webp/decode.h"
#include "utils.h"
#include "dialog.h"

pMonExg MONEXG=NULL;

pMonExg GetMonExg() { return MONEXG;}

pMonitor Monitor_New() {
    pMonitor m=(pMonitor)calloc(sizeof(monitor),1);
    if(!m) return NULL;

    m->dibits=malloc(3000*2000*3);
    if(!m->dibits) return NULL;

    //m->compressed=(WebPMemoryWriter*)malloc(sizeof(WebPMemoryWriter));
    m->compressed=(uint8_t*)malloc(3000*2000*3);
    if(!m->compressed) return NULL;
    //WebPMemoryWriterInit((WebPMemoryWriter*)m->compressed);

    m->predibits=malloc(3000*2000*3);
    if(!m->predibits) return NULL;

    m->hashcodes=malloc(sizeof(uint64_t)*2000);
    if(!m->hashcodes) return NULL;

    m->prehashcodes=malloc(sizeof(uint64_t)*2000);
    if(!m->prehashcodes) return NULL;

    m->dibits_segs=malloc(3000*2000*3);
    if(!m->dibits_segs) return NULL;

    m->predibits_segs=malloc(3000*2000*3);
    if(!m->predibits_segs) return NULL;

    m->decompressed=malloc(3000*2000*3);
    if(!m->decompressed) return NULL;

    m->kbytes=0.0f;

    //m->mode=1;

    pMonExg context=(pMonExg)calloc(sizeof(RMonExg),1);
    if(context) {
        context->dibits=(uint8_t*)m->dibits;
        context->compressed=(uint8_t*)m->compressed;
        context->pcompressed_len=&m->compressed_size;
        context->decompressed=(uint8_t*)m->decompressed;
        context->pdecompressed_len=&m->decompressed_len;
        context->pquality=&m->compress_quality;

        context->hashcodes=m->hashcodes;
        context->predibits=m->predibits; 
        context->prehashcodes=m->prehashcodes;
        context->predibits_segs=m->predibits_segs;
        context->dibits_segs=m->dibits_segs;
    }
    MONEXG=context;

    m->context=context;

    return m;
}

void Monitor_Delete(pMonitor m) {
    if(m->dibits) {
        free(m->dibits);
        m->dibits=NULL;
    }

    if(m->filtered) {
        free(m->filtered);
        m->filtered=NULL;
    }

    if(m->compressed) {
        //WebPMemoryWriterClear((WebPMemoryWriter*)m->compressed);
        //free((WebPMemoryWriter*)m->compressed);
        free(m->compressed);
        m->compressed=NULL;
    }
    
    if(m->decompressed) {
        free(m->decompressed);
        m->decompressed=NULL;
    }

    if(m->predibits) {
        free(m->predibits);
        m->predibits=NULL;
    }

    if(m->hashcodes) {
        free(m->hashcodes);
        m->hashcodes=NULL;
    }

    if(m->prehashcodes) {
        free(m->predibits);
        m->prehashcodes=NULL;
    }

    if(m->dibits_segs) {
        free(m->dibits_segs);
        m->dibits_segs=NULL;
    }

    if(m->predibits_segs) {
        free(m->predibits_segs);
        m->predibits_segs=NULL;
    }

    if(m->context) {
        pMonExg pme=(pMonExg)m->context;

        if(pme) {
            if(pme->net) {
                NetIo_Clear(pme->net);
            }
        }

        free(m->context);
        m->context=NULL;
    }

    free(m);
}

int Monitor_InitialSettings(HWND hwnd) {
    // pMonitor m=(pMonitor)calloc(sizeof(monitor),1);
    // if(!m) return -1;

    // m->dibits=malloc(3000*2000*3);
    // if(!m->dibits) return -1;

    // m->compressed=(WebPMemoryWriter*)malloc(sizeof(WebPMemoryWriter));
    // if(!m->compressed) return -1;
    // WebPMemoryWriterInit((WebPMemoryWriter*)m->compressed);

    // m->decompressed=malloc(3000*2000*3);
    // if(!m->decompressed) return -1;

    // m->kbytes=0.0f;
    pMonitor m=Monitor_New();

    SetWindowLongPtr(hwnd,GWLP_USERDATA,(ULONG_PTR)m);

    return 0;
}

int Monitor_SetNetIo(pMonitor m,pnet_io net) {
    if(!m||!net) return -1;

    pMonExg exg=(pMonExg)m->context;
    if(!exg) return -1;

    exg->net=net;
    net->context=exg;//互相绑定
    exg->timer=(pTimerSettings)net->timer;

    if(net->mode==0) {
        /**
         * 服务端
         */
        exg->timer->timer_proc=TimerProc_SVR2;
    } else {

        //exg->timer->timer_proc=TimerProc_CLIENT;
    }

    return 0;
}

int Monitor_SetSrcSize(HWND hwnd,int width,int height) {
    pMonitor m=Monitor_GetSettings(hwnd);
    if(!m) return -1;
    if(width<=0||height<=0) return -1;

    m->src_width=width;
    m->src_height=height;

    return 0;
}

int Monitor_InitReportStamp(HWND hwnd) {
    pMonitor m=Monitor_GetSettings(hwnd);
    if(!m) return -1;

    m->stamp=get_current_timestamp_ms();
}

int Monitor_SetFilter(HWND hwnd,int cx,int cy) {
    pMonitor m=Monitor_GetSettings(hwnd);
    if(!m) return -1;
    if(cx<=0||cy<=0) return -1;

    int filtersize=((cx*3+3)/4)*4*cy;
    if(m->filtered) {
        int pre_size=_msize(m->filtered);
        if(filtersize>pre_size) {
            free(m->filtered);
            m->filtered=NULL;
        }
    }

    m->filter_cx=cx;m->filter_cy=cy;
    if(!m->filtered) {
        m->filtered=malloc(filtersize);
    }

    return (m->filtered)?0:-1;
}

int Monitor_SetCompressQuality(HWND hwnd, int quality_factor) {
    pMonitor m=Monitor_GetSettings(hwnd);
    if(!m) return -1;

    m->compress_quality=quality_factor;
    return 0;
}

pMonitor Monitor_GetSettings(HWND hwnd) {
    return (pMonitor)GetWindowLongPtr(hwnd,GWLP_USERDATA);
}

void Monitor_ClearSettings(HWND hwnd) {
    pMonitor m=Monitor_GetSettings(hwnd);
    if(!m) return;

    Monitor_Delete(m);
}

LRESULT CALLBACK MonitorViewProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) { 
    switch(msg) {
    case WM_PAINT: {
        return MointorViewPaint(hwnd,wParam,lParam);
    } break;
    case WM_ERASEBKGND: return 1;
    case WM_NCDESTROY: {
        Monitor_ClearSettings(hwnd);
    } break;
    case WM_LBUTTONDOWN: { 
        pMonitor m=Monitor_GetSettings(hwnd);
        if(!m) return -1;
        
        POINT pt={GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam)};
        int hitcode=Monitor_HitTest(hwnd,pt);
        if(hitcode==ZHSTHUMB) {
            //水平滚动条
            SetCapture(hwnd);
            m->prept.x=pt.x;
            m->prept.y=-1;
            return 0;
        } else if(hitcode==ZVSTHUMB) {
            //垂直滚动
            SetCapture(hwnd);
            m->prept.y=pt.y;
            m->prept.x=-1;
            return 0;
        } else if(hitcode==CTRL_EXPEND) {
            if(m->mode!=1) {
                m->mode=1;
                InvalidateRect(hwnd,NULL,TRUE);
            }
        } else if(hitcode==CTRL_COLLAPSE) {
            if(m->mode!=0) {
                m->mode=0;
                m->offset={0,0};

                InvalidateRect(hwnd,NULL,TRUE);
            }
        } else if(hitcode==CTRL_CFG) {
            HMODULE cfg=LoadLibrary("MonitorCfg.dll");
            if(cfg) {
                SHOWDIALOGPROC GetSvrAddr=(SHOWDIALOGPROC)GetProcAddress(cfg,"MonitorGetSVRADDR");
                if(GetSvrAddr) {
                    char addr[256]={0};
                    GetSvrAddr((HINSTANCE)cfg,hwnd,(LPARAM)addr);
                    
                    pMonExg pme=(pMonExg)m->context;
                    if(!pme) return 0;
                    
                    //sscanf(addr,"%[^:]:%d",pme->net->svr_ip,&pme->net->port);
                } else {
                    MessageBox(hwnd,"fuck, proc addr not found.","异常",MB_OK|MB_ICONINFORMATION);
                }
                FreeLibrary(cfg);
            } else {
                MessageBox(hwnd,"加载Proj配置失败","异常",MB_OK|MB_ICONINFORMATION);
            }
            return 0;
        }
    } break;
    case WM_MOUSEMOVE: {
        pMonitor m=Monitor_GetSettings(hwnd);
        if(!m) return -1;

        POINT pt={
            GET_X_LPARAM(lParam),
            GET_Y_LPARAM(lParam)
        };

        if(m->mode==1&&GetCapture()==hwnd) {
            POINT ptSize;
            MonitorView_GetContentXY(hwnd,&ptSize);
            if(m->prept.y!=-1) {
                //垂直滚动
                RECT rcVertThumb,rcVert;
                MonitorView_GetZoneRect(hwnd,ZVSCROLL,&rcVert);
                MonitorView_GetZoneRect(hwnd,ZVSTHUMB,&rcVertThumb);

                m->offset.y+=(double)(pt.y-m->prept.y)/((rcVert.bottom-rcVert.top)-(rcVertThumb.bottom-rcVertThumb.top))*(ptSize.y-(rcVert.bottom-rcVert.top));
                m->prept.y=pt.y;
            }
            if(m->prept.x!=-1) {
                //水平滚动
                RECT rcHorz,rcHorzThumb;
                MonitorView_GetZoneRect(hwnd,ZHSCROLL,&rcHorz);
                MonitorView_GetZoneRect(hwnd,ZHSTHUMB,&rcHorzThumb);

                m->offset.x+=(double)(pt.x-m->prept.x)/((rcHorz.right-rcHorz.left)-(rcHorzThumb.right-rcHorzThumb.left))*(ptSize.x-(rcHorz.right-rcHorz.left));
                m->prept.x=pt.x;
            }
        }

        m->pt={pt.x,pt.y};
        InvalidateRect(hwnd,NULL,TRUE);
    } break;
    case WM_LBUTTONUP: {
        if(GetCapture()==hwnd) ReleaseCapture();
    } break;
    }

    return CallWindowProc(DefWindowProc,hwnd,msg,wParam,lParam);
}

int MointorViewPaint(HWND hwnd,WPARAM wParam,LPARAM lParam) {
    pMonitor m=Monitor_GetSettings(hwnd);
    if(!m) return 0;

    PAINTSTRUCT ps={0};
    RECT rc,rcMem;
    GetClientRect(hwnd,&rc);
    OffsetRect(&rc,-rc.left,-rc.top);
    CopyRect(&rcMem,&rc);
    int cx=rcMem.right,cy=rcMem.bottom;
    if(!(cx>0&&cy>0)) return 0;

    pMonExg pme=(pMonExg)m->context;
    if(!pme||pme->width<=0||pme->height<=0||*pme->pcompressed_len<=0) return 0;

    HDC hdc=BeginPaint(hwnd,&ps);
    HDC memdc=CreateCompatibleDC(hdc);
    HBITMAP bmp=CreateCompatibleBitmap(hdc,cx,cy);
    HBITMAP prebmp=(HBITMAP)SelectObject(memdc,bmp);

    HDC imgdc=CreateCompatibleDC(hdc);
    HBITMAP imgbmp=CreateCompatibleBitmap(hdc,pme->width,pme->height);
    HBITMAP preimgbmp=(HBITMAP)SelectObject(imgdc,imgbmp);
    // HBRUSH brushbk=CreateSolidBrush(RGB(20,20,20));
    // FillRect(memdc,&rcMem,brushbk);
    // DeleteObject(brushbk);

    // //测试从屏幕DC 获取，贴在这里
    // size_t size;
    // if(0!=capturebmpdata(m->dibits,&size)) {
    //     MessageBox(hwnd,"capturebmpdata 失败.","Exception",MB_OK);
    // }
    
    // Monitor_SetFilter(hwnd,cx,cy);
    // clipbmpdata(m->src_width,m->src_height,m->dibits,0,0,cx,cy,m->filtered,&size);

    // //压缩
    // encodebmp2webp(m->filtered,m->filter_cx,m->filter_cy,size,m->compress_quality,m->compressed,&m->compressed_size);
    // m->compress_rate=(float)m->compressed_size/size;
    // m->kbytes+=((float)m->compressed_size)/1024;
    // decodewebp2bmp(m->compressed,m->compressed_size,m->decompressed,&m->dec_width,&m->dec_height,&size);
    
    if(0!=loadbmpdata(imgdc,imgbmp,pme->width,pme->height,m->decompressed,m->decompressed_len)) {
        MessageBox(hwnd,"loadbmpdata 失败.","Exception",MB_OK);
    }

    RECT rcView;
    if(0==MonitorView_GetZoneRect(hwnd,IMG_VIEW,&rcView)) {
        int view_cx=rcView.right-rcView.left;
        int view_cy=rcView.bottom-rcView.top;

        if(m->mode==0) StretchBlt(memdc,rcView.left,rcView.top,view_cx,view_cy,
                        imgdc,0,0,pme->width,pme->height,SRCCOPY);
        else BitBlt(memdc,rcView.left,rcView.top,view_cx,view_cy,imgdc,m->offset.x,m->offset.y,SRCCOPY);
    }

    MonitorView_PaintScrollbar(hwnd,m,memdc);
    Monitor_PaintPanel(hwnd,m,memdc);

    // SetTextColor(memdc,RGB(0,125,250));
    // SetBkMode(memdc,TRANSPARENT);
    // int cx_text=180,cy_text=90;
    // int paddingcx=25,paddingcy=25;
    // RECT rcText={
    //     rcMem.right-paddingcx-cx_text,rcMem.top+paddingcy,
    //     rcMem.right-paddingcx,rcMem.top+paddingcy+cy_text
    // };
    //char rp_text[256]={0};
    //sprintf(rp_text,"压缩比率:%.5lf\n网络传输:%.3f Kbtyes/s\n压缩质量:%d\n单帧压缩数据：%.5lf Kbytes",m->compress_rate,m->trans_rate,m->compress_quality,(float)m->compressed_size/1024);
    //DrawText(memdc,rp_text,-1,&rcText,DT_LEFT|DT_WORDBREAK);

    BitBlt(hdc,0,0,cx,cy,memdc,0,0,SRCCOPY);

    SelectObject(imgdc,preimgbmp);
    DeleteObject(imgbmp);
    DeleteObject(imgdc);

    SelectObject(memdc,prebmp);
    DeleteObject(bmp);
    DeleteDC(memdc);
    
    EndPaint(hwnd,&ps);
    return 0;    
}

int MonitorView_PaintScrollbar(HWND hwnd,pMonitor m,HDC hdc) {
    if(!m) return -1;

    RECT rc;
    GetClientRect(hwnd,&rc);
    
    POINT ptSize;
    if(MonitorView_GetContentXY(hwnd,&ptSize)!=0) {
        return -1;
    }

    int viewcx=rc.right-rc.left;
    int viewcy=rc.bottom-rc.top;
    
    BOOL hasvertbar=viewcy<ptSize.y; //垂直滚动条
    BOOL hashorzbar=viewcx<ptSize.x; //水平滚动条

    Gdiplus::Graphics graphics(hdc);
    if(hasvertbar) {
        RECT rcVert,rcVertThumb;

        if(0==MonitorView_GetZoneRect(hwnd,ZVSTHUMB,&rcVertThumb)&&
           0==MonitorView_GetZoneRect(hwnd,ZVSCROLL,&rcVert)) {
            //垂直滚动条
            COLORREF colorBar=RGB(5,5,5);
            HBRUSH brushBar=CreateSolidBrush(colorBar);
            FillRect(hdc,&rcVert,brushBar);
            DeleteObject(brushBar);

            Gdiplus::GraphicsPath path;
            Gdiplus::LinearGradientBrush pbrush(Rect(rcVertThumb.left,rcVertThumb.top,rcVertThumb.right-rcVertThumb.left,rcVertThumb.bottom-rcVertThumb.top),
                Gdiplus::Color(255,53,53,53),
                Gdiplus::Color(155,0,0,0),
                LinearGradientModeHorizontal
            );
            graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
            path.AddArc(rcVertThumb.left,rcVertThumb.top,SCROLLBAR_PIXLS,SCROLLBAR_PIXLS,180,180);
            path.AddArc(rcVertThumb.left,rcVertThumb.bottom-SCROLLBAR_PIXLS,SCROLLBAR_PIXLS,SCROLLBAR_PIXLS,0,180);
            graphics.FillPath(&pbrush,&path);
        }
    }

    if(hashorzbar) {
        RECT rcHorz,rcHorzThumb;
        if(0==MonitorView_GetZoneRect(hwnd,ZHSTHUMB,&rcHorzThumb)&&
           0==MonitorView_GetZoneRect(hwnd,ZHSCROLL,&rcHorz)) {
            //水平滚动条
            COLORREF colorBar=RGB(5,5,5);
            HBRUSH brushBar=CreateSolidBrush(colorBar);
            FillRect(hdc,&rcHorz,brushBar);
            DeleteObject(brushBar);

            Gdiplus::GraphicsPath path;
            Gdiplus::LinearGradientBrush pbrush(Rect(rcHorzThumb.left,rcHorzThumb.top,rcHorzThumb.right-rcHorzThumb.left,rcHorzThumb.bottom-rcHorzThumb.top),
                Gdiplus::Color(255,53,53,53),
                Gdiplus::Color(155,0,0,0),
                LinearGradientModeVertical
            );
            graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
            path.AddArc(rcHorzThumb.left,rcHorzThumb.top,SCROLLBAR_PIXLS,SCROLLBAR_PIXLS,90,180);
            path.AddArc(rcHorzThumb.right-SCROLLBAR_PIXLS,rcHorzThumb.top,SCROLLBAR_PIXLS,SCROLLBAR_PIXLS,-90,180);
            graphics.FillPath(&pbrush,&path);    
        }
    }

    return 0;
}

int Monitor_PaintPanel(HWND hwnd,pMonitor m,HDC hdc) {
    if(!m) return -1;

    RECT rcPanel;
    if(0==MonitorView_GetZoneRect(hwnd,CTRL_PANEL,&rcPanel)) {
        if(PtInRect(&rcPanel,m->pt)) {
            Gdiplus::Graphics graphics(hdc);
            graphics.SetSmoothingMode(SmoothingModeAntiAlias);
            Gdiplus::GraphicsPath path;
            int slashoffset=rcPanel.bottom-rcPanel.top;
            path.AddLine((int)rcPanel.left, (int)rcPanel.top,(int)rcPanel.left+slashoffset,(int)rcPanel.bottom);
            path.AddLine((int)rcPanel.left+slashoffset,(int)rcPanel.bottom,(int)rcPanel.right-slashoffset,(int)rcPanel.bottom);
            path.AddLine((int)rcPanel.right-slashoffset,(int)rcPanel.bottom,(int)rcPanel.right,(int)rcPanel.top);
            path.AddLine((int)rcPanel.right,(int)rcPanel.top,(int)rcPanel.left,(int)rcPanel.top);

            Gdiplus::LinearGradientBrush panelBrush(Gdiplus::Rect(rcPanel.left,rcPanel.top,rcPanel.right-rcPanel.left,rcPanel.bottom-rcPanel.top),
                Gdiplus::Color(255,55,55,55),
                Gdiplus::Color(255,35,35,35),
                LinearGradientModeVertical
            );
            graphics.FillPath(&panelBrush,&path);
        }
    }
    return 0;
}

int Monitor_HitTest(HWND hwnd,POINT pt) {
    pMonitor m=Monitor_GetSettings(hwnd);
    if(!m) return -1;

    RECT rcView,rcHSThumb,rcVSThumb;
    if(MonitorView_GetZoneRect(hwnd,ZHSTHUMB,&rcHSThumb)==0) {
        if(PtInRect(&rcHSThumb,pt)) {
            return ZHSTHUMB;
        }
    }
    if(MonitorView_GetZoneRect(hwnd,ZVSTHUMB,&rcVSThumb)==0) {
        if(PtInRect(&rcVSThumb,pt)) {
            return ZVSTHUMB;
        }
    }

    RECT rcPanel;
    if(0==MonitorView_GetZoneRect(hwnd,CTRL_PANEL,&rcPanel)) {
        RECT rcExpend,rcCollapse,rcCfg;
        if(0==MonitorView_GetZoneRect(hwnd,CTRL_EXPEND,&rcExpend)&&
           0==MonitorView_GetZoneRect(hwnd,CTRL_COLLAPSE,&rcCollapse)&&
           0==MonitorView_GetZoneRect(hwnd,CTRL_CFG,&rcCfg)) {
            if(PtInRect(&rcExpend,pt)) return CTRL_EXPEND;
            else if(PtInRect(&rcCollapse,pt)) return CTRL_COLLAPSE;
            else if(PtInRect(&rcCfg,pt)) return CTRL_CFG;
        }

        if(PtInRect(&rcPanel,pt)) return CTRL_PANEL;
    }

    return 0;
}

/**
 * 获取内容的高宽，与视图高宽对应，
 * 当内容高宽超出了视图高宽之后，会出现滚动条
 */
int MonitorView_GetContentXY(HWND hwnd,POINT* lp) {
    pMonitor m=Monitor_GetSettings(hwnd);
    if(!m) return -1;

    if(m->mode==0) {
        RECT rc;
        GetClientRect(hwnd,&rc);
        lp->x=rc.right-rc.left;
        lp->y=rc.bottom-rc.top;
    } else {
        pMonExg pme=(pMonExg)m->context;
        if(!pme) return -1;

        lp->x=pme->width;
        lp->y=pme->height;
    }

    return 0;
}

int MonitorView_GetZoneRect(HWND hwnd,int poscode,LPRECT prc) {
    pMonitor m=Monitor_GetSettings(hwnd);
    if(!m) return -1;

    RECT rc;
    GetClientRect(hwnd,&rc);
    int viewcx=rc.right-rc.left;
    int viewcy=rc.bottom-rc.top;
    
    POINT ptSize;
    if(MonitorView_GetContentXY(hwnd,&ptSize)!=0) {
        return -1;
    }
    
    BOOL hasvertbar=viewcy<ptSize.y; //垂直滚动条
    BOOL hashorzbar=viewcx<ptSize.x; //水平滚动条
    
    int panel_cy=35;
    switch(poscode) {
    case ZHSTHUMB: {
        if(!hashorzbar) return -1;
        int contentcx=viewcx-(hasvertbar?SCROLLBAR_PIXLS:0);
        int xpage=contentcx;
        int xrange1=0;
        int xrange2=ptSize.x-xpage;
        /*
         * contentcx 展示视图区
         * xrange : offsetx 值域范围
         * xpage : 页宽
         * psv->pixels_x 内容宽度
         * 等式：
         * 
         */
        if(m->offset.x<xrange1) m->offset.x=xrange1;
        if(m->offset.x>xrange2) m->offset.x=xrange2;

        int horzcx=((double)contentcx)*contentcx/ptSize.x;
        horzcx=((horzcx<THUMB_MIN_PIXLS)?THUMB_MIN_PIXLS:horzcx);
        int horz_offset=((double)(contentcx-horzcx)*m->offset.x)/(ptSize.x-contentcx);
        SetRect(prc,rc.left+horz_offset,
            rc.bottom-SCROLLBAR_PIXLS,
            rc.left+horz_offset+horzcx,
            rc.bottom-1);
    } break;
    case ZHSCROLL: {
        if(!hashorzbar) return -1;
        int contentcx=viewcx-(hasvertbar?SCROLLBAR_PIXLS:0);
        int xpage=contentcx;
        int xrange1=0;
        int xrange2=ptSize.x-xpage;
        /*
         * contentcx 展示视图区
         * xrange : offsetx 值域范围
         * xpage : 页宽
         * psv->pixels_x 内容宽度
         * 等式：
         * 
         */
        if(m->offset.x<xrange1) m->offset.x=xrange1;
        if(m->offset.x>xrange2) m->offset.x=xrange2;

        SetRect(prc,rc.left,
            rc.bottom-SCROLLBAR_PIXLS,
            rc.right-(hasvertbar?SCROLLBAR_PIXLS:0),
            rc.bottom-1);
    } break;
    case ZVSTHUMB: {
        if(!hasvertbar) return -1;
        int contentcy=viewcy-(hashorzbar?SCROLLBAR_PIXLS:0);
        int ypage=contentcy;
        int yrange1=0;
        int yrange2=ptSize.y-ypage;
        /*
         * contentcx 展示视图区
         * xrange : offsetx 值域范围
         * xpage : 页宽
         * psv->pixels_x 内容宽度
         * 等式：
         * 
         */
        if(m->offset.y<yrange1) m->offset.y=yrange1;
        if(m->offset.y>yrange2) m->offset.y=yrange2;

        int vertcy=((double)contentcy)*contentcy/ptSize.y;
        vertcy=((vertcy<THUMB_MIN_PIXLS)?THUMB_MIN_PIXLS:vertcy);
        int vert_offset=((double)(contentcy-vertcy)*m->offset.y)/(ptSize.y-contentcy);
        SetRect(prc,rc.right-SCROLLBAR_PIXLS,
            rc.top+vert_offset,
            rc.right-1,
            rc.top+vert_offset+vertcy);        
    } break;
    case ZVSCROLL: {
        if(!hasvertbar) return -1;
        int contentcy=viewcy-(hashorzbar?SCROLLBAR_PIXLS:0);
        int ypage=contentcy;
        int yrange1=0;
        int yrange2=ptSize.y-ypage;
        /*
         * contentcx 展示视图区
         * xrange : offsetx 值域范围
         * xpage : 页宽
         * psv->pixels_x 内容宽度
         * 等式：
         * 
         */
        if(m->offset.y<yrange1) m->offset.y=yrange1;
        if(m->offset.y>yrange2) m->offset.y=yrange2;

        // int vertcy=((double)contentcy)*contentcy/psv->pixels_y;
        // int vert_offset=((double)(contentcy-vertcy)*psv->offset.y)/(psv->pixels_y-contentcy);
        SetRect(prc,rc.right-SCROLLBAR_PIXLS,
            rc.top,
            rc.right-1,
            rc.bottom-1);   
    } break;
    case IMG_VIEW: {
        int viewcx,viewcy;
        POINT ptbase={rc.right,rc.bottom};
        SetRect(prc,rc.left,rc.top,rc.right,rc.bottom);
        if(m->mode==0) {
            /*
             * 图像纵横比
             */
            pMonExg pme=(pMonExg)m->context;
            if(!pme||pme->width<=0||pme->height<=0) return -1;
            ptSize={pme->width,pme->height};

            double img_ratio=(double)ptSize.x/ptSize.y;
            viewcx=rc.right-rc.left;
            viewcy=rc.bottom-rc.top;
            double view_ratio=(double)viewcx/viewcy;

            //img的宽高比大于 视图的 宽高比
            //则 以 视图的宽为下限
            if(img_ratio>view_ratio) {
                /*
                 * 以 viewcx 为基准进行比例适配
                 */
                if(viewcx<ptSize.x) {
                    /*
                     * 图像会被压缩
                     */
                    int imgviewcx=viewcx;
                    int imgviewcy=imgviewcx/((double)ptSize.x/ptSize.y);
                    int offsety=(viewcy-imgviewcy)>>1;
                    SetRect(
                        prc,
                        rc.left,
                        rc.top+offsety,
                        rc.right,
                        rc.top+offsety+imgviewcy
                    );
                } else {
                    /*
                     * 原图尺寸
                     */
                    int imgviewcx=ptSize.x;
                    int imgviewcy=ptSize.y;
                    int offsetx=(viewcx-imgviewcx)>>1;
                    int offsety=(viewcy-imgviewcy)>>1;
                    SetRect(prc,rc.left+offsetx,rc.top+offsety,
                        rc.left+offsetx+imgviewcx,rc.top+offsety+imgviewcy
                    );
                }
            } else {
                /*
                 * 以 viewcy 为基准进行比例适配
                 */
                if(viewcy<ptSize.y) {
                    int imgviewcy=viewcy;
                    int imgviewcx=imgviewcy*((double)ptSize.x/ptSize.y);
                    int offsetx=(viewcx-imgviewcx)>>1;
                    SetRect(prc,
                        rc.left+offsetx,rc.top,rc.left+offsetx+imgviewcx,rc.bottom
                    );
                } else {
                    int imgviewcy=ptSize.y;
                    int imgviewcx=ptSize.x;

                    int offsetx=(viewcx-imgviewcx)>>1;
                    int offsety=(viewcy-imgviewcy)>>1;
                    SetRect(prc,rc.left+offsetx,rc.top+offsety,
                        rc.left+offsetx+imgviewcx,rc.top+offsety+imgviewcy
                    );
                }
            }
            break;
        }
        if(m->mode==1) {

            /*
             * 当前具有垂直滚动条
             * 说明 右侧有滚动条占据
             * 视图右侧区域需要调整
             */
            if(hasvertbar) {
                ptbase.x-=SCROLLBAR_PIXLS;//垂直滚动条
                prc->right=ptbase.x;
            }
            if(hashorzbar) {
                ptbase.y-=SCROLLBAR_PIXLS;//水平滚动条
                prc->bottom=ptbase.y;
            }
        } 

        /*
         * 调整prc居中位置
         * 以长度 psv->pixels_x/psv->pixels_y 为准
         */
        viewcx=rc.right-prc->left;
        viewcy=prc->bottom-prc->top;
        if(ptSize.x<viewcx) {
            prc->left+=(viewcx-ptSize.x)>>1;
        }
        if(ptSize.y<viewcy) {
            prc->top+=(viewcy-ptSize.y)>>1;
        }

        if(prc->left+ptSize.x<prc->right) prc->right=prc->left+ptSize.x;
        if(prc->top+ptSize.y<prc->bottom) prc->bottom=prc->top+ptSize.y;
    } break;
    case CTRL_PANEL: {
        int panel_cx= panel_cy*5;
        int offsetx=(rc.right-rc.left-panel_cx)>>1;
        SetRect(prc,rc.left+offsetx,rc.top,rc.left+offsetx+panel_cx,rc.top+panel_cy);
    } break;
    case CTRL_EXPEND: {
        RECT rcPanel;
        if(0==MonitorView_GetZoneRect(hwnd,CTRL_PANEL,&rcPanel)) {
            RECT rcBase={
                rcPanel.left,rcPanel.top,rcPanel.left+panel_cy,rcPanel.bottom
            };

            OffsetRect(&rcBase,panel_cy,0);
            CopyRect(prc,&rcBase);
            return 0;
        } else return -1;
    } break;
    case CTRL_COLLAPSE: {
        RECT rcPanel;
        if(0==MonitorView_GetZoneRect(hwnd,CTRL_PANEL,&rcPanel)) {
            RECT rcBase={
                rcPanel.left,rcPanel.top,rcPanel.left+panel_cy,rcPanel.bottom
            };

            OffsetRect(&rcBase,panel_cy*2,0);
            CopyRect(prc,&rcBase);
            return 0;
        } else return -1;
    } break;
    case CTRL_CFG: {
        RECT rcPanel;
        if(0==MonitorView_GetZoneRect(hwnd,CTRL_PANEL,&rcPanel)) {
            RECT rcBase={
                rcPanel.left,rcPanel.top,rcPanel.left+panel_cy,rcPanel.bottom
            };

            OffsetRect(&rcBase,panel_cy*3,0);
            CopyRect(prc,&rcBase);
            return 0;
        } else return -1;
    } break;
    default: return -1;
    }

    return 0;
}

/**
 * 返回值： 0-处理成功
 *        其它-报告异常
 */
int pack_protocol_process(void* out_context,pNIHeader head,uint8_t** plts,int* lens,int count,void* inner_context) {
    pstream_io stream=(pstream_io)inner_context;
    if(!stream) return -1;
    
    printf("**************%d-%s(%d)**********\n",head->seq,PROTOCOL_TRANSCODE_DESC[head->type],head->len);

    //默认分配200K
    if(!stream->send_buffer) {
        stream->send_buffer_size=1024*200;
        stream->send_buffer=(uint8_t*)malloc(stream->send_buffer_size);
    }

    QUIC_BUFFER* rsp_buff=NULL;
    int rsp_quic_buffer_idx=0;
    for(int idx=1;idx<stream->quic_buffer_size;idx++) {
        if(stream->quic_buffer_flags[idx-1]+stream->quic_buffer_flags[idx]==0) {
            rsp_quic_buffer_idx=idx-1;
            rsp_buff=(QUIC_BUFFER*)(stream->send_buffer+sizeof(QUIC_BUFFER)*(idx-1));
        }
    }
    if(rsp_buff==NULL) {
        printf("StreamSend QUIC_BUFFER 缓冲已耗尽.\n");
        return -1;
    }
    int rsp_buff_count=1;
    rsp_buff->Buffer=stream->send_buffer; //回复的 QUIC_BUFFER 指针
    rsp_buff->Length=0;
    pNIHeader rsp_head=(pNIHeader)rsp_buff->Buffer; //回复报文头地址

    switch(head->type) {
    ////////////////////////////////////////////////////////////////
    ////////////////////    SERVER     /////////////////////////////
    /////////////      客户端向服务端 发送请求 REQ          //////////
    /////////////      服务端RECV的都是 REQ                //////////
    case REQ_TRANS_RESET: {
        //请求重置传输（初始化）
        rsp_head->mode=0;
        rsp_head->type=RSP_TRANS_RESET;
        stream->send_size=sizeof(NIHeader);
        return StreamIo_SendData(stream,(uint8_t*)rsp_head,sizeof(NIHeader));
    } break;
    case REQ_CTRL_PARAM: {
        //请求控制参数
        rsp_head->mode=0;
        rsp_head->seq=head->seq+1;
        rsp_head->type=RSP_CTRL_PARAM;

        rsp_head->len=sizeof(CtrlParam);
        pCtrlParam p=(pCtrlParam)(rsp_buff->Buffer+sizeof(NIHeader));
        p->frequency=12;
        HDC dc=GetDC(NULL);
        p->width=GetDeviceCaps(dc,HORZRES);
        p->height=GetDeviceCaps(dc,VERTRES);
        ReleaseDC(NULL,dc);
        p->quality=15;

        //同步设置数据交互缓冲区的参数
        pMonExg exg=(pMonExg)out_context;
        if(exg) {
            exg->height=p->height;
            exg->width=p->width;
        }

        // rsp_buff->Length=sizeof(NIHeader)+sizeof(CtrlParam);
        return StreamIo_SendData(stream,(uint8_t*)rsp_head,sizeof(NIHeader)+sizeof(CtrlParam));
    } break;
    case REQ_IMG_PARAM: {
        //请求图象参数
    } break;
    case REQ_IMG_DATA: {
        //请求图像数据
        //这里需要两组QUIC_BUFFER
        //第一组，记录RSP的 NIHeader.
        //第二组，记录compressed data
        //第一组、第二组需要连续，所以send_buffer 首先被两组 QUIC_BUFFER占据
        rsp_head->mode=0;
        rsp_head->seq=head->seq+1;
        rsp_head->type=RSP_IMG_DATA;

        //先解析图像数据的协商参数
        /**
         * 一般在请求图像数据前，客户端将向服务端请求控制参数，
         * 服务端会回复服务端设定的初始服务参数
         * 客户端会回复客户端选定的控制参数，并以此为准
         * 所以在REQ_IMG_DATA 中 会带有一个确定的控制参数
         */
        CtrlParam* ctrl_param=(CtrlParam*)(stream->send_buffer+sizeof(NIHeader));
        uint8_t* plt=(uint8_t*)(stream->send_buffer+sizeof(NIHeader));
        int bytes_accessed=0;
        for(int idx=0;idx<count;idx++) {
            int read_bytes=sizeof(CtrlParam)-bytes_accessed;
            if(read_bytes>lens[idx]) {
                read_bytes=lens[idx];
                memcpy(plt+bytes_accessed,plts[idx],read_bytes);
                bytes_accessed+=read_bytes;
            } else {
                memcpy(plt,plts[idx],read_bytes);
                bytes_accessed+=read_bytes;
                break;
            }
        }
        printf("[Confirmed]屏幕参数 %d × %d ，压缩质量 %d, FPS %d\n",ctrl_param->width,ctrl_param->height,ctrl_param->quality,ctrl_param->frequency);
        //获取图像
        pMonExg pme=(pMonExg)out_context;
        if(!pme) {
            printf("未挂载图像缓冲区参数,无法获取图像源\n");
            break;
        }
        pme->frequency=ctrl_param->frequency;
        *pme->pquality=ctrl_param->quality;
        pme->stream=stream;
        pme->timer->param=pme;
        
        printf("正在启动定时器...\n");
        //启动定时器
        if(pme->timer==NULL) {
            printf("定时器回调函数尚未设置\n");
        } else {
            /**
             * 目前的上下文运行在msquic的上下文，该上下文所在的线程是没有消息循环的
             * 所以在该线程内创建定时器是注定不会正常运行的
             */
            TimerSettings_SetTimer(pme->timer,5,(float)1000/pme->frequency);
            //TimerSettings_SetTimer(pme->timer,5,250);
            TimerSettings_Start(pme->timer);

            printf("定时器已经启动\n");
        }
        return 0;
    } break;
    ////////////////////////////////////////////////////////////////
    ////////////////////   CLIENT     //////////////////////////////
    /////////////      服务端向客户端 回复响应 RSP          //////////
    /////////////      客户端RECV的都是 RSP ，继续发送请求  ////////// 
    case RSP_TRANS_RESET: {
        //请求重置传输
    } break;
    case RSP_CTRL_PARAM: {
        //请求控制参数 plts 是包含NIHeader的
        CtrlParam ctrl_param;
        uint8_t* plt=(uint8_t*)&ctrl_param;
        int bytes_accessed=0;
        for(int idx=0;idx<count;idx++) {
            int read_bytes=sizeof(CtrlParam)-bytes_accessed;
            if(read_bytes>lens[idx]) {
                read_bytes=lens[idx];
                memcpy(plt+bytes_accessed,plts[idx],read_bytes);
                bytes_accessed+=read_bytes;
            } else {
                memcpy(plt,plts[idx],read_bytes);
                bytes_accessed+=read_bytes;
                break;
            }
        }

        printf("屏幕参数 %d × %d ，压缩质量 %d, FPS %d\n",ctrl_param.width,ctrl_param.height,ctrl_param.quality,ctrl_param.frequency);

        //发送请求图像数据的请求,同样带一个 CtrlParam参数
        //回复的CtrlParam参数将被服务端应用为实际的参数
        ctrl_param.frequency=10;
        ctrl_param.quality=25;

        pMonExg pme=(pMonExg)out_context;
        if(pme) {
            pme->width=ctrl_param.width;
            pme->height=ctrl_param.height;
            pme->frequency=ctrl_param.frequency;
            *pme->pquality=ctrl_param.quality;
        }

        rsp_head->mode=1;
        rsp_head->seq=head->seq+1;
        rsp_head->type=REQ_IMG_DATA;
        rsp_head->len=sizeof(CtrlParam);
        memcpy(((uint8_t*)rsp_head)+sizeof(NIHeader),plt,rsp_head->len);
        
        //rsp_buff->Length=sizeof(NIHeader)+sizeof(CtrlParam);
        return StreamIo_SendData(stream,(uint8_t*)rsp_head,sizeof(NIHeader)+sizeof(CtrlParam));
    } break;
    case RSP_IMG_PARAM: {
        //返回图象参数

    } break;
    case RSP_IMG_DATA: {
        //返回图像数据
        pMonExg pme=(pMonExg)out_context;
        if(!pme) {
            printf("未挂载图像缓冲区参数,无法处理图像数据,将挂载默认交换缓冲\n");
            pme=GetMonExg();
        }

        if(pme->compressed&&pme->decompressed) {
            int bytes_accessed=0;
            for(int idx=0;idx<count;idx++) {
                memcpy(pme->compressed+bytes_accessed,plts[idx],lens[idx]);
                bytes_accessed+=lens[idx];
            }
            
            int compressed_len=bytes_accessed;
            int cx_decompressed,cy_decompressed;
            size_t decompressed_len;
            if(0!=decodewebp2bmp2(pme->compressed,compressed_len,pme->decompressed,&cx_decompressed,&cy_decompressed,&decompressed_len)) {
                printf("图像数据解压失败.\n");
                return -1;
            }

            //WebPDecodeRGBAInto

            *(pme->pcompressed_len)=compressed_len;
            *(pme->pdecompressed_len)=decompressed_len;
            pme->width=cx_decompressed;
            pme->height=cy_decompressed;

            InvalidateRect(pme->hwnd,NULL,TRUE);
        }
        return 0;
    } break;
    case RSP_IMG_DATA_PARTITIONMODE: {
        //接收到差异图传数据
        //返回图像数据
        pMonExg pme=(pMonExg)out_context;
        if(!pme) {
            printf("未挂载图像缓冲区参数,无法处理图像数据,将挂载默认交换缓冲\n");
            pme=GetMonExg();
        }

        //partition count
        if(head->len==sizeof(uint64_t)) {
            /**
             * 当前帧没有更新
             */
            break;
        } else {
            if(pme->compressed&&pme->decompressed&&pme->dibits_segs) {
                int bytes_accessed=0;
                for(int idx=0;idx<count;idx++) {
                    memcpy(pme->compressed+bytes_accessed,plts[idx],lens[idx]);
                    bytes_accessed+=lens[idx];
                }

                int bytes_offset=0;
                uint64_t* partition_count_plt=(uint64_t*)pme->compressed;
                
                if(*partition_count_plt>0) {
                    int clip_width=64,clip_height=64;
                    int rows=(pme->height+clip_height-1)/clip_height;
                    int cols=(pme->width+clip_width-1)/clip_width;

                    size_t decompressed_lens[2048]={0};
                    size_t decompressed_offset=0;
                    uint64_t* block_idx_base_plt=partition_count_plt+1;
                    bytes_offset+=sizeof(uint64_t)+(*partition_count_plt)*sizeof(uint64_t);
                    for(int idx=0;idx<*partition_count_plt;idx++) {
                        uint64_t curr_block_idx=*((uint64_t*)((uint8_t*)pme->compressed+bytes_offset));
                        uint64_t curr_block_size=*((uint64_t*)((uint8_t*)pme->compressed+(bytes_offset+sizeof(uint64_t))));
                        uint8_t* curr_block_data=(uint8_t*)pme->compressed+(bytes_offset+sizeof(uint64_t)*2);
                        
                        //解压数据,临时放置在 dibits_segs
                        int cx_decompressed,cy_decompressed;
                        if(0!=decodewebp2bmp2(curr_block_data,curr_block_size,(uint8_t*)pme->dibits_segs+decompressed_offset,&cx_decompressed,&cy_decompressed,&decompressed_lens[idx])) {
                            printf("图像数据解压失败.\n");
                            return -1;
                        }

                        //将数据写入dibits
                        //根据curr_block_idx 还原 clip_x,clip_y,clip_width,clip_height
                        int clip_x,clip_y;
                        int curr_clip_width=clip_width;
                        int curr_clip_height=clip_height;
                        int row_idx=curr_block_idx/cols;
                        int col_idx=curr_block_idx%cols;
                        clip_x=col_idx*clip_width;
                        clip_y=row_idx*clip_height;
                        if(col_idx==cols-1) {
                            curr_clip_width=pme->width-col_idx*clip_width;
                        }
                        if(row_idx==rows-1) {
                            curr_clip_height=pme->height-row_idx*clip_height;
                        }
                        printf("差异块 索引 %d, 转化为 行列索引 row_idx %d, col_idx %d,块起点坐标(%d,%d) 宽 %d,高%d\n",
                            curr_block_idx,
                            row_idx,col_idx,
                            clip_x,clip_y,
                            curr_clip_width,curr_clip_height
                        );
                        if(0!=loadclipbmpdata(pme->decompressed,pme->width,pme->height,(uint8_t*)pme->dibits_segs+decompressed_offset,clip_x,clip_y,curr_clip_width,curr_clip_height,decompressed_lens[idx])) {
                            printf("载入切片(%d,%d) %d × %d DIBITs数据失败\n",clip_x,clip_y,curr_clip_width,curr_clip_height);
                            return -1;
                        }

                        decompressed_offset+=decompressed_lens[idx];
                        bytes_offset+=sizeof(uint64_t)*2+curr_block_size;
                    }
                }
            }
            InvalidateRect(pme->hwnd,NULL,TRUE);
        }
    } break;
    default: {} break;
    }

    printf("**************ANALYSIS DONE**********\n");
    return 0;
}

int TimerProc_SVR2(timer_settings* timer,void* param) {
    printf("TimerProc_SVR2 Triggered.\n");

    pMonExg exg=GetMonExg();
    if(!exg) {
        printf("交换参数尚未准备完毕.\n");
        return -1;
    }

    /**
     * 判断是否存在前置缓冲是否启用
     */
    if(!exg->dibits||!exg->predibits_segs||!exg->prehashcodes||!exg->dibits_segs||!exg->hashcodes) {
        printf("Partition Segment Transfer Mode Parameters Invalid.\n");
        return -1;
    }

    size_t size;
    if(0!=capturebmpdata(exg->dibits,&size)) {
        printf("capturebmpdata 失败.\n");
    }

    /**
     * 参数
     */
    int clip_width=64,clip_height=64;
    int clip_data_stride=clip_width*clip_height*3;//BGR 24位图
    int rows=(exg->height+(clip_height-1))/clip_height;
    int cols=(exg->width+(clip_width-1))/clip_width;

    if(exg->predibits_flag==0) {
        int ret=TimerProc_SVR(timer,param);
    }

    XXH64_state_t* xxhash_context=XXH64_createState();
    uint64_t indexes_changed_blocks[1024*10]={0};
    uint64_t counts_changed_blocks=0;
    uint64_t compressed_offset=0;
    for(int row_idx=0;row_idx<rows;row_idx++) {
        for(int col_idx=0;col_idx<cols;col_idx++) {
            int clip_x=col_idx*clip_width;
            int clip_y=row_idx*clip_height;
            int curr_clip_width=(col_idx==cols-1)?(exg->width%clip_width==0?clip_width:exg->width%clip_width):clip_width;
            int curr_clip_height=(row_idx==rows-1)?(exg->height%clip_height==0?clip_height:exg->height%clip_height):clip_height;
            int dibits_segs_offset=curr_clip_height*curr_clip_width*3; //存储dibits_segs的偏移量
            /**
             * 如果clip的列不是4字节对齐，会被添加对齐，对于1920，是对齐的
             * 所以理论上dibits_segs_offset 和 clip_data_size 是一致的
             */
            int block_idx=row_idx*cols+col_idx;

            uint8_t* dibits_seg_plt=((uint8_t*)(exg->dibits_segs))+dibits_segs_offset;
            size_t clip_data_size;
            uint64_t* hashcode_plt=((uint64_t*)exg->hashcodes)+block_idx;
            size_t hash_size;
            hashbmpdata(exg->width,exg->height,exg->dibits,clip_x,clip_y,curr_clip_width,curr_clip_height,dibits_seg_plt,&clip_data_size,xxhash_context,hashcode_plt,&hash_size);

            if(1) {
                uint64_t* prehash_plt=((uint64_t*)exg->prehashcodes)+block_idx;

                if(*prehash_plt!=*hashcode_plt) {
                    indexes_changed_blocks[counts_changed_blocks]=block_idx;
                    counts_changed_blocks++;
                    //写入块号，尺寸
                    uint64_t content_head[2]={
                        block_idx,
                    };
                    uint64_t* pcontent_head=(uint64_t*)((uint8_t*)exg->compressed+compressed_offset);
                    compressed_offset+=sizeof(content_head);
                    encodebmp2webp2(dibits_seg_plt,curr_clip_width,curr_clip_height,clip_data_size,*exg->pquality,(uint8_t*)exg->compressed+compressed_offset,&content_head[1]);
                    compressed_offset+=content_head[1];
                    memcpy(pcontent_head,content_head,sizeof(content_head));
                    
                    //用新数据覆盖旧数据
                    uint8_t* predibits_seg_plt=((uint8_t*)exg->predibits_segs)+dibits_segs_offset;
                    memcpy(predibits_seg_plt,dibits_seg_plt,clip_data_size);
                    *prehash_plt=*hashcode_plt;
                }
            }
        }
    }

    XXH64_freeState(xxhash_context);

    if(counts_changed_blocks>0&&exg->predibits_flag==1) {
        //构造数据发送协议
        pNIHeader head=(pNIHeader)(exg->stream->send_buffer+sizeof(QUIC_BUFFER)*2);
        head->mode=0;
        head->seq=8964;
        head->type=RSP_IMG_DATA_PARTITIONMODE;
        head->len=sizeof(uint64_t)+sizeof(uint64_t)*counts_changed_blocks+compressed_offset;
        printf("需要发送 %d 个差异分块，差异帧 %d bytes.\n",counts_changed_blocks,head->len);

        if(0!=StreamIo_SendData(exg->stream,(uint8_t*)head,sizeof(NIHeader))) {
            printf("发送图像数据 报文 头NIHeadr 失败\n");
        }
        if(0!=StreamIo_SendData(exg->stream,(uint8_t*)&counts_changed_blocks,sizeof(uint64_t))) {
            printf("发送图像数据 报文 内容 PartitionParam::segcounts 失败\n");
        }
        if(0!=StreamIo_SendData(exg->stream,(uint8_t*)indexes_changed_blocks,sizeof(uint64_t)*counts_changed_blocks)) {
            printf("发送图像数据 报文 内容 PartitionParam::seg_indexes 失败\n");
        }
        if(0!=StreamIo_SendData(exg->stream,(uint8_t*)exg->compressed,compressed_offset)) {
            printf("发送图像数据 报文 内容 SegmentData失败\n");
        }
    } else if(counts_changed_blocks==0&&exg->predibits_flag==1) {
        /**
         * 没有变化
         */
        pNIHeader head=(pNIHeader)(exg->stream->send_buffer+sizeof(QUIC_BUFFER)*2);
        head->mode=0;
        head->seq=8964;
        head->type=RSP_IMG_DATA_PARTITIONMODE;
        head->len=sizeof(uint64_t);
        printf("需要发送 %d 个差异分块，差异帧 %d bytes.\n",counts_changed_blocks,head->len);

        if(0!=StreamIo_SendData(exg->stream,(uint8_t*)head,sizeof(NIHeader))) {
            printf("发送图像数据 报文 头NIHeadr 失败\n");
        }
        if(0!=StreamIo_SendData(exg->stream,(uint8_t*)&counts_changed_blocks,sizeof(uint64_t))) {
            printf("发送图像数据 报文 内容 PartitionParam::segcounts 失败\n");
        }
    }

    if(exg->predibits_flag==0) exg->predibits_flag=1;

    return 0;
}

int TimerProc_SVR(timer_settings* timer,void* param) {
    printf("TimerProc_SVR Triggered.\n");
    
    pMonExg exg=GetMonExg();
    if(!exg) {
        printf("交换参数尚未准备完毕.\n");
        return -1;
    }

    pNIHeader head=(pNIHeader)(exg->stream->send_buffer+sizeof(QUIC_BUFFER)*2);
    head->mode=0;
    head->seq=999;
    head->type=RSP_IMG_DATA;

    size_t size;
    if(0!=capturebmpdata(exg->dibits,&size)) {
        printf("capturebmpdata 失败.\n");
    }
    
    //压缩
    encodebmp2webp2(exg->dibits,exg->width,exg->height,size,*exg->pquality,exg->compressed,exg->pcompressed_len);
    head->len=*exg->pcompressed_len;
    
    // (qbuff+1)->Buffer=exg->compressed;
    // (qbuff+1)->Length=*exg->pcompressed_len;

    QUIC_API_TABLE* api=NetIo_GetAPI();
    //QUIC_SEND_FLAG_DELAY_SEND
    /**
     * StreamSend中的QUIC_BUFFER*参数有一个要点：
     * 当数据尚未发送完毕时，QUIC_BUFFER* 的内容是会被使用的
     * 这里最开始使用的是 stream->send_buffer 来处理的，
     * 当该函数重复被调用的时候，存在一种情况：
     * 前一次发送对应的 数据尚未发送完毕，新的发送调用已经产生
     * 由于都使用了 stream->send_buffer 作为 QUIC_BUFFER* 参数
     * 后一次的调用 会覆写前一次正在使用的 QUIC_BUFFER* , 继而
     * 引发异常。
     * 
     * 不能直接使用stream->send_buffer 作为缓冲，需要从中挑选尚未占用的部分
     * 我们以 1秒为缓冲量，假设最大1s的冗余就能完成吞吐
     * 可以将 stream->send_buffer 划分为 大于 等于 帧率的 分区，每次发送使用一个
     * 在发送完成的消息中将被占用的区域更新为可用
     */
    if(0!=StreamIo_SendData(exg->stream,(uint8_t*)head,sizeof(NIHeader))) {
        printf("发送图像数据报文 头 失败\n");
    }
    if(0!=StreamIo_SendData(exg->stream,exg->compressed,head->len)) {
        printf("发送图像数据报文 体 失败\n");
    }

    printf("图像数据 头 %d bytes, 帧 %d bytes\n",sizeof(NIHeader),head->len);

    return 0;
}

int TimerProc_CLIENT(timer_settings* timer,void* param) {

    return 0;
}

pTimerSettings TimerSettings_Initial(LPTHREAD_START_ROUTINE thread_proc,timer_callback timer_proc,void* param) {
    int errcode=0;

    pTimerSettings ts=(pTimerSettings)calloc(sizeof(TimerSettings),1);
    if(!ts) return NULL;

    ts->param=param;
    ts->thread_proc=thread_proc;
    ts->timer_proc=timer_proc;
    if(ts->thread_proc==NULL) ts->thread_proc=TimerSettings_DefaultThreadProc;

    ts->timer=CreateWaitableTimer(NULL,FALSE,NULL);
    if(ts->timer==NULL) {
        printf("创建定时器内核句柄失败.\n");
        errcode=-1;
        goto TIMERSETTINGS_INITIAL_EXIT;
    }

    ts->thread_status=0;
    ts->thread=CreateThread(NULL,0,ts->thread_proc,ts,CREATE_SUSPENDED,&ts->threadID);
    if(ts->thread==NULL) {
        printf("创建定时器线程失败.\n");
        errcode=-1;
        goto TIMERSETTINGS_INITIAL_EXIT;
    }

TIMERSETTINGS_INITIAL_EXIT:
    if(errcode!=0&&ts) {
        TimerSettings_Clear(ts);
        ts=NULL;
    }

    return (errcode==0)?ts:NULL;
}

DWORD WINAPI TimerSettings_DefaultThreadProc(LPVOID param) {
    pTimerSettings ts=(pTimerSettings)param;
    while(ts->thread_status!=3) {
        DWORD dwWait=WaitForSingleObject(ts->timer,INFINITE);
        if(dwWait==WAIT_OBJECT_0) {
            if(ts->timer_proc) ts->timer_proc(ts,ts->param);
        } else {
            break;
        }
    }
    ts->thread_status=2;
    return 0;
}

void TimerSettings_Clear(pTimerSettings ts) {
    if(ts) {
        if(ts->timer) {
            CancelWaitableTimer(ts->timer);
            CloseHandle(ts->timer);
            ts->timer_proc=NULL;
        }

        if(ts->thread) {
            ts->thread_status=3;//指令退出
            ResumeThread(ts->thread);
            WaitForSingleObject(ts->thread,3000);
            CloseHandle(ts->thread);
            ts->thread=NULL;
        }

        free(ts);
    }
}

int TimerSettings_Start(pTimerSettings ts) {
    if(!ts) return -1;

    if(ts->thread_status!=2) {
        ts->thread_status=1;
        ResumeThread(ts->thread);

        return 0;
    } else {
        printf("定时器线程已经无效\n");
        return -1;
    }
}

/**
 * 参数介绍：
 * wait_millsec： 即 等待多久（毫秒）去触发定时器，从调用到第一次触发的事件
 * millsec: 定时器的时间间隔，连续两个触发之间的时间间隔
 */
int TimerSettings_SetTimer(pTimerSettings ts,int wait_millsec,int millsec) {
    LARGE_INTEGER liDueTime;
    liDueTime.QuadPart=-10000*wait_millsec;//100纳秒单位 1毫秒等于 1,000微秒，等于 1,000,000 纳秒

    if(!ts||ts->timer==NULL) return -1;
    if(!SetWaitableTimer(ts->timer,&liDueTime,millsec,NULL,NULL,FALSE)) {
        return -1;
    } else {
        return 0;
    }
}

int TimerSettings_Stop(pTimerSettings ts)  {
    if(!ts) return -1;

    if(ts->thread_status!=2) {
        ts->thread_status=0;
        SuspendThread(ts->thread);

        return 0;
    } else {
        printf("定时器线程已经无效\n");
        return -1;
    }
}
