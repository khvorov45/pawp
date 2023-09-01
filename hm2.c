#include "cbuild.h"
#include "math.h"

#define function static
#define assert(x) prb_assert(x)
#define absval(x) ((x) < 0 ? -(x) : (x))
#define STR(x) prb_STR(x)
#define LIT(x) prb_LIT(x)

#ifdef PAWP_PROFILE
#define profileThroughputBegin(name, dataSize) TimedSection name##Section = profileThroughputBegin_(STR(#name), __COUNTER__ + 1, dataSize)
#define profileThroughputEnd(name) profileThroughputEnd_(name##Section)
#define profileThroughput(name, dataSize) profileThroughputBegin(name, dataSize); for (int _i_ = 0; _i_ == 0; _i_++, profileThroughputEnd(name))
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

typedef intptr_t isize;
typedef uint64_t u64;
typedef float    f32;
typedef double   f64;

typedef prb_Str        Str;
typedef prb_GrowingStr GrowingStr;
typedef prb_Arena      Arena;

typedef struct ProfileAnchor {
    Str   name;
    u64   timeTakenSelf;
    u64   timeTakenWithChildren;
    isize count;
    isize dataSize;
} ProfileAnchor;

typedef struct TimedSection {
    u64   timeBegin;
    u64   oldTimeWithChildren;
    isize anchorIndex;
    isize parentIndex;
} TimedSection;

#define PROFILE_ANCHOR_COUNT 1024
typedef struct Profile {
    u64           timeStart;
    isize         currentOpenIndex;
    ProfileAnchor anchors[PROFILE_ANCHOR_COUNT];
} Profile;

static Profile globalProfile;

