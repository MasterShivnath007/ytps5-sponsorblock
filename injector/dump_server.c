/*
 * ytps5 proxy / capture helper
 *
 * A tiny HTTP listener on port 4040:
 *   GET  /<videoID>  -> SponsorBlock skipSegments JSON for that video
 *   POST /dump       -> append the raw body to dump.log (debug only)
 *   GET  /ping       -> []
 *
 * The injected JS already falls back to http://127.0.0.1:4040/<videoID>
 * when its direct SponsorBlock fetch is blocked by CSP.
 */
#include <arpa/inet.h>
#include <curl/curl.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define LOG_FILE "/data/ytps5-sponsorblock/dump.log"
#define SRV_LOG "/data/ytps5-sponsorblock/proxy_server.log"
#define MAX_BODY (4 * 1024 * 1024)

#define UPDATE_URL \
  "https://raw.githubusercontent.com/MasterShivnath007/ytps5-sponsorblock/main/version.json"
#define API_PREFIX "https://sponsor.ajay.app/api/skipSegments?videoID="
#define API_SUFFIX \
  "&categories=%5B%22sponsor%22%2C%22intro%22%2C%22outro%22%2C%22interaction%22%2C%22selfpromo%22%2C%22preview%22%2C%22filler%22%2C%22music_offtopic%22%5D"

typedef struct {
  char *data;
  size_t len;
  int owned;
} Buffer;

static void logf_(const char *fmt, ...) {
  char buf[512];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  FILE *fp = fopen(SRV_LOG, "a");
  if (fp) {
    fprintf(fp, "%s\n", buf);
    fflush(fp);
    fsync(fileno(fp));
    fclose(fp);
  }
}

static void write_dump(const char *body, int len) {
  FILE *fp = fopen(LOG_FILE, "ab");
  if (!fp) return;
  fwrite(body, 1, (size_t)len, fp);
  fputc('\n', fp);
  fflush(fp);
  fsync(fileno(fp));
  fclose(fp);
}

static size_t curl_write(char *ptr, size_t size, size_t nmemb, void *ud) {
  Buffer *buf = (Buffer *)ud;
  size_t n = size * nmemb;
  char *new_data = (char *)realloc(buf->data, buf->len + n + 1);
  if (!new_data) return 0;
  buf->data = new_data;
  buf->owned = 1;
  memcpy(buf->data + buf->len, ptr, n);
  buf->len += n;
  buf->data[buf->len] = 0;
  return n;
}

static void set_empty(Buffer *out) {
  if (out->owned && out->data) free(out->data);
  out->data = (char *)malloc(3);
  if (out->data) {
    memcpy(out->data, "[]", 3);
    out->len = 2;
    out->owned = 1;
  } else {
    out->data = NULL;
    out->len = 0;
    out->owned = 0;
  }
}

static void fetch_segments(const char *video_id, Buffer *out) {
  out->data = NULL;
  out->len = 0;
  out->owned = 0;

  logf_("fetch start %s", video_id);
  CURL *curl = curl_easy_init();
  logf_("curl_easy_init=%p", (void *)curl);
  if (!curl) return;

  char url[512];
  snprintf(url, sizeof(url), "%s%s%s", API_PREFIX, video_id, API_SUFFIX);

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, out);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "ytps5-sponsorblock/1.0");

  logf_("perform start");
  CURLcode rc = curl_easy_perform(curl);
  logf_("perform rc=%d", (int)rc);
  long status = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
  curl_easy_cleanup(curl);

  if (rc != CURLE_OK || status == 404 || out->len == 0) {
    set_empty(out);
    logf_("segments %s -> [] (curl=%d http=%ld)", video_id, (int)rc,
          status);
    return;
  }

  logf_("segments %s -> %zu bytes (http=%ld)", video_id, out->len, status);
}

static char g_update_cache[4096];
static size_t g_update_len = 0;
static time_t g_update_time = 0;

