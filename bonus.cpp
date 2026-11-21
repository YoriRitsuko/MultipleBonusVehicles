#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <cstring>

// These addresses are from the English retail EXE, loaded at 0x400000.
namespace {
constexpr uintptr_t ImageBase = 0x400000;
constexpr unsigned BonusLimit = 20;
constexpr unsigned VehicleLimit = 30;
constexpr unsigned LoadQueueSize = 200;

struct Position { float x, y, z; };
using Zone = uint64_t;

#pragma pack(push, 4)
struct CarInfo {
    char name[33];
    void* car;
    void* husk;
    Zone zone;
};
static_assert(offsetof(CarInfo, car) == 0x24);
static_assert(offsetof(CarInfo, zone) == 0x2c);
static_assert(sizeof(CarInfo) == 0x34);

// Each slot looks like its own parked car manager to the game's car routines.
// The padding keeps the fields they actually use at the original offsets.
struct FreeCarState {
    uint8_t unusedVtable[4];
    uintptr_t completionVtable;
    uint8_t unusedParkedState[0xe0];
    CarInfo info;
    void* locator;
    bool enabled;
};
#pragma pack(pop)
static_assert(offsetof(FreeCarState, info) == 0xe8);
static_assert(offsetof(FreeCarState, locator) == 0x11c);
static_assert(offsetof(FreeCarState, enabled) == 0x120);

enum class Phase { Free, Loading, Cancelled, Creating, Live, Removing };
struct Slot {
    FreeCarState state;
    void* owner;
    Phase phase;
    char section[16];
};
static_assert(offsetof(Slot, state) == 0);

Slot slots[BonusLimit]{};
wchar_t logPath[MAX_PATH]{};

template<typename T> T& Field(void* object, size_t offset)
{
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(object) + offset);
}

template<typename F> F Native(uintptr_t address)
{
    return reinterpret_cast<F>(address);
}

void* Inventory() { return *reinterpret_cast<void**>(0x65ef60); }
void* LoadingManager() { return *reinterpret_cast<void**>(0x6c8ff4); }
void* VehicleCentral() { return *reinterpret_cast<void**>(0x6c84d8); }

void Log(const char* message, const char* name = "")
{
    char line[256] = "[Multiple Bonus Vehicles] ";
    strcat(line, message);
    strcat(line, name);
    strcat(line, "\r\n");
    OutputDebugStringA(line);
    if (logPath[0]) {
        const HANDLE file = CreateFileW(logPath, FILE_APPEND_DATA,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE) {
            DWORD written;
            WriteFile(file, line, static_cast<DWORD>(strlen(line)), &written, nullptr);
            CloseHandle(file);
        }
    }
}

bool ValidCarName(const char* name)
{
    if (!name || !name[0]) return false;
    for (size_t i = 0; i <= 32; ++i) {
        const char c = name[i];
        if (!c) return true;
        if ((c < 'a' || c > 'z') && (c < 'A' || c > 'Z') &&
            (c < '0' || c > '9') && c != '_' && c != '-') return false;
    }
    return false;
}

bool CancellingLoads(void* loader)
{
    return !loader || Field<bool>(loader, 0x73ad);
}

bool QueueAvailable()
{
    void* loader = LoadingManager();
    return !CancellingLoads(loader) &&
        (Field<unsigned>(loader, 0x73a8) + 1) % LoadQueueSize !=
        Field<unsigned>(loader, 0x73a4);
}

bool VehicleSpaceAvailable()
{
    void* central = VehicleCentral();
    return central && Field<unsigned>(central, 0x1a4) < VehicleLimit;
}

void DeleteSection(Slot& slot)
{
    if (!slot.section[0]) return;
    if (void* inventory = Inventory()) {
        Zone section;
        Native<void (__cdecl*)(Zone*, const char*)>(0x56f3d0)(&section, slot.section);
        Native<void (__thiscall*)(void*, Zone)>(0x571040)(inventory, section);
    }
    slot.section[0] = '\0';
}

Position VehiclePosition(void* vehicle)
{
    Position position{};
    const auto vtable = Field<uintptr_t*>(vehicle, 0);
    Native<void (__thiscall*)(void*, Position*)>(vtable[0x40 / 4])(vehicle, &position);
    return position;
}

} // namespace

extern "C" {
// hooks.S puts the arguments in the registers the game expects.
void NativeAdd(FreeCarState*, const char*, void*);
void NativeCreate(FreeCarState*);
void NativeRemove(FreeCarState*);
void NativeDestructor(void*);
void* CurrentVehicle();
void* AvatarVehicle();
bool IsConstructing(void*);
void AddHook();
void RequestHook();
void FindHook();
void UnloadHook();
void NearbyHook();
void DestructorHook();
void CountHook();
void CancelHook();
void VehicleInitHook();
void DemoLocatorHook();
}

