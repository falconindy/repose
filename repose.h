#ifndef REPOMAN_H
#define REPOMAN_H

#include <stdbool.h>
#include "alpm/pkghash.h"

enum state {
    REPO_NEW,
    REPO_CLEAN,
    REPO_DIRTY
};

enum compress {
    COMPRESS_NONE,
    COMPRESS_GZIP,
    COMPRESS_BZIP2,
    COMPRESS_XZ,
    COMPRESS_COMPRESS
};

typedef struct file {
    char *file, *link_file;
    char *sig,  *link_sig;
} file_t;

typedef struct repo {
    size_t cachesize;
    alpm_pkghash_t *pkgcache;
    char *pool;
    char *root;
    file_t db;
    file_t files;
    enum state state;
    enum compress compression;
    int poolfd;
    int rootfd;
} repo_t;

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

typedef struct config {
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
} config_t;

extern config_t cfg;

int repo_database_update(repo_t *repo, int argc, char *argv[]);
int repo_database_verify(repo_t *repo);
int repo_database_remove(repo_t *repo, int argc, char *argv[]);
int repo_database_query(repo_t *repo, int argc, char *argv[]);

int repo_write(repo_t *repo);
int unlink_package(repo_t *repo, const alpm_pkg_meta_t *pkg);
alpm_pkghash_t *get_filecache(repo_t *repo, int argc, char *argv[]);

#endif
