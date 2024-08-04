/* This file is licensed under the GNU GPL v3.0 license. */

#include <argparse.h>
#include <u8string.h>
#include <tdl.h>
#include <tdl/tdl_char.h>
#include <tdl/tdl_image.h>
#include <tdl/tdl_style.h>

#include <ctype.h>
/* On some platforms, the M_PI macro requires this macro to be defined. */
#define _USE_MATH_DEFINES
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

/* If the _USE_MATH_DEFINES did not help... */
#ifndef M_PI
# define M_PI 3.14159265358979323846
#endif

#define CLOCK_FACE_RADIUS  20
#define CLOCK_FACE_X_RATIO 2
#define SEC_HAND_LEN  18
#define MIN_HAND_LEN  15
#define HOUR_HAND_LEN 18
#define HAND_X_RATIO  1.8

#define MIN_MARK_CHAR   tdl_char("*", tdl_style(tdl_point_color(TDL_DEFAULT_COLOR, TDL_BLUE), TDL_NO_ATTRIBUTES))
#define HOUR_MARK_STYLE tdl_style(tdl_point_color(TDL_DEFAULT_COLOR, TDL_BRIGHT_GREEN), TDL_BOLD)

#define SEC_HAND_CHAR  tdl_char("s", tdl_style(tdl_point_color(TDL_DEFAULT_COLOR, TDL_BRIGHT_RED),   TDL_NO_ATTRIBUTES))
#define MIN_HAND_CHAR  tdl_char("m", tdl_style(tdl_point_color(TDL_DEFAULT_COLOR, TDL_BRIGHT_BLUE),  TDL_NO_ATTRIBUTES))
#define HOUR_HAND_CHAR tdl_char("h", tdl_style(tdl_point_color(TDL_DEFAULT_COLOR, TDL_BRIGHT_GREEN), TDL_NO_ATTRIBUTES))

#define CLOCK_WIDTH  CLOCK_FACE_RADIUS * 2 * CLOCK_FACE_X_RATIO + 4
#define CLOCK_HEIGHT CLOCK_FACE_RADIUS * 2 + 1

#define DIG_CLOCK_COLON_STYLE  tdl_style(tdl_point_color(TDL_DEFAULT_COLOR, TDL_YELLOW), TDL_NO_ATTRIBUTES)
#define DIG_CLOCK_NUMBER_STYLE tdl_style(tdl_point_color(TDL_DEFAULT_COLOR, TDL_BRIGHT_YELLOW),  TDL_NO_ATTRIBUTES)

#define DIG_CLOCK_SYMBOL_HEIGHT  3
#define DIG_CLOCK_SYMBOL_WIDTH   3
#define DIG_CLOCK_SYMBOL_SPACING 1
#define DIG_CLOCK_HEIGHT DIG_CLOCK_SYMBOL_HEIGHT

#define CHAR_TO_INT(_ch) ((_ch) - '0')

#define LETTER_A_IDX 0
#define LETTER_M_IDX 1
#define LETTER_P_IDX 2

typedef enum option {
    option_digital       = (1 << 0),
    option_12_hour_time  = (1 << 1),
    option_hour_min_mode = (1 << 2)
} option_t;

static tdl_canvas_t *_main_canvas = NULL;
static tdl_canvas_t *_clock_face_canvas = NULL;
static tdl_image_t _clock_face;
static tdl_point_t _clock_coords;
static option_t _options;

static const char *_digital_letters[3][DIG_CLOCK_SYMBOL_HEIGHT] = {
    {
        "┌─┐",
        "├─┤",
        "╵ ╵"
    },
    {
        "┌┬┐",
        "│╵│",
        "╵ ╵"
    },
    {
        "┌─┐",
        "├─┘",
        "╵  "
    },
};

