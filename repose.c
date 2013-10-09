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

#include "repose.h"

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

#define NOCOLOR     "\033[0m"
#define BOLD        "\033[1m"

#define BLACK       "\033[0;30m"
#define RED         "\033[0;31m"
#define GREEN       "\033[0;32m"
#define YELLOW      "\033[0;33m"
#define BLUE        "\033[0;34m"
#define MAGENTA     "\033[0;35m"
#define CYAN        "\033[0;36m"

#define BOLDBLACK   "\033[1;30m"
#define BOLDRED     "\033[1;31m"
#define BOLDGREEN   "\033[1;32m"
#define BOLDYELLOW  "\033[1;33m"
#define BOLDBLUE    "\033[1;34m"
#define BOLDMAGENTA "\033[1;35m"
#define BOLDCYAN    "\033[1;36m"

enum action {
    ACTION_VERIFY,
    ACTION_UPDATE,
    ACTION_REMOVE,
    ACTION_QUERY,
    INVALID_ACTION
};

typedef struct colstr {
    const char *colon;
    const char *warn;
    const char *error;
    const char *nocolor;
} colstr_t;

static struct {
    const char *key;
    const char *arch;
    char *pool;
    enum action action;

    short clean;
    bool info;
    bool sign;
    bool files;
    bool rebuild;

    bool color;
    colstr_t colstr;
} cfg = {
    .action = INVALID_ACTION,
    .colstr = {
        .colon   = ":: ",
        .warn    = "",
        .error   = "",
        .nocolor = ""
    }
};

static int colon_printf(const char *fmt, ...)
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

static void alloc_pkghash(repo_t *repo)
{
    DIR *dirp = opendir(repo->pool);
    struct dirent *entry;

    repo->cachesize = 0;
    while ((entry = readdir(dirp)) != NULL) {
        if (entry->d_type == DT_REG)
            repo->cachesize++;
    }

    repo->pkgcache = _alpm_pkghash_create(repo->cachesize);
    closedir(dirp);
}

static int populate_db_files(file_t *db, const char *name, const char *base, const char *ext)
{
    /* db full filename */
    if (asprintf(&db->file, "%s.%s%s", name, base, ext) < 0)
        return -1;
    /* db signature */
    if (asprintf(&db->sig, "%s.%s%s.sig", name, base, ext) < 0)
        return -1;
    /* link to db file */
    if (asprintf(&db->link_file, "%s.%s", name, base) < 0)
        return -1;
    /* link to signature */
    if (asprintf(&db->link_sig,  "%s.%s.sig", name, base) < 0)
        return -1;
    return 0;
}

static repo_t *repo_new(char *path)
{
    size_t len;
    char *div, *dot, *name;
    char *dbpath = NULL, *real = NULL;

    real = realpath(path, NULL);
    if (real) {
        dbpath = real;
    } else {
        if (errno != ENOENT)
            err(EXIT_FAILURE, "failed to find repo");
        dbpath = path;
    }

    repo_t *repo = malloc(sizeof(repo_t));
    *repo = (repo_t){
        .state = REPO_CLEAN,
        .pool  = cfg.pool
    };

    len = strlen(dbpath);
    div = memrchr(dbpath, '/', len);

    if (div) {
        dot = memchr(div, '.', len - (dbpath - div));
        name = strndup(div + 1, dot - div - 1);
        repo->root = strndup(dbpath, div - dbpath);
    } else {
        dot = memchr(dbpath, '.', len);
        name = strndup(dbpath, dot - dbpath);
        repo->root = get_current_dir_name();
    }
    if (cfg.rebuild) {
        repo->state = REPO_NEW;
    } else if (faccessat(repo->rootfd, repo->db.file, F_OK, 0) < 0) {
        if (errno != ENOENT) {
            err(EXIT_FAILURE, "couldn't access database %s", repo->db.file);
        }
        repo->state = REPO_NEW;
    } else {
        colon_printf("Reading existing database into memory...\n");

        /* load the databases if possible */
        load_database(repo, &repo->db);
        if (cfg.files) {
            load_database(repo, &repo->files);
        }

        repo_database_reduce(repo);
    }

    free(name);
    free(real);
    return repo;
}