namespace {
void Remove(Slot& slot)
{
    if (slot.phase == Phase::Free || slot.phase == Phase::Removing) return;
    slot.phase = Phase::Removing;
    if (slot.state.info.car) Log("Removed ", slot.state.info.name);
    NativeRemove(&slot.state);
    DeleteSection(slot);
    slot = {};
}

bool InUse(const CarInfo& info, void* vehicle)
{
    return vehicle && (vehicle == info.car || vehicle == info.husk);
}

bool Protected(const CarInfo& info)
{
    return InUse(info, CurrentVehicle()) || InUse(info, AvatarVehicle()) ||
        IsConstructing(info.car);
}

void __thiscall LoadComplete(void* callbackObject, void*)
{
    // The loader gets a pointer to the callback inside the slot, 4 bytes in.
    auto& slot = *reinterpret_cast<Slot*>(static_cast<uint8_t*>(callbackObject) - 4);
    if (slot.phase != Phase::Loading && slot.phase != Phase::Cancelled) return;
    if (slot.phase == Phase::Cancelled || CancellingLoads(LoadingManager()) ||
        !VehicleSpaceAvailable() || !QueueAvailable()) {
        Remove(slot);
        return;
    }
    slot.phase = Phase::Creating;
    NativeCreate(&slot.state);
    if (!slot.state.info.car) {
        Remove(slot);
        return;
    }
    slot.phase = Phase::Live;
    Log("Spawned ", slot.state.info.name);
}

const auto completionVtable = &LoadComplete;
} // namespace

extern "C" void OnAdd(void* manager, const char* name, void* locator)
{
    if (!locator || !Field<bool>(manager, 0x120) || !ValidCarName(name) ||
        !QueueAvailable() || !Inventory()) return;

    for (auto& slot : slots) {
        if (slot.phase != Phase::Free) continue;
        slot.phase = Phase::Loading;
        slot.owner = manager;
        slot.state.enabled = true;
        slot.state.completionVtable = reinterpret_cast<uintptr_t>(&completionVtable);
        NativeAdd(&slot.state, name, locator);
        // Even if queuing failed, NativeAdd already took a locator reference.
        if (slot.phase == Phase::Loading && !slot.section[0]) Remove(slot);
        return;
    }
    Log("Bonus vehicle slot limit reached.");
}

extern "C" const char* PrepareRequest(void* callbackObject)
{
    auto& slot = *reinterpret_cast<Slot*>(static_cast<uint8_t*>(callbackObject) - 4);
    void* inventory = Inventory();
    if (!inventory || Field<unsigned>(inventory, 0x14) >= 256 ||
        !slot.state.info.zone || !QueueAvailable()) return nullptr;

    const unsigned index = static_cast<unsigned>(&slot - slots);
    strcpy(slot.section, "BonusDLL_00");
    slot.section[9] = static_cast<char>('0' + index / 10);
    slot.section[10] = static_cast<char>('0' + index % 10);
    Native<void (__thiscall*)(void*, const char*)>(0x570f60)(inventory, slot.section);
    Log("Queued ", slot.state.info.name);
    return slot.section;
}

extern "C" void FindCarInfo(void* manager, void* vehicle, CarInfo** info, bool* bonus)
{
    *info = nullptr;
    *bonus = false;
    if (!vehicle) return;
    for (auto& slot : slots) {
        if (slot.owner == manager && slot.phase == Phase::Live &&
            slot.state.info.car == vehicle) {
            *info = &slot.state.info;
            *bonus = true;
            return;
        }
    }
    auto* parked = Field<CarInfo*>(manager, 8);
    const unsigned count = Field<unsigned>(manager, 0xc);
    for (unsigned index = 0; index < count; ++index) {
        if (parked[index].car == vehicle) {
            *info = &parked[index];
            return;
        }
    }
}

extern "C" void OnUnloadZone(void* manager, const Zone* zone)
{
    for (auto& slot : slots) {
        if (slot.owner != manager) continue;
        auto& info = slot.state.info;
        if (slot.phase == Phase::Loading && info.zone == *zone) {
            slot.phase = Phase::Cancelled;
        } else if (slot.phase == Phase::Live && (info.zone == *zone || !info.zone)) {
            if (Protected(info)) info.zone = 0;
            else Remove(slot);
        }
    }
}

extern "C" void OnRemoveNearby(void* manager, const Position* point)
{
    for (auto& slot : slots) {
        if (slot.owner != manager || slot.phase != Phase::Live ||
            Protected(slot.state.info)) continue;
        const auto pos = VehiclePosition(slot.state.info.husk ?
            slot.state.info.husk : slot.state.info.car);
        const float x = point->x - pos.x;
        const float y = point->y - pos.y;
        const float z = point->z - pos.z;
        if (x * x + y * y + z * z < 100.0f) Remove(slot);
    }
}

extern "C" void OnDestroy(void* manager)
{
    for (auto& slot : slots) {
        if (slot.owner != manager) continue;
        if (slot.phase == Phase::Loading || slot.phase == Phase::Cancelled) {
            // Drop the locator before the level heap goes away. Keep the slot
            // around in case the loader still calls back.
            slot.phase = Phase::Cancelled;
            slot.owner = nullptr;
            NativeRemove(&slot.state);
        } else {
            Remove(slot);
        }
    }
    NativeDestructor(manager);
}

