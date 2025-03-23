#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrender.h>
#include <X11/Xft/Xft.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>  // For variable argument logging

#include <sys/inotify.h>
#include <sys/select.h>
#include <limits.h>
#include <ctype.h>   // For isspace()

#define MAX_ITEMS 100
#define MAX_LABEL 256
#define TERMINAL "alacritty -e"
#define SUBMENU_INDENT 20
#define GRACE_PERIOD_SECONDS 0.1
#define VERTICAL_PADDING 4
#define MAX_SEGMENTS 10

/* Global log file pointer. All log messages will be appended to this file. */
FILE *log_file = NULL;

/* Simple logging macro */
#define LOG(...) do { \
    if (log_file) { \
        fprintf(log_file, __VA_ARGS__); \
        fprintf(log_file, "\n"); \
        fflush(log_file); \
    } \
} while(0)

struct Config {
    unsigned long fg, bg, selfg, selbg, border_color;
    double alpha;      // Transparency for non-selected items.
    double selalpha;   // Transparency for selected (hover) items.
    char *font;
    int border_width;
    int submenu_offset;
    double mouse_delay;
    int icon_left_padding;
    int icon_right_padding;
};

struct LabelSegment {
    char text[MAX_LABEL];
    unsigned long color; // e.g. 0xRRGGBB
};

struct Item {
    char label[MAX_LABEL];   // Fallback plain text label.
    char output[MAX_LABEL];
    struct LabelSegment segments[MAX_SEGMENTS];
    int nsegments;
    struct Item *submenu;
    int nsubitems;
    int separator;           // 0 = normal, 1 = separator
};

Display *dpy;
int screen;
Window win;
Window submenu_win = 0;
Window root;
GC gc;
XftFont *xft_font = NULL;
XVisualInfo menu_vi;
Colormap menu_cmap;
XftDraw *main_draw = NULL;
XftDraw *submenu_draw = NULL;
XftColor xft_fg;
XftColor xft_selfg;
struct Item items[MAX_ITEMS];
int nitems = 0;
int selected_item = -1, selected_subitem = -1;
int menuwidth = 150, itemheight = 16, menuheight = 0;
int max_menuwidth = 150;
struct Config config = {
    .fg = 0xFFFFFF,
    .bg = 0x1A1A1A,
    .selfg = 0x000000,
    .selbg = 0xFFD700,
    .border_color = 0x333333,  // Default border color.
    .alpha = 0.8,
    .selalpha = 0.7,          // Could be set differently if you want.
    .font = "fixed",
    .border_width = 2,
    .submenu_offset = 20,
    .mouse_delay = 0.1,
    .icon_left_padding = 10,
    .icon_right_padding = 5
};
int menu_x = 0, menu_y = 0;
int last_selected_item = -1;
static struct timeval menu_open_time;

/* Function declarations */
static void setup(void);
static void read_input(const char *path);
static void drawmenu(void);
static void draw_item(Window w, int x, int y, int width, struct Item *item, int selected);
static void draw_submenu(struct Item *parent);
static void handle_event(XEvent *ev);
static void load_config(const char *path);
static void load_font(void);
static void cleanup(void);
static void show_menu(int x, int y);
static int text_width(const char *text);
static void execute_command(const char *cmd);
static void calculate_menu_width(void);
static int calculate_submenu_width(struct Item *parent);
static void create_submenu_window(struct Item *parent, int x, int y);
static void destroy_submenu_window(void);
static void regrab_button(void);
static int is_menu_mapped(void);
static void parse_label(const char *input, struct LabelSegment segments[], int *nsegments);

/* New helper function: measure total text width of an Item by summing all segments */
static int item_text_width(struct Item *item) {
    if (item->nsegments > 0) {
        int total_width = 0;
        for (int i = 0; i < item->nsegments; i++) {
            total_width += text_width(item->segments[i].text);
        }
        return total_width;
    } else {
        return text_width(item->label);
    }
}

int main(int argc, char *argv[]) {
    if (!(dpy = XOpenDisplay(NULL))) {
        fprintf(stderr, "gmenu: cannot open display\n");
        exit(1);
    }
    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);

    const char *home = getenv("HOME");
    char config_path[256], items_path[256];
    snprintf(config_path, sizeof(config_path), "%s/.config/gmenu/.gmenurc", home);
    snprintf(items_path, sizeof(items_path), "%s/.config/gmenu/.gmenu_items", home);
    load_config(config_path);

    load_font();
    read_input(items_path);
    calculate_menu_width();
    setup();
    regrab_button();

    XEvent ev;
    while (!XNextEvent(dpy, &ev))
        handle_event(&ev);

    cleanup();
    return 0;
}

