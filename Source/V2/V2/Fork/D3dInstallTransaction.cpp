#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "D3dInstallTransaction.h"
#include "StartupIdentity.h"
#include "Win32Memory.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

#if !defined(_M_IX86)
#error The D3D install transaction supports Win32 only.
#endif

namespace fork_boundary {
namespace {

constexpr std::uint32_t kPinnedExeImageSize=0x1092000;
constexpr std::size_t kMaxSections=32;
constexpr std::size_t kMaxDescriptors=64;
constexpr std::size_t kMaxThunks=512;
constexpr std::size_t kMaxName=96;
constexpr std::size_t kMaxRelocations=30000;
constexpr std::size_t kMaxFile=8*1024*1024;
constexpr std::size_t kPathChars=32768;

struct GateExit {
    volatile long* gate;
    long next=0;
    ~GateExit() noexcept {
        InterlockedExchange(reinterpret_cast<volatile LONG*>(gate),static_cast<LONG>(next));
    }
};

bool InRange(std::uint32_t rva,std::size_t count,std::uint32_t size) noexcept {
    return count && rva<size && count<=size-rva;
}

template<class T>
bool ReadMapped(const MemoryAccess& memory,std::uint32_t rva,
                std::uint32_t size,T& output) {
    return InRange(rva,sizeof(T),size) &&
        memory.read(rva,reinterpret_cast<unsigned char*>(&output),sizeof(T));
}

template<class T>
bool ReadRaw(const std::vector<unsigned char>& bytes,std::size_t offset,T& output) {
    if (offset>bytes.size() || sizeof(T)>bytes.size()-offset) return false;
    std::memcpy(&output,bytes.data()+offset,sizeof(T));
    return true;
}

bool ReadMappedName(const MemoryAccess& memory,std::uint32_t rva,
                    std::uint32_t size,char (&output)[kMaxName]) {
    for (std::size_t i=0;i<kMaxName;++i) {
        if (rva>std::numeric_limits<std::uint32_t>::max()-i ||
            !InRange(rva+static_cast<std::uint32_t>(i),1,size) ||
            !memory.read(rva+static_cast<std::uint32_t>(i),
                reinterpret_cast<unsigned char*>(&output[i]),1)) return false;
        if (!output[i]) return true;
    }
    return false;
}

char LowerAscii(char value) noexcept {
    return value>='A' && value<='Z' ? static_cast<char>(value-'A'+'a') : value;
}
bool SameAsciiNoCase(const char* left,const char* right) noexcept {
    for (;*left && *right;++left,++right)
        if (LowerAscii(*left)!=LowerAscii(*right)) return false;
    return *left==*right;
}

bool ReadMappedPe(const MemoryAccess& memory,std::uint32_t size,
                  IMAGE_NT_HEADERS32& nt,std::uint32_t& ntRva) {
    IMAGE_DOS_HEADER dos{};
    if (!ReadMapped(memory,0,size,dos) || dos.e_magic!=IMAGE_DOS_SIGNATURE ||
        dos.e_lfanew<0) return false;
    ntRva=static_cast<std::uint32_t>(dos.e_lfanew);
    return ReadMapped(memory,ntRva,size,nt) && nt.Signature==IMAGE_NT_SIGNATURE &&
        nt.FileHeader.Machine==IMAGE_FILE_MACHINE_I386 &&
        nt.FileHeader.NumberOfSections>0 && nt.FileHeader.NumberOfSections<=kMaxSections &&
        nt.FileHeader.SizeOfOptionalHeader>=sizeof(IMAGE_OPTIONAL_HEADER32) &&
        nt.OptionalHeader.Magic==IMAGE_NT_OPTIONAL_HDR32_MAGIC &&
        nt.OptionalHeader.SizeOfImage==size;
}

bool AuthenticateExeImport(const MemoryAccess& memory,const D3dInstallExpectation& expected) {
    IMAGE_NT_HEADERS32 nt{};
    std::uint32_t ntRva=0;
    if (!ReadMappedPe(memory,expected.exeImageSize,nt,ntRva) ||
        nt.OptionalHeader.NumberOfRvaAndSizes<=IMAGE_DIRECTORY_ENTRY_IMPORT) return false;
    const auto& dir=nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!InRange(dir.VirtualAddress,dir.Size,expected.exeImageSize) ||
        dir.Size<sizeof(IMAGE_IMPORT_DESCRIPTOR) ||
        dir.Size>kMaxDescriptors*sizeof(IMAGE_IMPORT_DESCRIPTOR)) return false;
    bool found=false,ended=false;
    for (std::size_t d=0;d<dir.Size/sizeof(IMAGE_IMPORT_DESCRIPTOR);++d) {
        IMAGE_IMPORT_DESCRIPTOR desc{};
        const auto rva=dir.VirtualAddress+static_cast<std::uint32_t>(d*sizeof(desc));
        if (!ReadMapped(memory,rva,expected.exeImageSize,desc)) return false;
        if (!desc.Name && !desc.FirstThunk && !desc.OriginalFirstThunk) {ended=true;break;}
        if (!desc.Name) return false;
        char dll[kMaxName]{};
        if (!ReadMappedName(memory,desc.Name,expected.exeImageSize,dll)) return false;
        if (!SameAsciiNoCase(dll,"d3d9.dll")) continue;
        if (!desc.FirstThunk || !desc.OriginalFirstThunk) return false;
        bool thunkEnded=false;
        for (std::size_t t=0;t<kMaxThunks;++t) {
            const auto offset=static_cast<std::uint64_t>(t)*4;
            if (static_cast<std::uint64_t>(desc.FirstThunk)+offset>0xffffffffULL ||
                static_cast<std::uint64_t>(desc.OriginalFirstThunk)+offset>0xffffffffULL)
                return false;
            const auto lookupRva=desc.OriginalFirstThunk+static_cast<std::uint32_t>(offset);
            const auto slotRva=desc.FirstThunk+static_cast<std::uint32_t>(offset);
            std::uint32_t lookup=0,value=0;
            if (!ReadMapped(memory,lookupRva,expected.exeImageSize,lookup) ||
                !ReadMapped(memory,slotRva,expected.exeImageSize,value)) return false;
            if (!lookup) {thunkEnded=true;break;}
            if (lookup&IMAGE_ORDINAL_FLAG32) continue;
            if (lookup>expected.exeImageSize-2) return false;
            char name[kMaxName]{};
            if (!ReadMappedName(memory,lookup+2,expected.exeImageSize,name)) return false;
            if (std::strcmp(name,"Direct3DCreate9")==0) {
                if (found || slotRva!=expected.exeIatRva || (slotRva&3) || !value)
                    return false;
                found=true;
            }
        }
        if (!thunkEnded) return false;
    }
    return found && ended;
}

struct DiskPe {
    IMAGE_NT_HEADERS32 nt{};
    std::array<IMAGE_SECTION_HEADER,kMaxSections> sections{};
    std::size_t sectionCount=0;
    std::uint32_t textRva=0,textSize=0,textRawSize=0;

