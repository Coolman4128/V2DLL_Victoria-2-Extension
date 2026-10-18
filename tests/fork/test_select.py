import argparse
import hashlib
import pathlib
import subprocess
import sys
import struct


def extract_function(source: str, signature: str) -> str:
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    state = "code"
    i = brace
    while i < len(source):
        c = source[i]
        n = source[i + 1] if i + 1 < len(source) else ""
        if state == "code":
            if c == '"': state = "string"
            elif c == "'": state = "char"
            elif c == "/" and n == "/": state = "line"; i += 1
            elif c == "/" and n == "*": state = "block"; i += 1
            elif c == "{": depth += 1
            elif c == "}":
                depth -= 1
                if depth == 0: return source[start:i + 1]
        elif state == "string" or state == "char":
            if c == "\\": i += 1
            elif (state == "string" and c == '"') or (state == "char" and c == "'"): state = "code"
        elif state == "line":
            if c in "\r\n": state = "code"
        elif state == "block" and c == "*" and n == "/": state = "code"; i += 1
        i += 1
    raise ValueError("unclosed source function: " + signature)


def sha(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def main() -> int:
    ap = argparse.ArgumentParser(description="Build and run the actual-source Win32 select contract harness.")
    ap.add_argument("--build-root", type=pathlib.Path, required=True)
    ap.add_argument("--msvc", default=r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat")
    ap.add_argument("--r02d-source", type=pathlib.Path, default=pathlib.Path.home() / "AppData/Local/V2DLLFork/R02d/build-checkpoint/source/V2TechButton.cpp")
    args = ap.parse_args()
    repo = pathlib.Path(__file__).resolve().parents[2]
    root = args.build_root.resolve()
    if root == repo or repo in root.parents or root.exists():
        raise SystemExit("build root must be new and outside the repository")
    root.mkdir(parents=True)
    srcpath = repo / "Source/V2/V2/V2TechButton.cpp"
    source = srcpath.read_text(encoding="utf-8-sig")
    r02d = args.r02d_source.resolve()
    if not r02d.is_file(): raise SystemExit("missing R02d staged source: " + str(r02d))
    staged_hash = sha(r02d)
    current_hash = sha(srcpath)
    if staged_hash != current_hash: raise SystemExit(f"current source differs from R02d staged snapshot: {current_hash} != {staged_hash}")
    start = source.index("struct SockTimeVal")
    end = source.index("typedef int (WINAPI* tRecv)", start)
    defs = source[start:end]
    for signature in ("static void NoteSelect(", "static int WINAPI HookSelect(", "static void InstallSelectHook("):
        extract_function(defs, signature)
    harness_path = pathlib.Path(__file__).with_name("select_tests.cpp")
    harness = harness_path.read_text(encoding="ascii")
    injected = "static bool HookIatOrdinal(HMODULE,const char*,WORD,void*,void**);\n" + defs + r'''
static unsigned providerCalls = 0;
static int providerResult = 0;
static int providerExpectedNfds = 0;
static void* providerExpectedRead = NULL;
static void* providerExpectedWrite = NULL;
static void* providerExpectedExcept = NULL;
static SockTimeVal* providerSeenTv = NULL;
static SockTimeVal providerSeenValue = {0,0};
static int providerSawExact = 0;
static int WINAPI ProviderSelect(int nfds, void* r, void* w, void* e, SockTimeVal* tv) {
    ++providerCalls; providerSeenTv=tv; if(tv) providerSeenValue=*tv;
    providerSawExact=nfds==providerExpectedNfds && r==providerExpectedRead && w==providerExpectedWrite && e==providerExpectedExcept;
    return providerResult;
}
static int WINAPI RealWinsockSelect(int nfds, void* r, void* w, void* e, SockTimeVal* tv) {
    return ::select(nfds,(fd_set*)r,(fd_set*)w,(fd_set*)e,(TIMEVAL*)tv);
}
static unsigned iatCalls=0;
static HMODULE iatModule[2]; static char iatName[2][16]; static WORD iatOrdinal[2];
static int iatMode=0;
static bool HookIatOrdinal(HMODULE module,const char* name,WORD ordinal,void* hook,void** orig) {
    (void)hook; unsigned i=iatCalls++; if(i<2){iatModule[i]=module; strncpy_s(iatName[i],name,_TRUNCATE); iatOrdinal[i]=ordinal;}
    if((iatMode==1 && i==0)||(iatMode==2 && i==1)){*orig=(void*)ProviderSelect;return true;} return false;
}
'''
    if harness.count("// ACTUAL_SELECT_SNIPPETS") != 1: raise SystemExit("harness injection marker error")
    generated = harness.replace("// ACTUAL_SELECT_SNIPPETS", injected)
    # Test install behavior before running HookSelect contracts.
    generated = generated.replace("unsigned checks = 0;", r'''unsigned checks = 0;
#define CHECK(x) do { if(!(x)){ fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); return 2;} ++checks;} while(0)
    HMODULE expectedExe=GetModuleHandleA(NULL); iatMode=1; iatCalls=0; g_realSelect=NULL; InstallSelectHook();
    CHECK(iatCalls==1 && iatModule[0]==expectedExe && strcmp(iatName[0],"ws2_32.dll")==0 && iatOrdinal[0]==18 && g_realSelect==(tSelect)ProviderSelect);
    iatMode=2; iatCalls=0; g_realSelect=NULL; InstallSelectHook();
    CHECK(iatCalls==2 && strcmp(iatName[0],"ws2_32.dll")==0 && strcmp(iatName[1],"WS2_32.dll")==0 && iatOrdinal[0]==18 && iatOrdinal[1]==18 && g_realSelect==(tSelect)ProviderSelect);
    iatMode=0; iatCalls=0; g_realSelect=(tSelect)ProviderSelect; InstallSelectHook();
    CHECK(iatCalls==2 && g_realSelect==(tSelect)ProviderSelect);
    iatCalls=0;''')
    outcpp = root / "select_contract.cpp"
    outcpp.write_text(generated, encoding="utf-8", newline="\n")
    metadata = root / "identity.txt"
    harness_hash = sha(harness_path)
    runner_hash = sha(pathlib.Path(__file__).resolve())
    metadata.write_text(f"current_source_sha256={current_hash}\nr02d_staged_source_sha256={staged_hash}\nharness_sha256={harness_hash}\nrunner_sha256={runner_hash}\ngenerated_sha256={sha(outcpp)}\nsource_path={srcpath}\nstaged_path={r02d}\n", encoding="ascii")
    exe = root / "select_contract.exe"
    command = f'@echo off\r\ncall "{args.msvc}" x86 >nul\r\nif errorlevel 1 exit /b %errorlevel%\r\ncl /nologo /W4 /WX /O2 /EHsc /MD /DWIN32_LEAN_AND_MEAN /D_CRT_SECURE_NO_WARNINGS /utf-8 "{outcpp}" /Fe:"{exe}" /link ws2_32.lib\r\n'
    buildcmd = root / "build.cmd"
    buildcmd.write_text(command, encoding="ascii", newline="")
    build = subprocess.run(["cmd.exe", "/d", "/c", str(buildcmd)], cwd=root, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (root / "compile.log").write_text(build.stdout, encoding="utf-8")
    if build.returncode: print(build.stdout); return build.returncode
    image = exe.read_bytes()
    peoff = struct.unpack_from("<I", image, 0x3C)[0]
    if image[peoff:peoff+4] != b"PE\0\0" or struct.unpack_from("<H",image,peoff+24)[0] != 0x10B:
        raise SystemExit("generated contract executable is not PE32")
    base = struct.unpack_from("<I",image,peoff+24+28)[0]
    print(f"RUNTIME executable={exe} sha256={sha(exe)} PE32_ImageBase=0x{base:08X} preferred_base=0x{base:08X} RVA=0x0 path={exe}")
    print("RUNTIME ws2_32!select ordinal18=", end="")
    try:
        import ctypes
        dll=ctypes.WinDLL("ws2_32.dll")
        kernel=ctypes.WinDLL("kernel32.dll")
        kernel.GetProcAddress.argtypes=[ctypes.c_void_p,ctypes.c_char_p]
        kernel.GetProcAddress.restype=ctypes.c_void_p
        kernel.GetModuleHandleExA.argtypes=[ctypes.c_uint,ctypes.c_char_p,ctypes.POINTER(ctypes.c_void_p)]
        kernel.GetModuleFileNameA.argtypes=[ctypes.c_void_p,ctypes.c_char_p,ctypes.c_uint]
        addr=kernel.GetProcAddress(ctypes.cast(dll._handle,ctypes.c_void_p),ctypes.c_char_p(18))
        if not addr: raise OSError("ordinal 18 lookup failed")
        owner=ctypes.c_void_p()
        if not kernel.GetModuleHandleExA(0x00000004|0x00000002,ctypes.c_char_p(addr),ctypes.byref(owner)):
            raise OSError("GetModuleHandleExA address owner lookup failed")
        pathbuf=ctypes.create_string_buffer(260); kernel.GetModuleFileNameA(owner,pathbuf,260)
        print(f"address=0x{addr:08X} allocation_base=0x{owner.value:08X} RVA=0x{addr-owner.value:X} path={pathbuf.value.decode(errors='replace')}")
    except Exception as exc: print("GetProcAddress unavailable:", exc)
    run = subprocess.run([str(exe)], cwd=root, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=10)
    runtime_log = root / "runtime.log"
    runtime_log.write_text(run.stdout, encoding="utf-8")
    print(run.stdout, end="")
    if run.returncode == 0:
        metadata.write_text(metadata.read_text(encoding="ascii") + f"build_manifest_sha256={sha(buildcmd)}\ncompile_log_sha256={sha(root / 'compile.log')}\nexe_sha256={sha(exe)}\nruntime_log_sha256={sha(runtime_log)}\n", encoding="ascii")
        print(f"ARTIFACTS harness={harness_hash} runner={runner_hash} generated={sha(outcpp)} build_manifest={sha(buildcmd)} compile_log={sha(root / 'compile.log')} exe={sha(exe)} runtime_log={sha(runtime_log)}")
    return run.returncode


if __name__ == "__main__":
    sys.exit(main())