function TimedSection
profileThroughputBegin_(Str name, isize index, isize dataSize) {
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

function void
profileThroughputEnd_(TimedSection section) {
    ProfileAnchor* parent = globalProfile.anchors + section.parentIndex;
    ProfileAnchor* anchor = globalProfile.anchors + section.anchorIndex;
    assert(anchor->name.ptr);
    anchor->count += 1;

    u64 diff = __rdtsc() - section.timeBegin;
    anchor->timeTakenSelf += diff;
    anchor->timeTakenWithChildren = section.oldTimeWithChildren + diff;
    parent->timeTakenSelf -= diff;

    globalProfile.currentOpenIndex = section.parentIndex;
}

function void
addTime(prb_GrowingStr* gstr, u64 total, u64 freqPerSec, u64 diff) {
    f64 diffSec = (f64)diff / (f64)freqPerSec;
    f64 prop = (f64)diff / (f64)total;
    prb_addStrSegment(gstr, "%llu %.2gs %.2g%%", (unsigned long long)(diff), diffSec, prop * 100.0);
}

function void
profileEnd(Arena* arena, u64 rdtscFrequencyPerSecond) {
    u64 timeEnd = __rdtsc();

    prb_GrowingStr gstr = prb_beginStr(arena);
    prb_addStrSegment(&gstr, "\n");
    u64 total = timeEnd - globalProfile.timeStart;
    for (isize ind = 1; ind < PROFILE_ANCHOR_COUNT; ind++) {
        ProfileAnchor* anchor = globalProfile.anchors + ind;
        if (anchor->name.ptr == 0) {
            break;
        }
        assert(anchor->count > 0);
        prb_addStrSegment(&gstr, "%.*s: ", LIT(anchor->name));
        addTime(&gstr, total, rdtscFrequencyPerSecond, anchor->timeTakenWithChildren);
        if (anchor->timeTakenWithChildren - anchor->timeTakenSelf > 0) {
            prb_addStrSegment(&gstr, " excl: ");
            addTime(&gstr, total, rdtscFrequencyPerSecond, anchor->timeTakenSelf);
        }
        if (anchor->count > 1) {
            prb_addStrSegment(&gstr, " x%lld avg for 1: ", (long long)anchor->count);
            addTime(&gstr, total, rdtscFrequencyPerSecond, anchor->timeTakenWithChildren / anchor->count);
        }
        if (anchor->dataSize > 0) {
            f64 seconds = (f64)anchor->timeTakenWithChildren / (f64)rdtscFrequencyPerSecond;
            f64 MB = 1024 * 1024;
            f64 GB = MB * 1024;
            f64 dataSizeMB = (f64)anchor->dataSize / MB;
            f64 dataSizeGB = (f64)anchor->dataSize / GB;
            f64 gbPerSec = dataSizeGB / seconds;
            prb_addStrSegment(&gstr, " data: %.2fMB, throughput: %.2fgb/s", dataSizeMB, gbPerSec);
        }
        prb_addStrSegment(&gstr, "\n");
    }
    prb_addStrSegment(&gstr, "total: %llu %.2gs\n", (unsigned long long)total, (f64)total / (f64)rdtscFrequencyPerSecond);
    Str msg = prb_endStr(&gstr);
    prb_writeToStdout(msg);
}

typedef struct Input {
    Str  json;
    f64* referenceHaversine;
    f64  expectedAverage;
} Input;

typedef struct Pair {
    f64 x0, y0, x1, y1;
} Pair;

static f64
Square(f64 A) {
    f64 Result = (A * A);
    return Result;
}

static f64
RadiansFromDegrees(f64 Degrees) {
    f64 Result = 0.01745329251994329577 * Degrees;
    return Result;
}

// From https://github.com/cmuratori/computer_enhance/blob/main/perfaware/part2/listing_0065_haversine_formula.cpp
// NOTE(casey): EarthRadius is generally expected to be 6372.8
static f64
ReferenceHaversine(f64 X0, f64 Y0, f64 X1, f64 Y1, f64 EarthRadius) {
    /* NOTE(casey): This is not meant to be a "good" way to calculate the Haversine distance.
       Instead, it attempts to follow, as closely as possible, the formula used in the real-world
       question on which these homework exercises are loosely based.
    */

    f64 lat1 = Y0;
    f64 lat2 = Y1;
    f64 lon1 = X0;
    f64 lon2 = X1;

    f64 dLat = RadiansFromDegrees(lat2 - lat1);
    f64 dLon = RadiansFromDegrees(lon2 - lon1);
    lat1 = RadiansFromDegrees(lat1);
    lat2 = RadiansFromDegrees(lat2);

    f64 a = Square(sin(dLat / 2.0)) + cos(lat1) * cos(lat2) * Square(sin(dLon / 2));
    f64 c = 2.0 * asin(sqrt(a));

    f64 Result = EarthRadius * c;
    return Result;
}

function f32
randomFraction(prb_Rng* rng, f32 min) {
    f32 result = prb_randomF3201(rng);
    while (result < min) {
        result = prb_randomF3201(rng);
    }
    return result;
}

typedef enum JsonTokenKind {
    JsonTokenKind_None,
    JsonTokenKind_CurlyOpen,
    JsonTokenKind_CurlyClose,
    JsonTokenKind_SquareOpen,
    JsonTokenKind_SquareClose,
    JsonTokenKind_Colon,
    JsonTokenKind_Comma,
    JsonTokenKind_String,
    JsonTokenKind_Number,
} JsonTokenKind;

typedef struct JsonToken {
    JsonTokenKind kind;
    Str           str;
} JsonToken;

typedef struct JsonIter {
    Str       str;
    isize     offset;
    JsonToken token;
} JsonIter;

function JsonIter
createJsonIter(Str input) {
    JsonIter iter = {.str = input};
    return iter;
}

function prb_Status
jsonIterNext(JsonIter* iter) {
    prb_Status result = prb_Failure;

    for (; iter->offset < iter->str.len;) {
        char ch = iter->str.ptr[iter->offset];
        if (ch != ' ' && ch != '\n' && ch != '\r' && ch != '\t' && ch != '\v' && ch != '\f') {
            break;
        }
        iter->offset += 1;
    }

    if (iter->offset < iter->str.len) {
        result = prb_Success;
        iter->token = (JsonToken) {};

        char ch = iter->str.ptr[iter->offset++];
        switch (ch) {
            case '{': iter->token.kind = JsonTokenKind_CurlyOpen; break;
            case '}': iter->token.kind = JsonTokenKind_CurlyClose; break;
            case '[': iter->token.kind = JsonTokenKind_SquareOpen; break;
            case ']': iter->token.kind = JsonTokenKind_SquareClose; break;
            case ':': iter->token.kind = JsonTokenKind_Colon; break;
            case ',': iter->token.kind = JsonTokenKind_Comma; break;

            case '"': {
                char* start = (char*)iter->str.ptr + iter->offset;
                isize len = 0;
                bool  endFound = false;
                for (; iter->offset < iter->str.len;) {
                    char ch = iter->str.ptr[iter->offset++];
                    if (ch == '"') {
                        endFound = true;
                        break;
                    }
                    len += 1;
                }
                assert(endFound);
                iter->token = (JsonToken) {JsonTokenKind_String, {start, len}};
            } break;

            case '-':
            case '.':
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9': {
                char* start = (char*)iter->str.ptr + iter->offset - 1;
                isize len = 0;
                bool  endFound = false;
                for (; iter->offset < iter->str.len;) {
                    char ch = iter->str.ptr[iter->offset];
                    if (!(ch == '.' || (ch >= '0' && ch <= '9'))) {
                        endFound = true;
                        break;
                    }
                    len += 1;
                    iter->offset += 1;
                }
                assert(endFound);
                iter->token = (JsonToken) {JsonTokenKind_Number, {start, len}};
            } break;
        }
    }
    return result;
}

