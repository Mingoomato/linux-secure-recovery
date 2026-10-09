#include <dirent.h>
#include <fcntl.h>
#include <linux/openat2.h>
#include <linux/fs.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <sqlite3.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr std::uint64_t kMaxFileBytes = 1ULL << 30;
struct Entry {
    std::string path;
    std::string hash;
    std::uint64_t size = 0;
    std::uint32_t mode = 0;
    std::uint32_t uid = 0;
    std::uint32_t gid = 0;
    std::int64_t mtime_ns = 0;
};

std::string get_arg(int argc, char **argv, const std::string &name,
                    const std::string &fallback = {}) {
    for (int i = 2; i + 1 < argc; ++i) if (name == argv[i]) return argv[i + 1];
    return fallback;
}

bool has_flag(int argc, char **argv, const std::string &name) {
    return std::find(argv + 2, argv + argc, name) != argv + argc;
}

std::string canonical_dir(const std::string &path) {
    if (path.empty()) throw std::runtime_error("missing path");
    struct stat input_st {};
    if (lstat(path.c_str(), &input_st) != 0 || !S_ISDIR(input_st.st_mode) || S_ISLNK(input_st.st_mode))
        throw std::runtime_error("path must be a non-symlink directory: " + path);
    char resolved[PATH_MAX];
    if (!realpath(path.c_str(), resolved)) throw std::runtime_error("cannot resolve " + path);
    struct stat st {};
    if (lstat(resolved, &st) != 0 || !S_ISDIR(st.st_mode) || S_ISLNK(st.st_mode))
        throw std::runtime_error("path is not a real directory: " + path);
    return resolved;
}

std::string json_quote(const std::string &value) {
    std::ostringstream out;
    out << '"';
    static const char hex[] = "0123456789abcdef";
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') out << '\\' << static_cast<char>(c);
        else if (c == '\b') out << "\\b";
        else if (c == '\f') out << "\\f";
        else if (c == '\n') out << "\\n";
        else if (c == '\r') out << "\\r";
        else if (c == '\t') out << "\\t";
        else if (c < 0x20U) out << "\\u00" << hex[c >> 4U] << hex[c & 0x0fU];
        else out << static_cast<char>(c);
    }
    out << '"';
    return out.str();
}

bool beneath(const std::string &parent, const std::string &child) {
    return child == parent || (child.size() > parent.size() &&
           child.compare(0, parent.size(), parent) == 0 && child[parent.size()] == '/');
}

int open_beneath(int rootfd, const std::string &relative, int flags, mode_t mode = 0) {
    struct open_how how {};
    how.flags = static_cast<std::uint64_t>(flags | O_CLOEXEC);
    how.mode = static_cast<std::uint64_t>(mode);
    how.resolve = RESOLVE_BENEATH | RESOLVE_NO_SYMLINKS | RESOLVE_NO_MAGICLINKS;
    return static_cast<int>(syscall(SYS_openat2, rootfd, relative.c_str(), &how, sizeof(how)));
}

std::string sha256_fd(int fd) {
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) throw std::runtime_error("EVP context allocation failed");
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx); throw std::runtime_error("SHA-256 initialization failed");
    }
    std::array<unsigned char, 65536> buffer {};
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest {};
    unsigned int digest_len = 0;
    while (true) {
        ssize_t n = read(fd, buffer.data(), buffer.size());
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) { EVP_MD_CTX_free(ctx); throw std::runtime_error("source read failed"); }
        if (n == 0) break;
        if (EVP_DigestUpdate(ctx, buffer.data(), static_cast<std::size_t>(n)) != 1) {
            EVP_MD_CTX_free(ctx); throw std::runtime_error("SHA-256 update failed");
        }
    }
    if (EVP_DigestFinal_ex(ctx, digest.data(), &digest_len) != 1 || digest_len != 32) {
        EVP_MD_CTX_free(ctx); throw std::runtime_error("SHA-256 finalization failed");
    }
    EVP_MD_CTX_free(ctx);
    std::ostringstream out;
    out << std::hex;
    for (unsigned int i = 0; i < digest_len; ++i)
        out.width(2), out.fill('0'), out << static_cast<unsigned>(digest[i]);
    return out.str();
}

void write_all(int fd, const unsigned char *data, std::size_t n) {
    std::size_t off = 0;
    while (off < n) {
        ssize_t w = write(fd, data + off, n - off);
        if (w < 0 && errno == EINTR) continue;
        if (w <= 0) throw std::runtime_error("write failed (possibly no space left)");
        off += static_cast<std::size_t>(w);
    }
}

