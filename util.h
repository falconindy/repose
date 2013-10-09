/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright (C) Simon Gomizelj, 2013
 */

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