    bool Map(std::uint32_t rva,std::size_t count,std::size_t fileSize,
             std::size_t& offset) const noexcept {
        if (!count) return false;
        if (rva<nt.OptionalHeader.SizeOfHeaders &&
            count<=nt.OptionalHeader.SizeOfHeaders-rva &&
            rva<=fileSize && count<=fileSize-rva) {
            offset=rva;return true;
        }
        for (std::size_t i=0;i<sectionCount;++i) {
            const auto& section=sections[i];
            if (rva<section.VirtualAddress ||
                rva-section.VirtualAddress>=section.SizeOfRawData ||
                count>section.SizeOfRawData-(rva-section.VirtualAddress)) continue;
            const auto raw=static_cast<std::uint64_t>(section.PointerToRawData)+
                (rva-section.VirtualAddress);
            if (raw>fileSize || count>fileSize-raw) return false;
            offset=static_cast<std::size_t>(raw);return true;
        }
        return false;
    }
};

bool ParseDiskPe(const std::vector<unsigned char>& bytes,
                 const D3dInstallExpectation& expected,DiskPe& pe) {
    IMAGE_DOS_HEADER dos{};
    if (!ReadRaw(bytes,0,dos) || dos.e_magic!=IMAGE_DOS_SIGNATURE ||
        dos.e_lfanew<0 || !ReadRaw(bytes,static_cast<std::uint32_t>(dos.e_lfanew),pe.nt) ||
        pe.nt.Signature!=IMAGE_NT_SIGNATURE ||
        pe.nt.FileHeader.Machine!=IMAGE_FILE_MACHINE_I386 ||
        pe.nt.FileHeader.TimeDateStamp!=expected.providerTimestamp ||
        pe.nt.FileHeader.SizeOfOptionalHeader<sizeof(IMAGE_OPTIONAL_HEADER32) ||
        pe.nt.OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
        pe.nt.OptionalHeader.SizeOfImage!=expected.providerImageSize ||
        pe.nt.OptionalHeader.ImageBase!=expected.providerPreferredBase ||
        pe.nt.FileHeader.NumberOfSections==0 ||
        pe.nt.FileHeader.NumberOfSections>kMaxSections ||
        pe.nt.OptionalHeader.NumberOfRvaAndSizes<=IMAGE_DIRECTORY_ENTRY_BASERELOC)
        return false;
    pe.sectionCount=pe.nt.FileHeader.NumberOfSections;
    const auto sectionOffset=static_cast<std::uint64_t>(dos.e_lfanew)+sizeof(DWORD)+
        sizeof(IMAGE_FILE_HEADER)+pe.nt.FileHeader.SizeOfOptionalHeader;
    for (std::size_t i=0;i<pe.sectionCount;++i) {
        const auto offset=sectionOffset+i*sizeof(IMAGE_SECTION_HEADER);
        if (offset>bytes.size() ||
            !ReadRaw(bytes,static_cast<std::size_t>(offset),pe.sections[i])) return false;
        const auto& section=pe.sections[i];
        if (std::memcmp(section.Name,".text\0\0\0",8)!=0) continue;
        if (pe.textRva || section.VirtualAddress!=expected.providerTextRva ||
            section.Misc.VirtualSize!=expected.providerTextVirtualSize ||
            section.SizeOfRawData!=expected.providerTextRawSize ||
            !(section.Characteristics&IMAGE_SCN_MEM_EXECUTE) ||
            (section.Characteristics&IMAGE_SCN_MEM_WRITE)) return false;
        pe.textRva=section.VirtualAddress;
        pe.textSize=section.Misc.VirtualSize;
        pe.textRawSize=section.SizeOfRawData;
    }
    return pe.textRva!=0 && InRange(pe.textRva,pe.textSize,expected.providerImageSize);
}

bool ParseHighlowRelocations(const std::vector<unsigned char>& bytes,
                             const DiskPe& pe,std::vector<std::uint32_t>& highlow) {
    const auto& dir=pe.nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    if (!InRange(dir.VirtualAddress,dir.Size,pe.nt.OptionalHeader.SizeOfImage) ||
        dir.Size<sizeof(IMAGE_BASE_RELOCATION)) return false;
    std::uint32_t cursor=0;
    while (cursor<dir.Size) {
        if (dir.Size-cursor<sizeof(IMAGE_BASE_RELOCATION)) return false;
        std::size_t offset=0;
        if (!pe.Map(dir.VirtualAddress+cursor,sizeof(IMAGE_BASE_RELOCATION),bytes.size(),offset))
            return false;
        IMAGE_BASE_RELOCATION block{};
        if (!ReadRaw(bytes,offset,block) || block.SizeOfBlock<sizeof(block) ||
            block.SizeOfBlock>dir.Size-cursor || ((block.SizeOfBlock-sizeof(block))&1)) return false;
        const auto count=(block.SizeOfBlock-sizeof(block))/2;
        for (std::uint32_t i=0;i<count;++i) {
            const auto entryRva=static_cast<std::uint64_t>(dir.VirtualAddress)+cursor+
                sizeof(block)+2ull*i;
            if (entryRva>0xffffffffULL || !pe.Map(static_cast<std::uint32_t>(entryRva),
                                                  2,bytes.size(),offset)) return false;
            std::uint16_t entry=0;
            if (!ReadRaw(bytes,offset,entry)) return false;
            const auto kind=entry>>12;
            if (kind==IMAGE_REL_BASED_ABSOLUTE) continue;
            if (kind!=IMAGE_REL_BASED_HIGHLOW || highlow.size()>=kMaxRelocations ||
                block.VirtualAddress>std::numeric_limits<std::uint32_t>::max()-(entry&0xfff))
                return false;
            const auto rva=block.VirtualAddress+(entry&0xfff);
            if (!InRange(rva,4,pe.nt.OptionalHeader.SizeOfImage)) return false;
            highlow.push_back(rva);
        }
        cursor+=block.SizeOfBlock;
    }
    std::sort(highlow.begin(),highlow.end());
    if (std::adjacent_find(highlow.begin(),highlow.end())!=highlow.end()) return false;
    return !highlow.empty();
}

bool AuthenticateMappedText(const MemoryAccess& memory,std::uint32_t base,
                            const D3dInstallExpectation& expected,
                            const std::vector<std::uint32_t>& highlow) {
    std::vector<unsigned char> text(expected.providerTextVirtualSize);
    if (!memory.read(expected.providerTextRva,text.data(),text.size())) return false;
    const auto delta=base-expected.providerPreferredBase;
    std::uint32_t previous=0;
    for (const auto rva:highlow) {
        if (rva<expected.providerTextRva ||
            !InRange(rva-expected.providerTextRva,4,expected.providerTextVirtualSize)) continue;
        if (previous && rva<previous+4) return false;
        std::uint32_t value=0;
        const auto offset=rva-expected.providerTextRva;
        std::memcpy(&value,text.data()+offset,4);
        value-=delta;
        std::memcpy(text.data()+offset,&value,4);
        previous=rva;
    }
    unsigned char digest[32]{};
    fork_startup::HashBytes(text.data(),text.size(),digest);
    return std::memcmp(digest,expected.normalizedTextSha256.data(),32)==0;
}

bool ProviderExportMatches(const MemoryAccess& memory,
                           const D3dInstallExpectation& expected) {
    IMAGE_NT_HEADERS32 nt{};
    std::uint32_t ntRva=0;
    if (!ReadMappedPe(memory,expected.providerImageSize,nt,ntRva) ||
        nt.FileHeader.TimeDateStamp!=expected.providerTimestamp ||
        nt.OptionalHeader.ImageBase!=expected.providerPreferredBase ||
        nt.OptionalHeader.NumberOfRvaAndSizes<=IMAGE_DIRECTORY_ENTRY_EXPORT)
        return false;
    const auto& dir=nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!InRange(dir.VirtualAddress,dir.Size,expected.providerImageSize) ||
        dir.Size<sizeof(IMAGE_EXPORT_DIRECTORY)) return false;
    IMAGE_EXPORT_DIRECTORY exports{};
    if (!ReadMapped(memory,dir.VirtualAddress,expected.providerImageSize,exports) ||
        !exports.NumberOfNames || exports.NumberOfNames>4096 ||
        !exports.NumberOfFunctions || exports.NumberOfFunctions>4096 ||
        !exports.AddressOfNames || !exports.AddressOfNameOrdinals ||
        !exports.AddressOfFunctions) return false;
    bool found=false;
    for (std::uint32_t i=0;i<exports.NumberOfNames;++i) {
        const auto nameOff=static_cast<std::uint64_t>(exports.AddressOfNames)+4ull*i;
        const auto ordinalOff=static_cast<std::uint64_t>(exports.AddressOfNameOrdinals)+2ull*i;
        if (nameOff>0xffffffffULL || ordinalOff>0xffffffffULL) return false;
        std::uint32_t nameRva=0;
        std::uint16_t ordinalIndex=0;
        if (!ReadMapped(memory,static_cast<std::uint32_t>(nameOff),
                        expected.providerImageSize,nameRva) ||
            !ReadMapped(memory,static_cast<std::uint32_t>(ordinalOff),
                        expected.providerImageSize,ordinalIndex)) return false;
        // Unrelated export names may be arbitrarily long. Compare only the
        // exact target prefix plus its terminating NUL through bounded reads.
        constexpr char target[]="Direct3DCreate9";
        bool match=true;
        for (std::size_t j=0;j<sizeof(target);++j) {
            char value=0;
            if (nameRva>std::numeric_limits<std::uint32_t>::max()-j ||
                !InRange(nameRva+static_cast<std::uint32_t>(j),1,expected.providerImageSize) ||
                !memory.read(nameRva+static_cast<std::uint32_t>(j),
                             reinterpret_cast<unsigned char*>(&value),1)) return false;
            if (value!=target[j]) {match=false;break;}
        }
        if (!match) continue;
        if (found || ordinalIndex>=exports.NumberOfFunctions ||
            exports.Base>std::numeric_limits<std::uint32_t>::max()-ordinalIndex ||
            exports.Base+ordinalIndex!=expected.create9ExportOrdinal) return false;
        const auto functionOff=static_cast<std::uint64_t>(exports.AddressOfFunctions)+4ull*ordinalIndex;
        if (functionOff>0xffffffffULL) return false;
        std::uint32_t functionRva=0;
        if (!ReadMapped(memory,static_cast<std::uint32_t>(functionOff),
                        expected.providerImageSize,functionRva) ||
            functionRva!=expected.create9ExportRva ||
            (functionRva>=dir.VirtualAddress && functionRva-dir.VirtualAddress<dir.Size))
            return false;
        found=true;
    }
    return found;
}