bool same_state(const struct stat &a, const struct stat &b) {
    return a.st_dev == b.st_dev && a.st_ino == b.st_ino && a.st_size == b.st_size &&
        a.st_mtim.tv_sec == b.st_mtim.tv_sec && a.st_mtim.tv_nsec == b.st_mtim.tv_nsec;
}

std::int64_t mtime_ns(const struct stat &st) {
    return static_cast<std::int64_t>(st.st_mtim.tv_sec) * 1000000000LL + st.st_mtim.tv_nsec;
}

std::string now_id() {
    auto now = std::chrono::system_clock::now();
    auto sec = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    std::array<unsigned char, 16> random_id {};
    if (RAND_bytes(random_id.data(), static_cast<int>(random_id.size())) != 1)
        throw std::runtime_error("snapshot ID generation failed");
    std::ostringstream suffix;
    suffix << std::hex;
    for (unsigned char byte : random_id) suffix.width(2), suffix.fill('0'), suffix << static_cast<unsigned>(byte);
    return "snap-" + std::to_string(sec) + "-" + suffix.str();
}

void sql_check(int rc, sqlite3 *db, const char *operation) {
    if (rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_ROW)
        throw std::runtime_error(std::string(operation) + ": " + sqlite3_errmsg(db));
}

sqlite3 *open_db(const std::string &store, bool create) {
    std::string file = store + "/metadata.sqlite";
    sqlite3 *db = nullptr;
    int flags = SQLITE_OPEN_READWRITE | (create ? SQLITE_OPEN_CREATE : 0);
    int rc = sqlite3_open_v2(file.c_str(), &db, flags, nullptr);
    if (rc != SQLITE_OK) {
        std::string message = db ? sqlite3_errmsg(db) : "open failed";
        if (db) sqlite3_close(db);
        throw std::runtime_error("metadata database: " + message);
    }
    sqlite3_busy_timeout(db, 5000);
    const char *schema =
        "PRAGMA foreign_keys=ON; PRAGMA journal_mode=WAL; PRAGMA synchronous=FULL;"
        "CREATE TABLE IF NOT EXISTS snapshots(id TEXT PRIMARY KEY, source_root TEXT NOT NULL,"
        " created_at INTEGER NOT NULL, state TEXT NOT NULL, incomplete_count INTEGER NOT NULL DEFAULT 0);"
        "CREATE TABLE IF NOT EXISTS entries(snapshot_id TEXT NOT NULL, relative_path TEXT NOT NULL,"
        " type TEXT NOT NULL CHECK(type='file'), size INTEGER NOT NULL, sha256 TEXT NOT NULL,"
        " mode INTEGER NOT NULL, uid INTEGER NOT NULL, gid INTEGER NOT NULL, mtime_ns INTEGER NOT NULL,"
        " stable INTEGER NOT NULL, PRIMARY KEY(snapshot_id,relative_path),"
        " FOREIGN KEY(snapshot_id) REFERENCES snapshots(id) ON DELETE CASCADE);";
    char *error = nullptr;
    rc = sqlite3_exec(db, schema, nullptr, nullptr, &error);
    if (rc != SQLITE_OK) {
        std::string message = error ? error : sqlite3_errmsg(db);
        sqlite3_free(error); sqlite3_close(db);
        throw std::runtime_error("schema setup: " + message);
    }
    return db;
}

void ensure_store_dirs(const std::string &store) {
    for (const char *name : {"blobs", "temp", "manifests", "audit"}) {
        std::string p = store + "/" + name;
        if (mkdir(p.c_str(), 0700) != 0 && errno != EEXIST)
            throw std::runtime_error("cannot create store directory: " + p);
        struct stat st {};
        if (lstat(p.c_str(), &st) != 0 || !S_ISDIR(st.st_mode) || S_ISLNK(st.st_mode))
            throw std::runtime_error("store component must be a real directory: " + p);
    }
    std::string hash_root = store + "/blobs/sha256";
    std::error_code ec;
    std::filesystem::create_directories(hash_root, ec);
    if (ec) throw std::runtime_error("cannot create blob hash directory");
    struct stat hash_root_st {};
    if (lstat(hash_root.c_str(), &hash_root_st) != 0 || !S_ISDIR(hash_root_st.st_mode) || S_ISLNK(hash_root_st.st_mode))
        throw std::runtime_error("blob hash component must be a real directory");
    (void)chmod(hash_root.c_str(), 0700);
    (void)chmod(store.c_str(), 0700);
}

