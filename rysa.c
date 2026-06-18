/* rysa (v0.1)
 * Minimalist pixel-art editor for the terminal.
 *
 * Copyright (c) 2026 vihmu
 * https://github.com/vihmu/rysa
 * */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>

#define ESC "\x1b["
#define DEF_W 30
#define DEF_H 20
#define K_QUIT   'q'
#define K_UP     'A'
#define K_DOWN   'B'
#define K_LEFT   'D'
#define K_RIGHT  'C'
#define K_VUP    'k'
#define K_VDOWN  'j'
#define K_VLEFT  'h'
#define K_VRIGHT 'l'
#define K_DRAW   'z'
#define K_DEL    'x'
#define K_FILL   'f'
#define K_TRACE  't'
#define K_PICK   'c'
#define K_SAVE   's'
#define K_EXPORT 'e'
#define SVG 10
#define MIN 2
#define NAME "rysa"

#define ilen(arr) sizeof(arr) / sizeof(*arr)

struct termios default_term;

typedef struct {
    size_t w, h; /* width and height of canvas */
    size_t** c;  /* canvas data */
} Canvas;

typedef struct {
    size_t x, y;   /* cursor x/y position */
    size_t tx, ty; /* for drawing lines */
    size_t col;    /* selected color */
} Cursor;

/* terminal width and height */
typedef struct {
    size_t w, h;
} Window;

typedef enum { false, true } bool;

int colors[] = {40, 41, 42, 43, 44, 45, 46, 47};
char *colors_rgb[] = {
    "2D383A", /* black */
    "D92121", /* red */
    "3AA655", /* green */
    "FFDB00", /* yellow */
    "0081AB", /* blue */
    "C154C1", /* magenta */
    "44D7A8", /* cyan */
    "FDFDFD", /* white */
};
char *filename;
char *info;

/* Helpers */
size_t tocolor(size_t col)
{
    /* Returns a color from the array */
    return col < ilen(colors)
           ? colors[col]
           : colors[0];
}

char* tocolor_rgb(size_t col)
{
    /* Idem. but for RGB values */
    return col < ilen(colors_rgb)
           ? colors_rgb[col]
           : colors_rgb[0];
}

int str_endswith(const char *str, const char *ext)
{
    if (!str || !ext) return 0;
    size_t s_str = strlen(str);
    size_t s_ext = strlen(ext);
    if (s_ext > s_str) return 0;

    return strncmp(str + s_str - s_ext, ext, s_ext) == 0;
}

/* Canvas creation and manipulation */
Canvas makecanvas(size_t w, size_t h)
{
    Canvas data;
    /* Exit if canvas is too small */
    if (w < MIN || h < MIN) {
        printf("%s: Canvas too small.\n", NAME);
        exit(2);
    }

    /* Prepare memory allocation */
    size_t** cvs = malloc(h * sizeof(size_t*));
    cvs[0] = malloc(w * h * sizeof(size_t));
    if (cvs[0] == NULL) exit(1);

    /* Initialize canvas */
    for (size_t i = 0; i < h; ++i) cvs[i] = cvs[0] + i * w;

    /* Return new canvas data struct */
    data.w = w;
    data.h = h;
    data.c = cvs;
    return data;
}

Canvas load_canvas(const char *file)
{
    /* To avoid writing the previous function twice, we dynamically assign a new
     * canvas, then modify it based on the file data */
    Canvas cvs = {.w = 0, .h = 0};
    FILE* fp = fopen(file, "r");
    char line[256];
    size_t lcount = 0;
    size_t ly = 0;

    if (fp != NULL) {
        /* Get the first line */
        fgets(line, sizeof(line), fp);
        /* Get first two tokens */
        char *token = strtok(line, " ");
        /* Exit if file is empty */
        if (token == NULL) {
            printf("%s: File empty or not canvas data.\n", NAME);
            exit(1);
        }
        cvs.w = atoi(token);
        token = strtok(NULL, " ");
        cvs.h = atoi(token);
        /* Generate the new canvas image */
        cvs = makecanvas(cvs.w, cvs.h);

        /* Continue by interpreting the file and modifying the canvas accordingly */
        while (fgets(line, sizeof(line), fp)) {
            /* Return canvas when we reached the end */
            if (ly >= cvs.h) return cvs;
            /* Split every line by whitespace */
            token = strtok(line, " ");
            while ((token != NULL) && (lcount < cvs.w)) {
                /* Keeping track of the current position, we assign canvas[y][x] */
                cvs.c[ly][lcount] = atoi(token);
                token = strtok(NULL, " ");
                lcount += 1;
            }
            lcount = 0;
            ly += 1;
        }
    } else {
        printf("%s: Could not open or read file.\n", NAME);
        exit(1);
    }
    return cvs;
}

