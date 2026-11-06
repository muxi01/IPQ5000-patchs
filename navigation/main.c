#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ENTRIES         15
#define MAX_LINE_LEN        512
#define MAX_LABEL_LEN       64
#define MAX_URL_LEN         512
#define MAX_PATH_LEN        128
#define MAX_DESC_LEN        64
#define MAX_ICON_LEN        8
#define TEMPLATE_BUFFER     (1024 * 64)
#define PLACEHOLDER         "<!--GRID_PLACEHOLDER-->"

extern char _binary_nav_template_html_start[];
extern char _binary_nav_template_html_end[];

typedef struct {
    char path[MAX_PATH_LEN];
    char label[MAX_LABEL_LEN];
    char desc[MAX_DESC_LEN];
    char icon[MAX_ICON_LEN];
    char url[MAX_URL_LEN];
} NavEntry;

static int  parse_nginx_conf(const char *conf_path, NavEntry *entries,
                int max_entries);
static int  build_html(const NavEntry *entries, int count,
                const char *base_url, char *out, int out_size);
static void url_encode(const char *src, char *dst, int dst_size);
static void resolve_url(const char *base_url, const char *nav_url,
                char *out, int out_size);
static void icon_text(const char *label, char *out, int out_size);

#define COLS 4
#define TOTAL_SLOTS 12

static const char *icon_colors[] = {
    "#1e90ff", "#00b894", "#e17055", "#6c5ce7",
    "#0984e3", "#f39c12", "#d63031", "#636e72",
    "#a29bfe", "#fd79a8", "#2ed573", "#eccc68"
};

/*
    location /api-1/ {
        #navigation-lable: server-1
        #navigation-description: 服务1
        #navigation-icon: SF
        #navigation-url: http://192.168.10.200/xxx

        proxy_pass http://192.168.10.200/;
    }
*/


int main(int argc, char *argv[])
{
    const char *conf_path   = NULL;
    const char *base_url    = "";
    const char *output_path = NULL;
    NavEntry entries[MAX_ENTRIES];
    char html[TEMPLATE_BUFFER];
    int count;
    int html_len;
    FILE *fp;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            conf_path = argv[++i];
        } else if (strcmp(argv[i], "-b") == 0 && i + 1 < argc) {
            base_url = argv[++i];
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_path = argv[++i];
        } else {
            fprintf(stderr,
                "Usage: %s -c <nginx.conf> -o <output.html> [-b <base_url>]\n"
                "  -c   Path to nginx configuration file (required)\n"
                "  -o   Output HTML file path (required)\n"
                "  -b   Base URL prefix for relative navigation URLs\n",
                argv[0]);
            return 1;
        }
    }

    if (!conf_path) {
        fprintf(stderr, "Error: -c <nginx.conf> is required\n");
        return 1;
    }
    if (!output_path) {
        fprintf(stderr, "Error: -o <output.html> is required\n");
        return 1;
    }

    if (strlen(base_url) > 0 && base_url[strlen(base_url) - 1] == '/') {
        ((char *)base_url)[strlen(base_url) - 1] = '\0';
    }

    count = parse_nginx_conf(conf_path, entries, MAX_ENTRIES);
    printf("Parsed %d navigation entries from %s\n", count, conf_path);

    html_len = build_html(entries, count, base_url, html, sizeof(html));

    fp = fopen(output_path, "w");
    if (!fp) {
        perror("fopen output");
        return 1;
    }
    fwrite(html, 1, html_len, fp);
    fclose(fp);

    printf("Generated %s (%d bytes)\n", output_path, html_len);
    return 0;
}

