#include "tree.h"
#include "index.h"
#include "object.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>

// ─── MODE CONSTANTS ───────────────────────
#define MODE_FILE  0100644
#define MODE_EXEC  0100755
#define MODE_DIR   0040000

// ─── PROVIDED FUNCTION ─────────────────────
uint32_t get_file_mode(const char *path) {
    struct stat st;
    if (lstat(path, &st) != 0) return 0;

    if (S_ISDIR(st.st_mode)) return MODE_DIR;
    if (st.st_mode & S_IXUSR) return MODE_EXEC;
    return MODE_FILE;
}

// ─── TREE SERIALIZE (unchanged logic) ─────
int tree_serialize(const Tree *tree, void **data_out, size_t *len_out) {
    size_t max_size = tree->count * 300;
    uint8_t *buf = malloc(max_size);
    if (!buf) return -1;

    Tree sorted = *tree;

    for (int i = 0; i < sorted.count - 1; i++) {
        for (int j = i + 1; j < sorted.count; j++) {
            if (strcmp(sorted.entries[i].name, sorted.entries[j].name) > 0) {
                TreeEntry tmp = sorted.entries[i];
                sorted.entries[i] = sorted.entries[j];
                sorted.entries[j] = tmp;
            }
        }
    }

    size_t off = 0;
    for (int i = 0; i < sorted.count; i++) {
        TreeEntry *e = &sorted.entries[i];

        int n = sprintf((char *)buf + off, "%o %s", e->mode, e->name);
        off += n + 1;

        memcpy(buf + off, e->hash.hash, HASH_SIZE);
        off += HASH_SIZE;
    }

    *data_out = buf;
    *len_out = off;
    return 0;
}

// ─── FIXED: TREE FROM INDEX ───────────────
int tree_from_index(ObjectID *id_out) {
    Index index;
    if (index_load(&index) != 0) return -1;

    Tree tree;
    tree.count = index.count;

    for (int i = 0; i < index.count; i++) {
        tree.entries[i].mode = index.entries[i].mode;
        strcpy(tree.entries[i].name, index.entries[i].path);
        tree.entries[i].hash = index.entries[i].hash;
    }

    void *data;
    size_t len;

    if (tree_serialize(&tree, &data, &len) != 0)
        return -1;

    if (object_write(OBJ_TREE, data, len, id_out) != 0) {
        free(data);
        return -1;
    }

    free(data);
    return 0;
}