static const char *_digital_numbers[10][DIG_CLOCK_SYMBOL_HEIGHT] = {
    {
      "┌─┐",
      "│ │",
      "└─┘",
    },
    {
      " ╶┐",
      "  │",
      "  ╵",
    },
    {
      "╶─┐",
      "┌─┘",
      "└─╴",
    },
    {
      "╶─┐",
      " ─┤",
      "╶─┘",
    },
    {
      "╷ ╷",
      "└─┤",
      "  ╵",
    },
    {
      "┌─╴",
      "└─┐",
      "╶─┘",
    },
    {
      "┌─╴",
      "├─┐",
      "└─┘",
    },
    {
      "╶─┐",
      "  │",
      "  ╵",
    },
    {
      "┌─┐",
      "├─┤",
      "└─┘",
    },
    {
      "┌─┐",
      "└─┤",
      "╶─┘",
    }
};

void draw_symbol(tdl_canvas_t *canvas, tdl_point_t coords, const char *symbol[DIG_CLOCK_SYMBOL_HEIGHT], tdl_style_t style) {
    tdl_text_t column = tdl_text(u8string(""), style);

    for (int i = 0; i < DIG_CLOCK_SYMBOL_HEIGHT; ++i) {
        u8string_set(&column.string, (const cstr) symbol[i]);

        tdl_set_cursor_pos(canvas, tdl_point(coords.x, coords.y + i));
        tdl_print(canvas, column);
    }

    tdl_text_free(column);
}

void draw_colon(tdl_canvas_t *canvas, tdl_point_t coords) {
    static const char *colon[DIG_CLOCK_SYMBOL_HEIGHT] = {
        " ▅ ",
        "   ",
        " ▀ ",
    };

    draw_symbol(canvas, coords, colon, DIG_CLOCK_COLON_STYLE);
}

void draw_number(tdl_canvas_t *canvas, tdl_point_t coords, int number) {
    draw_symbol(canvas, coords, _digital_numbers[number], DIG_CLOCK_NUMBER_STYLE);
}

void draw_letter(tdl_canvas_t *canvas, tdl_point_t coords, char ch) {
    switch (ch) {
    case 'A':
        draw_symbol(canvas, coords, _digital_letters[LETTER_A_IDX], DIG_CLOCK_NUMBER_STYLE);

        break;
    case 'M':
        draw_symbol(canvas, coords, _digital_letters[LETTER_M_IDX], DIG_CLOCK_NUMBER_STYLE);

        break;
    case 'P':
        draw_symbol(canvas, coords, _digital_letters[LETTER_P_IDX], DIG_CLOCK_NUMBER_STYLE);

        break;
    }
}

void draw_str_time(tdl_canvas_t *canvas, tdl_point_t coords, const char *str) {
    size_t length = strlen(str);

    for (size_t i = 0; i < length; ++i) {
        char ch = str[i];

        tdl_set_cursor_pos(canvas, coords);

        if (isdigit(ch)) {
            draw_number(canvas, coords, CHAR_TO_INT(ch));
        } else if (ch == ':') {
            draw_colon(canvas, coords);
        } else if (isalpha(ch)) {
            draw_letter(canvas, coords, (char) toupper(ch));
        }

        coords.x += DIG_CLOCK_SYMBOL_WIDTH + DIG_CLOCK_SYMBOL_SPACING;
    }
}

void draw_clock_face(tdl_canvas_t *canvas, tdl_point_t coords, int radius) {
    const char *numbers[] = {
        "1", "2", "3", "4", "5", "6",
        "7", "8", "9", "10", "11", "12"
    };

    for (int i = 0; i < 60; ++i) {
        int x = coords.x + (int) (cos((i + 46) * 6 * M_PI / 180) * radius * CLOCK_FACE_X_RATIO);
        int y = coords.y + (int) (sin((i + 46) * 6 * M_PI / 180) * radius);

        if ((i + 1) % 5) {
            tdl_set_cursor_pos(canvas, tdl_point(x, y));

            tdl_putchar(canvas, MIN_MARK_CHAR);
        }
    }

    tdl_text_t text = tdl_text(u8string(""), HOUR_MARK_STYLE);

    for (int i = 0; i < 12; ++i) {
        int x = coords.x + (int) (cos((i + 46) * 30 * M_PI / 180) * radius * CLOCK_FACE_X_RATIO);
        int y = coords.y + (int) (sin((i + 46) * 30 * M_PI / 180) * radius);

        u8string_set(&text.string, (const cstr) numbers[i]);

        tdl_set_cursor_pos(canvas, tdl_point(x, y));
        tdl_print(canvas, text);
    }

    tdl_text_free(text);
}