class StoreLock {
public:
    explicit StoreLock(const std::string &store) {
        const std::string path = store + "/.operation.lock";
        fd_ = open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
        if (fd_ < 0) throw std::runtime_error("cannot open store operation lock: " + std::string(strerror(errno)));
        struct stat st {};
        if (fstat(fd_, &st) != 0 || !S_ISREG(st.st_mode) || st.st_uid != geteuid() ||
            (st.st_mode & 0077) != 0 || st.st_nlink != 1) {
            close(fd_);
            fd_ = -1;
            throw std::runtime_error("store operation lock must be a private, owner-owned regular file");
        }
        if (fchmod(fd_, 0600) != 0) {
            int error = errno;
            close(fd_);
            fd_ = -1;
            throw std::runtime_error("cannot set private store operation lock mode: " + std::string(strerror(error)));
        }
        while (flock(fd_, LOCK_EX) != 0) {
            if (errno == EINTR) continue;
            int error = errno;
            close(fd_);
            fd_ = -1;
            throw std::runtime_error("cannot acquire store operation lock: " + std::string(strerror(error)));
        }
    }
    StoreLock(const StoreLock &) = delete;
    StoreLock &operator=(const StoreLock &) = delete;
    ~StoreLock() {
        if (fd_ >= 0) {
            (void)flock(fd_, LOCK_UN);
            close(fd_);
        }
    }
private:
    int fd_ = -1;
};