bool Win32ImageBase(std::uint32_t base,std::uint32_t size) noexcept {
    if (!base || !size || base>std::numeric_limits<std::uint32_t>::max()-size) return false;
    MEMORY_BASIC_INFORMATION info{};
    return VirtualQuery(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(base)),
        &info,sizeof(info))==sizeof(info) &&
        reinterpret_cast<std::uintptr_t>(info.AllocationBase)==base &&
        info.Type==MEM_IMAGE && info.State==MEM_COMMIT;
}

bool Win32ImmutableText(std::uint32_t base,std::uint32_t address,std::size_t count,
                        std::uint32_t textRva,std::uint32_t textSize) noexcept {
    if (!base || !address || !count || address<base ||
        address-base<textRva || address-base-textRva>=textSize ||
        count>textSize-(address-base-textRva)) return false;
    std::uintptr_t cursor=address;
    const auto end=static_cast<std::uintptr_t>(address)+count;
    while (cursor<end) {
        MEMORY_BASIC_INFORMATION info{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor),&info,sizeof(info))!=sizeof(info) ||
            reinterpret_cast<std::uintptr_t>(info.AllocationBase)!=base ||
            info.Type!=MEM_IMAGE || info.State!=MEM_COMMIT ||
            (info.Protect&PAGE_GUARD)) return false;
        const auto protection=info.Protect&0xff;
        if (protection!=PAGE_READONLY && protection!=PAGE_EXECUTE_READ) return false;
        const auto next=reinterpret_cast<std::uintptr_t>(info.BaseAddress)+info.RegionSize;
        if (next<=cursor) return false;
        cursor=next;
    }
    return true;
}

bool Win32ReadPointer(std::uint32_t address,std::uint32_t& value) noexcept {
    if (!address || (address&3)) return false;
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(address)),
        &info,sizeof(info))!=sizeof(info) || info.State!=MEM_COMMIT ||
        (info.Protect&PAGE_GUARD) || (info.Protect&0xff)==PAGE_NOACCESS) return false;
    __try {value=*reinterpret_cast<const volatile std::uint32_t*>(
        static_cast<std::uintptr_t>(address));return true;}
    __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}

D3dCasResult Win32Cas(std::uint32_t address,std::uint32_t expected,
                      std::uint32_t desired,std::uint32_t& observed) noexcept {
    if (!address || (address&3)) return D3dCasResult::Failed;
    LONG prior=0;
    __try {prior=InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(
        static_cast<std::uintptr_t>(address)),static_cast<LONG>(desired),
        static_cast<LONG>(expected));}
    __except(EXCEPTION_EXECUTE_HANDLER) {return D3dCasResult::Failed;}
    observed=static_cast<std::uint32_t>(prior);
    return observed==expected ? D3dCasResult::Swapped : D3dCasResult::Mismatch;
}

