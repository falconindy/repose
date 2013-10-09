#ifndef REPOSE_UTIL_H
#define REPOSE_UTIL_H

#include <stddef.h>
#include <stdbool.h>
#include "alpm/pkghash.h"

void safe_asprintf(char **strp, const char *fmt, ...);
int colon_printf(const char *fmt, ...);

bool match_targets(alpm_pkg_meta_t *pkg, alpm_list_t *targets);
alpm_list_t *glob_cache(alpm_pkghash_t *pkgcache, const char *target);

#endif