int repo_write(repo_t *repo)
{
    switch (repo->state) {
    case REPO_NEW:
        printf("repo empty!\n");
        return 1;
    case REPO_CLEAN:
        printf("repo does not need updating\n");
        break;
    case REPO_DIRTY:
        colon_printf("Writing databases to disk...\n");
        printf("writing %s...\n", repo->db.file);
        compile_database(repo, &repo->db, DB_DESC | DB_DEPENDS);
        if (cfg.sign) {
            sign_database(repo, &repo->db, cfg.key);
        }

        if (cfg.files) {
            printf("writing %s...\n", repo->files.file);
            compile_database(repo, &repo->files, DB_FILES);
            if (cfg.sign) {
                sign_database(repo, &repo->db, cfg.key);
            }
        }
        printf("repo updated successfully\n");
        break;
    default:
        break;
    }

    return 0;
}

int unlink_package(repo_t *repo, const alpm_pkg_meta_t *pkg)
{
    if (faccessat(repo->poolfd, pkg->filename, F_OK, 0) < 0) {
        if (errno != ENOENT) {
            err(EXIT_FAILURE, "couldn't access package %s to unlink", pkg->filename);
        }
        return 0;
    }

    printf("deleting %s %s\n", pkg->name, pkg->version);
    unlinkat(repo->poolfd, pkg->filename, 0);
    unlinkat(repo->poolfd, pkg->signame, 0);
    return 0;
}

static alpm_pkg_meta_t *load_package(repo_t *repo, const char *filename)
{
    alpm_pkg_meta_t *pkg = NULL;
    int sigfd, pkgfd = openat(repo->poolfd, filename, O_RDONLY);
    if (pkgfd < 0) {
        err(EXIT_FAILURE, "failed to open %s", filename);
    }

    alpm_pkg_load_metadata(pkgfd, &pkg);

    if (strcmp(pkg->arch, cfg.arch) != 0 && strcmp(pkg->arch, "any") != 0) {
        alpm_pkg_free_metadata(pkg);
        return NULL;
    }
    pkg->filename = strdup(filename);

    safe_asprintf(&pkg->signame, "%s.sig", filename);

    sigfd = openat(repo->poolfd, pkg->signame, O_RDONLY);
    if (sigfd < 0) {
        if (errno != ENOENT)
            err(EXIT_FAILURE, "failed to open %s", pkg->signame);
    } else {
        read_pkg_signature(sigfd, pkg);
    }

    return pkg;
}

static inline alpm_pkghash_t *pkgcache_add(alpm_pkghash_t *cache, alpm_pkg_meta_t *pkg)
{
    alpm_pkg_meta_t *old = _alpm_pkghash_find(cache, pkg->name);
    int vercmp = old == NULL ? 0 : alpm_pkg_vercmp(pkg->version, old->version);

    if (vercmp == 0 || vercmp == 1) {
        if (old) {
            cache = _alpm_pkghash_remove(cache, old, NULL);
            /* if (cfg.clean >= 2) */
            /*     unlink_package(repo, old); */
            alpm_pkg_free_metadata(old);
        }
        return _alpm_pkghash_add(cache, pkg);
    } else {
        /* if (cfg.clean >= 2) */
        /*     unlink_package(repo, pkg); */
        alpm_pkg_free_metadata(pkg);
        return cache;
    }
}

