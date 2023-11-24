#pragma clang diagnostic ignored "-Wunused-function"

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>

#define Byte 1
#define Kilobyte 1024 * Byte
#define Megabyte 1024 * Kilobyte
#define Gigabyte 1024 * Megabyte

#define STRINGIFY_(x) #x
#define STRINGIFY(x) STRINGIFY_(x)
#define assert(cond) do {if (cond) {} else {printf(__FILE__ ":" STRINGIFY(__LINE__) ":1: error: assertion failure"); __debugbreak();}} while (0)
#define absval(x) ((x) < 0 ? -(x) : (x))

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;
typedef int64_t  i64;
typedef int32_t  i32;
typedef int16_t  i16;
typedef int8_t   i8;
typedef float    f32;
typedef double   f64;

typedef struct {u64* ptr; i64 len;} u64arr;
typedef struct {u32* ptr; i64 len;} u32arr;
typedef struct {u16* ptr; i64 len;} u16arr;
typedef struct {u8*  ptr; i64 len;} u8arr;
typedef struct {i64* ptr; i64 len;} i64arr;
typedef struct {i32* ptr; i64 len;} i32arr;
typedef struct {i16* ptr; i64 len;} i16arr;
typedef struct {i8*  ptr; i64 len;} i8arr;
typedef struct {f32* ptr; i64 len;} f32arr;
typedef struct {f64* ptr; i64 len;} f64arr;

#define STR(x) ((Str) {x, sizeof(x) - 1})
#define LIT(x) (int)x.len, x.ptr
typedef struct Str {char* ptr; i64 len;} Str;
typedef struct StrBuilder {char* ptr; i64 len, cap;} StrBuilder;
static void __attribute__((format(printf, 2, 3))) buildStr(StrBuilder* builder, char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int printResult = vsnprintf(builder->ptr + builder->len, builder->cap - builder->len, fmt, args);
    builder->len += printResult;
    va_end(args);
}

typedef struct Arena { void* base; i64 used, size, tempCount; } Arena;
static void* arenaFreeptr(Arena* arena)  {return arena->base + arena->used;}
static i64   arenaFreesize(Arena* arena) {return arena->size - arena->used;}

#define arenaAllocArray(Arena, Type, Count) (Type*)arenaAlloc(Arena, sizeof(Type) * Count)
static void* arenaAlloc(Arena* arena, i64 bytes) {
    assert(bytes <= arenaFreesize(arena));
    void* result = arenaFreeptr(arena);
    arena->used += bytes;
    return result;
}

#define tempMemBlock(Arena) for (TempMemory __temp__ = beginTempMemory(Arena); __temp__.arena; endTempMemory(&__temp__))
typedef struct TempMemory {Arena* arena; i64 usedBefore, tempCountBefore;} TempMemory;
static TempMemory beginTempMemory(Arena* arena) {return (TempMemory) {.arena = arena, .usedBefore = arena->used, .tempCountBefore = arena->tempCount++};}

static void endTempMemory(TempMemory* mem) {
    assert(mem->arena->used >= mem->usedBefore);
    assert(mem->arena->tempCount == mem->tempCountBefore + 1);
    mem->arena->used = mem->usedBefore;
    mem->arena->tempCount = mem->tempCountBefore;
    *mem = (TempMemory) {};
}

#ifdef PAWP_PROFILE
#define profileThroughputBegin(name, dataSize) profileThroughputBegin_(STR(name), __COUNTER__ + 1, dataSize)
#define profileThroughput(name, dataSize) for (TimedSection __timedSection__ = profileThroughputBegin(name, dataSize); __timedSection__.anchorIndex; profileThroughputEnd(&__timedSection__))
#define profileSectionBegin(name) profileThroughputBegin(name, 0)
#define profileSectionEnd(name) profileThroughputEnd(name)
#define profileSection(name) profileSectionBegin(name); for (int _i_ = 0; _i_ == 0; _i_++, profileSectionEnd(name))
#else
#define profileThroughputBegin(name, dataSize)
#define profileThroughputEnd(name)
#define profileThroughput(name, dataSize)
#define profileSectionBegin(name)
#define profileSectionEnd(name)
#define profileSection(name)
#endif