static void load_font(void) {
    xft_font = XftFontOpenName(dpy, screen, config.font);
    if (!xft_font) {
        xft_font = XftFontOpenName(dpy, screen, "fixed");
        if (!xft_font) {
            fprintf(stderr, "gmenu: No suitable font found.\n");
            exit(1);
        }
    }
    itemheight = xft_font->ascent + xft_font->descent + 4 + VERTICAL_PADDING;
}

static void setup(void) {
    XSetWindowAttributes wa;
    menuheight = nitems * itemheight + 2 * config.border_width + 4;
    if (!XMatchVisualInfo(dpy, screen, 32, TrueColor, &menu_vi)) {
        fprintf(stderr, "gmenu: No 32-bit TrueColor visual available\n");
        exit(1);
    }
    menu_cmap = XCreateColormap(dpy, root, menu_vi.visual, AllocNone);
    wa.colormap = menu_cmap;
    wa.background_pixmap = None;
    wa.background_pixel = config.bg;
    wa.border_pixel = config.border_color;
    wa.event_mask = ExposureMask | ButtonPressMask | PointerMotionMask | LeaveWindowMask;
    wa.override_redirect = True;
    win = XCreateWindow(dpy, root, 0, 0, max_menuwidth, menuheight,
                        config.border_width, menu_vi.depth, InputOutput, menu_vi.visual,
                        CWColormap | CWBorderPixel | CWBackPixel | CWEventMask | CWOverrideRedirect, &wa);
    main_draw = XftDrawCreate(dpy, win, menu_vi.visual, menu_cmap);
    XRenderPictFormat *fmt = XRenderFindVisualFormat(dpy, menu_vi.visual);
    if (!fmt) {
        fprintf(stderr, "gmenu: No matching XRenderPictFormat for visual\n");
        exit(1);
    }
    Picture pict = XRenderCreatePicture(dpy, win, fmt, 0, NULL);
    if (!pict) {
        fprintf(stderr, "gmenu: Failed to create picture for main window\n");
        exit(1);
    }
    XRenderColor bg = {
        .red = ((config.bg >> 16) & 0xFF) * 0xFFFF / 0xFF,
        .green = ((config.bg >> 8) & 0xFF) * 0xFFFF / 0xFF,
        .blue = (config.bg & 0xFF) * 0xFFFF / 0xFF,
        .alpha = config.alpha * 0xFFFF
    };
    XRenderFillRectangle(dpy, PictOpSrc, pict, &bg, 0, 0, max_menuwidth, menuheight);
    XRenderFreePicture(dpy, pict);
    gc = XCreateGC(dpy, win, 0, NULL);
    XRenderColor render_color;
    render_color.red = ((config.fg >> 16) & 0xFF) * 0xFFFF / 0xFF;
    render_color.green = ((config.fg >> 8) & 0xFF) * 0xFFFF / 0xFF;
    render_color.blue = (config.fg & 0xFF) * 0xFFFF / 0xFF;
    render_color.alpha = 0xFFFF;
    XftColorAllocValue(dpy, menu_vi.visual, menu_cmap, &render_color, &xft_fg);
    render_color.red = ((config.selfg >> 16) & 0xFF) * 0xFFFF / 0xFF;
    render_color.green = ((config.selfg >> 8) & 0xFF) * 0xFFFF / 0xFF;
    render_color.blue = (config.selfg & 0xFF) * 0xFFFF / 0xFF;
    render_color.alpha = 0xFFFF;
    XftColorAllocValue(dpy, menu_vi.visual, menu_cmap, &render_color, &xft_selfg);
}