void save_blob(int rootfd, const std::string &store, const std::string &snapshot,
               const std::string &relative, Entry &entry, unsigned index) {
    int src = open_beneath(rootfd, relative, O_RDONLY | O_NOFOLLOW);
    if (src < 0) throw std::runtime_error("openat2 rejected source " + relative + ": " + strerror(errno));
    struct stat before {}, after {};
    if (fstat(src, &before) != 0 || !S_ISREG(before.st_mode) || before.st_size < 0 ||
        static_cast<std::uint64_t>(before.st_size) > kMaxFileBytes) {
        close(src); throw std::runtime_error("unsupported source file: " + relative);
    }
    std::string temp = store + "/temp/" + snapshot + "-" + std::to_string(index) + ".part";
    int dst = open(temp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (dst < 0) { close(src); throw std::runtime_error("create temporary blob failed"); }
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx || EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
        if (ctx) EVP_MD_CTX_free(ctx);
        close(src);
        close(dst);
        unlink(temp.c_str());
        throw std::runtime_error("SHA-256 setup failed");
    }
    std::array<unsigned char, 65536> buffer {};
    std::uint64_t total = 0;
    try {
        while (true) {
            ssize_t n = read(src, buffer.data(), buffer.size());
            if (n < 0 && errno == EINTR) continue;
            if (n < 0) throw std::runtime_error("source read failed");
            if (n == 0) break;
            total += static_cast<std::uint64_t>(n);
            if (total > kMaxFileBytes) throw std::runtime_error("source exceeds file size limit");
            if (EVP_DigestUpdate(ctx, buffer.data(), static_cast<std::size_t>(n)) != 1)
                throw std::runtime_error("SHA-256 update failed");
            write_all(dst, buffer.data(), static_cast<std::size_t>(n));
        }
        if (fstat(src, &after) != 0 || !same_state(before, after))
            throw std::runtime_error("SNAPSHOT_UNSTABLE: " + relative);
        if (fsync(dst) != 0) throw std::runtime_error("blob fsync failed");
        std::array<unsigned char, EVP_MAX_MD_SIZE> digest {};
        unsigned int len = 0;
        if (EVP_DigestFinal_ex(ctx, digest.data(), &len) != 1 || len != 32)
            throw std::runtime_error("SHA-256 finalization failed");
        std::ostringstream hex;
        hex << std::hex;
        for (unsigned int i = 0; i < len; ++i)
            hex.width(2), hex.fill('0'), hex << static_cast<unsigned>(digest[i]);
        entry.hash = hex.str();
        entry.size = total; entry.mode = static_cast<std::uint32_t>(before.st_mode & 0777);
        entry.uid = static_cast<std::uint32_t>(before.st_uid);
        entry.gid = static_cast<std::uint32_t>(before.st_gid);
        entry.mtime_ns = mtime_ns(before);
    } catch (...) {
        EVP_MD_CTX_free(ctx); close(src); close(dst); unlink(temp.c_str()); throw;
    }
    EVP_MD_CTX_free(ctx); close(src);
    if (close(dst) != 0) { unlink(temp.c_str()); throw std::runtime_error("blob close failed"); }
    std::string shard = store + "/blobs/sha256/" + entry.hash.substr(0, 2);
    std::filesystem::create_directories(shard);
    struct stat shard_st {};
    if (lstat(shard.c_str(), &shard_st) != 0 || !S_ISDIR(shard_st.st_mode) || S_ISLNK(shard_st.st_mode)) {
        unlink(temp.c_str());
        throw std::runtime_error("blob shard must be a real directory");
    }
    (void)chmod((store + "/blobs").c_str(), 0700);
    (void)chmod((store + "/blobs/sha256").c_str(), 0700);
    (void)chmod(shard.c_str(), 0700);
    std::string final = shard + "/" + entry.hash;
    if (access(final.c_str(), F_OK) == 0) unlink(temp.c_str());
    else {
        if (rename(temp.c_str(), final.c_str()) != 0) { unlink(temp.c_str()); throw std::runtime_error("blob rename failed"); }
        int dfd = open(shard.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (dfd < 0 || fsync(dfd) != 0) {
            if (dfd >= 0) close(dfd);
            throw std::runtime_error("blob directory fsync failed");
        }
        close(dfd);
    }
}

void scan_tree(int rootfd, int currentfd, const std::string &prefix,
               dev_t rootdev, std::vector<Entry> &entries, sqlite3 *db,
               const std::string &snapshot, const std::string &store, unsigned &counter,
               unsigned depth = 0) {
    if (depth > 128U) throw std::runtime_error("directory depth limit exceeded");
    int dupfd = dup(currentfd);
    if (dupfd < 0) throw std::runtime_error("dup directory failed");
    DIR *dir = fdopendir(dupfd);
    if (!dir) { close(dupfd); throw std::runtime_error("open directory stream failed"); }
    try {
        while (dirent *de = readdir(dir)) {
            if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
            std::string rel = prefix.empty() ? de->d_name : prefix + "/" + de->d_name;
            struct stat st {};
            if (fstatat(currentfd, de->d_name, &st, AT_SYMLINK_NOFOLLOW) != 0)
                throw std::runtime_error("fstatat failed for " + rel);
            if (st.st_dev != rootdev || S_ISLNK(st.st_mode)) continue;
            if (S_ISDIR(st.st_mode)) {
                int child = open_beneath(rootfd, rel, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
                if (child < 0) throw std::runtime_error("openat2 rejected directory " + rel);
                scan_tree(rootfd, child, rel, rootdev, entries, db, snapshot, store, counter, depth + 1U);
                close(child);
            } else if (S_ISREG(st.st_mode)) {
                Entry e; e.path = rel;
                save_blob(rootfd, store, snapshot, rel, e, counter++);
                sqlite3_stmt *stmt = nullptr;
                sql_check(sqlite3_prepare_v2(db,
                    "INSERT INTO entries(snapshot_id,relative_path,type,size,sha256,mode,uid,gid,mtime_ns,stable)"
                    " VALUES(?1,?2,'file',?3,?4,?5,?6,?7,?8,1)", -1, &stmt, nullptr), db, "prepare entry");
                sqlite3_bind_text(stmt, 1, snapshot.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 2, e.path.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(e.size));
                sqlite3_bind_text(stmt, 4, e.hash.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int64(stmt, 5, e.mode); sqlite3_bind_int64(stmt, 6, e.uid);
                sqlite3_bind_int64(stmt, 7, e.gid); sqlite3_bind_int64(stmt, 8, e.mtime_ns);
                sql_check(sqlite3_step(stmt), db, "insert entry");
                sqlite3_finalize(stmt);
                entries.push_back(std::move(e));
            }
        }
    } catch (...) { closedir(dir); throw; }
    closedir(dir);
}

std::string blob_path(const std::string &store, const std::string &hash) {
    if (hash.size() != 64 || !std::all_of(hash.begin(), hash.end(), [](unsigned char c) { return std::isxdigit(c) != 0; }))
        throw std::runtime_error("invalid blob hash in manifest");
    return store + "/blobs/sha256/" + hash.substr(0, 2) + "/" + hash;
}

bool safe_relative_path(const std::string &path) {
    if (path.empty() || path.front() == '/')
        return false;
    std::size_t start = 0;
    while (start < path.size()) {
        std::size_t end = path.find('/', start);
        std::string part = path.substr(start, end == std::string::npos ? end : end - start);
        if (part.empty() || part == "." || part == "..") return false;
        if (end == std::string::npos) break;
        start = end + 1U;
    }
    return true;
}

std::vector<Entry> load_entries(sqlite3 *db, const std::string &id) {
    sqlite3_stmt *stmt = nullptr;
    sql_check(sqlite3_prepare_v2(db,
        "SELECT relative_path,sha256,size,mode,uid,gid,mtime_ns FROM entries WHERE snapshot_id=?1 ORDER BY relative_path",
        -1, &stmt, nullptr), db, "prepare manifest query");
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    std::vector<Entry> result;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Entry e;
        e.path = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        e.hash = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        e.size = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 2));
        e.mode = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 3));
        e.uid = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 4));
        e.gid = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 5));
        e.mtime_ns = sqlite3_column_int64(stmt, 6);
        if (!safe_relative_path(e.path)) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("unsafe relative path in manifest");
        }
        result.push_back(std::move(e));
    }
    sqlite3_finalize(stmt);
    return result;
}