function void
expectToken(JsonIter* iter, JsonToken token) {
    assert(jsonIterNext(iter));
    assert(iter->token.kind == token.kind);
    assert(prb_streq(iter->token.str, token.str));
}

function void
expectTokenKind(JsonIter* iter, JsonTokenKind kind) {
    assert(jsonIterNext(iter));
    assert(iter->token.kind == kind);
}

function void
expectString(JsonIter* iter, Str str) {
    expectToken(iter, (JsonToken) {.kind = JsonTokenKind_String, .str = str});
}

function f64
expectNumber(JsonIter* iter) {
    expectTokenKind(iter, JsonTokenKind_Number);
    prb_ParsedNumber parsed = prb_parseNumber(iter->token.str);
    assert(parsed.kind == prb_ParsedNumberKind_F64);
    return parsed.parsedF64;
}

function void
recursiveSleep(f32 ms) {
    profileSectionBegin(recursiveSleep);
    f32 toSub = 10.0f;
    if (ms <= toSub) {
        prb_sleep(ms);
    } else {
        prb_sleep(toSub);
        recursiveSleep(ms - toSub);
    }
    profileSectionEnd(recursiveSleep);
}

typedef struct RepetitionTester {
    Arena* arena;
    u64 freqPerSec;
    u64 toWait;
    u64 expectedSize;

    u64 minDiff;
    u64 maxDiff;

    u64 diffSum;
    u64 diffCount;

    u64 lastBegin;
    u64 waited;
} RepetitionTester;

function void
repeatBeginTime(RepetitionTester* tester) {
    tester->lastBegin = __rdtsc();
}

function void
repeatEndTime(RepetitionTester* tester) {
    u64 time = __rdtsc();
    u64 diff = time - tester->lastBegin;
    if (diff < tester->minDiff) {
        tester->minDiff = diff;
        tester->waited = 0;
    } else {
        tester->waited += diff;
    }
    if (diff > tester->maxDiff) {
        tester->maxDiff = diff;
    }

    tester->diffSum += diff;
    tester->diffCount += 1;
}

function bool
repeatShouldStop(RepetitionTester* tester) {
    bool result = tester->waited >= tester->toWait;
    return result;
}

function void
repeatTestReadFile(RepetitionTester* tester) {
    while (!repeatShouldStop(tester)) {
        prb_TempMemory temp = prb_beginTempMemory(tester->arena);

        repeatBeginTime(tester);
        prb_ReadEntireFileResult result = prb_readEntireFile(tester->arena, STR("input.json"));
        repeatEndTime(tester);

        assert(result.success);
        assert(tester->expectedSize == (u64)result.content.len);
        prb_endTempMemory(temp);
    }
}

function void
repeatPrint(RepetitionTester* tester) {
    f64 minSec = (f64)tester->minDiff / (f64)tester->freqPerSec;
    f64 maxSec = (f64)tester->maxDiff / (f64)tester->freqPerSec;

    f64 sizeGB = (f64)tester->expectedSize / (1024.0 * 1024.0 * 1024.0);
    f64 minBand = sizeGB / minSec;
    f64 maxBand = sizeGB / maxSec;

    prb_writeToStdout(prb_fmt(tester->arena, "repeat: min: %.2gs %.2ggb/s, max: %.2gs %.2ggb/s\n", minSec, minBand, maxSec, maxBand));
}