bool ReadOneFileSnapshot(std::uint32_t base,std::vector<unsigned char>& output) {
    std::vector<wchar_t> path(kPathChars,L'\0');
    const auto length=GetModuleFileNameW(reinterpret_cast<HMODULE>(
        static_cast<std::uintptr_t>(base)),path.data(),static_cast<DWORD>(path.size()));
    if (!length || length>=path.size()) return false;
    const auto file=CreateFileW(path.data(),GENERIC_READ,FILE_SHARE_READ,
        nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if (file==INVALID_HANDLE_VALUE) return false;
    struct FileCloser { HANDLE value; ~FileCloser() noexcept {CloseHandle(value);} } closer{file};
    BY_HANDLE_FILE_INFORMATION before{},after{};
    LARGE_INTEGER size{},finalSize{};
    bool ok=GetFileType(file)==FILE_TYPE_DISK &&
        GetFileInformationByHandle(file,&before) && GetFileSizeEx(file,&size) &&
        size.QuadPart>0 && size.QuadPart<=static_cast<LONGLONG>(kMaxFile);
    if (ok) {
        output.resize(static_cast<std::size_t>(size.QuadPart));
        std::size_t position=0;
        while (ok && position<output.size()) {
            const auto chunk=static_cast<DWORD>((std::min)(output.size()-position,
                                                         static_cast<std::size_t>(4096)));
            DWORD read=0;
            ok=ReadFile(file,output.data()+position,chunk,&read,nullptr)!=FALSE && read==chunk;
            position+=read;
        }
        unsigned char extra=0;
        DWORD read=0;
        ok=ok && ReadFile(file,&extra,1,&read,nullptr)!=FALSE && read==0 &&
            GetFileInformationByHandle(file,&after) && GetFileSizeEx(file,&finalSize) &&
            finalSize.QuadPart==size.QuadPart &&
            before.dwVolumeSerialNumber==after.dwVolumeSerialNumber &&
            before.nFileIndexHigh==after.nFileIndexHigh &&
            before.nFileIndexLow==after.nFileIndexLow &&
            before.ftLastWriteTime.dwHighDateTime==after.ftLastWriteTime.dwHighDateTime &&
            before.ftLastWriteTime.dwLowDateTime==after.ftLastWriteTime.dwLowDateTime;
    }
    if (!ok) output.clear();
    return ok;
}

bool ReadSlot(const MemoryAccess& memory,std::uint32_t rva,
              std::uint32_t imageSize,std::uint32_t& value) {
    return ReadMapped(memory,rva,imageSize,value);
}

bool ExpectedProviderSlot(const std::vector<unsigned char>& bytes,
                          const std::vector<std::uint32_t>& relocations,
                          const DiskPe& pe,std::uint32_t providerBase,
                          std::uint32_t rva,std::uint32_t& expected) {
    std::size_t offset=0;
    if (!pe.Map(rva,4,bytes.size(),offset) || !ReadRaw(bytes,offset,expected))
        return false;
    if (std::binary_search(relocations.begin(),relocations.end(),rva))
        expected+=providerBase-pe.nt.OptionalHeader.ImageBase;
    return true;
}

struct SlotChange {
    std::uint32_t rva=0;
    std::uint32_t original=0;
    std::uint32_t hook=0;
    std::uintptr_t token=0;
    bool open=false;
    bool attempted=false;
    bool recoveryGood=true;
    bool protectionGood=true;
};

// All resources, including report string capacity and this fixed journal, are
// prepared by the caller before the first protection change. Failed CAS is
// conservatively treated as possible full exchange, then every attempted slot
// is checked and recovered in reverse order.
D3dInstallState PatchGroup(const MemoryAccess& memory,std::uint32_t imageSize,
    std::uint32_t imageBase,const D3dInstallBackend& backend,
    std::array<SlotChange,2>& slots,std::size_t count,
    D3dInstallReport& report,bool& historicalExposure,
    bool* stageExposure,long& nextGate) {
    const char* failure=nullptr;
    for (std::size_t i=0;i<count;++i) {
        if (!memory.beginWrite(slots[i].rva,4,slots[i].token)) {
            failure="protection acquisition failed";break;
        }
        slots[i].open=true;
    }
    if (!failure) {
        for (std::size_t i=0;i<count;++i) {
            auto& slot=slots[i];
            slot.attempted=true;
            ++report.attemptedSlots;
            std::uint32_t observed=0;
            const auto outcome=backend.compareExchange(imageBase+slot.rva,
                slot.original,slot.hook,observed);
            if (outcome==D3dCasResult::Swapped ||
                outcome==D3dCasResult::Failed ||
                (outcome==D3dCasResult::Mismatch && observed==slot.hook)) {
                historicalExposure=true;
                if (stageExposure) *stageExposure=true;
                report.hookPossiblyExposed=true;
            }
            std::uint32_t after=0;
            if (!ReadSlot(memory,slot.rva,imageSize,after)) {
                failure="slot readback failed";break;
            }
            if (after==slot.hook) {
                historicalExposure=true;
                if (stageExposure) *stageExposure=true;
                report.hookPossiblyExposed=true;
                ++report.hookObservedBeforeRecovery;
            }
            if (outcome!=D3dCasResult::Swapped || after!=slot.hook) {
                failure=outcome==D3dCasResult::Mismatch ?
                    "CAS mismatch or foreign slot" : "CAS failed or wrong readback";
                break;
            }
        }
    }
    if (!failure) {
        for (std::size_t n=count;n>0;--n) {
            auto& slot=slots[n-1];
            if (!memory.restoreProtection(slot.rva,4,slot.token)) {
                failure="final protection restoration failed";break;
            }
            slot.open=false;
        }
    }
    if (!failure) return D3dInstallState::Applied;

    for (std::size_t n=count;n>0;--n) {
        auto& slot=slots[n-1];
        if (!slot.attempted) continue;
        std::uintptr_t recoveryToken=0;
        if (!memory.beginWrite(slot.rva,4,recoveryToken)) {
            slot.recoveryGood=false;
            slot.protectionGood=false;
            continue;
        }
        std::uint32_t at=0;
        if (!ReadSlot(memory,slot.rva,imageSize,at)) slot.recoveryGood=false;
        else if (at==slot.hook) {
            std::uint32_t observed=0;
            if (backend.compareExchange(imageBase+slot.rva,slot.hook,
                slot.original,observed)!=D3dCasResult::Swapped)
                slot.recoveryGood=false;
        } else if (at!=slot.original) slot.recoveryGood=false;
        std::uint32_t check=0;
        if (!ReadSlot(memory,slot.rva,imageSize,check) || check!=slot.original)
            slot.recoveryGood=false;
        else if (slot.recoveryGood) ++report.originalVerifiedAfterRecovery;
        if (!memory.restoreProtection(slot.rva,4,recoveryToken))
            slot.protectionGood=false;
    }
    for (std::size_t n=count;n>0;--n) {
        auto& slot=slots[n-1];
        if (!slot.open) continue;
        if (!memory.restoreProtection(slot.rva,4,slot.token))
            slot.protectionGood=false;
        else slot.open=false;
    }
    bool recovered=true;
    for (std::size_t i=0;i<count;++i) {
        std::uint32_t at=0;
        if (!ReadSlot(memory,slots[i].rva,imageSize,at) ||
            at!=slots[i].original || slots[i].open ||
            !slots[i].recoveryGood || !slots[i].protectionGood)
            recovered=false;
    }
    if (!recovered) nextGate=3;
    report.reason=failure;
    report.reason.append(recovered ? "; originals verified" : "; recovery incomplete");
    return recovered ? D3dInstallState::RolledBack : D3dInstallState::RollbackFailed;
}

bool InsideText(std::uint32_t address,std::uint32_t base,
                std::uint32_t textRva,std::uint32_t textSize) noexcept {
    return address>=base && address-base>=textRva &&
        address-base-textRva<textSize;
}

bool VerifyOwnedTables(const MemoryAccess& memory,std::uint32_t imageSize,
                       std::uint32_t providerBase,
                       const std::array<std::uint32_t,32>& createTables,
                       std::size_t createCount,std::uint32_t createHook,
                       const std::array<std::uint32_t,32>& deviceTables,
                       std::size_t deviceCount,std::uint32_t resetHook,
                       std::uint32_t presentHook) {
    for (std::size_t i=0;i<createCount;++i) {
        std::uint32_t current=0;
        if (createTables[i]<providerBase ||
            !ReadSlot(memory,createTables[i]-providerBase+4*kD3dCreateDeviceSlot,
                      imageSize,current) || current!=createHook) return false;
    }
    for (std::size_t i=0;i<deviceCount;++i) {
        if (deviceTables[i]<providerBase) return false;
        const auto rva=deviceTables[i]-providerBase;
        std::uint32_t reset=0,present=0;
        if (!ReadSlot(memory,rva+4*kD3dDeviceResetSlot,imageSize,reset) ||
            !ReadSlot(memory,rva+4*kD3dDevicePresentSlot,imageSize,present) ||
            reset!=resetHook || present!=presentHook) return false;
    }
    return true;
}

} // namespace

