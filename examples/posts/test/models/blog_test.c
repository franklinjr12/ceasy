#include "../../src/models/comment.h"
#include "../../src/models/post.h"
#include "../../src/models/user.h"
#include "../../src/queries/blog_queries.h"

#include <assert.h>

static void create_schema(Database *database) {
    assert(database_execute_sql(
        database,
        sv("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT NOT NULL, "
           "email TEXT NOT NULL COLLATE NOCASE UNIQUE, password_digest TEXT "
           "NOT NULL, bio TEXT NOT NULL, is_admin INTEGER NOT NULL DEFAULT 0, "
           "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, "
           "updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP); "
           "CREATE TABLE posts (id INTEGER PRIMARY KEY, user_id INTEGER NOT "
           "NULL REFERENCES users(id) ON DELETE CASCADE, title TEXT NOT NULL, "
           "summary TEXT NOT NULL, content TEXT NOT NULL, published INTEGER "
           "NOT NULL, published_at TEXT NOT NULL, created_at TEXT NOT NULL "
           "DEFAULT CURRENT_TIMESTAMP, updated_at TEXT NOT NULL DEFAULT "
           "CURRENT_TIMESTAMP); "
           "CREATE TABLE comments (id INTEGER PRIMARY KEY, post_id INTEGER "
           "NOT NULL REFERENCES posts(id) ON DELETE CASCADE, user_id INTEGER "
           "NOT NULL REFERENCES users(id) ON DELETE CASCADE, content TEXT "
           "NOT NULL, created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, "
           "updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)")));
}

static User make_user(Arena *arena, const char *name, const char *email,
                      bool admin) {
    return (User){.name = string_from_in(arena, stringv_from_cstr(name)),
                  .email = string_from_in(arena, stringv_from_cstr(email)),
                  .password_digest = string_from_in(arena, sv("digest")),
                  .bio = string_from_in(arena, sv("")),
                  .is_admin = admin};
}

static Post make_post(Arena *arena, int64_t user_id, const char *title,
                      bool published) {
    return (Post){.user_id = user_id,
                  .title = string_from_in(arena, stringv_from_cstr(title)),
                  .summary = string_from_in(arena, sv("A useful summary.")),
                  .content = string_from_in(arena, sv("The article body.")),
                  .published = published,
                  .published_at = string_from_in(arena, sv("2026-01-01"))};
}

int main(void) {
    Arena arena;
    Database database;
    Context context = {0};
    User alice;
    User bob;
    User *loaded = NULL;
    Post published;
    Post draft;
    Comment comment = {0};
    PostArray posts = {0};
    PostCardArray cards = {0};
    CommentViewArray comments = {0};
    bool has_next = false;

    assert(arena_init(&arena, 16384));
    assert(database_open(&database, ":memory:"));
    create_schema(&database);
    context.arena = &arena;
    context.database = &database;

    alice = make_user(&arena, "Alice", "alice@example.com", false);
    bob = make_user(&arena, "Bob", "bob@example.com", true);
    assert(user_insert(&context, &alice));
    assert(user_insert(&context, &bob));
    assert(user_find_by_email(&context, sv("ALICE@EXAMPLE.COM"), &loaded) ==
           MODEL_RESULT_OK);
    assert(loaded != NULL && loaded->id == alice.id);
    assert(user_find_by_email(&context, sv("missing@example.com"), &loaded) ==
           MODEL_RESULT_NOT_FOUND);

    published = make_post(&arena, alice.id, "Published article", true);
    draft = make_post(&arena, alice.id, "Private draft", false);
    assert(post_insert(&context, &published));
    assert(post_insert(&context, &draft));
    assert(post_all_for_user(&context, alice.id, &posts));
    assert(posts.length == 2);
    assert(post_all_published_for_user(&context, alice.id, &posts));
    assert(posts.length == 1);
    assert(post_cards_query(&context, (StringView){0}, 1, &cards, &has_next));
    assert(cards.length == 1 && cards.items[0].comment_count == 0);
    assert(post_cards_query(&context, sv("Private"), 1, &cards, &has_next));
    assert(cards.length == 0);

    comment.post_id = published.id;
    comment.user_id = alice.id;
    comment.content = string_from_in(&arena, sv("Hello."));
    assert(comment_insert(&context, &comment));
    assert(comments_query(&context, published.id, bob.id, false, &comments));
    assert(comments.length == 1 && !comments.items[0].can_delete);
    assert(comments_query(&context, published.id, alice.id, false, &comments));
    assert(comments.items[0].can_delete);
    assert(comments_query(&context, published.id, bob.id, true, &comments));
    assert(comments.items[0].can_delete);
    assert(post_cards_query(&context, (StringView){0}, 1, &cards, &has_next));
    assert(cards.items[0].comment_count == 1);
    assert(post_destroy(&context, &published) == MODEL_RESULT_OK);
    assert(comments_query(&context, published.id, bob.id, true, &comments));
    assert(comments.length == 0);

    database_close(&database);
    arena_destroy(&arena);
    return 0;
}