static int parse_nginx_conf(const char *conf_path, NavEntry *entries,
        int max_entries)
{
    FILE *fp;
    char line[MAX_LINE_LEN];
    char trimmed[MAX_LINE_LEN];
    int count = 0;
    int in_location = 0;
    char location_path[MAX_PATH_LEN] = "";
    char nav_label[MAX_LABEL_LEN] = "";
    char nav_desc[MAX_DESC_LEN] = "";
    char nav_icon[MAX_ICON_LEN] = "";
    char nav_url[MAX_URL_LEN] = "";
    char *p;

    fp = fopen(conf_path, "r");
    if (!fp) {
        perror("fopen nginx.conf");
        return 0;
    }

    while (fgets(line, sizeof(line), fp) && count < max_entries) {
        char *s = line;
        p = trimmed;
        while (*s == ' ' || *s == '\t') s++;
        while (*s && *s != '\n' && *s != '\r') *p++ = *s++;
        *p = '\0';

        if (strncmp(trimmed, "location", 8) == 0 && strchr(trimmed, '{')) {
            in_location = 1;
            location_path[0] = '\0';
            nav_label[0] = '\0';
            nav_desc[0] = '\0';
            nav_icon[0] = '\0';
            nav_url[0] = '\0';

            p = trimmed + 8;
            while (*p == ' ' || *p == '\t') p++;
            {
                char *end = p;
                while (*end && *end != ' ' && *end != '\t' && *end != '{')
                    end++;
                {
                    int len = (int)(end - p);
                    if (len >= MAX_PATH_LEN) len = MAX_PATH_LEN - 1;
                    memcpy(location_path, p, len);
                    location_path[len] = '\0';
                }
            }
            continue;
        }

        if (trimmed[0] == '}' && in_location) {
            if (nav_label[0] && nav_url[0] && count < max_entries) {
                strncpy(entries[count].path, location_path,
                    MAX_PATH_LEN - 1);
                entries[count].path[MAX_PATH_LEN - 1] = '\0';
                strncpy(entries[count].label, nav_label,
                    MAX_LABEL_LEN - 1);
                entries[count].label[MAX_LABEL_LEN - 1] = '\0';
                strncpy(entries[count].desc, nav_desc,
                    MAX_DESC_LEN - 1);
                entries[count].desc[MAX_DESC_LEN - 1] = '\0';
                strncpy(entries[count].icon, nav_icon,
                    MAX_ICON_LEN - 1);
                entries[count].icon[MAX_ICON_LEN - 1] = '\0';
                strncpy(entries[count].url, nav_url, MAX_URL_LEN - 1);
                entries[count].url[MAX_URL_LEN - 1] = '\0';
                count++;
            }
            in_location = 0;
            continue;
        }

        if (in_location && strncmp(trimmed, "#navigation-lable:", 18) == 0) {
            p = trimmed + 18;
            while (*p == ' ' || *p == '\t') p++;
            strncpy(nav_label, p, MAX_LABEL_LEN - 1);
            nav_label[MAX_LABEL_LEN - 1] = '\0';
            continue;
        }

        if (in_location && strncmp(trimmed, "#navigation-description:", 24) == 0) {
            p = trimmed + 24;
            while (*p == ' ' || *p == '\t') p++;
            strncpy(nav_desc, p, MAX_DESC_LEN - 1);
            nav_desc[MAX_DESC_LEN - 1] = '\0';
            continue;
        }

        if (in_location && strncmp(trimmed, "#navigation-icon:", 17) == 0) {
            p = trimmed + 17;
            while (*p == ' ' || *p == '\t') p++;
            strncpy(nav_icon, p, MAX_ICON_LEN - 1);
            nav_icon[MAX_ICON_LEN - 1] = '\0';
            continue;
        }

        if (in_location && strncmp(trimmed, "#navigation-url:", 16) == 0) {
            p = trimmed + 16;
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '`') {
                p++;
                strncpy(nav_url, p, MAX_URL_LEN - 1);
                nav_url[MAX_URL_LEN - 1] = '\0';
                {
                    char *end = strrchr(nav_url, '`');
                    if (end) *end = '\0';
                }
            } else {
                strncpy(nav_url, p, MAX_URL_LEN - 1);
                nav_url[MAX_URL_LEN - 1] = '\0';
            }
            continue;
        }
    }

    fclose(fp);
    return count;
}

