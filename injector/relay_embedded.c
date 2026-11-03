/*
 * ytps5 single-file installer
 *
 * Embeds the SponsorBlock proxy and the injector, then hands both to the
 * console's own elfldr on 127.0.0.1:9021. This is the only file users need to
 * upload through Payload Manager. It needs no kernel R/W handles, so it works
 * when launched directly by Payload Manager.
 */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define LOG_DIR "/data/ytps5-sponsorblock"
#define LOG_FILE LOG_DIR "/relay.log"

extern const unsigned char _binary_ytps5_dump_elf_start[];
extern const unsigned char _binary_ytps5_dump_elf_end[];
extern const unsigned char _binary_ytps5_injector_elf_start[];
extern const unsigned char _binary_ytps5_injector_elf_end[];

static FILE *g_log;

static void logf_(const char *fmt, ...) {
  char buf[512];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (!g_log) g_log = fopen(LOG_FILE, "a");
  if (g_log) {
    fprintf(g_log, "%s\n", buf);
    fflush(g_log);
    fsync(fileno(g_log));
  }
}

static int send_blob(const unsigned char *data, size_t size,
                     const char *label) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    logf_("relay: socket failed for %s", label);
    return -1;
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(9021);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    logf_("relay: connect to 127.0.0.1:9021 failed for %s", label);
    close(sock);
    return -2;
  }

  size_t sent = 0;
  while (sent < size) {
    ssize_t n = send(sock, data + sent, size - sent, 0);
    if (n <= 0) {
      logf_("relay: send failed for %s after %zu bytes", label, sent);
      close(sock);
      return -3;
    }
    sent += (size_t)n;
  }

  close(sock);
  logf_("relay: sent %s (%zu bytes)", label, sent);
  return 0;
}

int main(void) {
  mkdir(LOG_DIR, 0777);

  size_t proxy_len =
      (size_t)(_binary_ytps5_dump_elf_end - _binary_ytps5_dump_elf_start);
  size_t injector_len = (size_t)(_binary_ytps5_injector_elf_end -
                                 _binary_ytps5_injector_elf_start);

  logf_("relay: embedded proxy=%zu injector=%zu", proxy_len, injector_len);

  if (send_blob(_binary_ytps5_dump_elf_start, proxy_len, "proxy") != 0) {
    return 1;
  }
  sleep(1);
  if (send_blob(_binary_ytps5_injector_elf_start, injector_len,
                "injector") != 0) {
    return 2;
  }
  return 0;
}