const D3dInstallExpectation& PinnedD3dInstallExpectation() noexcept {
    static const D3dInstallExpectation value{
        kPinnedExeImageSize,kPinnedD3dCreate9IatRva,1550992,
        {0xFD,0x72,0xE0,0x21,0xC1,0x28,0xA9,0x8A,0x5D,0x68,0x8E,0xB9,0xAC,0xC0,0x7E,0x3E,
         0xB1,0x94,0x8C,0xA6,0xB6,0x30,0xB9,0xFA,0x5F,0x32,0x88,0x98,0xE7,0x8C,0x7D,0xBC},
        kPinnedD3dImageSize,0x3F41597F,0x10000000,0x1000,1420788,1420800,
        {0x2A,0xAF,0x23,0xCB,0xF0,0xF0,0xDA,0xF1,0xE3,0xE8,0xE8,0x1A,0xA3,0x51,0x69,0x0A,
         0xF3,0xC2,0x2A,0xA8,0x31,0x00,0x19,0xAA,0xFA,0xCA,0x2D,0x15,0xFB,0xF4,0x87,0x2B},
        0x815D0,37
    };
    return value;
}

D3dInstallReport D3dInstallTransaction::InstallExeImport(
    std::uint32_t activeIatProvider,std::uint32_t hookCreate9,
    const D3dInstallBackend& backend,const D3dInstallExpectation& expected) {
    D3dInstallReport report;
    report.stage=D3dInstallStage::ExeImport;
    const auto prior=InterlockedCompareExchange(
        reinterpret_cast<volatile LONG*>(&gate_),1,0);
    if (prior==1) {report.state=D3dInstallState::Busy;report.reason="busy";return report;}
    if (prior==3) {
        report.state=D3dInstallState::Poisoned;report.reason="poisoned";
        report.providerReferenceRetained=providerBase_!=0;
        report.originalsPublished=importPublished_;
        report.hookPossiblyExposed=anyHookPossiblyExposed_;
        return report;
    }
    if (prior!=0) {report.state=D3dInstallState::Busy;report.reason="gate changing";return report;}
    GateExit gate{&gate_};
    report.reason.reserve(512);
    report.providerReferenceRetained=providerBase_!=0;
    report.originalsPublished=importPublished_;
    report.hookPossiblyExposed=anyHookPossiblyExposed_;
    auto reject=[&](const char* reason,bool poison=false) {
        report.reason=reason;
        report.state=poison ? D3dInstallState::Poisoned : D3dInstallState::Rejected;
        if (poison) gate.next=3;
        return report;
    };
    if (!activeIatProvider && !importInstalled_) {
        report.state=D3dInstallState::Skipped;
        report.reason="D3D provider unavailable";
        return report;
    }
    if (!hookCreate9 ||
        !backend.exeMemory.read || !backend.exeMemory.beginWrite ||
        !backend.exeMemory.restoreProtection || !backend.providerMemory.read ||
        !backend.retainFromAddress || !backend.readDiskSnapshot ||
        !backend.isMappedImage || !backend.resolveCreate9 ||
        !backend.publishCreate9 || !backend.compareExchange ||
        !backend.exeMemoryBase || !backend.providerMemoryBase ||
        expected.exeIatRva&3 || !InRange(expected.exeIatRva,4,expected.exeImageSize) ||
        backend.exeMemoryBase>0xffffffffu-expected.exeIatRva)
        return reject("incomplete backend or invalid import arguments",importInstalled_);
    std::uint32_t current=0;
    if (!ReadSlot(backend.exeMemory,expected.exeIatRva,expected.exeImageSize,current))
        return reject("EXE IAT slot unreadable",importInstalled_);
    if (importInstalled_) {
        // Initial install authenticated all metadata. Hot retries only check
        // cached stable provider identity and the current atomic DWORD.
        if (!importPublished_ || !providerBase_ || hookCreate9!=hookCreate9_ ||
            activeIatProvider!=hookCreate9_ ||
            backend.providerMemoryBase!=providerBase_ ||
            backend.resolveCreate9(providerBase_)!=originalCreate9_ ||
            current!=hookCreate9_)
            return reject("installed D3D provider or EXE IAT changed",true);
        report.state=D3dInstallState::AlreadyInstalled;
        report.reason="EXE D3D IAT remains installed";
        return report;
    }
    if (hookCreate9==activeIatProvider)
        return reject("EXE IAT already contains requested hook before installation");
    if (current!=activeIatProvider)
        return reject("EXE IAT foreign or already hooked",anyHookPossiblyExposed_);
    if (providerBase_) {
        if (backend.providerMemoryBase!=providerBase_ ||
            activeIatProvider!=originalCreate9_)
            return reject("retained provider identity changed",true);
    } else {
        std::uint32_t retained=0;
        if (!backend.retainFromAddress(current,retained) || !retained)
            return reject("D3D provider retention failed");
        providerBase_=retained;
        originalCreate9_=current;
        report.providerReferenceRetained=true;
        if (retained!=backend.providerMemoryBase)
            return reject("retained provider differs from mapped backend",true);
    }
    if (expected.providerDiskSize>kMaxFile ||
        expected.providerImageSize<4u*(kD3dDevicePresentSlot+1) ||
        providerBase_>0xffffffffu-expected.providerImageSize ||
        !backend.isMappedImage(providerBase_,expected.providerImageSize) ||
        !AuthenticateExeImport(backend.exeMemory,expected))
        return reject("mapped image or named EXE import authentication failed",true);
    if (diskBytes_.empty()) {
        if (!backend.readDiskSnapshot(providerBase_,diskBytes_))
            return reject("provider disk snapshot failed",true);
    }
    unsigned char digest[32]{};
    if (diskBytes_.size()!=expected.providerDiskSize)
        return reject("provider disk size mismatch",true);
    fork_startup::HashBytes(diskBytes_.data(),diskBytes_.size(),digest);
    if (std::memcmp(digest,expected.providerDiskSha256.data(),32)!=0)
        return reject("provider disk SHA mismatch",true);
    DiskPe pe{};
    if (!ParseDiskPe(diskBytes_,expected,pe))
        return reject("provider disk PE mismatch",true);
    if (highlowRelocations_.empty()) {
        std::vector<std::uint32_t> parsedRelocations;
        if (!ParseHighlowRelocations(diskBytes_,pe,parsedRelocations))
            return reject("provider relocation table invalid",true);
        highlowRelocations_=std::move(parsedRelocations);
    }
    if (!AuthenticateMappedText(backend.providerMemory,providerBase_,
                                expected,highlowRelocations_) ||
        !ProviderExportMatches(backend.providerMemory,expected) ||
        backend.resolveCreate9(providerBase_)!=current ||
        !InsideText(current,providerBase_,expected.providerTextRva,
                    expected.providerTextVirtualSize))
        return reject("mapped D3D text or concrete export mismatch",true);
    providerImageSize_=expected.providerImageSize;
    exeImageSize_=expected.exeImageSize;
    exeIatRva_=expected.exeIatRva;
    exeBase_=backend.exeMemoryBase;
    providerTextRva_=expected.providerTextRva;
    providerTextSize_=expected.providerTextVirtualSize;
    if (importPublished_ && originalCreate9_!=current)
        return reject("published Create9 original changed",true);
    if (importPublished_ && hookCreate9_ && hookCreate9_!=hookCreate9)
        return reject("Create9 hook changed after prior preparation",anyHookPossiblyExposed_);
    if (!importPublished_) {
        backend.publishCreate9(current);
        importPublished_=true;
    }
    hookCreate9_=hookCreate9;
    report.originalsPublished=true;
    std::array<SlotChange,2> slots{};
    slots[0].rva=expected.exeIatRva;
    slots[0].original=current;
    slots[0].hook=hookCreate9;
    report.state=PatchGroup(backend.exeMemory,expected.exeImageSize,
        backend.exeMemoryBase,backend,slots,1,report,anyHookPossiblyExposed_,
        nullptr,gate.next);
    if (report.state==D3dInstallState::Applied) {
        importInstalled_=true;
        report.reason="EXE Direct3DCreate9 IAT installed";
    }
    return report;
}

