#include "pes.h"
#include "tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_BUF 65536

// helper: compare by name for sorting
static int cmp(const void *a, const void *b) {
    TreeEntry *ea = (TreeEntry *)a;
    TreeEntry *eb = (TreeEntry *)b;
    return strcmp(ea->name, eb->name);
}

/**
 * Serialize Tree → buffer
 */
int tree_serialize(const Tree *tree, void **buffer, size_t *len) {
    if (!tree || !buffer || !len) return -1;

    Tree copy = *tree;  // local copy so we can sort
    qsort(copy.entries, copy.count, sizeof(TreeEntry), cmp);

    char *buf = malloc(MAX_BUF);
    if (!buf) return -1;

    size_t offset = 0;

    for (int i = 0; i < copy.count; i++) {
        char line[512];

        snprintf(line, sizeof(line), "%06o %s %s\n",
                 copy.entries[i].mode,
                 copy.entries[i].hash.hex,
                 copy.entries[i].name);

        size_t l = strlen(line);
        memcpy(buf + offset, line, l);
        offset += l;
    }

    *buffer = buf;
    *len = offset;
    return 0;
}

/**
 * Parse buffer → Tree
 */
int tree_parse(const void *buffer, size_t len, Tree *tree) {
    if (!buffer || !tree) return -1;

    tree->count = 0;

    const char *buf = (const char *)buffer;
    size_t i = 0;

    while (i < len && tree->count < MAX_ENTRIES) {
        unsigned int mode;
        char hash[HASH_STRING_SIZE];
        char name[256];

        int n = sscanf(buf + i, "%o %64s %255s", &mode, hash, name);
        if (n != 3) break;

        TreeEntry *e = &tree->entries[tree->count++];

        e->mode = mode;
        strncpy(e->name, name, sizeof(e->name));

        // convert hex string → hash struct
        for (int j = 0; j < HASH_SIZE; j++) {
            sscanf(&hash[j * 2], "%2hhx", &e->hash.hash[j]);
        }

        // move to next line
        while (i < len && buf[i] != '\n') i++;
        i++;
    }

    return 0;
}