typedef struct ProfileAnchor {
    Str name;
    u64 timeTakenSelf;
    u64 timeTakenWithChildren;
    i64 count;
    i64 dataSize;
} ProfileAnchor;

typedef struct TimedSection {
    u64 timeBegin;
    u64 oldTimeWithChildren;
    i64 anchorIndex;
    i64 parentIndex;
} TimedSection;

#define PROFILE_ANCHOR_COUNT 1024
typedef struct Profile {
    u64 timeStart;
    i64 currentOpenIndex;
    ProfileAnchor anchors[PROFILE_ANCHOR_COUNT];
} Profile;

static Profile globalProfile;

static TimedSection profileThroughputBegin_(Str name, i64 index, i64 dataSize) {
    assert(index < PROFILE_ANCHOR_COUNT);
    ProfileAnchor* anchor = globalProfile.anchors + index;
    anchor->name = name;
    anchor->dataSize += dataSize;
    TimedSection section = {
        __rdtsc(),
        anchor->timeTakenWithChildren,
        index,
        globalProfile.currentOpenIndex,
    };
    globalProfile.currentOpenIndex = index;
    return section;
}

static void profileThroughputEnd(TimedSection* section) {
    ProfileAnchor* parent = globalProfile.anchors + section->parentIndex;
    ProfileAnchor* anchor = globalProfile.anchors + section->anchorIndex;
    assert(anchor->name.ptr);
    anchor->count += 1;

    u64 diff = __rdtsc() - section->timeBegin;
    anchor->timeTakenSelf += diff;
    anchor->timeTakenWithChildren = section->oldTimeWithChildren + diff;
    parent->timeTakenSelf -= diff;

    globalProfile.currentOpenIndex = section->parentIndex;
    *section = (TimedSection) {};
}

static void addTime(StrBuilder* gstr, u64 total, u64 freqPerSec, u64 diff) {
    f64 diffSec = (f64)diff / (f64)freqPerSec;
    f64 prop = (f64)diff / (f64)total;
    buildStr(gstr, "%llu %.2gs %.2g%%", (unsigned long long)(diff), diffSec, prop * 100.0);
}

static void profileEnd(Arena* arena, u64 rdtscFrequencyPerSecond) { tempMemBlock(arena) {
    u64 timeEnd = __rdtsc();
    StrBuilder gstr = {.cap = arenaFreesize(arena) / sizeof(char)};
    gstr.ptr = arenaAllocArray(arena, char, gstr.cap);
    buildStr(&gstr, "\n");
    u64 total = timeEnd - globalProfile.timeStart;
    for (i64 ind = 1; ind < PROFILE_ANCHOR_COUNT; ind++) {
        ProfileAnchor* anchor = globalProfile.anchors + ind;
        if (anchor->name.ptr == 0) {
            break;
        }
        assert(anchor->count > 0);
        buildStr(&gstr, "%.*s: ", LIT(anchor->name));
        addTime(&gstr, total, rdtscFrequencyPerSecond, anchor->timeTakenWithChildren);
        if (anchor->timeTakenWithChildren - anchor->timeTakenSelf > 0) {
            buildStr(&gstr, " excl: ");
            addTime(&gstr, total, rdtscFrequencyPerSecond, anchor->timeTakenSelf);
        }
        if (anchor->count > 1) {
            buildStr(&gstr, " x%lld avg for 1: ", (long long)anchor->count);
            addTime(&gstr, total, rdtscFrequencyPerSecond, anchor->timeTakenWithChildren / anchor->count);
        }
        if (anchor->dataSize > 0) {
            f64 seconds = (f64)anchor->timeTakenWithChildren / (f64)rdtscFrequencyPerSecond;
            f64 MB = 1024 * 1024;
            f64 GB = MB * 1024;
            f64 dataSizeMB = (f64)anchor->dataSize / MB;
            f64 dataSizeGB = (f64)anchor->dataSize / GB;
            f64 gbPerSec = dataSizeGB / seconds;
            buildStr(&gstr, " data: %.2fMB, throughput: %.2fgb/s", dataSizeMB, gbPerSec);
        }
        buildStr(&gstr, "\n");
    }
    buildStr(&gstr, "total: %llu %.2gs\n", (unsigned long long)total, (f64)total / (f64)rdtscFrequencyPerSecond);
    Str msg = {gstr.ptr, gstr.len};
    printf("%.*s", LIT(msg));
}}