D3dInstallReport D3dInstallTransaction::InstallCreateDeviceTable(
    std::uint32_t returnedD3dObject,std::uint32_t hookCreateDevice,
    const D3dInstallBackend& backend) {
    D3dInstallReport report;
    report.stage=D3dInstallStage::CreateDeviceTable;
    const auto prior=InterlockedCompareExchange(
        reinterpret_cast<volatile LONG*>(&gate_),1,0);
    if (prior==1) {report.state=D3dInstallState::Busy;report.reason="busy";return report;}
    if (prior==3) {
        report.state=D3dInstallState::Poisoned;report.reason="poisoned";
        report.providerReferenceRetained=providerBase_!=0;
        report.originalsPublished=createDevicePublished_;
        report.hookPossiblyExposed=anyHookPossiblyExposed_;
        return report;
    }
    if (prior!=0) {report.state=D3dInstallState::Busy;report.reason="gate changing";return report;}
    GateExit gate{&gate_};
    report.reason.reserve(512);
    report.providerReferenceRetained=providerBase_!=0;
    report.originalsPublished=createDevicePublished_;
    report.hookPossiblyExposed=anyHookPossiblyExposed_;
    auto reject=[&](const char* reason,bool poison=false) {
        report.reason=reason;
        report.state=poison ? D3dInstallState::Poisoned : D3dInstallState::Rejected;
        if (poison) gate.next=3;
        return report;
    };
    if (!returnedD3dObject) {
        report.state=D3dInstallState::Skipped;
        report.reason="native provider returned no IDirect3D9 object";
        return report;
    }
    if (!importInstalled_ || !providerBase_ || !hookCreateDevice ||
        !backend.readPointer || !backend.immutableProviderStorage ||
        !backend.exeMemory.read || backend.exeMemoryBase!=exeBase_ ||
        !backend.providerMemory.read || !backend.providerMemory.beginWrite ||
        !backend.providerMemory.restoreProtection ||
        !backend.publishCreateDevice || !backend.compareExchange ||
        backend.providerMemoryBase!=providerBase_)
        return reject("CreateDevice table prerequisites unavailable");
    std::uint32_t importCurrent=0;
    if (!ReadSlot(backend.exeMemory,exeIatRva_,exeImageSize_,importCurrent) ||
        importCurrent!=hookCreate9_ ||
        !VerifyOwnedTables(backend.providerMemory,providerImageSize_,providerBase_,
            createDeviceTables_,createDeviceTableCount_,hookCreateDevice_,
            deviceTables_,deviceTableCount_,hookReset_,hookPresent_))
        return reject("previously owned D3D slot changed or unreadable",true);
    std::uint32_t table=0;
    if (!backend.readPointer(returnedD3dObject,table) ||
        !table || (table&3) || table<providerBase_ ||
        table-providerBase_>providerImageSize_-4u*kD3dCreateDeviceSlot-4)
        return reject("returned IDirect3D9 vtable invalid");
    const auto slotRva=table-providerBase_+4u*kD3dCreateDeviceSlot;
    const auto slotAddress=providerBase_+slotRva;
    if (!backend.immutableProviderStorage(providerBase_,table,
        4u*(kD3dCreateDeviceSlot+1)) ||
        !InsideText(slotAddress,providerBase_,providerTextRva_,providerTextSize_))
        return reject("CreateDevice slot storage is not immutable provider text");
    std::uint32_t current=0;
    if (!ReadSlot(backend.providerMemory,slotRva,providerImageSize_,current))
        return reject("CreateDevice slot unreadable");
    for (std::size_t i=0;i<createDeviceTableCount_;++i) {
        if (createDeviceTables_[i]!=table) continue;
        if (current!=hookCreateDevice_ || hookCreateDevice!=hookCreateDevice_)
            return reject("owned CreateDevice slot was changed",true);
        report.state=D3dInstallState::AlreadyInstalled;
        report.reason="CreateDevice table remains installed";
        return report;
    }
    if (createDeviceTableCount_>=createDeviceTables_.size())
        return reject("CreateDevice table journal capacity exhausted");
    DiskPe pe{};
    // Test expectations may use a surrogate. DiskPe is already authenticated
    // at import stage; find this exact RVA in its immutable section headers.
    IMAGE_DOS_HEADER dos{};
    if (!ReadRaw(diskBytes_,0,dos) || dos.e_lfanew<0 ||
        !ReadRaw(diskBytes_,static_cast<std::uint32_t>(dos.e_lfanew),pe.nt))
        return reject("cached provider snapshot unavailable",true);
    pe.sectionCount=pe.nt.FileHeader.NumberOfSections;
    if (pe.sectionCount>kMaxSections) return reject("cached provider sections invalid",true);
    const auto sectionOffset=static_cast<std::uint64_t>(dos.e_lfanew)+4+
        sizeof(IMAGE_FILE_HEADER)+pe.nt.FileHeader.SizeOfOptionalHeader;
    for (std::size_t i=0;i<pe.sectionCount;++i)
        if (!ReadRaw(diskBytes_,static_cast<std::size_t>(sectionOffset+
            i*sizeof(IMAGE_SECTION_HEADER)),pe.sections[i]))
            return reject("cached provider section unreadable",true);
    std::uint32_t original=0;
    if (!ExpectedProviderSlot(diskBytes_,highlowRelocations_,pe,
                              providerBase_,slotRva,original) ||
        !InsideText(original,providerBase_,providerTextRva_,providerTextSize_) ||
        current!=original || hookCreateDevice==original)
        return reject("CreateDevice disk baseline or method target mismatch");
    if (createDevicePublished_ &&
        (originalCreateDevice_!=original || hookCreateDevice_!=hookCreateDevice))
        return reject("CreateDevice original or hook tuple changed",createDevicePossiblyExposed_);
    if (!createDevicePublished_) {
        backend.publishCreateDevice(original);
        originalCreateDevice_=original;
        hookCreateDevice_=hookCreateDevice;
        createDevicePublished_=true;
    }
    report.originalsPublished=true;
    std::array<SlotChange,2> slots{};
    slots[0].rva=slotRva;slots[0].original=original;slots[0].hook=hookCreateDevice;
    report.state=PatchGroup(backend.providerMemory,providerImageSize_,
        providerBase_,backend,slots,1,report,anyHookPossiblyExposed_,
        &createDevicePossiblyExposed_,gate.next);
    if (report.state==D3dInstallState::Applied) {
        createDeviceTables_[createDeviceTableCount_++]=table;
        createDevicePossiblyExposed_=true;
        report.reason="CreateDevice table installed";
    }
    return report;
}

