#include "cbuild.h"
#include "math.h"

#define STR(x) prb_STR(x)
#define LIT(x) prb_LIT(x)

typedef intptr_t isize;
typedef float    f32;
typedef double   f64;

typedef prb_Str        Str;
typedef prb_GrowingStr GrowingStr;
typedef prb_Arena      Arena;

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

f32
randomFraction(prb_Rng* rng, f32 min) {
    f32 result = prb_randomF3201(rng);
    while (result < min) {
        result = prb_randomF3201(rng);
    }
    return result;
}

int
main() {
    Arena  arena_ = prb_createArenaFromVmem(1 * prb_GIGABYTE);
    Arena* arena = &arena_;

    Str rootDir = prb_getParentDir(arena, STR(__FILE__));

    Input input = {};
    {
        isize seed = 8;
        isize pairCount = 100;
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
            Pair pair = {
                .x0 = prb_randomF3201(&rng) * xrange + xmin,
                .x1 = prb_randomF3201(&rng) * xrange + xmin,
                .y0 = prb_randomF3201(&rng) * yrange + ymin,
                .y1 = prb_randomF3201(&rng) * yrange + ymin,
            };

            pairs[ind] = pair;

            f64 average = ReferenceHaversine(pair.x0, pair.y0, pair.x1, pair.y1, 6372.8);
            input.referenceHaversine[ind] = average;
            input.expectedAverage += average / pairCount;
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
    prb_writeEntireFile(arena, prb_pathJoin(arena, rootDir, STR("input.json")), input.json.ptr, input.json.len);

    if (true) {
        prb_writeToStdout(prb_fmt(arena, "Expected average: %f\n", input.expectedAverage));
    }

    return 0;
}
