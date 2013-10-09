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
#include <fcntl.h>
#include <errno.h>
#include <err.h>

#include "repose.h"
#include "util.h"
#include "alpm/signing.h"
#include "alpm/alpm_metadata.h"
#include "alpm/util.h"

static int verify_pkg_sig(repo_t *repo, const alpm_pkg_meta_t *pkg)
{
    int pkgfd, sigfd = openat(repo->poolfd, pkg->signame, O_RDONLY);
    if (sigfd < 0) {
        if (errno == ENOENT)
            return -1;
        err(EXIT_FAILURE, "failed to open %s", pkg->signame);
    }

    pkgfd = openat(repo->poolfd, pkg->filename, O_RDONLY);
    if (pkgfd < 0) {
        err(EXIT_FAILURE, "failed to open %s", pkg->filename);
    }

    int rc = gpgme_verify(pkgfd, sigfd);
    close(pkgfd);
    close(sigfd);
    return rc;
}

static int verify_pkg(repo_t *repo, const alpm_pkg_meta_t *pkg)
{
    if (faccessat(repo->poolfd, pkg->filename, F_OK, 0) < 0) {
        warn("couldn't find pkg %s at %s", pkg->name, pkg->filename);
        return 1;
    }

    /* if we have a signature, verify it */
    if (verify_pkg_sig(repo, pkg) < 0) {
        warnx("package %s, signature is invalid or corrupt!", pkg->name);
        return 1;
    }

    /* if we have a md5sum, verify it */
    if (pkg->md5sum) {
        char *md5sum = _compute_md5sum(repo->poolfd, pkg->filename);
        if (strcmp(pkg->md5sum, md5sum) != 0) {
            warnx("md5 sum for pkg %s is different", pkg->name);
            return 1;
        }
        free(md5sum);
    }

    /* if we have a sha256sum, verify it */
    if (pkg->sha256sum) {
        char *sha256sum = _compute_sha256sum(repo->poolfd, pkg->filename);
        if (strcmp(pkg->sha256sum, sha256sum) != 0) {
            warnx("sha256 sum for pkg %s is different", pkg->name);
            return 1;
        }
        free(sha256sum);
    }

    return 0;
}

int repo_database_verify(repo_t *repo)
{
    alpm_list_t *node;
    int rc = 0;

    if (repo->state != REPO_NEW)
        return 0;

    for (node = repo->pkgcache->list; node; node = node->next) {
        alpm_pkg_meta_t *pkg = node->data;
        rc |= verify_pkg(repo, pkg);
    }

    if (rc == 0)
        printf("repo okay!\n");

    return rc;
}