void fill_canvas(Canvas cvs, size_t col)
{
    /* Fill canvas with a specified color */
    col = col >= ilen(colors) ? 0 : col;
    for (size_t i = 0; i < cvs.h; ++i)
        for (size_t j = 0; j < cvs.w; ++j)
            cvs.c[i][j] = col;
}

void freecanvas(Canvas cvs)
{
    free(cvs.c[0]);
    free(cvs.c);
}

/* Terminal */
Window get_term_size(void)
{
    struct winsize w;
    Window ws;

    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    ws.w = w.ws_col;
    ws.h = w.ws_row;
    return ws;
}

void restore_term(void)
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &default_term);
}

int set_term_raw(void)
{
    tcgetattr(STDIN_FILENO, &default_term);
    /* Always restore terminal when program exits */
    /*atexit(restore_term);*/

    /* Disable terminal echo, canonical mode, and Ctrl-Z/Ctrl-C */
    struct termios raw = default_term;
    raw.c_iflag &= ~(BRKINT|ICRNL|INPCK|ISTRIP|IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO|ICANON|IEXTEN|ISIG);
    /* Don't wait for user input */
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    /* Set all options */
    return tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void toggle_cursor(bool mode)
{
    /* Hide cursor if true, show it again if false */
    if (mode) printf("%s?25l", ESC);
    else printf("%s?25h", ESC);
}

void clear_screen(void)
{
    /* Clears the screen */
    printf("%sH%s2J", ESC, ESC);
}

void move(size_t x, size_t y)
{
    /* Moves cursor to x,y position */
    printf("%s%ld;%ldH", ESC, y, x);
}

/* Graphics*/
void draw_canvas(Canvas cvs)
{
    /* Loop through canvas array */
    for (size_t i = 0; i < cvs.h; ++i) {
        for (size_t j = 0; j < cvs.w; ++j) {
            /* (x+1)*2-1; where j is x
             * x+1 is needed since move(0,0) is the same as move(1,1)
             * *2 ensures proper aspect ratio in the terminal (2x1)
             * -1 removes the offset */
            move((j+1)*2-1, i+1);
            printf("%s%ldm  %sm", ESC, tocolor(cvs.c[i][j]), ESC);
        }
    }
}

void draw_cursor(Cursor crs, size_t col, char *block)
{
    /* We specify the color and block chars to use so that this function can be
     * reused for when redrawing previous sections of the canvas */
    move((crs.x+1)*2-1, crs.y+1);
    printf("%s%ldm%s%sm", ESC, tocolor(col), block, ESC);
}

size_t pickmenu(char *prompt)
{
    /* Move to the top and print the prompt */
    move(0, 0);
    printf("%s", prompt);
    /* Substract 48 to get an integer from the ASCII value */
    size_t c = getchar()-48;
    return c < ilen(colors) ? c : 0;
}

void bresenham(Canvas cvs, int x, int y, int xx, int yy, size_t col)
{
    /* Parameters needed for slope calculation */
    int dx = abs(xx - x);
    int dy = abs(yy - y);
    int sx = x < xx ? 1 : -1;
    int sy = y < yy ? 1 : -1;
    int err = (dx>dy ? dx : -dy)/2;

    /* Start of line */
    while (true) {
        /* Plot first coord. */
        cvs.c[y][x] = col;
        if (x == xx && y == yy) return;
        if (err > -dx) {
            err -= dy;
            x += sx;
            if (x == xx && y == yy) {
                /* Reached end of line */
                cvs.c[y][x] = col;
                return;
            }
        }
        if (err < dy) {
            err += dx;
            y += sy;
        }
    }
}

/* File IO */
int export_canvas(Canvas cvs, char *format)
{
    /* Allocate a new string for the filename so that we can append the format */
    size_t fsize = snprintf(NULL, 0, "%s.%s", filename, format) + 1;
    char *file = malloc(fsize);
    if (!file) return 1;
    snprintf(file, fsize, "%s.%s", filename, format);

    /* Open file for writing in binary mode */
    FILE* fp;
    fp = fopen(file, "w");
    /* Cancel if file could not be opened */
    if (fp == NULL) {
        info = "Could not write to file.";
        return 1;
    }

    switch (format[0])
    {
    /* .dat format is used for saving */
    case 'd':
        /* Top: width and height */
        fprintf(fp, "%ld %ld\n", cvs.w, cvs.h);
        /* Space separated colors per pixel */
        for (size_t i = 0; i < cvs.h; ++i) {
            for (size_t j = 0; j < cvs.w; ++j)
                fprintf(fp, "%ld ", cvs.c[i][j]);
            fputs("\n", fp);
        }
        info = "File saved.";
        break;
    /* SVG */
    case 's':
        /* Beginning */
        fprintf(fp,
            "<svg width=\"%ld\" height=\"%ld\" xmlns=\"http://www.w3.org/2000/svg\">\n",
            cvs.w*SVG, cvs.h*SVG);
        for (size_t i = 0; i < cvs.h; ++i)
            for (size_t j = 0; j < cvs.w; ++j)
                /* Draw a rectangle of size SVG (10px) at x*SVG, y*SVG */
                fprintf(fp,
                    "<rect x=\"%ld\" y=\"%ld\" width=\"%d\" height=\"%d\" fill=\"#%s\" />\n",
                    j*SVG, i*SVG, SVG, SVG, tocolor_rgb(cvs.c[i][j]));
        /* Close SVG file */
        fputs("</svg>\n", fp);
        info = "Exported to SVG.";
        break;
    /* X-PixMap */
    case 'x':
        /* XPM files oddly enough use C syntax: */
        fputs("/* XPM */\nstatic char *default_xpm[] = {\n", fp);
        fprintf(fp, "\"%ld %ld %lu 1\",\n", cvs.w, cvs.h, ilen(colors));
        /* Declare all colors to be used */
        for (size_t n = 0; n < ilen(colors); ++n) {
            fprintf(fp, "\"%ld  c #%s\",\n", n, tocolor_rgb(n));
        }
        /* Each row of pixels is a string */
        for (size_t i = 0; i < cvs.h; ++i) {
            fputs("\"", fp);
            for (size_t j = 0; j < cvs.w; ++j)
                fprintf(fp, "%ld", cvs.c[i][j]);
            fputs("\",\n", fp);
        }
        fputs("};\n", fp);
        info = "Exported to XPM.";
        break;
    /* PPM */
    case 'p':
        /* The PPM format begins as a regular text file, then switches to a
         * binary format containing each pixel */
        /* Header (the magic number) */
        fputs("P6\n", fp);
        /* Canvas width and height, max value per pixel (255) */
        fprintf(fp, "%ld %ld\n255\n", cvs.w, cvs.h);
        /* Writing every pixel's RGB value */
        for (size_t i = 0; i < cvs.h; ++i) {
            for (size_t j = 0; j < cvs.w; ++j) {
                unsigned int r, g, b;
                /* Turn the hex color string into values from 0-255 */
                sscanf(tocolor_rgb(cvs.c[i][j]), "%2x%2x%2x", &r, &g, &b);
                fputc((int)r, fp);
                fputc((int)g, fp);
                fputc((int)b, fp);
            }
        }
        info = "Exported to PPM.";
        break;
    /* ASCII */
    case 'a':
        /* Provides a shell script that outputs the ASCII canvas on the terminal
         * using cat << EOF */
        fputs("#!/bin/sh\ncat << EOF\n", fp);
        for (size_t i = 0; i < cvs.h; ++i) {
            for (size_t j = 0; j < cvs.w; ++j)
                fprintf(fp, "%s%ldm  %sm", ESC, tocolor(cvs.c[i][j]), ESC);
            fputs("\n", fp);
        }
        fputs("EOF\n", fp);
        info = "Exported to ASCII";
        break;
    default: break;
    }
    fclose(fp);
    free(file);
    return 0;
}

int main(int argc, char **argv)
{
    /* Initialize state */
    bool quit = false, saved = false;
    char key;
    info = "Welcome!";
    Canvas canvas;
    /* Parse args
     * 3 args: new file with w/h */
    if (argc > 2) {
        filename = argv[1];
        /* Automatically remove file extension (4 chars) */
        if (str_endswith(filename, ".dat"))
            filename[strlen(filename)-4] = '\0';

        canvas = makecanvas(atoi(argv[2]), atoi(argv[3]));
        fill_canvas(canvas, ilen(colors)-1);
    } else if (argc > 1) {
        filename = argv[1];
        /* Load canvas from the provided file */
        canvas = load_canvas(filename);
        /* Then remove extension if needed */
        if (str_endswith(filename, ".dat"))
            filename[strlen(filename)-4] = '\0';
    /* no args: default canvas image */
    } else {
        filename = "default";
        canvas = makecanvas(DEF_W, DEF_H);
        /* Fill the new canvas */
        fill_canvas(canvas, ilen(colors)-1);
    }

    /* Initialize window */
    Window window = get_term_size();
    /* Quit if terminal window is too small */
    if ((canvas.w*2 > window.w) || (canvas.h > window.h)) {
        printf("%s: Terminal window too small.\n", NAME);
        exit(2);
    }

    /* Initialize cursor */
    Cursor crs = {
        .x = 0, .y = 0,
        .col = 0,
        .tx = canvas.w,
        .ty = canvas.h
    };

    /* Prepare terminal */
    set_term_raw();
    clear_screen();
    toggle_cursor(true);

    /* Initial canvas drawing
     * Note that we do not need to redraw the canvas constantly, since only the
     * pixels underneath the cursor actually change */
    draw_canvas(canvas);

    while (!quit) {
        /* Cursor position */
        draw_cursor(crs, crs.col, "[]");

        /* Information bar */
        move(0, canvas.h+1);
        printf("%s.dat (%ld,%ld)[%ldx%ld] c:%s%ldm %sm | %s     \r",
            filename, crs.x+1, crs.y+1, canvas.w, canvas.h,
            ESC, tocolor(crs.col), ESC, info);

        /* Interpret input */
        key = getchar();
        switch (key) {
        case K_QUIT:
            /* Ask before quiting
             * Also let the user know if quitting without saving */
            if (!saved) {
                move(0, 2);
                printf("Image is not saved!");
            }
            if (pickmenu("Press 1 to quit.") == 1) {
                quit = !quit;
                info = "Bye bye!";
            } else {
                /* We need to update the screen */
                clear_screen();
                draw_canvas(canvas);
            }
            break;
        /* Movement keys */
        case K_UP:
        case K_VUP:
            /* Avoid redrawing entire canvas by only redrawing previous block */
            draw_cursor(crs, canvas.c[crs.y][crs.x], "  ");
            crs.y = crs.y > 0 ? (crs.y - 1) : canvas.h-1;
            break;
        case K_DOWN:
        case K_VDOWN:
            draw_cursor(crs, canvas.c[crs.y][crs.x], "  ");
            crs.y = crs.y < canvas.h-1 ? (crs.y + 1) : 0;
            break;
        case K_LEFT:
        case K_VLEFT:
            draw_cursor(crs, canvas.c[crs.y][crs.x], "  ");
            crs.x = crs.x > 0 ? (crs.x - 1) : canvas.w-1;
            break;
        case K_RIGHT:
        case K_VRIGHT:
            draw_cursor(crs, canvas.c[crs.y][crs.x], "  ");
            crs.x = crs.x < canvas.w-1 ? (crs.x + 1) : 0;
            break;
        /* Drawing keys */
        case K_DRAW:
            if (saved) saved = false;
            canvas.c[crs.y][crs.x] = crs.col;
            break;
        case K_DEL:
            if (saved) saved = false;
            canvas.c[crs.y][crs.x] = ilen(colors)-1;
            break;
        case K_FILL:
            /* Ask user before filling */
            if (pickmenu("Press 1 to fill background.") == 1)
                fill_canvas(canvas, crs.col);
            clear_screen();
            draw_canvas(canvas);
            break;
        /* Drawing a line */
        case K_TRACE:
            if ((crs.tx < canvas.w) && (crs.ty < canvas.h)) {
                bresenham(canvas, crs.x, crs.y, crs.tx, crs.ty, crs.col);
                /* Reset first coords. */
                crs.tx = canvas.w;
                crs.ty = canvas.h;
                info = "Line drawn.";
                /* Screen needs to be updated */
                clear_screen();
                draw_canvas(canvas);
            } else {
                crs.tx = crs.x;
                crs.ty = crs.y;
                info = "Plotted first coord.";
            }
            break;
        case K_PICK:
            crs.col = pickmenu("Choose a color (0-9)");
            clear_screen();
            draw_canvas(canvas);
            break;
        /* Saving and exporting */
        case K_SAVE:
            if (export_canvas(canvas, "dat") == 0)
                saved = true;
            break;
        case K_EXPORT:
            /* Just reuse the output from pickmenu() and assign each integer to a format */
            move(0, 2);
            printf("Available formats:");
            move(0, 3);
            printf("1: SVG\t2: XPM\t3: PPM\t4: ASCII");
            switch (pickmenu("Choose a format")) {
            case 1:
                export_canvas(canvas, "svg");
                break;
            case 2:
                export_canvas(canvas, "xpm");
                break;
            case 3:
                export_canvas(canvas, "ppm");
                break;
            case 4:
                export_canvas(canvas, "asc");
                break;
            /* ignore other input */
            default:
                info = "Not exported.";
                break;
            }
            clear_screen();
            draw_canvas(canvas);
            break;
        /* ignore any other keys */
        default: break;
        }
    }

    /* Free and show cursor again */
    freecanvas(canvas);
    restore_term();
    clear_screen();
    toggle_cursor(false);
    printf("%s\n", info);
    return 0;
}
