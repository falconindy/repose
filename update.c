#include <stdlib.h>
#include <stdio.h>
#include <err.h>

#include "repose.h"
#include "util.h"
#include "alpm/alpm_metadata.h"

static inline alpm_pkghash_t *_alpm_pkghash_replace(alpm_pkghash_t *cache, alpm_pkg_meta_t *new,
                                                    alpm_pkg_meta_t *old)
{
    cache = _alpm_pkghash_remove(cache, old, NULL);
    return _alpm_pkghash_add(cache, new);
}

/* read the existing repo or construct a new package cache */
int repo_database_update(repo_t *repo, int argc, char *argv[])
{
    alpm_list_t *node, *pkgs;
    bool force = argc > 0 ? true : false;

    if (repo->state == REPO_NEW)
        warnx("repo doesn't exist, creating...");

    alpm_pkghash_t *filecache = get_filecache(repo, argc, argv);
    pkgs = filecache->list;

    colon_printf("Updating repo database...\n");
    for (node = pkgs; node; node = node->next) {
        alpm_pkg_meta_t *pkg = node->data;
        alpm_pkg_meta_t *old = _alpm_pkghash_find(repo->pkgcache, pkg->name);
        int vercmp;

        /* if the package isn't in the cache, add it */
        if (!old) {
            printf("adding %s %s\n", pkg->name, pkg->version);
            repo->pkgcache = _alpm_pkghash_add(repo->pkgcache, pkg);
            repo->state = REPO_DIRTY;
            continue;
        }

        vercmp = alpm_pkg_vercmp(pkg->version, old->version);

        /* if the package is in the cache, but we're doing a forced
         * update, replace it anywaysj*/
        if (force) {
            printf("replacing %s %s => %s\n", pkg->name, old->version, pkg->version);
            repo->pkgcache = _alpm_pkghash_replace(repo->pkgcache, pkg, old);
            /* if ((vercmp == -1 && cfg.clean >= 1) || (vercmp == 1 && cfg.clean >= 2)) */
            /*     unlink_package(repo, old); */
            alpm_pkg_free_metadata(old);
            repo->state = REPO_DIRTY;
            continue;
        }

        /* if the package is in the cache and we have a newer version,
         * replace it */
        switch(vercmp) {
        case 1:
            printf("updating %s %s => %s\n", pkg->name, old->version, pkg->version);
            repo->pkgcache = _alpm_pkghash_replace(repo->pkgcache, pkg, old);
            /* if (cfg.clean >= 1) */
            /*     unlink_package(repo, old); */
            alpm_pkg_free_metadata(old);
            repo->state = REPO_DIRTY;
            break;
        case 0:
            /* XXX: REFACTOR */
            if (pkg->builddate > old->builddate) {
                printf("updating %s %s [newer build]\n", pkg->name, pkg->version);
                repo->pkgcache = _alpm_pkghash_replace(repo->pkgcache, pkg, old);
                alpm_pkg_free_metadata(old);
                repo->state = REPO_DIRTY;
            } else if (old->base64_sig == NULL && pkg->base64_sig) {
                /* check to see if the package now has a signature */
                printf("adding signature for %s\n", pkg->name);
                repo->pkgcache = _alpm_pkghash_replace(repo->pkgcache, pkg, old);
                alpm_pkg_free_metadata(old);
                repo->state = REPO_DIRTY;
            }
            break;
        case -1:
            break;
            /* if (cfg.clean >= 2) { */
            /*     unlink_package(repo, pkg); */
            /* } */
        }

    }

    return repo_write(repo);
}