typedef struct Rng {
    u64 state;
    u64 inc; // Must be odd
} Rng;

// PCG-XSH-RR
// state_new = a * state_old + b
// output = rotate32((state ^ (state >> 18)) >> 27, state >> 59)
// as per `PCG: A Family of Simple Fast Space-Efficient Statistically Good Algorithms for Random Number Generation`
static u32 randomU32(Rng* rng) {
    u64 state = rng->state;
    u64 xorWith = state >> 18u;
    u64 xored = state ^ xorWith;
    u64 shifted64 = xored >> 27u;
    u32 shifted32 = (u32)shifted64;
    u32 rotateBy = state >> 59u;
    u32 shiftRightBy = rotateBy;
    u32 resultRight = shifted32 >> shiftRightBy;
    // NOTE(khvorov) This is `32 - rotateBy` but produces 0 when rotateBy is 0
    // Shifting a 32 bit value by 32 is apparently UB and the compiler is free to remove that code
    // I guess, so we are avoiding it by doing this weird bit hackery
    u32 shiftLeftBy = (-rotateBy) & 0b11111u;
    u32 resultLeft = shifted32 << shiftLeftBy;
    u32 result = resultRight | resultLeft;
    // NOTE(khvorov) This is just one of those magic LCG constants "in common use"
    // https://en.wikipedia.org/wiki/Linear_congruential_generator#Parameters_in_common_use
    rng->state = 6364136223846793005ULL * state + rng->inc;
    return result;
}

static Rng createRng(u32 seed) {
    Rng rng = {.state = seed, .inc = seed | 1};
    // NOTE(khvorov) When seed is 0 the first 2 numbers are always 0 which is probably not what we want
    randomU32(&rng);
    randomU32(&rng);
    return rng;
}

static u32 randomU32Bound(Rng* rng, u32 max) {
    // NOTE(khvorov) This is equivalent to (UINT32_MAX + 1) % max;
    u32 threshold = -max % max;
    u32 unbound = randomU32(rng);
    while (unbound < threshold) { unbound = randomU32(rng); }
    u32 result = unbound % max;
    return result;
}

static float randomF3201(Rng* rng) {
    u32 rU32 = randomU32(rng);
    float randomF32 = (float)rU32;
    float onePastMaxRandomU32 = (float)(1ULL << 32ULL);
    float result = randomF32 / onePastMaxRandomU32;
    return result;
}

static f32 randomFraction(Rng* rng, f32 min) {
    f32 result = randomF3201(rng);
    while (result < min) { result = randomF3201(rng); }
    return result;
}

static f64 square(f64 x) { return x * x; }
static f64 degreesToRadians(f64 degrees) { return 0.01745329251994329577 * degrees; }

#include <math.h>

// From https://github.com/cmuratori/computer_enhance/blob/main/perfaware/part2/listing_0065_haversine_formula.cpp
// NOTE(casey): EarthRadius is generally expected to be 6372.8
static f64 haversineDistance(f64 X0, f64 Y0, f64 X1, f64 Y1, f64 EarthRadius) {
    // NOTE(casey): This is not meant to be a "good" way to calculate the Haversine distance.
    // Instead, it attempts to follow, as closely as possible, the formula used in the real-world
    // question on which these homework exercises are loosely based.

    f64 lat1 = Y0;
    f64 lat2 = Y1;
    f64 lon1 = X0;
    f64 lon2 = X1;

    f64 dLat = degreesToRadians(lat2 - lat1);
    f64 dLon = degreesToRadians(lon2 - lon1);
    lat1 = degreesToRadians(lat1);
    lat2 = degreesToRadians(lat2);

    f64 a = square(sin(dLat / 2.0)) + cos(lat1) * cos(lat2) * square(sin(dLon / 2));
    f64 c = 2.0 * asin(sqrt(a));

    f64 Result = EarthRadius * c;
    return Result;
}

typedef struct Pair { f64 x0, y0, x1, y1; } Pair;

#include <windows.h>
#include <psapi.h>

#pragma comment(lib, "advapi32")

