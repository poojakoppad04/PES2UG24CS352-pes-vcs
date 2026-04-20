// object.c — Content-addressable object store

#include "pes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <openssl/evp.h>

// ─────────────────────────────────────────────
// PROVIDED FUNCTIONS (UNCHANGED)
// ─────────────────────────────────────────────

void hash_to_hex(const ObjectID *id, char *hex_out) {
    for (int i = 0; i < HASH_SIZE; i++) {
        sprintf(hex_out + i * 2, "%02x", id->hash[i]);
    }
    hex_out[HASH_HEX_SIZE] = '\0';
}

int hex_to_hash(const char *hex, ObjectID *id_out) {
    if (strlen(hex) < HASH_HEX_SIZE) return -1;
    for (int i = 0; i < HASH_SIZE; i++) {
        unsigned int byte;
        if (sscanf(hex + i * 2, "%2x", &byte) != 1) return -1;
        id_out->hash[i] = (uint8_t)byte;
    }
    return 0;
}

void compute_hash(const void *data, size_t len, ObjectID *id_out) {
    unsigned int hash_len;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(ctx, data, len);
    EVP_DigestFinal_ex(ctx, id_out->hash, &hash_len);
    EVP_MD_CTX_free(ctx);
}

void object_path(const ObjectID *id, char *path_out, size_t path_size) {
    char hex[HASH_HEX_SIZE + 1];
    hash_to_hex(id, hex);
    snprintf(path_out, path_size, "%s/%.2s/%s", OBJECTS_DIR, hex, hex + 2);
}

int object_exists(const ObjectID *id) {
    char path[512];
    object_path(id, path, sizeof(path));
    return access(path, F_OK) == 0;
}

// ─────────────────────────────────────────────
// OBJECT WRITE
// ─────────────────────────────────────────────

int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out) {

    const char *t =
        (type == OBJ_BLOB) ? "blob" :
        (type == OBJ_TREE) ? "tree" : "commit";

    char header[128];
    int hlen = snprintf(header, sizeof(header), "%s %zu", t, len) + 1;

    size_t total = hlen + len;

    char *buf = malloc(total);
    if (!buf) return -1;

    memcpy(buf, header, hlen);
    memcpy(buf + hlen, data, len);

    compute_hash(buf, total, id_out);

    if (object_exists(id_out)) {
        free(buf);
        return 0;
    }

    char path[512];
    object_path(id_out, path, sizeof(path));

    char dir[512];
    strcpy(dir, path);
    *strrchr(dir, '/') = '\0';

    mkdir(dir, 0755);

    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);

    int fd = open(tmp, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        free(buf);
        return -1;
    }

    write(fd, buf, total);
    fsync(fd);
    close(fd);

    rename(tmp, path);

    int dfd = open(dir, O_DIRECTORY);
    if (dfd >= 0) {
        fsync(dfd);
        close(dfd);
    }

    free(buf);
    return 0;
}

// ─────────────────────────────────────────────
// OBJECT READ
// ─────────────────────────────────────────────

int object_read(const ObjectID *id, ObjectType *type_out, void **data_out, size_t *len_out) {

    char path[512];
    object_path(id, path, sizeof(path));

    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    size_t sz = ftell(f);
    rewind(f);

    char *buf = malloc(sz);
    if (!buf) {
        fclose(f);
        return -1;
    }

    fread(buf, 1, sz, f);
    fclose(f);

    ObjectID check;
    compute_hash(buf, sz, &check);

    if (memcmp(check.hash, id->hash, HASH_SIZE) != 0) {
        free(buf);
        return -1;
    }

    char *sep = memchr(buf, '\0', sz);
    if (!sep) {
        free(buf);
        return -1;
    }

    if (strncmp(buf, "blob", 4) == 0)
        *type_out = OBJ_BLOB;
    else if (strncmp(buf, "tree", 4) == 0)
        *type_out = OBJ_TREE;
    else
        *type_out = OBJ_COMMIT;

    size_t header_len = sep - buf;
    size_t data_len = sz - header_len - 1;

    *data_out = malloc(data_len);
    memcpy(*data_out, sep + 1, data_len);
    *len_out = data_len;

    free(buf);
    return 0;
}
