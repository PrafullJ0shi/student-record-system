/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Prafull Joshi
 */
#include "internal.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void bb_init(ByteBuf *buf)
{
    buf->data = NULL;
    buf->len = 0;
    buf->cap = 0;
}

void bb_free(ByteBuf *buf)
{
    free(buf->data);
    bb_init(buf);
}

static SrmsStatus bb_reserve(ByteBuf *buf, size_t extra)
{
    size_t need;
    size_t cap;
    char *next;

    if (extra > (size_t)-1 - buf->len - 1u) {
        srms_set_detail("The register image is too large.");
        return SRMS_ERR_RANGE;
    }
    need = buf->len + extra + 1u;
    if (need <= buf->cap) {
        return SRMS_OK;
    }
    cap = buf->cap == 0 ? 256u : buf->cap;
    while (cap < need) {
        if (cap > ((size_t)-1) / 2u) {
            cap = need;
            break;
        }
        cap *= 2u;
    }
    next = realloc(buf->data, cap);
    if (next == NULL) {
        srms_set_detail("Out of memory while building the register.");
        return SRMS_ERR_NOMEM;
    }
    buf->data = next;
    buf->cap = cap;
    return SRMS_OK;
}

SrmsStatus bb_append(ByteBuf *buf, const char *data, size_t n)
{
    SrmsStatus status = bb_reserve(buf, n);
    if (status != SRMS_OK) {
        return status;
    }
    if (n > 0) {
        memcpy(buf->data + buf->len, data, n);
        buf->len += n;
    }
    buf->data[buf->len] = '\0';
    return SRMS_OK;
}

SrmsStatus bb_printf(ByteBuf *buf, const char *fmt, ...)
{
    char stack[1024];
    va_list ap;
    int wrote;

    va_start(ap, fmt);
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
    wrote = vsnprintf(stack, sizeof stack, fmt, ap);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    va_end(ap);
    if (wrote < 0) {
        srms_set_detail("Could not format a register line.");
        return SRMS_ERR_IO;
    }
    if ((size_t)wrote >= sizeof stack) {
        srms_set_detail("A formatted line exceeded 1023 characters.");
        return SRMS_ERR_RANGE;
    }
    return bb_append(buf, stack, (size_t)wrote);
}

static SrmsStatus fsync_parent(const char *path)
{
    char dir[4096];
    const char *slash;
    int fd;

    slash = strrchr(path, '/');
    if (slash == NULL) {
        memcpy(dir, ".", 2);
    } else if (slash == path) {
        memcpy(dir, "/", 2);
    } else {
        size_t n = (size_t)(slash - path);
        if (n >= sizeof dir) {
            return SRMS_ERR_RANGE;
        }
        memcpy(dir, path, n);
        dir[n] = '\0';
    }
    fd = open(dir, O_RDONLY | O_DIRECTORY);
    if (fd < 0) {
        return SRMS_OK;
    }
    if (fsync(fd) != 0) {
        close(fd);
        return SRMS_ERR_IO;
    }
    if (close(fd) != 0) {
        return SRMS_ERR_IO;
    }
    return SRMS_OK;
}

SrmsStatus atomic_write(const char *path, const void *data, size_t len)
{
    char tmp[4096];
    int fd;
    const char *bytes;
    size_t off;
    size_t path_len;

    path_len = strlen(path);
    if (path_len == 0 || path_len + 5u >= sizeof tmp) {
        srms_set_detail("The path is empty or too long.");
        return SRMS_ERR_RANGE;
    }
    memcpy(tmp, path, path_len);
    memcpy(tmp + path_len, ".tmp", 5);

    fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        srms_set_detail("Could not open %s for writing.", tmp);
        return SRMS_ERR_IO;
    }

    bytes = data;
    off = 0;
    while (off < len) {
        ssize_t wrote = write(fd, bytes + off, len - off);
        if (wrote < 0) {
            if (errno == EINTR) {
                continue;
            }
            close(fd);
            unlink(tmp);
            srms_set_detail("Could not write %s.", tmp);
            return SRMS_ERR_IO;
        }
        off += (size_t)wrote;
    }
    if (fsync(fd) != 0) {
        close(fd);
        unlink(tmp);
        srms_set_detail("Could not flush %s.", tmp);
        return SRMS_ERR_IO;
    }
    if (close(fd) != 0) {
        unlink(tmp);
        srms_set_detail("Could not close %s.", tmp);
        return SRMS_ERR_IO;
    }
    if (rename(tmp, path) != 0) {
        unlink(tmp);
        srms_set_detail("Could not replace %s.", path);
        return SRMS_ERR_IO;
    }
    if (fsync_parent(path) != SRMS_OK) {
        srms_set_detail("Wrote %s, but the directory could not be flushed.", path);
        return SRMS_ERR_IO;
    }
    return SRMS_OK;
}