D3dInstallReport D3dInstallTransaction::InstallDeviceTablePair(
    std::uint32_t returnedDevice,std::uint32_t hookReset,
    std::uint32_t hookPresent,const D3dInstallBackend& backend) {
    D3dInstallReport report;
    report.stage=D3dInstallStage::DeviceTablePair;
    const auto prior=InterlockedCompareExchange(
        reinterpret_cast<volatile LONG*>(&gate_),1,0);
    if (prior==1) {report.state=D3dInstallState::Busy;report.reason="busy";return report;}
    if (prior==3) {
        report.state=D3dInstallState::Poisoned;report.reason="poisoned";
        report.providerReferenceRetained=providerBase_!=0;
        report.originalsPublished=devicePairPublished_;
        report.hookPossiblyExposed=anyHookPossiblyExposed_;
        return report;
    }
    if (prior!=0) {report.state=D3dInstallState::Busy;report.reason="gate changing";return report;}
    GateExit gate{&gate_};
    report.reason.reserve(512);
    report.providerReferenceRetained=providerBase_!=0;
    report.originalsPublished=devicePairPublished_;
    report.hookPossiblyExposed=anyHookPossiblyExposed_;
    auto reject=[&](const char* reason,bool poison=false) {
        report.reason=reason;
        report.state=poison ? D3dInstallState::Poisoned : D3dInstallState::Rejected;
        if (poison) gate.next=3;
        return report;
    };
    if (!returnedDevice) {
        report.state=D3dInstallState::Skipped;
        report.reason="native provider returned no device";
        return report;
    }
    if (!createDeviceTableCount_ || !providerBase_ ||
        !hookReset || !hookPresent || hookReset==hookPresent ||
        !backend.readPointer || !backend.immutableProviderStorage ||
        !backend.exeMemory.read || backend.exeMemoryBase!=exeBase_ ||
        !backend.providerMemory.read || !backend.providerMemory.beginWrite ||
        !backend.providerMemory.restoreProtection || !backend.publishDevicePair ||
        !backend.compareExchange || backend.providerMemoryBase!=providerBase_)
        return reject("device table prerequisites unavailable");
    std::uint32_t importCurrent=0;
    if (!ReadSlot(backend.exeMemory,exeIatRva_,exeImageSize_,importCurrent) ||
        importCurrent!=hookCreate9_ ||
        !VerifyOwnedTables(backend.providerMemory,providerImageSize_,providerBase_,
            createDeviceTables_,createDeviceTableCount_,hookCreateDevice_,
            deviceTables_,deviceTableCount_,hookReset_,hookPresent_))
        return reject("previously owned D3D slot changed or unreadable",true);
    std::uint32_t table=0;
    if (!backend.readPointer(returnedDevice,table) || !table || (table&3) ||
        table<providerBase_ ||
        table-providerBase_>providerImageSize_-4u*kD3dDevicePresentSlot-4)
        return reject("returned device vtable invalid");
    const auto resetRva=table-providerBase_+4u*kD3dDeviceResetSlot;
    const auto presentRva=table-providerBase_+4u*kD3dDevicePresentSlot;
    if (!backend.immutableProviderStorage(providerBase_,table,
        4u*(kD3dDevicePresentSlot+1)) ||
        !InsideText(providerBase_+resetRva,providerBase_,providerTextRva_,providerTextSize_) ||
        !InsideText(providerBase_+presentRva,providerBase_,providerTextRva_,providerTextSize_))
        return reject("device slot storage is not immutable provider text");
    std::uint32_t resetCurrent=0,presentCurrent=0;
    if (!ReadSlot(backend.providerMemory,resetRva,providerImageSize_,resetCurrent) ||
        !ReadSlot(backend.providerMemory,presentRva,providerImageSize_,presentCurrent))
        return reject("device slot unreadable");
    for (std::size_t i=0;i<deviceTableCount_;++i) {
        if (deviceTables_[i]!=table) continue;
        if (resetCurrent!=hookReset_ || presentCurrent!=hookPresent_ ||
            hookReset!=hookReset_ || hookPresent!=hookPresent_)
            return reject("owned device slot changed",true);
        report.state=D3dInstallState::AlreadyInstalled;
        report.reason="Reset/Present table remains installed";
        return report;
    }
    if (deviceTableCount_>=deviceTables_.size())
        return reject("device table journal capacity exhausted");
    DiskPe pe{};
    IMAGE_DOS_HEADER dos{};
    if (!ReadRaw(diskBytes_,0,dos) || dos.e_lfanew<0 ||
        !ReadRaw(diskBytes_,static_cast<std::uint32_t>(dos.e_lfanew),pe.nt))
        return reject("cached provider snapshot unavailable",true);
    pe.sectionCount=pe.nt.FileHeader.NumberOfSections;
    if (pe.sectionCount>kMaxSections) return reject("cached provider sections invalid",true);
    const auto sectionOffset=static_cast<std::uint64_t>(dos.e_lfanew)+4+
        sizeof(IMAGE_FILE_HEADER)+pe.nt.FileHeader.SizeOfOptionalHeader;
    for (std::size_t i=0;i<pe.sectionCount;++i)
        if (!ReadRaw(diskBytes_,static_cast<std::size_t>(sectionOffset+
            i*sizeof(IMAGE_SECTION_HEADER)),pe.sections[i]))
            return reject("cached provider section unreadable",true);
    std::uint32_t resetOriginal=0,presentOriginal=0;
    if (!ExpectedProviderSlot(diskBytes_,highlowRelocations_,pe,
            providerBase_,resetRva,resetOriginal) ||
        !ExpectedProviderSlot(diskBytes_,highlowRelocations_,pe,
            providerBase_,presentRva,presentOriginal) ||
        !InsideText(resetOriginal,providerBase_,providerTextRva_,providerTextSize_) ||
        !InsideText(presentOriginal,providerBase_,providerTextRva_,providerTextSize_) ||
        resetCurrent!=resetOriginal || presentCurrent!=presentOriginal ||
        hookReset==resetOriginal || hookPresent==presentOriginal)
        return reject("Reset/Present disk baseline or method target mismatch");
    if (devicePairPublished_ &&
        (originalReset_!=resetOriginal || originalPresent_!=presentOriginal ||
         hookReset_!=hookReset || hookPresent_!=hookPresent))
        return reject("Reset/Present original or hook tuple changed",devicePairPossiblyExposed_);
    if (!devicePairPublished_) {
        backend.publishDevicePair(resetOriginal,presentOriginal);
        originalReset_=resetOriginal;originalPresent_=presentOriginal;
        hookReset_=hookReset;hookPresent_=hookPresent;
        devicePairPublished_=true;
    }
    report.originalsPublished=true;
    std::array<SlotChange,2> slots{};
    slots[0].rva=resetRva;slots[0].original=resetOriginal;slots[0].hook=hookReset;
    slots[1].rva=presentRva;slots[1].original=presentOriginal;slots[1].hook=hookPresent;
    report.state=PatchGroup(backend.providerMemory,providerImageSize_,
        providerBase_,backend,slots,2,report,anyHookPossiblyExposed_,
        &devicePairPossiblyExposed_,gate.next);
    if (report.state==D3dInstallState::Applied) {
        deviceTables_[deviceTableCount_++]=table;
        devicePairPossiblyExposed_=true;
        report.reason="Reset/Present table pair installed";
    }
    return report;
}