std::string snapshot_source(sqlite3 *db, const std::string &id) {
    sqlite3_stmt *stmt = nullptr;
    sql_check(sqlite3_prepare_v2(db, "SELECT source_root FROM snapshots WHERE id=?1 AND state='COMMITTED'",
                                 -1, &stmt, nullptr), db, "prepare snapshot query");
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    std::string root;
    if (rc == SQLITE_ROW) root = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    sqlite3_finalize(stmt);
    if (root.empty()) throw std::runtime_error("snapshot missing or not committed");
    return root;
}

void audit(const std::string &store, const std::string &operation, const std::string &id,
           std::size_t count, bool confirmed) {
    std::ofstream out(store + "/audit/events.ndjson", std::ios::app);
    if (!out) throw std::runtime_error("cannot write audit log");
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    out << "{\"schema_version\":\"operation.v1\",\"operation\":" << json_quote(operation)
        << ",\"snapshot_id\":" << json_quote(id) << ",\"affected_count\":" << count
        << ",\"confirmed\":" << (confirmed ? "true" : "false")
        << ",\"observed_at\":" << now << "}\n";
    out.flush();
}

int cmd_init(int argc, char **argv) {
    std::string source = canonical_dir(get_arg(argc, argv, "--source"));
    std::string store_path = get_arg(argc, argv, "--store");
    if (store_path.empty()) throw std::runtime_error("init requires --source and --store");
    if (mkdir(store_path.c_str(), 0700) != 0 && errno != EEXIST)
        throw std::runtime_error("cannot create backup store");
    std::string store = canonical_dir(store_path);
    if (beneath(source, store) || beneath(store, source))
        throw std::runtime_error("SRE_ROOT_OVERLAP: source and store must be disjoint");
    StoreLock lock(store);
    ensure_store_dirs(store);
    sqlite3 *db = open_db(store, true);
    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO snapshots(id,source_root,created_at,state) VALUES('__store__',?1,?2,'COMMITTED')",
                       -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, source.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(std::time(nullptr)));
    int rc = sqlite3_step(stmt); sqlite3_finalize(stmt); sqlite3_close(db);
    if (rc != SQLITE_DONE) throw std::runtime_error("cannot initialize store metadata");
    std::cout << "store initialized\n";
    return 0;
}

int cmd_snapshot(int argc, char **argv) {
    std::string source = canonical_dir(get_arg(argc, argv, "--source"));
    std::string store = canonical_dir(get_arg(argc, argv, "--store"));
    if (beneath(source, store) || beneath(store, source))
        throw std::runtime_error("SRE_ROOT_OVERLAP");
    StoreLock lock(store);
    ensure_store_dirs(store);
    int rootfd = open(source.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (rootfd < 0) throw std::runtime_error("cannot open source root");
    struct stat rootst {};
    if (fstat(rootfd, &rootst) != 0) { close(rootfd); throw std::runtime_error("cannot stat source root"); }
    std::string id = now_id();
    sqlite3 *db = open_db(store, true);
    sqlite3_exec(db, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr);
    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(db, "INSERT INTO snapshots(id,source_root,created_at,state) VALUES(?1,?2,?3,'COPYING')",
                       -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, source.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(std::time(nullptr)));
    int rc = sqlite3_step(stmt); sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) { sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr); close(rootfd); sqlite3_close(db); throw std::runtime_error("snapshot id collision"); }
    sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr);
    sqlite3_exec(db, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr);
    std::vector<Entry> entries; unsigned counter = 0;
    try {
        scan_tree(rootfd, rootfd, "", rootst.st_dev, entries, db, id, store, counter);
        sqlite3_prepare_v2(db, "UPDATE snapshots SET state='COMMITTED',incomplete_count=0 WHERE id=?1",
                           -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt); sqlite3_finalize(stmt);
        if (rc != SQLITE_DONE) throw std::runtime_error("manifest commit failed");
        if (sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr) != SQLITE_OK)
            throw std::runtime_error("snapshot transaction commit failed");
        std::ofstream manifest(store + "/manifests/" + id + ".json");
        manifest << "{\"schema_version\":\"manifest.v1\",\"snapshot_id\":" << json_quote(id)
                 << ",\"source_root\":" << json_quote(source) << ",\"consistency\":\"per_file_verified\","
                    "\"entry_count\":" << entries.size() << ",\"incomplete_count\":0}\n";
        manifest.flush();
        if (!manifest) throw std::runtime_error("manifest file write failed");
        audit(store, "snapshot", id, entries.size(), true);
        close(rootfd); sqlite3_close(db);
        std::cout << id << " files=" << entries.size() << '\n';
        return 0;
    } catch (...) {
        sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
        sqlite3_prepare_v2(db, "UPDATE snapshots SET state='FAILED',incomplete_count=1 WHERE id=?1",
                           -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
        (void)sqlite3_step(stmt); sqlite3_finalize(stmt);
        close(rootfd); sqlite3_close(db); throw;
    }
}