static void read_input(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        // Fallback if no .gmenu_items
        nitems = 3;
        strncpy(items[0].label, "File Manager", MAX_LABEL - 1);
        strncpy(items[0].output, "thunar", MAX_LABEL - 1);
        items[0].submenu = NULL;
        items[0].nsubitems = 0;

        strncpy(items[1].label, "Reboot", MAX_LABEL - 1);
        strncpy(items[1].output, "reboot", MAX_LABEL - 1);
        items[1].submenu = NULL;
        items[1].nsubitems = 0;

        strncpy(items[2].label, "Shutdown", MAX_LABEL - 1);
        strncpy(items[2].output, "poweroff", MAX_LABEL - 1);
        items[2].submenu = NULL;
        items[2].nsubitems = 0;
        return;
    }

    char line[512];
    struct Item *current_parent = NULL;
    nitems = 0;

    while (fgets(line, sizeof(line), fp) && nitems < MAX_ITEMS) {
        line[strcspn(line, "\n")] = 0;  // strip newline
        if (!line[0])
            continue;

        // Separator line
        if (strcmp(line, "___") == 0) {
            struct Item sep_item = {0};
            sep_item.separator = 1;
            items[nitems++] = sep_item;
            current_parent = NULL;
            continue;
        }

        // Submenu lines start with "=="
        int is_submenu = (strncmp(line, "==", 2) == 0);
        char *text = is_submenu ? line + 2 : line;
        while (*text && isspace(*text)) text++;

        // We expect "label = command"
        char *delim = strstr(text, " = ");
        char label_buf[MAX_LABEL];
        char output_buf[MAX_LABEL];
        if (delim) {
            int labellen = delim - text;
            if (labellen >= MAX_LABEL) {
                labellen = MAX_LABEL - 1;
            }
            strncpy(label_buf, text, labellen);
            label_buf[labellen] = '\0';

            strncpy(output_buf, delim + 3, MAX_LABEL - 1);
            output_buf[MAX_LABEL - 1] = '\0';
        } else {
            strncpy(label_buf, text, MAX_LABEL - 1);
            label_buf[MAX_LABEL - 1] = '\0';
            output_buf[0] = '\0';
        }

        struct Item item = {0};
        item.separator = 0;
        parse_label(label_buf, item.segments, &item.nsegments);
        if (item.nsegments == 0) {
            // no color segments, store plain label
            strncpy(item.label, label_buf, MAX_LABEL - 1);
        }
        strncpy(item.output, output_buf, MAX_LABEL - 1);

        if (!is_submenu) {
            // top-level item
            items[nitems++] = item;
            current_parent = &items[nitems - 1];
        } else if (current_parent) {
            // Expand parent's submenu array
            struct Item *new_submenu = realloc(current_parent->submenu,
                                               (current_parent->nsubitems + 1) * sizeof(struct Item));
            if (!new_submenu) {
                fprintf(stderr, "gmenu: Memory allocation failed for submenu\n");
                fclose(fp);
                cleanup();
                exit(1);
            }
            current_parent->submenu = new_submenu;
            current_parent->submenu[current_parent->nsubitems++] = item;
        }
    }

    fclose(fp);
}

static void parse_label(const char *input, struct LabelSegment segments[], int *nsegments) {
    *nsegments = 0;
    while (*input && isspace(*input)) {
        input++;
    }
    const char *p = input;

    while (*p && *nsegments < MAX_SEGMENTS) {
        if (strncmp(p, "<color='", 8) == 0) {
            // e.g. <color='#0078D7'> ... </color>
            p += 8;  // Skip "<color='"
            char color_code[8] = {0};
            int i = 0;
            while (*p && *p != '\'' && i < 7) {
                color_code[i++] = *p++;
            }
            color_code[i] = '\0';
            if (color_code[0] == '#') {
                memmove(color_code, color_code + 1, strlen(color_code));
            }
            if (*p == '\'') p++;  // skip closing quote
            if (*p == '>') p++;   // skip '>'
            unsigned long color = strtoul(color_code, NULL, 16);

            // find the matching </color>
            const char *end = strstr(p, "</color>");
            if (end) {
                int len = end - p;
                if (len >= MAX_LABEL) {
                    len = MAX_LABEL - 1;
                }
                strncpy(segments[*nsegments].text, p, len);
                segments[*nsegments].text[len] = '\0';

                // trim whitespace
                // quick trim function inline
                char *seg_str = segments[*nsegments].text;
                while (*seg_str && isspace(*seg_str)) seg_str++;
                memmove(segments[*nsegments].text, seg_str, strlen(seg_str)+1);
                // trailing
                char *endtrim = segments[*nsegments].text + strlen(segments[*nsegments].text) - 1;
                while (endtrim > segments[*nsegments].text && isspace(*endtrim)) {
                    *endtrim-- = '\0';
                }

                segments[*nsegments].color = color;
                (*nsegments)++;
                p = end + 8;  // skip "</color>"
            } else {
                break;
            }
        } else {
            // plain text until next <color= or end
            const char *start = p;
            while (*p && strncmp(p, "<color='", 8) != 0) {
                p++;
            }
            int len = p - start;
            if (len >= MAX_LABEL) {
                len = MAX_LABEL - 1;
            }
            strncpy(segments[*nsegments].text, start, len);
            segments[*nsegments].text[len] = '\0';

            // trim whitespace
            char *seg_str = segments[*nsegments].text;
            while (*seg_str && isspace(*seg_str)) seg_str++;
            memmove(segments[*nsegments].text, seg_str, strlen(seg_str)+1);
            char *endtrim = segments[*nsegments].text + strlen(segments[*nsegments].text) - 1;
            while (endtrim > segments[*nsegments].text && isspace(*endtrim)) {
                *endtrim-- = '\0';
            }

            segments[*nsegments].color = config.fg;
            (*nsegments)++;
        }
    }
}

