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

#include "buffer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline size_t next_power(size_t x)
{
    return 1 << (32 - __builtin_clz(x - 1));
}

/* Extend the buffer in buf by at least len bytes.  Note len should
 * include the space required for the NUL terminator */
static inline void buffer_extendby(buffer_t *buf, size_t len)
{
    char* data;

    len += buf->len;
    if (len <= buf->buflen)
        return;
    if (!buf->buflen)
        buf->buflen = 32;

    buf->buflen = next_power(len);
    /* printf("--- expand: %zd\n", buf->buflen); */
    data = realloc(buf->data, buf->buflen);
    buf->data = data;
}

void buffer_init(buffer_t *buf, size_t reserve)
{
    buf->data = NULL;
    buf->len = 0;
    buf->buflen = 0;

    if (reserve)
        buffer_extendby(buf, reserve);
}

void buffer_free(buffer_t *buf)
{
    if (buf->data)
        free(buf->data);
}

void buffer_clear(buffer_t *buf)
{
    buf->len = 0;
    if (buf->data)
        buf->data[buf->len] = '\0';
}

void buffer_putc(buffer_t *buf, const char c)
{
    buffer_extendby(buf, 2);
    buf->data[buf->len++] = c;
    buf->data[buf->len] = '\0';
}

void buffer_printf(buffer_t *buf, const char *fmt, ...)
{
    size_t len = buf->buflen - buf->len;
    char *p = &buf->data[buf->len];

    va_list ap;
    va_start(ap, fmt);
    size_t rc = vsnprintf(p, len, fmt, ap);
    va_end(ap);

    if (rc >= len) {
        buffer_extendby(buf, rc + 1);
        p = &buf->data[buf->len];

        va_start(ap, fmt);
        rc = vsnprintf(p, rc + 1, fmt, ap);
        va_end(ap);
    }
    buf->len += rc;
}