int cmd_list(const std::string &store) {
    StoreLock lock(store);
    sqlite3 *db = open_db(store, false);
    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(db, "SELECT id,source_root,created_at,state FROM snapshots WHERE id!='__store__' ORDER BY created_at DESC",
                       -1, &stmt, nullptr);
    while (sqlite3_step(stmt) == SQLITE_ROW)
        std::cout << reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)) << " "
                  << reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1)) << " "
                  << sqlite3_column_int64(stmt, 2) << " "
                  << reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3)) << '\n';
    sqlite3_finalize(stmt); sqlite3_close(db); return 0;
}

int cmd_verify(const std::string &store, const std::string &id) {
    StoreLock lock(store);
    sqlite3 *db = open_db(store, false);
    (void)snapshot_source(db, id);
    auto entries = load_entries(db, id);
    if (entries.empty()) {
        sqlite3_stmt *s = nullptr; sqlite3_prepare_v2(db, "SELECT 1 FROM entries WHERE snapshot_id=?1 LIMIT 1", -1, &s, nullptr);
        sqlite3_bind_text(s, 1, id.c_str(), -1, SQLITE_TRANSIENT);
        int has_rows = sqlite3_step(s) == SQLITE_ROW; sqlite3_finalize(s);
        if (has_rows) { sqlite3_close(db); throw std::runtime_error("manifest query failed"); }
    }
    sqlite3_close(db);
    std::size_t bad = 0;
    for (const auto &e : entries) {
        std::string path = blob_path(store, e.hash);
        int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
        if (fd < 0) { std::cout << "missing " << e.path << '\n'; ++bad; continue; }
        std::string digest = sha256_fd(fd); close(fd);
        if (digest != e.hash) { std::cout << "corrupt " << e.path << '\n'; ++bad; }
    }
    audit(store, "verify", id, entries.size(), false);
    std::cout << "{\"snapshot_id\":" << json_quote(id) << ",\"verified\":" << entries.size()
              << ",\"bad\":" << bad << "}\n";
    return bad ? 2 : 0;
}

