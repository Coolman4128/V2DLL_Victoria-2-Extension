#include <winsock2.h>
#include <windows.h>
#include <intrin.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

static DWORD controlledReturnAddress;
static DWORD controlledTid;
static DWORD controlledNow;
static DWORD Controlled_ReturnAddress() { return controlledReturnAddress; }
static DWORD Controlled_GetCurrentThreadId() { return controlledTid; }
static DWORD Controlled_NowMs() { return controlledNow++; }
#define _ReturnAddress() Controlled_ReturnAddress()
#define GetCurrentThreadId() Controlled_GetCurrentThreadId()
#define NowMs() Controlled_NowMs()

static volatile LONG g_selectCalls = 0;
static volatile LONG g_selMainN = 0;
static volatile LONG g_selMainMs = 0;
static volatile DWORD g_presentTid = 0;
static DWORD g_base = 0;
static DWORD g_imageSize = 0;
static void Log(const char*, ...) {}
static void LogDbg(const char*, ...) {}
// ACTUAL_SELECT_SNIPPETS
int main() {
    static_assert(sizeof(SockTimeVal) == sizeof(TIMEVAL), "SockTimeVal/TIMEVAL size");
    static_assert(offsetof(SockTimeVal, tv_sec) == offsetof(TIMEVAL, tv_sec), "tv_sec offset");
    static_assert(offsetof(SockTimeVal, tv_usec) == offsetof(TIMEVAL, tv_usec), "tv_usec offset");
    static_assert(sizeof(long) == 4, "Win32 long ABI");
    unsigned checks = 0;
    g_base = 0x10000000; g_imageSize = 0x900000; g_presentTid = 77;
    char exePath[MAX_PATH]={0}; GetModuleFileNameA(NULL,exePath,MAX_PATH);
    printf("RUNTIME exe_allocation_base=%p path=%s\n",GetModuleHandleA(NULL),exePath);
    g_realSelect = ProviderSelect;
    SockTimeVal t; void* r=(void*)0x1111; void* w=(void*)0x2222; void* e=(void*)0x3333;
    providerExpectedNfds=9; providerExpectedRead=r; providerExpectedWrite=w; providerExpectedExcept=e;
#define RESET(sec,usec) do { t.tv_sec=(sec); t.tv_usec=(usec); providerCalls=0; providerSeenTv=NULL; providerSawExact=0; providerResult=23; controlledNow=100; } while(0)
    // Address interval is half-open: mixer calls copy/clamp only at 2ms+.
    controlledReturnAddress=g_base+0x689C00; controlledTid=77; RESET(0,2000);
    int boundaryResult=HookSelect(9,r,w,e,&t); CHECK(boundaryResult==23 && providerCalls==1 && providerSawExact && providerSeenTv==&t && providerSeenValue.tv_usec==2000 && t.tv_sec==0 && t.tv_usec==2000);
    RESET(0,2001); HookSelect(9,r,w,e,&t); CHECK(providerSeenTv && providerSeenTv!=&t && providerSeenValue.tv_sec==0 && providerSeenValue.tv_usec==1000 && t.tv_sec==0 && t.tv_usec==2001);
    RESET(1,0); HookSelect(9,r,w,e,&t); CHECK(providerSeenTv!=&t && providerSeenValue.tv_sec==0 && providerSeenValue.tv_usec==1000 && t.tv_sec==1 && t.tv_usec==0);
    RESET(-1,0); HookSelect(9,r,w,e,&t); CHECK(providerSeenTv==&t && t.tv_sec==-1 && t.tv_usec==0);
    RESET(0,-1); HookSelect(9,r,w,e,&t); CHECK(providerSeenTv==&t && t.tv_usec==-1);
    controlledReturnAddress=g_base+0x68BFFF; RESET(0,2001); HookSelect(9,r,w,e,&t); CHECK(providerSeenTv!=&t && t.tv_usec==2001);
    controlledReturnAddress=g_base+0x689BFF; RESET(0,2001); HookSelect(9,r,w,e,&t); CHECK(providerSeenTv==&t && t.tv_usec==2001);
    controlledReturnAddress=g_base+0x68C000; RESET(1,0); HookSelect(9,r,w,e,&t); CHECK(providerSeenTv==&t && t.tv_sec==1);
    controlledReturnAddress=0x12345678; RESET(1,0); HookSelect(9,r,w,e,&t); CHECK(providerSeenTv==&t);
    // NULL passes through exactly; fake provider avoids an unbounded wait.
    controlledReturnAddress=g_base+0x68B47D; RESET(0,0); HookSelect(9,r,w,e,NULL); CHECK(providerCalls==1 && providerSeenTv==NULL);
    // Provider absent returns -1, preserves timeval and still accounts call.
    g_realSelect=NULL; RESET(0,2001); int missing=HookSelect(9,r,w,e,&t); CHECK(missing==-1 && providerCalls==0 && t.tv_usec==2001);
    // Matching/nonmatching present TID accounting uses elapsed delta; controlled clock advances 1ms/call.
    g_realSelect=ProviderSelect; LONG oldN=g_selMainN, oldMs=g_selMainMs; g_presentTid=77; controlledTid=77; RESET(0,0); HookSelect(9,r,w,e,&t);
    CHECK(g_selMainN==oldN+1 && g_selMainMs==oldMs+1);
    oldN=g_selMainN; oldMs=g_selMainMs; controlledTid=78; RESET(0,0); HookSelect(9,r,w,e,&t);
    CHECK(g_selMainN==oldN && g_selMainMs==oldMs);
    CHECK(g_selectCalls==13);
    CHECK(g_seenSelectN>0);
    // Actual owned Winsock provider: loopback UDP only, bounded readiness and timeout.
    WSADATA wd; CHECK(WSAStartup(MAKEWORD(2,2),&wd)==0);
    SOCKET rx=INVALID_SOCKET, tx=INVALID_SOCKET; int result=3;
    do {
        rx=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP); tx=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP); if(rx==INVALID_SOCKET||tx==INVALID_SOCKET) break;
        sockaddr_in addr; memset(&addr,0,sizeof addr); addr.sin_family=AF_INET; addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK); addr.sin_port=0;
        if(bind(rx,(sockaddr*)&addr,sizeof addr)==SOCKET_ERROR) break;
        int alen=sizeof addr; if(getsockname(rx,(sockaddr*)&addr,&alen)==SOCKET_ERROR) break;
        const char byte='x'; if(sendto(tx,&byte,1,0,(sockaddr*)&addr,sizeof addr)!=1) break;
        fd_set reads; FD_ZERO(&reads); FD_SET(rx,&reads); SockTimeVal ready={1,0}; controlledReturnAddress=g_base+0x680000; g_realSelect=RealWinsockSelect;
        int readyResult=HookSelect(0,&reads,NULL,NULL,&ready); CHECK(readyResult==1 && FD_ISSET(rx,&reads));
        char received=0; if(recvfrom(rx,&received,1,0,NULL,NULL)!=1 || received!=byte) break;
        fd_set empty; FD_ZERO(&empty); FD_SET(rx,&empty); SockTimeVal timeout={0,100000}; DWORD before=GetTickCount();
        int timeoutResult=HookSelect(0,&empty,NULL,NULL,&timeout); DWORD elapsed=GetTickCount()-before;
        CHECK(timeoutResult==0 && elapsed<1500);
        result=0;
    } while(0);
    if(rx!=INVALID_SOCKET) closesocket(rx); if(tx!=INVALID_SOCKET) closesocket(tx); WSACleanup();
    if(result){ fprintf(stderr,"FAIL bounded loopback Winsock setup/operation: WSA=%d\n",WSAGetLastError()); return 3; }
    printf("PASS select source-contract checks=%u controlled calls=%ld present=%ld/%ld; Winsock loopback readiness=1 timeout=0 elapsed<=1500ms; owned sockets closed, WSACleanup completed\n",checks,(long)g_selectCalls,(long)g_selMainN,(long)g_selMainMs);
    return 0;
}