int
main() {
    globalProfile.timeStart = __rdtsc();

    profileSectionBegin(arenaInit);
    Arena  arena_ = prb_createArenaFromVmem(1 * prb_GIGABYTE);
    Arena* arena = &arena_;
    profileSectionEnd(arenaInit);

    u64 rdtscFrequencyPerSecond = 0;
    {
        profileSectionBegin(getRdtscFreq);

        prb_TimeStart timerStart = prb_timeStart();
        u64           rdtscStart = __rdtsc();
        f32           msToWait = 100.0f;
        while (prb_getMsFrom(timerStart) < msToWait) {}
        u64 rdtscEnd = __rdtsc();
        u64 rdtscDiff = rdtscEnd - rdtscStart;
        rdtscFrequencyPerSecond = rdtscDiff / (u64)msToWait * 1000;
        prb_writeToStdout(prb_fmt(arena, "rdtsc freq: %llu\n", (unsigned long long)rdtscFrequencyPerSecond));

        profileSectionEnd(getRdtscFreq);
    }

    // NOTE(khvorov) Create some pagefaults
    if (false) {
        u64 size = 100 * prb_MEGABYTE;
        f32 toWaitMs = 5000;
        for (;;) {
            prb_TempMemory temp = prb_beginTempMemory(arena);

            prb_writeToStdout(STR("allocAndSet\n"));
            u64 allocAndSetMin = UINT64_MAX;
            for (prb_TimeStart start = prb_timeStart(); prb_getMsFrom(start) < toWaitMs;) {
                void* mem = prb_vmemAlloc(size);
                u64 start = __rdtsc();
                prb_memset(mem, 0, size);
                u64 end = __rdtsc();
                prb_assert(VirtualFree(mem, size, MEM_DECOMMIT));
                allocAndSetMin = prb_min(allocAndSetMin, end - start);
            }

            prb_writeToStdout(STR("preallocAndSet\n"));
            u64 preallocAndSetMin = UINT64_MAX;
            void* mem = prb_vmemAlloc(size);
            for (prb_TimeStart start = prb_timeStart(); prb_getMsFrom(start) < toWaitMs;) {
                u64 start = __rdtsc();
                prb_memset(mem, 0, size);
                u64 end = __rdtsc();
                preallocAndSetMin = prb_min(preallocAndSetMin, end - start);
            }
            prb_assert(VirtualFree(mem, size, MEM_DECOMMIT));

            f64 allocAndSetSec = (f64)allocAndSetMin / (f64)rdtscFrequencyPerSecond;
            f64 preallocAndSetSec = (f64)preallocAndSetMin / (f64)rdtscFrequencyPerSecond;
            f64 allocAndSetBandwidth = (f64)size / allocAndSetSec / (f64)(prb_GIGABYTE);
            f64 preallocAndSetBandwidth = (f64)size / preallocAndSetSec / (f64)(prb_GIGABYTE);

            prb_writeToStdout(prb_fmt(arena, "allocAndSetSec: %.2gms (%.2ggb/s) preallocAndSetSec: %.2gms (%.2ggb/s)\n", allocAndSetSec * 1000.0f, allocAndSetBandwidth, preallocAndSetSec * 1000.0f, preallocAndSetBandwidth));

            prb_endTempMemory(temp);
        }
    }

    Str rootDir = prb_getParentDir(arena, STR(__FILE__));

    f64 earthRadius = 6372.8;

    Input input = {};
    {
        isize pairCount = 1000000;
        profileThroughput(genInput, pairCount * sizeof(Pair)) {
            isize seed = 8;
            Pair* pairs = 0;
            arrsetlen(pairs, pairCount);
            arrsetlen(input.referenceHaversine, pairCount);

            prb_Rng rng = prb_createRng(seed);

            f32 xmin = -180.0f;
            f32 xrange = 360.0f;
            f32 ymin = -90.0f;
            f32 yrange = 180.0f;

            bool sector = true;
            if (sector) {
                xmin = prb_randomF3201(&rng) * 180.0f - 180.0f;
                xrange = randomFraction(&rng, 0.1) * 180.0f;
                ymin = prb_randomF3201(&rng) * 90.0f - 90.0f;
                yrange = randomFraction(&rng, 0.1) * 90.0f;
            }

            for (isize ind = 0; ind < pairCount; ind++) {
                profileThroughput(genPair, sizeof(Pair)) {
                    Pair pair = {
                        .x0 = prb_randomF3201(&rng) * xrange + xmin,
                        .x1 = prb_randomF3201(&rng) * xrange + xmin,
                        .y0 = prb_randomF3201(&rng) * yrange + ymin,
                        .y1 = prb_randomF3201(&rng) * yrange + ymin,
                    };

                    pairs[ind] = pair;

                    f64 haversine = ReferenceHaversine(pair.x0, pair.y0, pair.x1, pair.y1, earthRadius);
                    input.referenceHaversine[ind] = haversine;
                    input.expectedAverage += haversine / pairCount;
                }
            }

            GrowingStr builder = prb_beginStr(arena);
            prb_addStrSegment(&builder, "{\"pairs\":[\n");

            for (isize ind = 0; ind < pairCount; ind++) {
                Pair pair = pairs[ind];
                prb_addStrSegment(&builder, "    {\"x0\":%.16f, \"x1\":%.16f, \"y0\":%.16f, \"y1\":%.16f}", pair.x0, pair.x1, pair.y0, pair.y1);
                if (ind < pairCount - 1) {
                    prb_addStrSegment(&builder, ",");
                }
                prb_addStrSegment(&builder, "\n");
            }

            prb_addStrSegment(&builder, "]}");
            input.json = prb_endStr(&builder);
        }
    }

    profileSection(writeInput) {
        prb_writeEntireFile(arena, prb_pathJoin(arena, rootDir, STR("input.json")), input.json.ptr, input.json.len);
    }

    if (true) {
        prb_writeToStdout(prb_fmt(arena, "Expected average: %f\n", input.expectedAverage));
    }

    profileThroughput(ReadInput, input.json.len) {
        prb_TempMemory temp = prb_beginTempMemory(arena);
        prb_ReadEntireFileResult result = prb_readEntireFile(arena, STR("input.json"));
        assert(result.success);
        assert(input.json.len == result.content.len);
        prb_endTempMemory(temp);
    }

    profileThroughput(parseAndCheck, input.json.len) {
        JsonIter jsonIter = createJsonIter(input.json);
        expectTokenKind(&jsonIter, JsonTokenKind_CurlyOpen);
        expectString(&jsonIter, STR("pairs"));
        expectTokenKind(&jsonIter, JsonTokenKind_Colon);
        expectToken(&jsonIter, (JsonToken) {.kind = JsonTokenKind_SquareOpen});
        f64 average = 0;
        for (isize pairIndex = 0;; pairIndex++) {
            expectTokenKind(&jsonIter, JsonTokenKind_CurlyOpen);

            Pair pair = {};

            expectString(&jsonIter, STR("x0"));
            expectTokenKind(&jsonIter, JsonTokenKind_Colon);
            pair.x0 = expectNumber(&jsonIter);
            expectTokenKind(&jsonIter, JsonTokenKind_Comma);

            expectString(&jsonIter, STR("x1"));
            expectTokenKind(&jsonIter, JsonTokenKind_Colon);
            pair.x1 = expectNumber(&jsonIter);
            expectTokenKind(&jsonIter, JsonTokenKind_Comma);

            expectString(&jsonIter, STR("y0"));
            expectTokenKind(&jsonIter, JsonTokenKind_Colon);
            pair.y0 = expectNumber(&jsonIter);
            expectTokenKind(&jsonIter, JsonTokenKind_Comma);

            expectString(&jsonIter, STR("y1"));
            expectTokenKind(&jsonIter, JsonTokenKind_Colon);
            pair.y1 = expectNumber(&jsonIter);

            f64 haversine = ReferenceHaversine(pair.x0, pair.y0, pair.x1, pair.y1, earthRadius);
            assert(pairIndex < arrlen(input.referenceHaversine));
            f64 referenceVal = input.referenceHaversine[pairIndex];
            assert(absval(haversine - referenceVal) < 0.00001);
            average += haversine;

            expectTokenKind(&jsonIter, JsonTokenKind_CurlyClose);

            assert(jsonIterNext(&jsonIter));
            bool breakLoop = false;
            switch (jsonIter.token.kind) {
                case JsonTokenKind_Comma: break;
                case JsonTokenKind_SquareClose: breakLoop = true; break;
                default: assert(!"unexpectedToken"); break;
            }
            if (breakLoop) {
                average /= pairIndex + 1;
                break;
            }
        }
        assert(absval(average - input.expectedAverage) < 0.00001);
        expectTokenKind(&jsonIter, JsonTokenKind_CurlyClose);
        assert(!jsonIterNext(&jsonIter));
    }

    recursiveSleep(100);

    profileEnd(arena, rdtscFrequencyPerSecond);

    RepetitionTester tester = {.arena = arena, .freqPerSec = rdtscFrequencyPerSecond, .toWait = rdtscFrequencyPerSecond, .expectedSize = input.json.len, .minDiff = UINT64_MAX};
    repeatTestReadFile(&tester);
    repeatPrint(&tester);

    return 0;
}
