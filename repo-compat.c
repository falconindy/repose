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

/* {{{ repo-add compat */
static void __attribute__((__noreturn__)) repo_add_usage(FILE *out)
{
    fprintf(out, "usage: %s [options] <path-to-db> [pkgs|deltas ...]\n", program_invocation_short_name);
    fputs("Options:\n"
        " -h, --help            display this help and exit\n"
        " -d, --delta           generate and add deltas for package updates\n"
        " -f, --files           update database's file list\n"
        " --nocolor             turn off color in output\n"
        " -q, --quiet           minimize output\n"
        " -s, --sign            sign database with GnuPG after update\n"
        " -k, --key=KEY         use the specified key to sign the database\n"
        " -v, --verify          verify the contents of the database\n", out);

    exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

static void parse_repo_add_args(int *argc, char **argv[])
{
    static const struct option opts[] = {
        { "help",    no_argument,       0, 'h' },
        { "delta",   no_argument,       0, 'd' },
        { "files",   no_argument,       0, 'f' },
        { "nocolor", no_argument,       0, 0x100 },
        { "quiet",   no_argument,       0, 'q' },
        { "sign",    no_argument,       0, 's' },
        { "key",     required_argument, 0, 'k' },
        { "verify",  no_argument,       0, 'v' },
        { 0, 0, 0, 0 }
    };

    while (true) {
        int opt = getopt_long(*argc, *argv, "hdfqsk:v", opts, NULL);
        if (opt == -1)
            break;

        switch (opt) {
        case 'h':
            repo_add_usage(stdout);
            break;
        case 'f':
            cfg.files = true;
            break;
        case 's':
            cfg.sign = true;
            break;
        case 'k':
            cfg.key = optarg;
            break;
        default:
            repo_add_usage(stderr);
        }
    }

    cfg.action = ACTION_UPDATE;

    *argc -= optind;
    *argv += optind;
}
/* }}} */

/* {{{ repo-remove compat */
static void __attribute__((__noreturn__)) repo_remove_usage(FILE *out)
{
    fprintf(out, "usage: %s [options] <path-to-db> [pkgs|deltas ...]\n", program_invocation_short_name);
    fputs("Options:\n"
        " -h, --help            display this help and exit\n"
        " --nocolor             turn off color in output\n"
        " -q, --quiet           minimize output\n"
        " -s, --sign            sign database with GnuPG after update\n"
        " -k, --key=KEY         use the specified key to sign the database\n"
        " -v, --verify          verify the contents of the database\n", out);

    exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

static void parse_repo_remove_args(int *argc, char **argv[])
{
    static const struct option opts[] = {
        { "help",    no_argument,       0, 'h' },
        { "nocolor", no_argument,       0, 0x100 },
        { "quiet",   no_argument,       0, 'q' },
        { "sign",    no_argument,       0, 's' },
        { "key",     required_argument, 0, 'k' },
        { "verify",  no_argument,       0, 'v' },
        { 0, 0, 0, 0 }
    };

    while (true) {
        int opt = getopt_long(*argc, *argv, "hdfqsk:v", opts, NULL);
        if (opt == -1)
            break;

        switch (opt) {
        case 'h':
            repo_remove_usage(stdout);
            break;
        case 's':
            cfg.sign = true;
            break;
        case 'k':
            cfg.key = optarg;
            break;
        default:
            repo_remove_usage(stderr);
        }
    }

    cfg.action = ACTION_REMOVE;

    *argc -= optind;
    *argv += optind;
}
/* }}} */