void draw_sec_hand(tdl_canvas_t *canvas, tdl_point_t coords, int secs) {
    int x = coords.x + (int) (cos((secs + 45) * 6 * M_PI / 180) * SEC_HAND_LEN * HAND_X_RATIO);
    int y = coords.y + (int) (sin((secs + 45) * 6 * M_PI / 180) * SEC_HAND_LEN);

    tdl_draw_line(canvas, SEC_HAND_CHAR, tdl_line(coords, tdl_point(x, y)));
}

void draw_min_hand(tdl_canvas_t *canvas, tdl_point_t coords, int min) {
    int x = coords.x + (int) (cos((min + 45) * 6 * M_PI / 180) * MIN_HAND_LEN * HAND_X_RATIO);
    int y = coords.y + (int) (sin((min + 45) * 6 * M_PI / 180) * MIN_HAND_LEN);

    tdl_draw_line(canvas, MIN_HAND_CHAR, tdl_line(coords, tdl_point(x, y)));
}

void draw_hour_hand(tdl_canvas_t *canvas, tdl_point_t coords, int mins, int hours) {
    int x = coords.x + (int) (cos(((hours + mins / 60.0) + 45) * 30 * M_PI / 180) * HOUR_HAND_LEN * HAND_X_RATIO);
    int y = coords.y + (int) (sin(((hours + mins / 60.0) + 45) * 30 * M_PI / 180) * HOUR_HAND_LEN);

    tdl_draw_line(canvas, HOUR_HAND_CHAR, tdl_line(coords, tdl_point(x, y)));
}

void deinit(void) {
    tdl_destroy_canvas(_clock_face_canvas);
    _clock_face_canvas = NULL;

    tdl_destroy_canvas(_main_canvas);
    _main_canvas = NULL;

    tdl_image_free(&_clock_face);

    tdl_terminal_set_alternate_screen(false);
    tdl_terminal_set_cursor(true);
}

bool init_graphics(void) {
    tdl_terminal_set_alternate_screen(true);
    tdl_terminal_set_cursor(false);

    memset(&_clock_face, 0, sizeof(tdl_image_t));

    _main_canvas = tdl_canvas();
    _clock_face_canvas = tdl_canvas();

    return _main_canvas && _clock_face_canvas;
}

void interrupt_handler(int signal) {
    (void) signal;

    deinit();

    exit(EXIT_SUCCESS);
}

void draw_analog_clock(tdl_canvas_t *canvas, tdl_point_t coords, struct tm *time) {
    tdl_image_print_to_canvas(canvas, _clock_face, tdl_point(0, 0));

    if (!(_options & option_hour_min_mode)) {
        draw_sec_hand(canvas, coords, time->tm_sec);
    }

    draw_min_hand(canvas, coords, time->tm_min);
    draw_hour_hand(canvas, coords, time->tm_min, time->tm_hour);
}

void draw_digital_clock(tdl_canvas_t *canvas, tdl_point_t coords, struct tm *time) {
    char time_buf[12];
    char time_fmt[12];
    
    if (_options & option_12_hour_time) {
        strcpy(time_fmt, "%I:%M");
    } else {
        strcpy(time_fmt, "%H:%M");
    }

    if (!(_options & option_hour_min_mode)) {
        strcat(time_fmt, ":%S");
    }

    if (_options & option_12_hour_time) {
        strcat(time_fmt, time->tm_hour >= 12 ? " PM" : " AM");
    }

    strftime(time_buf, 12, time_fmt, time);

    draw_str_time(canvas, coords, time_buf);
}

