#include "yaotau_semver.h"
#include <stdio.h>

static void parse3(const char *s, int *maj, int *min, int *pat) {
    *maj = *min = *pat = 0;
    if (!s) return;
    // Simple “x.y.z” parse; ignores suffixes.
    sscanf(s, "%d.%d.%d", maj, min, pat);
}

int yaotau_semver_compare(const char *a, const char *b) {
    int am, an, ap, bm, bn, bp;
    parse3(a, &am, &an, &ap);
    parse3(b, &bm, &bn, &bp);
    if (am != bm) return am - bm;
    if (an != bn) return an - bn;
    return ap - bp;
}
