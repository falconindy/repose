#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <dirent.h>
#include <fnmatch.h>
#include <sys/utsname.h>

#include "alpm/signing.h"
#include "alpm/util.h"
#include "alpm/base64.h"
#include "database.h"
#include "util.h"

int repo_database_remove(repo_t *repo, int argc, char *argv[])
{
    if (repo->state == REPO_NEW) {
        warnx("repo doesn't exist...");
        return 1;
    } else if (argc == 0) {
        return 0;
    }

    int i;
    for (i = 0; i < argc; ++i) {
        alpm_list_t *node, *pkgs = glob_cache(repo->pkgcache, argv[i]);

        if (!pkgs)
            warnx("didn't find entry: %s", argv[0]);

        for (node = pkgs; node; node = node->next) {
            alpm_pkg_meta_t *pkg = node->data;

            printf("removing %s %s\n", pkg->name, pkg->version);
            repo->pkgcache = _alpm_pkghash_remove(repo->pkgcache, pkg, NULL);
            if (cfg.clean >= 1)
                unlink_package(repo, pkg);
            alpm_pkg_free_metadata(pkg);
            repo->state = REPO_DIRTY;
        }
    }

    return repo_write(repo);
}
