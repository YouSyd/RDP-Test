##------------------------------------------------------##
##     rdp frame test                                   ##
##------------------------------------------------------##
##  该程序将实现一个类似rdp的测试平台                      ##
##                                                      ##
##------------------------------------------------------##
## 生成调试信息的程序
##CC = cl /Zi /EHsc /D"SCRATCHVIEW_DBGTEST"
CC = cl /Zi /EHsc
### $(CCDBG) ......
LINKDBG = /DEBUG
### LINK ... $(LINKDBG)

##CC = cl
CFLAGS = user32.lib gdi32.lib kernel32.lib Shell32.lib Shlwapi.lib Comctl32.lib comdlg32.lib container.lib frame.lib dialog.lib button.lib Ole32.lib libwebp.lib msquic.lib xxhash.lib ws2_32.lib

## yDark 控件模块
LIBC=E:\SRC_REF\dsc_test\dsctrls\lib
INCC=E:\SRC_REF\dsc_test\dsctrls\include
LIBDBGC=E:\SRC_REF\dsc_test\dsctrls\DBGlib

## msquic 网络模块
LIBQ=E:\SRC_REF\vcpkg\vcpkg\installed\x64-windows\lib
INCQ=E:\SRC_REF\vcpkg\vcpkg\installed\x64-windows\include

# WebP 模块
WEBP_INCPATH="E:/SRC_REF/search_proj/libwebp-1.2.0-rc3-windows-x64/include/"
WEBP_LIBPATH="E:/SRC_REF/search_proj/libwebp-1.2.0-rc3-windows-x64/lib"

all: frame_proj.exe quic_client_test.exe quic_server_test.exe MonitorCfg.dll
    @echo.Done.

target: frame_proj.exe
    @call frame_proj.exe

clean:
    @if exist *.obj @DEL -S -Q *.obj
    @if exist *.lib @DEL -S -Q *.lib
    @if exist *.exp @DEL -S -Q *.exp
    @if exist *.dll @DEL -S -Q *.dll
    @if exist *.bak @DEL -S -Q *.bak
    @if exist *.pdb @DEL -S -Q *.pdb
    @if exist *.ilk @DEL -S -Q *.ilk
    @if exist frame_proj.exe @DEL -S -Q frame_proj.exe
    
frame_proj.obj:frame_proj.cpp
    $(CC) -c frame_proj.cpp /I $(INCC)

monitor.obj:monitor.cpp
    $(CC) -c monitor.cpp /I $(INCC)
	
monitor.lib:monitor.obj
   LIB monitor.obj /LIBPATH:$(LIBC) /OUT:monitor.lib

bmpdata.obj:bmpdata.cpp
    $(CC) -c bmpdata.cpp /I $(INCC) /I $(WEBP_INCPATH)
	
bmpdata.lib:bmpdata.obj
   LIB bmpdata.obj /LIBPATH:$(LIBC) /LIBPATH:$(WEBP_LIBPATH) /OUT:bmpdata.lib

netio.obj:netio.cpp
    $(CC) -c netio.cpp /I $(INCQ) /I $(WEBP_INCPATH)
	
netio.lib:netio.obj
   LIB netio.obj /LIBPATH:$(LIBQ) /LIBPATH:$(WEBP_LIBPATH) /OUT:netio.lib

utils.obj:utils.cpp
    $(CC) -c utils.cpp /I $(INCC)
	
utils.lib:utils.obj
   LIB utils.obj /LIBPATH:$(LIBC) /OUT:utils.lib

frame_proj.exe:frame_proj.obj monitor.lib bmpdata.lib utils.lib netio.lib
    link frame_proj.obj monitor.lib bmpdata.lib utils.lib netio.lib $(LIBDBGC)\container.obj $(CFLAGS) /out:$@ /LIBPATH:$(LIBC) /LIBPATH:$(LIBQ) /MANIFEST:EMBED $(LINKDBG) 

quic_server_test.obj: quic_server_test.cpp
    $(CC) -c quic_server_test.cpp /I $(INCQ)

quic_server_test.exe: quic_server_test.obj netio.obj monitor.obj bmpdata.obj utils.obj
    link quic_server_test.obj netio.obj monitor.obj bmpdata.obj utils.obj $(CFLAGS) /out:$@ /LIBPATH:$(LIBC) /LIBPATH:$(LIBQ) /MANIFEST:EMBED $(LINKDBG) 

quic_client_test.obj: quic_client_test.cpp
    $(CC) -c quic_client_test.cpp /I $(INCQ)

quic_client_test.exe: quic_client_test.obj netio.obj  monitor.obj bmpdata.obj utils.obj
    link quic_client_test.obj netio.obj monitor.obj bmpdata.obj utils.obj $(CFLAGS) /out:$@ /LIBPATH:$(LIBC) /LIBPATH:$(LIBQ) /MANIFEST:EMBED $(LINKDBG) 
# workflow_proj.res: workflow_proj.rc
#     @RC workflow_proj.rc

MonitorCfg.res: config.rc
    @RC /fo MonitorCfg.res config.rc 

MonitorCfg.obj: config.cpp
    $(CC) -c config.cpp /Fo:MonitorCfg.obj

MonitorCfg.dll: MonitorCfg.obj MonitorCfg.res
    link -DLL MonitorCfg.obj MonitorCfg.res $(CFLAGS) /out:MonitorCfg.dll


test: all
    @ECHO.frame_proj Proj assumble.
    @if exist frame_proj.exe call frame_proj.exe

svr: all
    @echo "Current directory: $(PWD)"
    @echo "Looking for quic_server_test.exe"
    @ECHO.quic_server_test Proj assumble.
    @if exist quic_server_test.exe call quic_server_test.exe

client: all
    @ECHO.quic_client_test Proj assumble.
    @if exist quic_client_test.exe call quic_client_test.exe