extern "C" void OnCancelLoads()
{
    for (auto& slot : slots) {
        if (slot.phase == Phase::Loading || slot.phase == Phase::Cancelled) Remove(slot);
    }
}

namespace {
struct Patch {
    uintptr_t address;
    const char* expected;
    uint8_t length;
    uint8_t opcode;
    void (*target)();
};

const Patch patches[] = {
    {0x43ee40, "\x83\x7f\x04\x01\x75\x18", 6, 0xe9, DemoLocatorHook},
    {0x43eea7, "\xe8\x24\xab\x0c\x00", 5, 0xe8, AddHook},
    {0x509ae3, "\xe8\x28\x49\xf3\xff", 5, 0xe8, RequestHook},
    {0x509790, "\xc7\x06\x00\x00\x00\x00", 6, 0xe9, FindHook},
    {0x509329, "\x83\xec\x08\x8d\xbd\x14\x01\x00\x00", 9, 0xe9, UnloadHook},
    {0x509be0, "\x83\xec\x0c\x53\x8b\xd8", 6, 0xe9, NearbyHook},
    {0x509dd3, "\xe8\x18\x00\x00\x00", 5, 0xe8, DestructorHook},
    {0x509544, "\xff\x45\x10\x5f\x5e", 5, 0xe9, CountHook},
    {0x43e5e3, "\x5f\x5e\x5b\xc3\x90", 5, 0xe9, CancelHook},
    {0x509f24, "\x8b\xf0\x8b\x16\x8b\xce", 6, 0xe9, VehicleInitHook}
};

bool SupportedImage()
{
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(GetModuleHandleW(nullptr));
    if (reinterpret_cast<uintptr_t>(dos) != ImageBase ||
        dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    const auto* pe = reinterpret_cast<const IMAGE_NT_HEADERS32*>(
        reinterpret_cast<const uint8_t*>(dos) + dos->e_lfanew);
    return pe->Signature == IMAGE_NT_SIGNATURE &&
        pe->FileHeader.Machine == IMAGE_FILE_MACHINE_I386 &&
        pe->FileHeader.TimeDateStamp == 0x21544c66 &&
        pe->OptionalHeader.SizeOfImage == 0x2d4000 &&
        memcmp(reinterpret_cast<void*>(0x5099d0), "\x83\xec\x54\xa1\xa0\x91\x64\x00", 8) == 0 &&
        memcmp(reinterpret_cast<void*>(0x509f00), "\x83\xec\x0c\x53\x56\x6a\x01\x6a", 8) == 0 &&
        memcmp(reinterpret_cast<void*>(0x509b00), "\x55\x56\x8b\xb3\x0c\x01\x00\x00", 8) == 0;
}

bool Install()
{
    if (!SupportedImage()) return false;
    for (const Patch& patch : patches) {
        if (memcmp(reinterpret_cast<const void*>(patch.address),
                   patch.expected, patch.length) != 0) return false;
    }

    // Get write access everywhere before patching. Some hooks share a page,
    // so undo the protection changes in reverse order.
    DWORD protections[sizeof(patches) / sizeof(patches[0])];
    size_t writable = 0;
    for (const Patch& patch : patches) {
        if (!VirtualProtect(reinterpret_cast<void*>(patch.address), patch.length,
                            PAGE_EXECUTE_READWRITE, &protections[writable])) break;
        ++writable;
    }
    const bool ready = writable == sizeof(protections) / sizeof(protections[0]);
    if (ready) {
        for (const Patch& patch : patches) {
            auto* code = reinterpret_cast<uint8_t*>(patch.address);
            const uint32_t offset = reinterpret_cast<uintptr_t>(patch.target) - (patch.address + 5);
            code[0] = patch.opcode;
            memcpy(code + 1, &offset, sizeof(offset));
            memset(code + 5, 0x90, patch.length - 5);
            FlushInstructionCache(GetCurrentProcess(), code, patch.length);
        }
    }
    while (writable) {
        const Patch& patch = patches[--writable];
        DWORD ignored;
        VirtualProtect(reinterpret_cast<void*>(patch.address), patch.length,
                       protections[writable], &ignored);
    }
    return ready;
}

// Lucas calls this event when it's time to install the game hooks.
enum class HackEvent : unsigned { InstallHacks = 1 };
} // namespace

extern "C" __declspec(dllexport) unsigned __cdecl HackEntryPoint(HackEvent event, void*)
{
    if (event != HackEvent::InstallHacks) return 0;

    const DWORD length = GetEnvironmentVariableW(L"SHAR_BONUS_LOG", logPath, MAX_PATH);
    if (!length || length >= MAX_PATH) logPath[0] = L'\0';
    if (Install()) {
        // Keep this loaded as long as the game's jumps point into it.
        HMODULE pinned;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                           reinterpret_cast<LPCWSTR>(&HackEntryPoint), &pinned);
        Log("Installed retail hooks.");
    } else {
        Log("Inactive on this executable or hook conflict.");
    }
    return 0;
}