D3dInstallBackend MakeWin32D3dInstallBackend(
    std::uint32_t exeBase,std::uint32_t exeImageSize,
    std::uint32_t providerBase,std::uint32_t providerImageSize,
    std::function<void(std::uint32_t)> publishCreate9,
    std::function<void(std::uint32_t)> publishCreateDevice,
    std::function<void(std::uint32_t,std::uint32_t)> publishDevicePair) {
    D3dInstallBackend backend;
    backend.exeMemory=MakeWin32ImageMemory(exeBase,exeImageSize);
    backend.exeMemoryBase=exeBase;
    backend.providerMemory=MakeWin32ImageMemory(providerBase,providerImageSize);
    backend.providerMemoryBase=providerBase;
    backend.retainFromAddress=[](std::uint32_t address,std::uint32_t& retainedBase) noexcept {
        HMODULE module=nullptr;
        if (!address || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            reinterpret_cast<LPCWSTR>(static_cast<std::uintptr_t>(address)),&module)) return false;
        retainedBase=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(module));
        return true;
    };
    backend.readDiskSnapshot=[](std::uint32_t base,std::vector<unsigned char>& bytes) {
        return ReadOneFileSnapshot(base,bytes);
    };
    backend.isMappedImage=[](std::uint32_t base,std::uint32_t size) noexcept {
        return Win32ImageBase(base,size);
    };
    backend.readPointer=[](std::uint32_t object,std::uint32_t& vtable) noexcept {
        return Win32ReadPointer(object,vtable);
    };
    backend.immutableProviderStorage=[providerImageSize](std::uint32_t base,
        std::uint32_t address,std::size_t count) noexcept {
        if (providerImageSize!=kPinnedD3dImageSize) return false;
        return Win32ImmutableText(base,address,count,0x1000,1420788);
    };
    backend.resolveCreate9=[](std::uint32_t authenticatedBase) noexcept -> std::uint32_t {
        return authenticatedBase ? static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(
            GetProcAddress(reinterpret_cast<HMODULE>(
                static_cast<std::uintptr_t>(authenticatedBase)),"Direct3DCreate9"))) : 0;
    };
    backend.publishCreate9=std::move(publishCreate9);
    backend.publishCreateDevice=std::move(publishCreateDevice);
    backend.publishDevicePair=std::move(publishDevicePair);
    backend.compareExchange=[](std::uint32_t address,std::uint32_t expected,
        std::uint32_t desired,std::uint32_t& observed) noexcept {
        return Win32Cas(address,expected,desired,observed);
    };
    return backend;
}

} // namespace fork_boundary