static int calculate_submenu_width(struct Item *parent) {
    int submenu_width = 150; // Minimum width
    for (int i = 0; i < parent->nsubitems; i++) {
        // measure the full text (including segments)
        int textw = item_text_width(&parent->submenu[i]);
        // Add padding/indent, etc.
        int width = textw
                    + 20  // arbitrary extra space
                    + SUBMENU_INDENT
                    + config.icon_left_padding
                    + config.icon_right_padding;
        if (width > submenu_width) {
            submenu_width = width;
        }
    }
    return submenu_width;
}

static void calculate_menu_width(void) {
    if (!xft_font) {
        LOG("Error: Font not loaded in calculate_menu_width");
        fprintf(stderr, "gmenu: Font not loaded\n");
        exit(1);
    }
    max_menuwidth = 150;
    for (int i = 0; i < nitems; i++) {
        // measure top-level item text (including color segments)
        int textw = item_text_width(&items[i]);
        // If item has a submenu, maybe add some extra space for the arrow
        // But we already do that in the draw function. Let's just be safe:
        if (items[i].nsubitems > 0) {
            textw += 20; // space for the arrow
        }
        // add left/right padding
        int width = textw
                    + config.icon_left_padding
                    + config.icon_right_padding
                    + 20; // any extra margin you want

        if (width > max_menuwidth) {
            max_menuwidth = width;
        }

        // also consider the subitems, if you want to ensure the main menu can accommodate them
        // but typically the submenu has its own window. We'll skip that here.
    }
    LOG("Calculated max_menuwidth: %d", max_menuwidth);
}

static void create_submenu_window(struct Item *parent, int x, int y) {
    destroy_submenu_window();
    XSetWindowAttributes wa;
    int submenu_width = calculate_submenu_width(parent) + 2 * config.border_width;
    int submenu_height = parent->nsubitems * itemheight + 2 * config.border_width;
    int screen_width = DisplayWidth(dpy, screen);
    int screen_height = DisplayHeight(dpy, screen);
    int proposed_x = x + config.submenu_offset;
    if (proposed_x + submenu_width > screen_width)
        proposed_x = menu_x - submenu_width - config.submenu_offset;
    if (proposed_x < 0)
        proposed_x = 0;
    if (y + submenu_height > screen_height)
        y = screen_height - submenu_height;
    if (y < 0)
        y = 0;

    wa.colormap = menu_cmap;
    wa.background_pixmap = None;
    wa.background_pixel = config.bg;
    wa.border_pixel = config.border_color;
    wa.event_mask = ExposureMask | ButtonPressMask | PointerMotionMask | LeaveWindowMask;
    wa.override_redirect = True;
    submenu_win = XCreateWindow(dpy, root, proposed_x, y, submenu_width, submenu_height,
                                config.border_width, menu_vi.depth, InputOutput, menu_vi.visual,
                                CWColormap | CWBorderPixel | CWBackPixel | CWEventMask | CWOverrideRedirect, &wa);
    submenu_draw = XftDrawCreate(dpy, submenu_win, menu_vi.visual, menu_cmap);
    XRenderPictFormat *fmt = XRenderFindVisualFormat(dpy, menu_vi.visual);
    if (!fmt) {
        LOG("Error: No matching XRenderPictFormat for submenu visual");
        fprintf(stderr, "gmenu: No matching XRenderPictFormat for submenu visual\n");
        exit(1);
    }
    Picture pict = XRenderCreatePicture(dpy, submenu_win, fmt, 0, NULL);
    if (!pict) {
        LOG("Error: Failed to create picture for submenu window");
        fprintf(stderr, "gmenu: Failed to create picture for submenu window\n");
        exit(1);
    }
    XRenderColor bg = {
        .red = ((config.bg >> 16) & 0xFF) * 0xFFFF / 0xFF,
        .green = ((config.bg >> 8) & 0xFF) * 0xFFFF / 0xFF,
        .blue = (config.bg & 0xFF) * 0xFFFF / 0xFF,
        .alpha = config.alpha * 0xFFFF
    };
    XRenderFillRectangle(dpy, PictOpSrc, pict, &bg, 0, 0, submenu_width, submenu_height);
    XRenderFreePicture(dpy, pict);
    XMapRaised(dpy, submenu_win);
    LOG("Created submenu window at (%d,%d) with size %dx%d", proposed_x, y, submenu_width, submenu_height);
}

static void destroy_submenu_window(void) {
    if (submenu_win) {
        if (submenu_draw) {
            XftDrawDestroy(submenu_draw);
            submenu_draw = NULL;
        }
        XDestroyWindow(dpy, submenu_win);
        submenu_win = 0;
        selected_subitem = -1;
        LOG("Destroyed submenu window");
    }
}

