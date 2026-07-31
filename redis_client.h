#ifndef REDIS_CLIENT_H
#define REDIS_CLIENT_H

#include <hiredis/hiredis.h>
#include <hiredis/hiredis_ssl.h>
#include <string>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <vector>
#include <utility>
#include <algorithm>

class RedisClient {
private:
    redisContext* ctx = nullptr;
    redisSSLContext* ssl_ctx = nullptr;

    std::string trim_str(const char* raw) {
        if (!raw) return "";
        std::string s(raw);
        size_t start = s.find_first_not_of(" \t\n\r\"'");
        if (start == std::string::npos) return "";
        size_t end = s.find_last_not_of(" \t\n\r\"'");
        return s.substr(start, end - start + 1);
    }

public:
    RedisClient() {
        std::string raw_url = trim_str(std::getenv("REDIS_URL"));
        std::string host = "127.0.0.1";
        int port = 6379;
        std::string password = "";
        bool is_tls = false;

        if (!raw_url.empty()) {
            std::string url = raw_url;
            if (url.rfind("rediss://", 0) == 0) {
                is_tls = true;
                url = url.substr(9);
            } else if (url.rfind("redis://", 0) == 0) {
                url = url.substr(8);
            }
            size_t at_pos = url.find('@');
            if (at_pos != std::string::npos) {
                std::string user_pass = url.substr(0, at_pos);
                url = url.substr(at_pos + 1);
                size_t colon_pos = user_pass.find(':');
                if (colon_pos != std::string::npos) {
                    password = user_pass.substr(colon_pos + 1);
                } else {
                    password = user_pass;
                }
            }
            size_t colon_pos = url.find(':');
            if (colon_pos != std::string::npos) {
                host = url.substr(0, colon_pos);
                port = std::stoi(url.substr(colon_pos + 1));
            } else {
                host = url;
            }
        } else {
            std::string h = trim_str(std::getenv("REDIS_HOST"));
            if (!h.empty()) host = h;
        }

        if (is_tls) {
            redisInitOpenSSL();
            redisSSLContextError err;
            redisSSLOptions ssl_opts;
            std::memset(&ssl_opts, 0, sizeof(ssl_opts));
            ssl_opts.server_name = host.c_str();
            ssl_ctx = redisCreateSSLContextWithOptions(&ssl_opts, &err);
        }

        ctx = redisConnect(host.c_str(), port);
        if (!ctx || ctx->err) {
            std::cerr << "Redis Connect Error: " << (ctx ? ctx->errstr : "null") << std::endl;
            return;
        }

        if (is_tls && ssl_ctx) {
            if (redisInitiateSSLWithContext(ctx, ssl_ctx) != REDIS_OK) {
                std::cerr << "Redis SSL Error: " << ctx->errstr << std::endl;
                return;
            }
        }

        if (!password.empty()) {
            redisReply* reply = (redisReply*)redisCommand(ctx, "AUTH %s", password.c_str());
            if (reply) freeReplyObject(reply);
        }
    }

    ~RedisClient() {
        if (ctx) redisFree(ctx);
        if (ssl_ctx) redisFreeSSLContext(ssl_ctx);
    }

    void set(const std::string& key, const std::string& val) {
        if (!ctx) return;
        redisReply* reply = (redisReply*)redisCommand(ctx, "SET %b %b", key.data(), key.size(), val.data(), val.size());
        if (reply) freeReplyObject(reply);
    }

    void mset(const std::vector<std::pair<std::string, std::string>>& kv_pairs) {
        if (!ctx || kv_pairs.empty()) return;
        size_t chunk_size = 200;
        for (size_t i = 0; i < kv_pairs.size(); i += chunk_size) {
            size_t end = std::min(kv_pairs.size(), i + chunk_size);
            size_t count = end - i;
            std::vector<const char*> argv;
            std::vector<size_t> argvlen;
            argv.reserve(1 + count * 2);
            argvlen.reserve(1 + count * 2);
            static const char* mset_cmd = "MSET";
            argv.push_back(mset_cmd);
            argvlen.push_back(4);
            for (size_t k = i; k < end; ++k) {
                argv.push_back(kv_pairs[k].first.data());
                argvlen.push_back(kv_pairs[k].first.size());
                argv.push_back(kv_pairs[k].second.data());
                argvlen.push_back(kv_pairs[k].second.size());
            }
            redisReply* reply = (redisReply*)redisCommandArgv(ctx, (int)argv.size(), argv.data(), argvlen.data());
            if (reply) freeReplyObject(reply);
        }
    }

    std::optional<std::string> get(const std::string& key) {
        if (!ctx) return std::nullopt;
        redisReply* reply = (redisReply*)redisCommand(ctx, "GET %b", key.data(), key.size());
        if (!reply) return std::nullopt;
        if (reply->type == REDIS_REPLY_STRING) {
            std::string res(reply->str, reply->len);
            freeReplyObject(reply);
            return res;
        }
        freeReplyObject(reply);
        return std::nullopt;
    }

    bool exists(const std::string& key) {
        if (!ctx) return false;
        redisReply* reply = (redisReply*)redisCommand(ctx, "EXISTS %b", key.data(), key.size());
        if (!reply) return false;
        bool res = (reply->type == REDIS_REPLY_INTEGER && reply->integer > 0);
        freeReplyObject(reply);
        return res;
    }
};

#endif
