#include "commit.h"
#include "tree.h"
#include "index.h"
#include "object.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ─── CREATE COMMIT ───────────────────────
int commit_create(const char *message, ObjectID *commit_id_out) {
    Commit c;
    memset(&c, 0, sizeof(c));

    // 1. tree from index
    if (tree_from_index(&c.tree) != 0)
        return -1;

    // 2. parent
    if (head_read(&c.parent) == 0)
        c.has_parent = 1;
    else
        c.has_parent = 0;

    // 3. author
    const char *author = getenv("PES_AUTHOR");
    if (!author) author = "unknown";
    strncpy(c.author, author, sizeof(c.author));

    // 4. timestamp
    c.timestamp = (uint64_t)time(NULL);

    // 5. message
    strncpy(c.message, message, sizeof(c.message));

    // 6. serialize
    void *data;
    size_t len;

    if (commit_serialize(&c, &data, &len) != 0)
        return -1;

    // 7. write object
    ObjectID id;
    if (object_write(OBJ_COMMIT, data, len, &id) != 0) {
        free(data);
        return -1;
    }

    free(data);

    // 8. update HEAD
    if (head_update(&id) != 0)
        return -1;

    if (commit_id_out)
        *commit_id_out = id;

    return 0;
}

// ─── WALK COMMITS ───────────────────────
int commit_walk(commit_walk_fn callback, void *ctx) {
    ObjectID id;

    if (head_read(&id) != 0)
        return -1;

    while (1) {
        ObjectType type;
        void *data;
        size_t len;

        if (object_read(&id, &type, &data, &len) != 0)
            return -1;

        Commit c;
        if (commit_parse(data, len, &c) != 0) {
            free(data);
            return -1;
        }

        free(data);

        callback(&id, &c, ctx);

        if (!c.has_parent)
            break;

        id = c.parent;
    }

    return 0;
}
