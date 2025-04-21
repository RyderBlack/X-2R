-- CREATE DATABASE db_discord;
-- \c db_discord;

CREATE TABLE users (
                       user_id SERIAL PRIMARY KEY,
                       first_name VARCHAR(50),
                       last_name VARCHAR(50),
                       email VARCHAR(100) UNIQUE NOT NULL,
                       password TEXT NOT NULL,
                       status VARCHAR(20) DEFAULT 'offline'
);

CREATE TABLE roles (
                       role_id SERIAL PRIMARY KEY,
                       name VARCHAR(20) NOT NULL UNIQUE
);

CREATE TABLE channels (
                          channel_id SERIAL PRIMARY KEY,
                          name VARCHAR(100) NOT NULL,
                          is_private BOOLEAN DEFAULT FALSE,
                          created_by INTEGER REFERENCES users(user_id) ON DELETE SET NULL
);

CREATE TABLE user_channels (
                               user_id INTEGER REFERENCES users(user_id) ON DELETE CASCADE,
                               channel_id INTEGER REFERENCES channels(channel_id) ON DELETE CASCADE,
                               role_id INTEGER REFERENCES roles(role_id) ON DELETE SET NULL,
                               PRIMARY KEY (user_id, channel_id)
);

CREATE TABLE messages (
                          message_id SERIAL PRIMARY KEY,
                          channel_id INTEGER REFERENCES channels(channel_id) ON DELETE CASCADE,
                          sender_id INTEGER REFERENCES users(user_id) ON DELETE SET NULL,
                          content TEXT NOT NULL,
                          timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE reactions (
                           reaction_id SERIAL PRIMARY KEY,
                           message_id INTEGER REFERENCES messages(message_id) ON DELETE CASCADE,
                           user_id INTEGER REFERENCES users(user_id) ON DELETE CASCADE,
                           emoji VARCHAR(10) NOT NULL
);

CREATE TABLE files (
                       file_id SERIAL PRIMARY KEY,
                       message_id INTEGER REFERENCES messages(message_id) ON DELETE CASCADE,
                       file_url TEXT NOT NULL,
                       file_type VARCHAR(20)
);

CREATE TABLE mentions (
                          mention_id SERIAL PRIMARY KEY,
                          message_id INTEGER REFERENCES messages(message_id) ON DELETE CASCADE,
                          mentioned_user_id INTEGER REFERENCES users(user_id) ON DELETE CASCADE
);
