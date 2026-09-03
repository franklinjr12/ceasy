CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    email TEXT NOT NULL COLLATE NOCASE UNIQUE,
    password_digest TEXT NOT NULL,
    bio TEXT NOT NULL DEFAULT '',
    is_admin INTEGER NOT NULL DEFAULT 0,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

INSERT OR IGNORE INTO users (id, name, email, password_digest, bio, is_admin)
VALUES (1, 'Ceasy Maintainer', 'admin@ceasy.local', '',
        'Building a pleasant web framework in C.', 1);

CREATE TABLE posts_new (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    title TEXT NOT NULL,
    summary TEXT NOT NULL DEFAULT '',
    content TEXT NOT NULL,
    published INTEGER NOT NULL DEFAULT 0,
    published_at TEXT NOT NULL DEFAULT '',
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO posts_new (id, user_id, title, summary, content, published,
                       published_at, created_at, updated_at)
SELECT id, 1, title, substr(content, 1, 500), content, 1, created_at,
       created_at, updated_at
FROM posts;

DROP TABLE posts;
ALTER TABLE posts_new RENAME TO posts;
CREATE INDEX index_posts_on_user_id ON posts(user_id);
CREATE INDEX index_posts_on_published ON posts(published, published_at);

CREATE TABLE comments (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    post_id INTEGER NOT NULL REFERENCES posts(id) ON DELETE CASCADE,
    user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    content TEXT NOT NULL,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE INDEX index_comments_on_post_id ON comments(post_id);
CREATE INDEX index_comments_on_user_id ON comments(user_id);

CREATE TABLE ceasy_sessions (
    token_digest BLOB PRIMARY KEY,
    data BLOB NOT NULL,
    created_at INTEGER NOT NULL,
    updated_at INTEGER NOT NULL,
    expires_at INTEGER NOT NULL
);
CREATE INDEX index_ceasy_sessions_on_expires_at
ON ceasy_sessions(expires_at);
