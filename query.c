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
#include <err.h>

#include "repose.h"
#include "alpm/alpm_metadata.h"

static void print_metadata(const alpm_pkg_meta_t *pkg)
{
    if (cfg.info) {
        printf("Filename     : %s\n", pkg->filename);
        printf("Name         : %s\n", pkg->name);
        printf("Version      : %s\n", pkg->version);
        printf("Description  : %s\n", pkg->desc);
        printf("Architecture : %s\n", pkg->arch);
        printf("URL          : %s\n", pkg->url);
        printf("Packager     : %s\n\n", pkg->packager);
    } else {
        printf("%s %s\n", pkg->name, pkg->version);
    }
}

/* read the existing repo or construct a new package cache */
int repo_database_query(repo_t *repo, int argc, char *argv[])
{
    if (repo->state == REPO_NEW)
        return 0;

    if (argc > 0) {
        int i;
        for (i = 0; i < argc; ++i) {
            const alpm_pkg_meta_t *pkg = _alpm_pkghash_find(repo->pkgcache, argv[i]);
            if (pkg == NULL) {
                warnx("pkg not found");
                return 1;
            }
            print_metadata(pkg);
        }
    } else {
        alpm_list_t *node, *pkgs = repo->pkgcache->list;
        for (node = pkgs; node; node = node->next)
            print_metadata(node->data);
    }

    return 0;
}