typedef struct RepetitionTester {
    u64 freqPerSec;
    u64 toWait;
    u64 expectedSize;

    u64 minDiffTime;
    u64 maxDiffTime;
    u64 diffTimeSum;

    u64 minDiffPF;
    u64 maxDiffPF;

    u64 diffCount;

    u64 lastBegin;
    PROCESS_MEMORY_COUNTERS lastCounters;

    u64 waited;

    HANDLE process;
} RepetitionTester;

static RepetitionTester
createRepetitionTester(u64 freqPerSec, u64 expectedSize) {
    RepetitionTester tester = {
        .freqPerSec = freqPerSec, .toWait = freqPerSec, .expectedSize = expectedSize, .minDiffTime = UINT64_MAX, .minDiffPF = UINT64_MAX,
        .process = OpenProcess(PROCESS_QUERY_INFORMATION  | PROCESS_VM_READ, FALSE, GetCurrentProcessId())
    };
    assert(tester.process);
    return tester;
}

static void
repeatBeginTime(RepetitionTester* tester) {
    tester->lastBegin = __rdtsc();
    BOOL GetProcessMemoryInfoResult = GetProcessMemoryInfo(tester->process, &tester->lastCounters, sizeof(tester->lastCounters));
    assert(GetProcessMemoryInfoResult);
}

static void
repeatEndTime(RepetitionTester* tester) {
    u64 time = __rdtsc();
    u64 diffTime = time - tester->lastBegin;
    if (diffTime < tester->minDiffTime) {
        tester->minDiffTime = diffTime;
        tester->waited = 0;
    } else {
        tester->waited += diffTime;
    }
    if (diffTime > tester->maxDiffTime) {
        tester->maxDiffTime = diffTime;
    }

    tester->diffTimeSum += diffTime;
    tester->diffCount += 1;

    PROCESS_MEMORY_COUNTERS counters = {};
    BOOL GetProcessMemoryInfoResult = GetProcessMemoryInfo(tester->process, &counters, sizeof(counters));
    assert(GetProcessMemoryInfoResult);

    u64 diffPF = counters.PageFaultCount - tester->lastCounters.PageFaultCount;
    tester->minDiffPF = min(tester->minDiffPF, diffPF);
    tester->maxDiffPF = max(tester->maxDiffPF, diffPF);
}

static bool
repeatShouldStop(RepetitionTester* tester) {
    bool result = tester->waited >= tester->toWait;
    return result;
}

static void
repeatPrint(RepetitionTester* tester) {
    f64 minSec = (f64)tester->minDiffTime / (f64)tester->freqPerSec;
    f64 maxSec = (f64)tester->maxDiffTime / (f64)tester->freqPerSec;

    f64 sizeGB = (f64)tester->expectedSize / (1024.0 * 1024.0 * 1024.0);
    f64 minBand = sizeGB / minSec;
    f64 maxBand = sizeGB / maxSec;

    f64 sizeKB = (f64)tester->expectedSize / (1024.0);
    f64 minKBPerPF = sizeKB / (f64)tester->minDiffPF;
    f64 maxKBPerPF = sizeKB / (f64)tester->maxDiffPF;

    printf(
        "repeat: min: %.2gs %.2ggb/s %lluPF %.2gKB/PF, max: %.2gs %.2ggb/s %lluPF %.2gKB/PF\n",
        minSec, minBand, tester->minDiffPF, minKBPerPF, maxSec, maxBand, tester->maxDiffPF, maxKBPerPF
    );
}

static void writeEntireFile(char* path, void* content, i64 contentLen) {
    HANDLE handle = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    assert(handle != INVALID_HANDLE_VALUE);
    DWORD bytesWritten = 0;
    BOOL WriteFileResult = WriteFile(handle, content, (DWORD)contentLen, &bytesWritten, 0);
    assert(WriteFileResult);
    assert(bytesWritten == contentLen);
    CloseHandle(handle);
}

typedef struct OpenedFile {
    HANDLE handle;
    i64 size;
} OpenedFile;

static OpenedFile openFile(char* path) {
    HANDLE handle = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    assert(handle != INVALID_HANDLE_VALUE);
    LARGE_INTEGER fileSize = {};
    BOOL GetFileSizeExResult = GetFileSizeEx(handle, &fileSize);
    assert(GetFileSizeExResult);
    OpenedFile result = {handle, fileSize.QuadPart};
    return result;
}