static void drawmenu(void) {
    int y = config.border_width;
    XRenderPictFormat *fmt = XRenderFindVisualFormat(dpy, menu_vi.visual);
    if (!fmt) {
        LOG("Error: No matching XRenderPictFormat for main menu in drawmenu");
        fprintf(stderr, "gmenu: No matching XRenderPictFormat for main menu\n");
        exit(1);
    }
    Picture pict = XRenderCreatePicture(dpy, win, fmt, 0, NULL);
    if (!pict) {
        LOG("Error: Failed to create picture in drawmenu");
        fprintf(stderr, "gmenu: Failed to create picture for drawmenu\n");
        exit(1);
    }
    XRenderColor bg = {
        .red = ((config.bg >> 16) & 0xFF) * 0xFFFF / 0xFF,
        .green = ((config.bg >> 8) & 0xFF) * 0xFFFF / 0xFF,
        .blue = (config.bg & 0xFF) * 0xFFFF / 0xFF,
        .alpha = config.alpha * 0xFFFF
    };
    XRenderFillRectangle(dpy, PictOpSrc, pict, &bg, 0, 0, max_menuwidth, menuheight);
    XRenderFreePicture(dpy, pict);

    for (int i = 0; i < nitems; i++) {
        draw_item(win, config.border_width, y,
                  max_menuwidth - 2 * config.border_width,
                  &items[i], i == selected_item);
        y += itemheight;
    }

    // Submenu creation if hovered item has subitems
    if (selected_item != -1 && items[selected_item].nsubitems > 0) {
        if (selected_item != last_selected_item) {
            int submenu_x = menu_x + max_menuwidth + config.submenu_offset;
            int submenu_y = menu_y + (selected_item * itemheight) + config.border_width;
            create_submenu_window(&items[selected_item], submenu_x, submenu_y);
            draw_submenu(&items[selected_item]);
            last_selected_item = selected_item;
        }
    } else {
        destroy_submenu_window();
        last_selected_item = -1;
    }
    XFlush(dpy);
}

static void draw_submenu(struct Item *parent) {
    if (!submenu_win)
        return;

    XRenderPictFormat *fmt = XRenderFindVisualFormat(dpy, menu_vi.visual);
    if (!fmt) {
        LOG("Error: No matching XRenderPictFormat for submenu in draw_submenu");
        fprintf(stderr, "gmenu: No matching XRenderPictFormat for submenu\n");
        exit(1);
    }
    Picture pict = XRenderCreatePicture(dpy, submenu_win, fmt, 0, NULL);
    if (!pict) {
        LOG("Error: Failed to create picture in draw_submenu");
        fprintf(stderr, "gmenu: Failed to create picture for draw_submenu\n");
        exit(1);
    }
    XRenderColor bg = {
        .red = ((config.bg >> 16) & 0xFF) * 0xFFFF / 0xFF,
        .green = ((config.bg >> 8) & 0xFF) * 0xFFFF / 0xFF,
        .blue = (config.bg & 0xFF) * 0xFFFF / 0xFF,
        .alpha = config.alpha * 0xFFFF
    };

    int submenu_width = calculate_submenu_width(parent) + 2 * config.border_width;
    int submenu_height = parent->nsubitems * itemheight + 2 * config.border_width;

    XRenderFillRectangle(dpy, PictOpSrc, pict, &bg, 0, 0, submenu_width, submenu_height);
    XRenderFreePicture(dpy, pict);

    int submenu_width_items = submenu_width - 2 * config.border_width;

    for (int i = 0; i < parent->nsubitems; i++) {
        draw_item(submenu_win, config.border_width, i * itemheight,
                  submenu_width_items, &parent->submenu[i], i == selected_subitem);
    }

    XFlush(dpy);
}