void setup_analog_clock(void) {
    draw_clock_face(_clock_face_canvas, _clock_coords, CLOCK_FACE_RADIUS);
    
    _clock_face = tdl_image_crop_from_canvas(_clock_face_canvas, tdl_rectangle(tdl_point(0, 0), _clock_face_canvas->size));

    tdl_destroy_canvas(_clock_face_canvas);
    _clock_face_canvas = NULL;
}

int clock_width(void) {
    int width = 0;

    if (!(_options & option_digital)) {
        return CLOCK_WIDTH;
    }

    if (_options & option_12_hour_time) {
        width = DIG_CLOCK_SYMBOL_WIDTH * 9 + DIG_CLOCK_SYMBOL_SPACING * 5 + 1;
    } else {
        width = DIG_CLOCK_SYMBOL_WIDTH * 5 + DIG_CLOCK_SYMBOL_SPACING * 5 + 1;
    }

    if (!(_options & option_hour_min_mode)) {
        width += DIG_CLOCK_SYMBOL_WIDTH * 3 + DIG_CLOCK_SYMBOL_SPACING * 4 + 1;
    }

    return width - 1;
}

int clock_height(void) {
    if (!(_options & option_digital)) {
        return CLOCK_HEIGHT;
    }

    return DIG_CLOCK_HEIGHT;
}

void setup_digital_clock(void) {
    _clock_coords.x -= clock_width() / 2;

    _clock_coords.y -= DIG_CLOCK_HEIGHT / 2;
}

void parse_cli(int argc, const char *argv[]) {
    const char *const usage[] = {
        "tdl-clock [options]",
        NULL
    };

    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_BIT('d', "digital", &_options, "show digital clock",             NULL, option_digital,       0),
        OPT_BIT(0,   "hm",      &_options, "display only hours and minutes", NULL, option_hour_min_mode, 0),
        OPT_BIT(0,   "12",      &_options, "use 12-hour time instead of 24", NULL, option_12_hour_time,  0),
        OPT_END()
    };

    struct argparse cli_parser;

    argparse_init(&cli_parser, options, usage, 0);

    argparse_parse(&cli_parser, argc, argv);
}

bool init(void) {
    signal(SIGINT, interrupt_handler);

    if (!init_graphics()) {
        deinit();

        fprintf(stderr, "Cannot initialize graphics. Possibly out of memory.\n");

        return false;
    }

    if (_main_canvas->size.width < (size_t) clock_width() || _main_canvas->size.height < (size_t) clock_height()) {
        deinit();

        fprintf(stderr, "The terminal window is too small. The minimum size is %d columns and %d lines.\n", clock_width(), clock_height());

        return false;
    }

    _clock_coords = tdl_point((int) _main_canvas->size.width / 2, (int) _main_canvas->size.height / 2);

    return true;
}

void main_loop(void) {
    while (true) {
        time_t t = time(NULL);
        struct tm *time = localtime(&t);

        if (!time) {
            deinit();

            fprintf(stderr, "Cannot retrieve current local time.\n");

            exit(EXIT_FAILURE);
        }

        if (_options & option_digital) {
            draw_digital_clock(_main_canvas, _clock_coords, time);
        } else {
            draw_analog_clock(_main_canvas, _clock_coords, time);
        }

        tdl_display(_main_canvas);

        /* Let the CPU take a break for a second. */
        sleep(1);
    }
}


int main(int argc, char *argv[]) {
    parse_cli(argc, (const char **) argv);

    if (!init()) {
        return EXIT_FAILURE;
    }

    if (_options & option_digital) {
        setup_digital_clock();
    } else {
        setup_analog_clock();
    }

    main_loop();
}