static inline alpm_pkghash_t *pkgcache_from_file(alpm_pkghash_t *cache, repo_t *repo, const char *filepath)
{
    alpm_pkg_meta_t *pkg;
    const char *basename = strrchr(filepath, '/');

    if (basename) {
        if (memcmp(filepath, repo->pool, basename - filepath) != 0) {
            warnx("%s is not in the same path as the database", filepath);
            return cache;
        }
    }

    pkg = load_package(repo, filepath);
    if (!pkg)
        return cache;

    return pkgcache_add(cache, pkg);
}

static alpm_pkghash_t *scan_for_targets(alpm_pkghash_t *cache, repo_t *repo, alpm_list_t *targets)
{
    DIR *dir = opendir(repo->pool);
    const struct dirent *dp;

    while ((dp = readdir(dir))) {
        alpm_pkg_meta_t *pkg = NULL;

        if (!(dp->d_type & DT_REG))
            continue;

        if (fnmatch("*.pkg.tar*", dp->d_name, FNM_CASEFOLD) != 0 ||
            fnmatch("*.sig",      dp->d_name, FNM_CASEFOLD) == 0)
            continue;

        pkg = load_package(repo, dp->d_name);
        if (!pkg) {
            /* warnx("failed to open %s", dp->d_name); */
            continue;
        }

        if (targets == NULL || match_targets(pkg, targets))
            cache = pkgcache_add(cache, pkg);
    }

    return cache;
}

alpm_pkghash_t *get_filecache(repo_t *repo, int argc, char *argv[])
{
    alpm_pkghash_t *cache = _alpm_pkghash_create(argc ? argc : repo->cachesize);

    if (argc > 0) {
        alpm_list_t *targets = NULL;
        int i;

        for (i = 0; i < argc; ++i) {
            char *target = argv[i];

            if (target[0] == '/' || target[0] == '.') {
                cache = pkgcache_from_file(cache, repo, target);
            } else {
                targets = alpm_list_add(targets, target);
            }
        }

        if (targets) {
            cache = scan_for_targets(cache, repo, targets);
        }
    } else {
        cache = scan_for_targets(cache, repo, NULL);
    }

    return cache;
}

/* {{{ COMPAT! */
#include "repo-compat.c"
/* }}} */

static void __attribute__((__noreturn__)) elephant(void)
{
    static const unsigned char big_elephant[] =
        "ICAgICBfXwogICAgJy4gXAogICAgICctIFwKICAgICAgLyAvXyAgICAgICAgIC4tLS0uCiAgICAg"
        "LyB8IFxcLC5cLy0tLi8vICAgICkKICAgICB8ICBcLy8gICAgICAgICkvICAvCiAgICAgIFwgICcg"
        "XiBeICAgIC8gICAgKV9fX18uLS0tLS4uICA2CiAgICAgICAnLl9fX18uICAgIC5fX18vICAgICAg"
        "ICAgICAgXC5fKQogICAgICAgICAgLlwvLiAgICAgICAgICAgICAgICAgICAgICApCiAgICAgICAg"
        "ICAgJ1wgICAgICAgICAgICAgICAgICAgICAgIC8KICAgICAgICAgICBfLyBcLyAgICApLiAgICAg"
        "ICAgKSAgICAoCiAgICAgICAgICAvIyAgLiEgICAgfCAgICAgICAgL1wgICAgLwogICAgICAgICAg"
        "XCAgQy8vICMgIC8nLS0tLS0nJy8gIyAgLwogICAgICAgLiAgICdDLyB8ICAgIHwgICAgfCAgIHwg"
        "ICAgfG1yZiAgLAogICAgICAgXCksIC4uIC4nT09PLScuIC4uJ09PTydPT08tJy4gLi5cKCw=";

    static const unsigned char small_elephant[] =
        "ICAgIF8gICAgXwogICAvIFxfXy8gXF9fX19fCiAgLyAgLyAgXCAgXCAgICBgXAogICkgIFwnJy8g"
        "ICggICAgIHxcCiAgYFxfXykvX18vJ19cICAvIGAKICAgICAvL198X3x+fF98X3wKICAgICBeIiIn"
        "IicgIiInIic=";

    int ret = 0;
    unsigned char *data = NULL;

    srand(time(NULL));
    if (rand() % 2 == 0)
        ret = base64_decode(&data, big_elephant, sizeof(big_elephant) - 1);
    else
        ret = base64_decode(&data, small_elephant, sizeof(small_elephant) - 1);

    if (ret > 0) {
        puts((char *)data);
        exit(EXIT_SUCCESS);
    }

    exit(ret);
}