static void draw_item(Window w, int x, int y, int width, struct Item *item, int selected) {
    // If item is a separator, draw a horizontal line
    if (item->separator) {
        int padding = 1;
        int line_thickness = 1;
        int line_y = y + itemheight / 2;
        XRenderPictFormat *fmt = XRenderFindVisualFormat(dpy, menu_vi.visual);
        Picture pict = XRenderCreatePicture(dpy, w, fmt, 0, NULL);

        XRenderColor line_color = {
            .red   = ((config.border_color >> 16) & 0xFF) * 0xFFFF / 0xFF,
            .green = ((config.border_color >> 8)  & 0xFF) * 0xFFFF / 0xFF,
            .blue  = ((config.border_color)       & 0xFF) * 0xFFFF / 0xFF,
            .alpha = config.alpha * 0xFFFF
        };
        XRenderFillRectangle(dpy, PictOpSrc, pict, &line_color,
                             x + padding, line_y, width - 2 * padding, line_thickness);

        XRenderFreePicture(dpy, pict);
        return;
    }

    // text_x offset for icons/padding
    int text_x = x
                 + config.icon_left_padding
                 + (w == submenu_win ? SUBMENU_INDENT : 0);

    // center text vertically
    int text_y = y + (itemheight + xft_font->ascent - xft_font->descent) / 2;

    // fill background with appropriate transparency
    XRenderPictFormat *fmt = XRenderFindVisualFormat(dpy, menu_vi.visual);
    Picture pict = XRenderCreatePicture(dpy, w, fmt, 0, NULL);

    double alpha_val = selected ? config.selalpha : config.alpha;
    XRenderColor item_bg = {
        .red   = (((selected ? config.selbg : config.bg) >> 16) & 0xFF) * 0xFFFF / 0xFF,
        .green = (((selected ? config.selbg : config.bg) >> 8)  & 0xFF) * 0xFFFF / 0xFF,
        .blue  = ((selected ? config.selbg : config.bg) & 0xFF) * 0xFFFF / 0xFF,
        .alpha = alpha_val * 0xFFFF
    };

    // Fill the row
    XRenderFillRectangle(dpy, PictOpSrc, pict, &item_bg, x, y, width, itemheight);
    XRenderFreePicture(dpy, pict);

    // Draw segments if any
    if (item->nsegments > 0) {
        for (int i = 0; i < item->nsegments; i++) {
            XRenderColor render_color = {
                .red   = ((item->segments[i].color >> 16) & 0xFF) * 0xFFFF / 0xFF,
                .green = ((item->segments[i].color >> 8)  & 0xFF) * 0xFFFF / 0xFF,
                .blue  = ((item->segments[i].color)       & 0xFF) * 0xFFFF / 0xFF,
                .alpha = 0xFFFF
            };
            XftColor seg_color;
            XftColorAllocValue(dpy, menu_vi.visual, menu_cmap, &render_color, &seg_color);

            // If hovered, override color with selfg
            XftColor *draw_color = selected ? &xft_selfg : &seg_color;

            // Draw this segment
            XftDrawStringUtf8(
                (w == win) ? main_draw : submenu_draw,
                draw_color, xft_font,
                text_x, text_y,
                (FcChar8 *)item->segments[i].text,
                strlen(item->segments[i].text)
            );

            // measure the segment’s width
            XGlyphInfo extents;
            XftTextExtentsUtf8(dpy, xft_font,
                               (FcChar8 *)item->segments[i].text,
                               strlen(item->segments[i].text), &extents);
            int seg_width = extents.xOff;

            text_x += seg_width;
            XftColorFree(dpy, menu_vi.visual, menu_cmap, &seg_color);
        }
    } else {
        // plain text
        XftDrawStringUtf8(
            (w == win) ? main_draw : submenu_draw,
            selected ? &xft_selfg : &xft_fg,
            xft_font,
            text_x, text_y,
            (FcChar8 *)item->label,
            strlen(item->label)
        );
    }

    // Draw arrow if this is a main menu item with a submenu
    if (w == win && item->nsubitems > 0) {
        char arrow[] = "▶";
        XGlyphInfo extents;
        XftTextExtentsUtf8(dpy, xft_font, (FcChar8 *)arrow, strlen(arrow), &extents);
        int arrow_width = extents.xOff;
        int arrow_x = x + width - arrow_width - 10;

        XftDrawStringUtf8(
            main_draw,
            selected ? &xft_selfg : &xft_fg,
            xft_font,
            arrow_x, text_y,
            (FcChar8 *)arrow, strlen(arrow)
        );
    }
}

static int is_menu_mapped(void) {
    XWindowAttributes wa;
    if (!XGetWindowAttributes(dpy, win, &wa))
        return 0;
    return wa.map_state == IsViewable;
}