static int build_html(const NavEntry *entries, int count,
        const char *base_url, char *out, int out_size)
{
    char card_buf[2048];
    char encoded_url[MAX_URL_LEN * 3];
    char resolved_url[MAX_URL_LEN];
    char icon_str[8];
    char *template_start = _binary_nav_template_html_start;
    char *template_end   = _binary_nav_template_html_end;
    int template_len = (int)(template_end - template_start);
    char *placeholder_pos;
    int pos = 0;
    int i;
    int show_count;

    show_count = count < TOTAL_SLOTS ? count : TOTAL_SLOTS;
    placeholder_pos = strstr(template_start, PLACEHOLDER);

    if (!placeholder_pos) {
        if (pos + template_len < out_size) {
            memcpy(out + pos, template_start, template_len);
            pos += template_len;
        }
        if (pos < out_size) out[pos] = '\0';
        else out[out_size - 1] = '\0';
        return pos;
    }

    {
        int before_len = (int)(placeholder_pos - template_start);
        if (pos + before_len < out_size) {
            memcpy(out + pos, template_start, before_len);
            pos += before_len;
        }
    }

    for (i = 0; i < TOTAL_SLOTS; i++) {
        if (i < show_count) {
            resolve_url(base_url, entries[i].url,
                resolved_url, sizeof(resolved_url));
            url_encode(resolved_url, encoded_url, sizeof(encoded_url));
            if (entries[i].icon[0]) {
                strncpy(icon_str, entries[i].icon, sizeof(icon_str) - 1);
                icon_str[sizeof(icon_str) - 1] = '\0';
            } else {
                icon_text(entries[i].label, icon_str, sizeof(icon_str));
            }
            snprintf(card_buf, sizeof(card_buf),
                "<a class=\"card\" href=\"%s\" "
                "target=\"_blank\" rel=\"noopener noreferrer\" "
                "title=\"%s\">"
                "<div class=\"icon\" style=\"background:%s\">%s</div>"
                "<div class=\"title\">%s</div>"
                "<div class=\"desc\">%s</div>"
                "</a>\n",
                encoded_url,
                resolved_url,
                icon_colors[i % COLS],
                icon_str,
                entries[i].label,
                entries[i].desc);
        } else {
            snprintf(card_buf, sizeof(card_buf),
                "<div class=\"card empty\">"
                "<div class=\"icon\">-</div>"
                "<div class=\"title\">N/A</div>"
                "</div>\n");
        }

        {
            int card_len = (int)strlen(card_buf);
            if (pos + card_len < out_size) {
                memcpy(out + pos, card_buf, card_len);
                pos += card_len;
            }
        }
    }

    {
        int after_len = template_len
            - (int)(placeholder_pos + strlen(PLACEHOLDER) - template_start);
        char *after_start = placeholder_pos + strlen(PLACEHOLDER);
        if (pos + after_len < out_size) {
            memcpy(out + pos, after_start, after_len);
            pos += after_len;
        }
    }

    if (pos < out_size) {
        out[pos] = '\0';
    } else {
        out[out_size - 1] = '\0';
    }

    return pos;
}

static void url_encode(const char *src, char *dst, int dst_size)
{
    static const char hex[] = "0123456789ABCDEF";
    const char *s = src;
    char *d = dst;
    int remain = dst_size - 1;

    while (*s && remain > 0) {
        if ((*s >= 'A' && *s <= 'Z') ||
            (*s >= 'a' && *s <= 'z') ||
            (*s >= '0' && *s <= '9') ||
            *s == '-' || *s == '_' || *s == '.' || *s == '~' ||
            *s == '/' || *s == ':' || *s == '?' || *s == '&' ||
            *s == '=' || *s == '#') {
            if (remain < 1) break;
            *d++ = *s;
            remain--;
        } else {
            if (remain < 3) break;
            *d++ = '%';
            *d++ = hex[(unsigned char)*s >> 4];
            *d++ = hex[(unsigned char)*s & 0x0F];
            remain -= 3;
        }
        s++;
    }

    *d = '\0';
}

static void resolve_url(const char *base_url, const char *nav_url,
        char *out, int out_size)
{
    if (strncmp(nav_url, "http://", 7) == 0 ||
        strncmp(nav_url, "https://", 8) == 0 ||
        strncmp(nav_url, "//", 2) == 0) {
        strncpy(out, nav_url, out_size - 1);
        out[out_size - 1] = '\0';
        return;
    }

    if (base_url[0] && nav_url[0] == '/') {
        snprintf(out, out_size, "%s%s", base_url, nav_url);
        return;
    }

    strncpy(out, nav_url, out_size - 1);
    out[out_size - 1] = '\0';
}

static void icon_text(const char *label, char *out, int out_size)
{
    int len = 0;
    const char *p = label;

    while (*p && len < 2) {
        unsigned char c = (unsigned char)*p;
        if (c < 0x80) {
            out[len++] = *p++;
        } else {
            if (len + 1 >= 2) break;
            if ((c & 0xE0) == 0xC0) {
                out[len++] = *p++;
                out[len++] = *p++;
            } else if ((c & 0xF0) == 0xE0) {
                if (len + 2 > 2) break;
                out[len++] = *p++;
                out[len++] = *p++;
                out[len++] = *p++;
            } else if ((c & 0xF8) == 0xF0) {
                p += 4;
                if (len + 2 > 2) break;
                continue;
            } else {
                p++;
            }
        }
    }

    out[len] = '\0';
}