int ensure_parent(int rootfd, const std::string &path) {
    std::size_t pos = 0;
    int current = dup(rootfd);
    if (current < 0) return -1;
    while ((pos = path.find('/', pos)) != std::string::npos) {
        std::string part = path.substr(0, pos);
        std::size_t previous = path.rfind('/', pos - 1);
        std::string component = path.substr(previous == std::string::npos ? 0 : previous + 1,
                                            pos - (previous == std::string::npos ? 0 : previous + 1));
        (void)part;
        if (mkdirat(current, component.c_str(), 0700) != 0 && errno != EEXIST) { close(current); return -1; }
        int next = openat(current, component.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
        close(current);
        if (next < 0) return -1;
        current = next;
        ++pos;
    }
    return current;
}

int restore_one(int targetfd, const std::string &store, const Entry &e, bool overwrite) {
    std::size_t slash = e.path.find_last_of('/');
    std::string parent = slash == std::string::npos ? "" : e.path.substr(0, slash);
    std::string name = slash == std::string::npos ? e.path : e.path.substr(slash + 1);
    int dirfd = parent.empty() ? dup(targetfd) : ensure_parent(targetfd, parent + "/");
    if (dirfd < 0) return -1;
    struct stat existing {};
    int stat_rc = fstatat(dirfd, name.c_str(), &existing, AT_SYMLINK_NOFOLLOW);
    if (stat_rc == 0 && !overwrite) {
        close(dirfd); errno = EEXIST; return 1;
    }
    if (stat_rc != 0 && errno != ENOENT) { close(dirfd); return -1; }
    std::string blob = blob_path(store, e.hash);
    int src = open(blob.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (src < 0) { close(dirfd); return -1; }
    std::string temp = ".sre-" + std::to_string(getpid()) + "-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    int dst = openat(dirfd, temp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (dst < 0) { close(src); close(dirfd); return -1; }
    int staged_fd = -1;
    try {
        std::array<unsigned char, 65536> buf {};
        while (true) {
            ssize_t n = read(src, buf.data(), buf.size());
            if (n < 0 && errno == EINTR) continue;
            if (n < 0) throw std::runtime_error("blob read failed");
            if (n == 0) break;
            write_all(dst, buf.data(), static_cast<std::size_t>(n));
        }
        if (fsync(dst) != 0) throw std::runtime_error("restore fsync failed");
        close(dst); dst = -1;
        close(src); src = -1;
        staged_fd = openat(dirfd, temp.c_str(), O_RDWR | O_CLOEXEC | O_NOFOLLOW);
        if (staged_fd < 0) throw std::runtime_error("cannot reopen staged restore file");
        if (sha256_fd(staged_fd) != e.hash || fchmod(staged_fd, e.mode & 0777U) != 0 || fsync(staged_fd) != 0)
            throw std::runtime_error("staged restore verification or mode set failed");
    } catch (...) {
        if (src >= 0) close(src);
        if (dst >= 0) close(dst);
        if (staged_fd >= 0) close(staged_fd);
        unlinkat(dirfd, temp.c_str(), 0); close(dirfd); return -1;
    }
    struct stat staged_stat {};
    if (fstat(staged_fd, &staged_stat) != 0 || !S_ISREG(staged_stat.st_mode)) {
        close(staged_fd); unlinkat(dirfd, temp.c_str(), 0); close(dirfd); errno = EIO; return -1;
    }
    unsigned int flags = overwrite ? 0U : RENAME_NOREPLACE;
    int rc = static_cast<int>(syscall(SYS_renameat2, dirfd, temp.c_str(), dirfd, name.c_str(), flags));
    if (rc != 0) { close(staged_fd); unlinkat(dirfd, temp.c_str(), 0); close(dirfd); return -1; }
    struct stat installed_stat {};
    if (fstatat(dirfd, name.c_str(), &installed_stat, AT_SYMLINK_NOFOLLOW) != 0 ||
        !S_ISREG(installed_stat.st_mode) || installed_stat.st_dev != staged_stat.st_dev ||
        installed_stat.st_ino != staged_stat.st_ino || fsync(dirfd) != 0) {
        close(staged_fd); close(dirfd); errno = EIO; return -1;
    }
    close(staged_fd);
    close(dirfd);
    return 0;
}

int cmd_restore(const std::string &store, const std::string &id, const std::string &target,
                bool dry, bool confirm, bool overwrite) {
    StoreLock lock(store);
    std::string root = canonical_dir(target);
    sqlite3 *db = open_db(store, false);
    (void)snapshot_source(db, id);
    auto entries = load_entries(db, id);
    sqlite3_close(db);
    if (!dry && !confirm) throw std::runtime_error("SRE_CONFIRM_REQUIRED: pass --confirm");
    int targetfd = open(root.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (targetfd < 0) throw std::runtime_error("cannot open restore root");
    int conflicts = 0, restored = 0, failed = 0;
    for (const auto &e : entries) {
        if (dry) {
            std::cout << "restore " << e.path << " sha256=" << e.hash << '\n';
            continue;
        }
        int rc = restore_one(targetfd, store, e, overwrite);
        if (rc == 1) { std::cout << "conflict " << e.path << '\n'; ++conflicts; }
        else if (rc != 0) { std::cout << "failed " << e.path << ": " << strerror(errno) << '\n'; ++failed; }
        else ++restored;
    }
    close(targetfd);
    audit(store, dry ? "restore_dry_run" : "restore", id, dry ? entries.size() : restored, confirm);
    std::cout << "{\"restored\":" << restored << ",\"conflicts\":" << conflicts
              << ",\"failed\":" << failed << ",\"dry_run\":" << (dry ? "true" : "false") << "}\n";
    return failed ? 2 : 0;
}

int cmd_prune(const std::string &store, unsigned keep) {
    StoreLock lock(store);
    sqlite3 *db = open_db(store, false);
    sqlite3_stmt *s = nullptr;
    sqlite3_prepare_v2(db, "SELECT id FROM snapshots WHERE id!='__store__' AND state='COMMITTED' ORDER BY created_at DESC",
                       -1, &s, nullptr);
    std::vector<std::string> ids;
    while (sqlite3_step(s) == SQLITE_ROW) ids.emplace_back(reinterpret_cast<const char *>(sqlite3_column_text(s, 0)));
    sqlite3_finalize(s);
    if (ids.size() > keep) {
        sqlite3_exec(db, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr);
        sqlite3_prepare_v2(db, "DELETE FROM snapshots WHERE id=?1", -1, &s, nullptr);
        for (std::size_t i = keep; i < ids.size(); ++i) {
            sqlite3_bind_text(s, 1, ids[i].c_str(), -1, SQLITE_TRANSIENT);
            (void)sqlite3_step(s); sqlite3_reset(s);
            unlink((store + "/manifests/" + ids[i] + ".json").c_str());
        }
        sqlite3_finalize(s);
        sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr);
    }
    sqlite3_prepare_v2(db, "SELECT DISTINCT e.sha256 FROM entries e", -1, &s, nullptr);
    std::set<std::string> referenced;
    while (sqlite3_step(s) == SQLITE_ROW) referenced.emplace(reinterpret_cast<const char *>(sqlite3_column_text(s, 0)));
    sqlite3_finalize(s); sqlite3_close(db);
    std::filesystem::path blobs = std::filesystem::path(store) / "blobs/sha256";
    if (std::filesystem::exists(blobs))
        for (const auto &p : std::filesystem::recursive_directory_iterator(blobs))
            if (p.is_regular_file() && !referenced.contains(p.path().filename().string()))
                std::filesystem::remove(p.path());
    audit(store, "prune", "retention", ids.size() > keep ? ids.size() - keep : 0, true);
    return 0;
}

int cmd_doctor(const std::string &store) {
    StoreLock lock(store);
    ensure_store_dirs(store);
    sqlite3 *db = open_db(store, false);
    sqlite3_stmt *stmt = nullptr;
    sql_check(sqlite3_prepare_v2(db,
        "UPDATE snapshots SET state='FAILED',incomplete_count=1 WHERE state='COPYING'",
        -1, &stmt, nullptr), db, "prepare stale snapshot recovery");
    sql_check(sqlite3_step(stmt), db, "recover stale snapshots");
    std::size_t recovered = static_cast<std::size_t>(sqlite3_changes(db));
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    std::size_t removed_temp = 0;
    std::error_code ec;
    const std::filesystem::path temp_dir = std::filesystem::path(store) / "temp";
    for (std::filesystem::directory_iterator it(temp_dir, ec), end; !ec && it != end; it.increment(ec)) {
        const std::string name = it->path().filename().string();
        if (name.size() < 5U || name.compare(name.size() - 5U, 5U, ".part") != 0) continue;
        std::error_code status_ec;
        auto status = it->symlink_status(status_ec);
        if (status_ec) throw std::runtime_error("cannot inspect temporary recovery artifact");
        if (!std::filesystem::is_regular_file(status) && !std::filesystem::is_symlink(status)) continue;
        if (!std::filesystem::remove(it->path(), status_ec) || status_ec)
            throw std::runtime_error("cannot remove stale temporary recovery artifact");
        ++removed_temp;
    }
    if (ec) throw std::runtime_error("cannot enumerate temporary recovery artifacts");
    std::cout << "{\"store\":\"ready\",\"openat2\":\"required\",\"coverage\":\"local-only\","
              << "\"recovered_incomplete\":" << recovered
              << ",\"removed_temp_files\":" << removed_temp << "}\n";
    return 0;
}
} // namespace

int main(int argc, char **argv) {
    try {
        if (argc < 2 || std::string(argv[1]) == "--help") {
            std::cout << "sre init --source DIR --store DIR\n"
                         "sre snapshot --source DIR --store DIR [--label TEXT]\n"
                         "sre list --store DIR\nsre verify --store DIR --snapshot ID\n"
                         "sre restore --store DIR --snapshot ID --target DIR --dry-run|--confirm [--overwrite]\n"
                         "sre prune --store DIR --keep N\nsre doctor --store DIR\n";
            return argc < 2 ? 2 : 0;
        }
        std::string command = argv[1];
        if (command == "init") return cmd_init(argc, argv);
        std::string store = canonical_dir(get_arg(argc, argv, "--store"));
        if (command == "snapshot") return cmd_snapshot(argc, argv);
        if (command == "list" || command == "show") return cmd_list(store);
        if (command == "verify") return cmd_verify(store, get_arg(argc, argv, "--snapshot"));
        if (command == "restore") return cmd_restore(store, get_arg(argc, argv, "--snapshot"),
            get_arg(argc, argv, "--target"), has_flag(argc, argv, "--dry-run"),
            has_flag(argc, argv, "--confirm"), has_flag(argc, argv, "--overwrite"));
        if (command == "prune") {
            unsigned long keep = std::stoul(get_arg(argc, argv, "--keep", "5"));
            if (keep > 100000UL) throw std::runtime_error("keep limit exceeded");
            return cmd_prune(store, static_cast<unsigned>(keep));
        }
        if (command == "doctor") {
            return cmd_doctor(store);
        }
        throw std::runtime_error("unknown command");
    } catch (const std::exception &e) {
        std::cerr << "sre: " << e.what() << '\n'; return 2;
    }
}