static void handle_event(XEvent *ev) {
    switch (ev->type) {
    case Expose:
        if (ev->xexpose.window == win) {
            drawmenu();
        } else if (ev->xexpose.window == submenu_win &&
                   selected_item != -1 && items[selected_item].nsubitems > 0) {
            draw_submenu(&items[selected_item]);
        }
        break;

    case ButtonPress:
        if (ev->xbutton.button == Button3) {
            Window root_return, child_return;
            int root_x, root_y, win_x, win_y;
            unsigned int mask;
            if (XQueryPointer(dpy, root, &root_return, &child_return,
                              &root_x, &root_y, &win_x, &win_y, &mask)) {
                if (child_return == None && ev->xbutton.window == root) {
                    if (!is_menu_mapped()) {
                        show_menu(ev->xbutton.x_root, ev->xbutton.y_root);
                        LOG("Menu shown at (%d,%d)", ev->xbutton.x_root, ev->xbutton.y_root);
                    }
                    XAllowEvents(dpy, SyncPointer, CurrentTime);
                } else {
                    XAllowEvents(dpy, ReplayPointer, CurrentTime);
                }
            }
        } else if (ev->xbutton.window == win) {
            int y = ev->xbutton.y;
            if (y >= 0 && y < menuheight) {
                int item_y = y - config.border_width;
                selected_item = item_y / itemheight;
                if (selected_item >= 0 && selected_item < nitems) {
                    if (items[selected_item].nsubitems == 0 &&
                        items[selected_item].output[0] != '\0') {
                        LOG("Executing command: %s", items[selected_item].output);
                        execute_command(items[selected_item].output);
                        XUngrabPointer(dpy, CurrentTime);
                        XUnmapWindow(dpy, win);
                        regrab_button();
                    }
                }
            }
        } else if (ev->xbutton.window == submenu_win) {
            int y = ev->xbutton.y;
            int sub_y = y - config.border_width;
            selected_subitem = sub_y / itemheight;
            if (selected_subitem >= 0 &&
                selected_subitem < items[selected_item].nsubitems) {
                LOG("Executing submenu command: %s",
                    items[selected_item].submenu[selected_subitem].output);
                execute_command(items[selected_item].submenu[selected_subitem].output);
                XUngrabPointer(dpy, CurrentTime);
                destroy_submenu_window();
                XUnmapWindow(dpy, win);
                regrab_button();
            }
        } else if (is_menu_mapped()) {
            XUngrabPointer(dpy, CurrentTime);
            destroy_submenu_window();
            XUnmapWindow(dpy, win);
            regrab_button();
        }
        break;

    case MotionNotify:
        if (ev->xmotion.window == win) {
            int y = ev->xmotion.y;
            if (y >= 0 && y < menuheight) {
                int item_y = y - config.border_width;
                int new_selected_item = item_y / itemheight;
                if (new_selected_item >= 0 && new_selected_item < nitems &&
                    new_selected_item != selected_item) {
                    selected_item = new_selected_item;
                    selected_subitem = -1;
                    drawmenu();
                }
            }
        } else if (ev->xmotion.window == submenu_win) {
            int y = ev->xmotion.y;
            int sub_y = y - config.border_width;
            int new_selected_subitem = sub_y / itemheight;
            if (new_selected_subitem >= 0 &&
                new_selected_subitem < items[selected_item].nsubitems &&
                new_selected_subitem != selected_subitem) {
                selected_subitem = new_selected_subitem;
                draw_submenu(&items[selected_item]);
            }
        }
        break;

    case LeaveNotify: {
        if (ev->xcrossing.window == win || ev->xcrossing.window == submenu_win) {
            struct timeval current_time;
            gettimeofday(&current_time, NULL);
            double time_elapsed = (current_time.tv_sec - menu_open_time.tv_sec) +
                                  (current_time.tv_usec - menu_open_time.tv_usec) / 1000000.0;
            if (time_elapsed < GRACE_PERIOD_SECONDS)
                return;
            int root_x, root_y, win_x, win_y;
            Window root_ret, child;
            unsigned int mask;
            if (XQueryPointer(dpy, root, &root_ret, &child,
                              &root_x, &root_y, &win_x, &win_y, &mask)) {
                int is_over_main = (root_x >= menu_x && root_x < menu_x + max_menuwidth &&
                                    root_y >= menu_y && root_y < menu_y + menuheight);
                int is_over_submenu = 0;
                if (submenu_win) {
                    XWindowAttributes wa;
                    if (XGetWindowAttributes(dpy, submenu_win, &wa)) {
                        is_over_submenu = (root_x >= wa.x && root_x < wa.x + wa.width &&
                                           root_y >= wa.y && root_y < wa.y + wa.height);
                    }
                }
                if (!is_over_main && !is_over_submenu) {
                    XUngrabPointer(dpy, CurrentTime);
                    if (submenu_win)
                        destroy_submenu_window();
                    XUnmapWindow(dpy, win);
                    regrab_button();
                }
            }
        }
    } break;
    }
}