static void fetch_update(Buffer *out) {
  out->data = NULL;
  out->len = 0;
  out->owned = 0;

  time_t now = time(NULL);
  if (g_update_len && now - g_update_time < 60) {
    out->data = (char *)malloc(g_update_len + 1);
    if (out->data) {
      memcpy(out->data, g_update_cache, g_update_len);
      out->data[g_update_len] = 0;
      out->len = g_update_len;
      out->owned = 1;
    }
    return;
  }

  CURL *curl = curl_easy_init();
  if (!curl) return;
  curl_easy_setopt(curl, CURLOPT_URL, UPDATE_URL);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, out);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "ytps5-sponsorblock/1.0");

  CURLcode rc = curl_easy_perform(curl);
  long status = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
  curl_easy_cleanup(curl);

  if (rc != CURLE_OK || status != 200 || out->len == 0) {
    set_empty(out);
    logf_("update check failed (curl=%d http=%ld)", (int)rc, status);
    return;
  }
  if (out->len < sizeof(g_update_cache)) {
    memcpy(g_update_cache, out->data, out->len);
    g_update_len = out->len;
    g_update_time = now;
  }
  logf_("update check %zu bytes (http=%ld)", out->len, status);
}

static int is_video_id(const char *path) {
  if (!path || path[0] != '/') return 0;
  const char *id = path + 1;
  size_t len = strlen(id);
  if (len < 6 || len > 32) return 0;
  for (size_t i = 0; i < len; ++i) {
    char c = id[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '-' || c == '_')) {
      return 0;
    }
  }
  return 1;
}

static void send_json(int c, const char *body, size_t len) {
  char header[256];
  int hlen = snprintf(header, sizeof(header),
                      "HTTP/1.1 200 OK\r\n"
                      "Access-Control-Allow-Origin: *\r\n"
                      "Cache-Control: no-store\r\n"
                      "Content-Type: application/json\r\n"
                      "Content-Length: %zu\r\n"
                      "Connection: close\r\n\r\n",
                      len);
  send(c, header, (size_t)hlen, 0);
  if (body && len) send(c, body, len, 0);
}

int main(void) {
  int ci = curl_global_init(CURL_GLOBAL_DEFAULT);
  logf_("curl_global_init=%d version=%s", ci, curl_version());

  int s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) {
    logf_("socket failed");
    return 1;
  }
  int one = 1;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(4040);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    logf_("bind 4040 failed");
    close(s);
    return 2;
  }
  if (listen(s, 8) != 0) {
    logf_("listen failed");
    close(s);
    return 3;
  }
  logf_("proxy listening on 0.0.0.0:4040");

  for (;;) {
    int c = accept(s, NULL, NULL);
    if (c < 0) continue;

    struct timeval tv = {5, 0};
    setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    static char header[8192];
    int n = recv(c, header, sizeof(header) - 1, 0);
    if (n <= 0) {
      close(c);
      continue;
    }
    header[n] = 0;

    char method[8] = {0};
    char path[256] = {0};
    sscanf(header, "%7s %255s", method, path);

    int length = 0;
    char *cl = strstr(header, "Content-Length:");
    if (!cl) cl = strstr(header, "content-length:");
    if (cl) length = atoi(cl + 15);
    if (length < 0 || length > MAX_BODY) length = 0;

    if (strncmp(method, "POST", 4) == 0 && strcmp(path, "/dump") == 0) {
      char *end = strstr(header, "\r\n\r\n");
      int already = end ? (int)(n - (end + 4 - header)) : 0;
      if (already < 0) already = 0;

      char *body = (char *)malloc((size_t)length + 1);
      if (body) {
        int got = 0;
        if (already > 0) {
          if (already > length) already = length;
          memcpy(body, end + 4, (size_t)already);
          got = already;
        }
        while (got < length) {
          int r = recv(c, body + got, (size_t)(length - got), 0);
          if (r <= 0) break;
          got += r;
        }
        body[got] = 0;
        write_dump(body, got);
        logf_("dump %d bytes", got);
        free(body);
      }
      send_json(c, "[]", 2);
    } else if (strcmp(path, "/update") == 0) {
      Buffer out = {0, 0, 0};
      fetch_update(&out);
      if (!out.data) {
        send_json(c, "{}", 2);
      } else {
        send_json(c, out.data, out.len);
        if (out.owned) free(out.data);
      }
    } else if (is_video_id(path)) {
      Buffer out = {0, 0, 0};
      fetch_segments(path + 1, &out);
      if (!out.data) {
        send_json(c, "[]", 2);
      } else {
        send_json(c, out.data, out.len);
        if (out.owned) free(out.data);
      }
    } else {
      logf_("request %s %s", method, path);
      send_json(c, "[]", 2);
    }

    close(c);
  }
}
