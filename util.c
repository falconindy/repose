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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <err.h>
#include <fnmatch.h>

#include "repose.h"
#include "util.h"
#include "alpm/alpm_metadata.h"

void safe_asprintf(char **strp, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    if (vasprintf(strp, fmt, ap) < 0)
        err(EXIT_FAILURE, "failed to allocate memory");
    va_end(ap);
}

int colon_printf(const char *fmt, ...)
{
    int ret;
    va_list args;

    va_start(args, fmt);
    fputs(cfg.colstr.colon, stdout);
    ret = vprintf(fmt, args);
    fputs(cfg.colstr.nocolor, stdout);
    va_end(args);

    fflush(stdout);
    return ret;
}

/* {{{ */
static bool match_target_r(alpm_pkg_meta_t *pkg, const char *target, char **buf)
{
    if (strcmp(target, pkg->filename) == 0)
        return true;
    else if (strcmp(target, pkg->name) == 0)
        return true;

    /* since this may be called multiple times, buf is external to avoid
     * recalculating it each time. */
    if (*buf == NULL)
        safe_asprintf(buf, "%s-%s", pkg->name, pkg->version);

    if (fnmatch(target, *buf, 0) == 0)
        return true;
    return false;
}

bool match_targets(alpm_pkg_meta_t *pkg, alpm_list_t *targets)
{
    bool ret = false;
    char *buf = NULL;
    const alpm_list_t *node;

    for (node = targets; node && !ret; node = node->next)
        ret = match_target_r(pkg, node->data, &buf);

    free(buf);
    return ret;
}

alpm_list_t *glob_cache(alpm_pkghash_t *pkgcache, const char *target)
{
    alpm_list_t *ret = NULL;
    char *buf = NULL;
    const alpm_list_t *node;

    for (node = pkgcache->list; node; node = node->next) {
        if (match_target_r(node->data, target, &buf))
            ret = alpm_list_add(ret, node->data);
    }

    free(buf);
    return ret;
}
/* }}} */