static void show_menu(int x, int y) {
    selected_item = -1;
    selected_subitem = -1;
    int screen_width = DisplayWidth(dpy, screen);
    int screen_height = DisplayHeight(dpy, screen);
    if (x + max_menuwidth > screen_width)
        x = screen_width - max_menuwidth;
    if (y + menuheight > screen_height)
        y = screen_height - menuheight;
    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    menu_x = x;
    menu_y = y;
    XMoveWindow(dpy, win, x, y);
    XMapRaised(dpy, win);
    drawmenu();
    XSync(dpy, False);
    XGrabPointer(dpy, root, True,
                 ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                 EnterWindowMask | LeaveWindowMask,
                 GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
    XSetInputFocus(dpy, win, RevertToPointerRoot, CurrentTime);
    XUngrabButton(dpy, Button3, AnyModifier, root);
    gettimeofday(&menu_open_time, NULL);
    LOG("Menu displayed at (%d,%d)", x, y);
}

static void regrab_button(void) {
    XGrabButton(dpy, Button3, AnyModifier, root, True,
                ButtonPressMask, GrabModeSync, GrabModeAsync, None, None);
}

static void load_config(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        LOG("No config file found at %s, using defaults", path);
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        // Look for "key = value" lines
        char *equals = strstr(line, " = ");
        if (equals) {
            char key[32];
            char value[256];

            // Extract the key
            strncpy(key, line, equals - line);
            key[equals - line] = '\0';

            // Extract the value (strip trailing newline)
            strcpy(value, equals + 3);
            value[strcspn(value, "\n")] = '\0';

            if (strcmp(key, "font") == 0) {
                // If config.font was previously allocated (and not "fixed"), free it
                if (config.font && strcmp(config.font, "fixed") != 0)
                    free(config.font);

                config.font = strdup(value);
                LOG("Config: font set to %s", value);

            } else if (strcmp(key, "foreground") == 0) {
                // value expected like "#F62198"; skip '#' by using (value + 1)
                config.fg = strtoul(value + 1, NULL, 16);
                LOG("Config: foreground set to %s", value);

            } else if (strcmp(key, "background") == 0) {
                config.bg = strtoul(value + 1, NULL, 16);
                LOG("Config: background set to %s", value);
                printf("Loaded background color: 0x%06lx\n", config.bg);

            } else if (strcmp(key, "selected_fg") == 0) {
                config.selfg = strtoul(value + 1, NULL, 16);
                LOG("Config: selected_fg set to %s", value);

            } else if (strcmp(key, "selected_bg") == 0) {
                config.selbg = strtoul(value + 1, NULL, 16);
                LOG("Config: selected_bg set to %s", value);

            } else if (strcmp(key, "border_color") == 0) {
                config.border_color = strtoul(value + 1, NULL, 16);
                LOG("Config: border_color set to %s", value);
                printf("Loaded border color: 0x%06lx\n", config.border_color);

            } else if (strcmp(key, "transparency") == 0) {
                // Normal (non-hover) alpha
                config.alpha = atof(value);
                LOG("Config: transparency set to %s", value);

            } else if (strcmp(key, "hover_transparency") == 0) {
                // Hover (selected) alpha
                config.selalpha = atof(value);
                LOG("Config: hover_transparency set to %s", value);

            } else if (strcmp(key, "border_width") == 0) {
                config.border_width = atoi(value);
                LOG("Config: border_width set to %s", value);

            } else if (strcmp(key, "submenu_offset") == 0) {
                config.submenu_offset = atoi(value);
                LOG("Config: submenu_offset set to %s", value);

            } else if (strcmp(key, "mouse_delay") == 0) {
                config.mouse_delay = atof(value);
                LOG("Config: mouse_delay set to %s", value);

            } else if (strcmp(key, "icon_left_padding") == 0) {
                config.icon_left_padding = atoi(value);
                LOG("Config: icon_left_padding set to %s", value);

            } else if (strcmp(key, "icon_right_padding") == 0) {
                config.icon_right_padding = atoi(value);
                LOG("Config: icon_right_padding set to %s", value);
            }
        }
    }
    fclose(fp);
}


static void cleanup(void) {
    destroy_submenu_window();
    for (int i = 0; i < nitems; i++) {
        free(items[i].submenu);
        items[i].submenu = NULL;
        items[i].nsubitems = 0;
    }
    if (config.font && strcmp(config.font, "fixed") != 0)
        free(config.font);
    if (xft_font)
        XftFontClose(dpy, xft_font);
    if (main_draw)
        XftDrawDestroy(main_draw);
    if (submenu_draw)
        XftDrawDestroy(submenu_draw);
    XftColorFree(dpy, menu_vi.visual, menu_cmap, &xft_fg);
    XftColorFree(dpy, menu_vi.visual, menu_cmap, &xft_selfg);
    XFreeGC(dpy, gc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    LOG("Cleanup complete");
}

static int text_width(const char *text) {
    if (!xft_font)
        return 0;
    XGlyphInfo extents;
    XftTextExtents8(dpy, xft_font, (FcChar8 *)text, strlen(text), &extents);
    return extents.xOff;
}

static void execute_command(const char *cmd) {
    if (!cmd || !*cmd)
        return;
    pid_t pid = fork();
    if (pid == -1) {
        LOG("Error: fork failed for command '%s'", cmd);
        perror("gmenu: fork failed");
        return;
    }
    if (pid == 0) {
        char *bg_cmd;
        if (strncmp(cmd, "bash", 4) == 0) {
            asprintf(&bg_cmd, "%s %s &", TERMINAL, cmd);
        } else {
            asprintf(&bg_cmd, "%s &", cmd);
        }
        if (!bg_cmd)
            _exit(1);
        LOG("Child process executing: %s", bg_cmd);
        execl("/bin/sh", "sh", "-c", bg_cmd, (char *)NULL);
        free(bg_cmd);
        _exit(1);
    }
}