static void __attribute__((__noreturn__)) usage(FILE *out)
{
    fprintf(out, "usage: %s [options] <path-to-db> [pkgs|deltas ...]\n", program_invocation_short_name);
    fputs("Options:\n"
        " -h, --help            display this help and exit\n"
        " -v, --version         display version\n"
        " -U, --update          update the database\n"
        " -R, --remove          remove an entry\n"
        " -Q, --query           query the database\n"
        " -V, --verify          verify the contents of the database\n"
        " -i, --info            show package info\n"
        " -p, --pool=POOL       set the pool to find packages in\n"
        " -c, --clean           remove stuff\n"
        " -f, --files           generate a complementing files database\n"
        " -a, --arch=ARCH       the primary architecture of the database\n"
        " -s, --sign            sign database(s) with GnuPG after update\n"
        " -k, --key=KEY         use the specified key to sign the database\n"
        "     --color=MODE      enable colour support\n"
        "     --rebuild         force rebuild the repo\n", out);

    exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

static void enable_colors(bool on)
{
    if (!on)
        return;

    cfg.colstr = (colstr_t){
        .colon   = BOLDBLUE "::" NOCOLOR BOLD " ",
        .warn    = BOLDYELLOW,
        .error   = BOLDRED,
        .nocolor = NOCOLOR
    };
}

static void parse_repose_args(int *argc, char **argv[])
{
    static const struct option opts[] = {
        { "help",     no_argument,       0, 'h' },
        { "version",  no_argument,       0, 'v' },
        { "verify",   no_argument,       0, 'V' },
        { "update",   no_argument,       0, 'U' },
        { "remove",   no_argument,       0, 'R' },
        { "query",    no_argument,       0, 'Q' },
        { "info",     no_argument,       0, 'i' },
        { "pool",     required_argument, 0, 'p' },
        { "clean",    no_argument,       0, 'c' },
        { "files",    no_argument,       0, 'f' },
        { "arch",     required_argument, 0, 'a' },
        { "sign",     no_argument,       0, 's' },
        { "key",      required_argument, 0, 'k' },
        { "color",    required_argument, 0, 0x100 },
        { "rebuild",  no_argument,       0, 0x101 },
        { "elephant", no_argument,       0, 0x102 },
        { 0, 0, 0, 0 }
    };

    cfg.color = isatty(fileno(stdout)) ? true : false;

    while (true) {
        int opt = getopt_long(*argc, *argv, "hvURQVicfsa:k:", opts, NULL);
        if (opt == -1)
            break;

        switch (opt) {
        case 'h':
            usage(stdout);
            break;
        case 'v':
            printf("%s %s\n", program_invocation_short_name, REPOSE_VERSION);
            exit(EXIT_SUCCESS);
        case 'V':
            cfg.action = ACTION_VERIFY;
            break;
        case 'U':
            cfg.action = ACTION_UPDATE;
            break;
        case 'R':
            cfg.action = ACTION_REMOVE;
            break;
        case 'Q':
            cfg.action = ACTION_QUERY;
            break;
        case 'i':
            cfg.info = true;
            break;
        case 'p':
            cfg.pool = optarg;
            break;
        case 'c':
            ++cfg.clean;
            break;
        case 'f':
            cfg.files = true;
            break;
        case 's':
            cfg.sign = true;
            break;
        case 'a':
            cfg.arch = optarg;
            break;
        case 'k':
            cfg.key = optarg;
            break;
        case 0x100:
            if (strcmp("never", optarg) == 0)
                cfg.color = false;
            else if (strcmp("always", optarg) == 0)
                cfg.color = true;
            else if (strcmp("auto", optarg) != 0)
                errx(EXIT_FAILURE, "invalid argument '%s' for --color", optarg);
            break;
        case 0x101:
            cfg.rebuild = true;
            break;
        case 0x102:
            elephant();
            break;
        default:
            usage(stderr);
        }
    }

    *argc -= optind;
    *argv += optind;
}

int main(int argc, char *argv[])
{
    struct utsname buf;
    repo_t *repo;
    int rc = 0;

    if (strcmp(program_invocation_short_name, "repo-add") == 0) {
        parse_repo_add_args(&argc, &argv);
    } else if (strcmp(program_invocation_short_name, "repo-remove") == 0) {
        parse_repo_remove_args(&argc, &argv);
    } else if (strcmp(program_invocation_short_name, "repo-elephant") == 0) {
        elephant();
    } else {
        parse_repose_args(&argc, &argv);
    }

    if (cfg.action == INVALID_ACTION)
        errx(EXIT_FAILURE, "no operation specified");
    if (argc == 0)
        errx(EXIT_FAILURE, "not enough arguments");

    /* enable colors if necessary */
    enable_colors(cfg.color);

    /* if arch isn't set, detect it */
    if (!cfg.arch) {
        uname(&buf);
        cfg.arch = buf.machine;
    }

    repo = repo_new(argv[0]);
    if (!repo)
        return 1;

    switch (cfg.action) {
    case ACTION_VERIFY:
        rc = repo_database_verify(repo);
        break;
    case ACTION_UPDATE:
        rc = repo_database_update(repo, argc - 1, argv + 1);
        break;
    case ACTION_REMOVE:
        rc = repo_database_remove(repo, argc - 1, argv + 1);
        break;
    case ACTION_QUERY:
        rc = repo_database_query(repo, argc - 1, argv + 1);
        break;
    default:
        break;
    };

    return rc;
}
#include "repose.h"

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

#define NOCOLOR     "\033[0m"
#define BOLD        "\033[1m"

#define BLACK       "\033[0;30m"
#define RED         "\033[0;31m"
#define GREEN       "\033[0;32m"
#define YELLOW      "\033[0;33m"
#define BLUE        "\033[0;34m"
#define MAGENTA     "\033[0;35m"
#define CYAN        "\033[0;36m"

#define BOLDBLACK   "\033[1;30m"
#define BOLDRED     "\033[1;31m"
#define BOLDGREEN   "\033[1;32m"
#define BOLDYELLOW  "\033[1;33m"
#define BOLDBLUE    "\033[1;34m"
#define BOLDMAGENTA "\033[1;35m"
#define BOLDCYAN    "\033[1;36m"

config_t cfg = {
    .action = INVALID_ACTION,
    .colstr = {
        .colon   = ":: ",
        .warn    = "",
        .error   = "",
        .nocolor = ""
    }
};

static void alloc_pkghash(repo_t *repo)
{
    DIR *dirp = opendir(repo->pool);
    struct dirent *entry;

    repo->cachesize = 0;
    while ((entry = readdir(dirp)) != NULL) {
        if (entry->d_type == DT_REG)
            repo->cachesize++;
    }

    repo->pkgcache = _alpm_pkghash_create(repo->cachesize);
    closedir(dirp);
}

static void populate_db_files(file_t *db, const char *name, const char *base, const char *ext)
{
    /* precompile some filepaths:
     *  - db full filename
     *  - link to db file
     *  - db signature
     *  - link to signature
     */

    safe_asprintf(&db->file, "%s.%s%s", name, base, ext);
    safe_asprintf(&db->sig, "%s.%s%s.sig", name, base, ext);
    safe_asprintf(&db->link_file, "%s.%s", name, base);
    safe_asprintf(&db->link_sig, "%s.%s.sig", name, base);
}

static repo_t *repo_new(char *path)
{
    size_t len;
    char *div, *dot, *name;
    char *dbpath = NULL, *real = NULL;

    real = realpath(path, NULL);
    if (real) {
        dbpath = real;
    } else {
        if (errno != ENOENT)
            err(EXIT_FAILURE, "failed to find repo");
        dbpath = path;
    }

    repo_t *repo = malloc(sizeof(repo_t));
    *repo = (repo_t){
        .state = REPO_CLEAN,
        .pool  = cfg.pool
    };

    len = strlen(dbpath);
    div = memrchr(dbpath, '/', len);

    if (div) {
        dot = memchr(div, '.', len - (dbpath - div));
        name = strndup(div + 1, dot - div - 1);
        repo->root = strndup(dbpath, div - dbpath);
    } else {
        dot = memchr(dbpath, '.', len);
        name = strndup(dbpath, dot - dbpath);
        repo->root = get_current_dir_name();
    }

    if (!dot) {
        errx(EXIT_FAILURE, "no file extension");
    } else if (strcmp(dot, ".db") == 0) {
        repo->compression = COMPRESS_GZIP;
    } else if (strcmp(dot, ".db.tar") == 0) {
        repo->compression = COMPRESS_NONE;
    } else if (strcmp(dot, ".db.tar.gz") == 0) {
        repo->compression = COMPRESS_GZIP;
    } else if (strcmp(dot, ".db.tar.bz2") == 0) {
        repo->compression = COMPRESS_BZIP2;
    } else if (strcmp(dot, ".db.tar.xz") == 0) {
        repo->compression = COMPRESS_XZ;
    } else if (strcmp(dot, ".db.tar.Z") == 0) {
        repo->compression = COMPRESS_COMPRESS;
    } else {
        errx(EXIT_FAILURE, "%s invalid repo type", dot);
    }

    /* open the directory so we can use openat later */
    repo->rootfd = open(repo->root, O_RDONLY);
    if (repo->poolfd < 0)
        err(EXIT_FAILURE, "cannot access repo root %s", repo->root);

    if (!repo->pool) {
        repo->pool = repo->root;
        repo->poolfd = repo->rootfd;
    } else {
        repo->poolfd = open(repo->pool, O_RDONLY);
        if (repo->poolfd < 0)
            err(EXIT_FAILURE, "cannot access repo pool %s", repo->pool);
    }

    alloc_pkghash(repo);

    /* skip '.db' */
    dot += 3;
    if (*dot == '\0') {
        dot = ".tar.gz";
    }

    populate_db_files(&repo->db, name, "db", dot);
    if (cfg.files)
        populate_db_files(&repo->files, name, "files", dot);

    if (cfg.rebuild) {
        repo->state = REPO_NEW;
    } else if (faccessat(repo->rootfd, repo->db.file, F_OK, 0) < 0) {
        if (errno != ENOENT) {
            err(EXIT_FAILURE, "couldn't access database %s", repo->db.file);
        }
        repo->state = REPO_NEW;
    } else {
        colon_printf("Reading existing database into memory...\n");

        /* load the databases if possible */
        load_database(repo, &repo->db);
        if (cfg.files) {
            load_database(repo, &repo->files);
        }

        repo_database_reduce(repo);
    }

    free(name);
    free(real);
    return repo;
}

int repo_write(repo_t *repo)
{
    switch (repo->state) {
    case REPO_NEW:
        printf("repo empty!\n");
        return 1;
    case REPO_CLEAN:
        printf("repo does not need updating\n");
        break;
    case REPO_DIRTY:
        colon_printf("Writing databases to disk...\n");
        printf("writing %s...\n", repo->db.file);
        compile_database(repo, &repo->db, DB_DESC | DB_DEPENDS);
        if (cfg.sign) {
            sign_database(repo, &repo->db, cfg.key);
        }

        if (cfg.files) {
            printf("writing %s...\n", repo->files.file);
            compile_database(repo, &repo->files, DB_FILES);
            if (cfg.sign) {
                sign_database(repo, &repo->db, cfg.key);
            }
        }
        printf("repo updated successfully\n");
        break;
    default:
        break;
    }

    return 0;
}

int unlink_package(repo_t *repo, const alpm_pkg_meta_t *pkg)
{
    if (faccessat(repo->poolfd, pkg->filename, F_OK, 0) < 0) {
        if (errno != ENOENT) {
            err(EXIT_FAILURE, "couldn't access package %s to unlink", pkg->filename);
        }
        return 0;
    }

    printf("deleting %s %s\n", pkg->name, pkg->version);
    unlinkat(repo->poolfd, pkg->filename, 0);
    unlinkat(repo->poolfd, pkg->signame, 0);
    return 0;
}

static alpm_pkg_meta_t *load_package(repo_t *repo, const char *filename)
{
    alpm_pkg_meta_t *pkg = NULL;
    int sigfd, pkgfd = openat(repo->poolfd, filename, O_RDONLY);
    if (pkgfd < 0) {
        err(EXIT_FAILURE, "failed to open %s", filename);
    }

    alpm_pkg_load_metadata(pkgfd, &pkg);

    if (strcmp(pkg->arch, cfg.arch) != 0 && strcmp(pkg->arch, "any") != 0) {
        alpm_pkg_free_metadata(pkg);
        return NULL;
    }
    pkg->filename = strdup(filename);

    safe_asprintf(&pkg->signame, "%s.sig", filename);

    sigfd = openat(repo->poolfd, pkg->signame, O_RDONLY);
    if (sigfd < 0) {
        if (errno != ENOENT)
            err(EXIT_FAILURE, "failed to open %s", pkg->signame);
    } else {
        read_pkg_signature(sigfd, pkg);
    }

    return pkg;
}

static inline alpm_pkghash_t *pkgcache_add(alpm_pkghash_t *cache, alpm_pkg_meta_t *pkg)
{
    alpm_pkg_meta_t *old = _alpm_pkghash_find(cache, pkg->name);
    int vercmp = old == NULL ? 0 : alpm_pkg_vercmp(pkg->version, old->version);

    if (vercmp == 0 || vercmp == 1) {
        if (old) {
            cache = _alpm_pkghash_remove(cache, old, NULL);
            /* if (cfg.clean >= 2) */
            /*     unlink_package(repo, old); */
            alpm_pkg_free_metadata(old);
        }
        return _alpm_pkghash_add(cache, pkg);
    } else {
        /* if (cfg.clean >= 2) */
        /*     unlink_package(repo, pkg); */
        alpm_pkg_free_metadata(pkg);
        return cache;
    }
}

static inline alpm_pkghash_t *pkgcache_from_file(alpm_pkghash_t *cache, repo_t *repo, const char *filepath)
{
    alpm_pkg_meta_t *pkg;
    const char *basename = strrchr(filepath, '/');

    if (basename) {
        if (memcmp(filepath, repo->pool, basename - filepath) != 0) {
            warnx("%s is not in the same path as the database", filepath);
            return cache;
        }
    }

    pkg = load_package(repo, filepath);
    if (!pkg)
        return cache;

    return pkgcache_add(cache, pkg);
}

        rc = repo_database_update(repo, argc - 1, argv + 1);
        break;
    case ACTION_REMOVE:
        rc = repo_database_remove(repo, argc - 1, argv + 1);
        break;
    case ACTION_QUERY:
        rc = repo_database_query(repo, argc - 1, argv + 1);
        break;
    default:
        break;
    };

    return rc;
}