static u8arr readAndCloseFile(Arena* arena, OpenedFile file) {
    u8arr result = {.len = file.size};
    result.ptr = arenaAllocArray(arena, u8, result.len);
    DWORD bytesRead = 0;
    BOOL ReadFileResult = ReadFile(file.handle, result.ptr, file.size, &bytesRead, 0);
    assert(ReadFileResult);
    assert(bytesRead == result.len);
    CloseHandle(file.handle);
    return result;
}

int main() {
    Arena arena_ = {.size = Gigabyte};
    Arena* arena = &arena_;
    {
        HANDLE tokenHandle = 0;
        BOOL OpenProcessTokenResult = OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &tokenHandle);
        assert(OpenProcessTokenResult);

        TOKEN_PRIVILEGES privs = {.PrivilegeCount = 1};
        privs.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        BOOL LookupPrivilegeValueAResult = LookupPrivilegeValueA(0, SE_LOCK_MEMORY_NAME, &privs.Privileges[0].Luid);
        assert(LookupPrivilegeValueAResult);

        BOOL AdjustTokenPrivilegesResult = AdjustTokenPrivileges(tokenHandle, 0, &privs, 0, 0, 0);
        assert(AdjustTokenPrivilegesResult);

        arena->base = VirtualAlloc(0, arena->size, MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES, PAGE_READWRITE);
        assert(arena->base);
    }

    u64 rdtscFrequencyPerSecond = 0;
    {
        LARGE_INTEGER pcountStart = {};
        QueryPerformanceCounter(&pcountStart);
        u64 rdtscStart = __rdtsc();
        Sleep(100);
        LARGE_INTEGER pcountEnd = {};
        QueryPerformanceCounter(&pcountEnd);
        u64 rdtscEnd = __rdtsc();

        u64 pdiff = pcountEnd.QuadPart - pcountStart.QuadPart;
        u64 rdtscDiff = rdtscEnd - rdtscStart;

        LARGE_INTEGER pfreq = {};
        QueryPerformanceFrequency(&pfreq);

        f64 secondsElapsed = (f64)pdiff / (f64)pfreq.QuadPart;
        rdtscFrequencyPerSecond = (f64)rdtscDiff / (f64)secondsElapsed;
        printf("rdtsc freq: %llu\n", (unsigned long long)rdtscFrequencyPerSecond);
    }

    char* inputPath = "input.json";

    bool generateInput = false;
    if (generateInput) tempMemBlock(arena) {
        i64 pairCount = 1000000;
        i64 seed = 8;
        struct {Pair* ptr; i64 len;} pairs = {.len = pairCount};
        pairs.ptr = arenaAllocArray(arena, Pair, pairs.len);
        struct {f64* ptr; i64 len;} referenceHaversine = {.len = pairCount};
        referenceHaversine.ptr = arenaAllocArray(arena, f64, referenceHaversine.len);

        Rng rng = createRng(seed);

        f32 xmin = -180.0f;
        f32 xrange = 360.0f;
        f32 ymin = -90.0f;
        f32 yrange = 180.0f;

        bool sector = true;
        if (sector) {
            xmin = randomF3201(&rng) * 180.0f - 180.0f;
            xrange = randomFraction(&rng, 0.1) * 180.0f;
            ymin = randomF3201(&rng) * 90.0f - 90.0f;
            yrange = randomFraction(&rng, 0.1) * 90.0f;
        }

        f64 expectedAverage = 0;
        for (i64 ind = 0; ind < pairCount; ind++) {
            Pair pair = {
                .x0 = randomF3201(&rng) * xrange + xmin,
                .x1 = randomF3201(&rng) * xrange + xmin,
                .y0 = randomF3201(&rng) * yrange + ymin,
                .y1 = randomF3201(&rng) * yrange + ymin,
            };

            pairs.ptr[ind] = pair;

            f64 earthRadius = 6372.8;
            f64 haversine = haversineDistance(pair.x0, pair.y0, pair.x1, pair.y1, earthRadius);
            referenceHaversine.ptr[ind] = haversine;
            expectedAverage += haversine / pairCount;
        }

        StrBuilder builder = {.cap = arenaFreesize(arena) / sizeof(char)};
        builder.ptr = arenaAllocArray(arena, char, builder.cap);
        buildStr(&builder, "{\"pairs\":[\n");

        for (i64 ind = 0; ind < pairCount; ind++) {
            Pair pair = pairs.ptr[ind];
            buildStr(&builder, "    {\"x0\":%.16f, \"x1\":%.16f, \"y0\":%.16f, \"y1\":%.16f}", pair.x0, pair.x1, pair.y0, pair.y1);
            if (ind < pairCount - 1) {
                buildStr(&builder, ",");
            }
            buildStr(&builder, "\n");
        }

        buildStr(&builder, "]}");

        Str inputJsonStr = {.ptr = builder.ptr, .len = builder.len};
        writeEntireFile(inputPath, inputJsonStr.ptr, inputJsonStr.len);

        // TODO(khvorov) Write out reference values as well I guess
    }

    bool repeatTestReadFile = false;
    if (repeatTestReadFile) {
        OpenedFile openedInputFile = openFile(inputPath);
        RepetitionTester tester_ = createRepetitionTester(rdtscFrequencyPerSecond, openedInputFile.size);
        RepetitionTester* tester = &tester_;
        CloseHandle(openedInputFile.handle);

        while (!repeatShouldStop(tester)) tempMemBlock(arena) {

            repeatBeginTime(tester);
            OpenedFile file = openFile(inputPath);
            u8arr content = readAndCloseFile(arena, file);
            repeatEndTime(tester);

            assert(tester->expectedSize == (u64)content.len);
        }

        repeatPrint(tester);
    }

    bool repeatTestWriteToMem = true;
    if (repeatTestWriteToMem) tempMemBlock(arena) {
        i64 bufSize = 100 * Megabyte;
        u8* buf = arenaAllocArray(arena, u8, bufSize);

        {
            RepetitionTester tester_ = createRepetitionTester(rdtscFrequencyPerSecond, bufSize);
            RepetitionTester* tester = &tester_;
            while (!repeatShouldStop(tester)) {
                repeatBeginTime(tester);
                for (i64 ind = 0; ind < bufSize; ind++) {
                    buf[ind] = ind;
                }
                repeatEndTime(tester);
            }
            repeatPrint(tester);
        }

        {
            RepetitionTester tester_ = createRepetitionTester(rdtscFrequencyPerSecond, bufSize);
            RepetitionTester* tester = &tester_;
            while (!repeatShouldStop(tester)) {
                repeatBeginTime(tester);
                void MOVAllBytesAsm(void* ptr, i64 bufSize);
                MOVAllBytesAsm(buf, bufSize);
                repeatEndTime(tester);
            }
            repeatPrint(tester);
        }

        {
            RepetitionTester tester_ = createRepetitionTester(rdtscFrequencyPerSecond, bufSize);
            RepetitionTester* tester = &tester_;
            while (!repeatShouldStop(tester)) {
                repeatBeginTime(tester);
                void NOPAllBytesAsm(void* ptr, i64 bufSize);
                NOPAllBytesAsm(buf, bufSize);
                repeatEndTime(tester);
            }
            repeatPrint(tester);
        }

        {
            RepetitionTester tester_ = createRepetitionTester(rdtscFrequencyPerSecond, bufSize);
            RepetitionTester* tester = &tester_;
            while (!repeatShouldStop(tester)) {
                repeatBeginTime(tester);
                void CMPAllBytesAsm(void* ptr, i64 bufSize);
                CMPAllBytesAsm(buf, bufSize);
                repeatEndTime(tester);
            }
            repeatPrint(tester);
        }
        
        {
            RepetitionTester tester_ = createRepetitionTester(rdtscFrequencyPerSecond, bufSize);
            RepetitionTester* tester = &tester_;
            while (!repeatShouldStop(tester)) {
                repeatBeginTime(tester);
                void DECAllBytesAsm(void* ptr, i64 bufSize);
                DECAllBytesAsm(buf, bufSize);
                repeatEndTime(tester);
            }
            repeatPrint(tester);
        }
    }

    tempMemBlock(arena) {
        OpenedFile openedInputFile = openFile(inputPath);
        profileThroughput("read input", openedInputFile.size) {
            u8arr inputContent = readAndCloseFile(arena, openedInputFile);
        }
    }

    profileEnd(arena, rdtscFrequencyPerSecond);
}
