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

#ifndef DATABASE_H
#define DATABASE_H

#include "repose.h"
#include "alpm/pkghash.h"

enum contents {
    DB_DESC    = 1,
    DB_DEPENDS = 1 << 2,
    DB_FILES   = 1 << 3
};

void compile_database(repo_t *repo, file_t *db, int contents);
int load_database(repo_t *repo, file_t *db);
void sign_database(repo_t *repo, file_t *db, const char *key);
void repo_database_reduce(repo_t *repo);

#endif
