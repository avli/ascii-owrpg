#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <ncurses.h>

static const size_t buf_min_size = 10;
static const size_t height = 1000;
static const size_t width = 1000;
static const unsigned int walks = 1000;
static const unsigned int steps = 2500;

static void usage(void) {
    puts("Usage:");
    puts("\tascii-owrpg [file]");
    puts("\tascii-owrpg -h");
}

struct world_s
{
    int height; /* The hight of the world map */
    int width;  /* The width of the world map */
    int y_pos;  /* Player y position */
    int x_pos;  /* Player x position */
    char** map;
};

typedef struct world_s world_t;

enum movement_e {
    UP,
    DOWN,
    LEFT,
    RIGHT
};

typedef enum movement_e movement_t;

world_t load_world(const char* f) {
    FILE* fp = fopen(f, "r");
    
    if (!fp) {
        perror("Error while reading file");
        exit(EXIT_FAILURE);
    }

    char* buf = malloc(sizeof(char) * buf_min_size);
    size_t buf_size = buf_min_size;
    size_t first_line_length;
    size_t current_line_length = 0;
    bool first_line = true;
    size_t line_number = 0;
    world_t new_world;
    new_world.map = NULL;
    bool player_already_found = false;
    char c;

    while((c = getc(fp)) != EOF) {
        if (c == '\n') {
            if (first_line) {
                first_line_length = current_line_length;
                new_world.width = first_line_length;
                first_line = false;
            }
            else {
                if (current_line_length != first_line_length) {
                    fprintf(stderr, "Error: map is not square\n");
                    exit(EXIT_FAILURE);
                }
            }
            new_world.map = realloc(new_world.map, sizeof(char*) * (line_number + 1));
            new_world.map[line_number] = malloc(sizeof(char) * current_line_length);
            memcpy(new_world.map[line_number], buf, current_line_length + 1);
            line_number++;
            current_line_length = 0;
            free(buf);
            buf = malloc(sizeof(char) * buf_min_size);
            buf_size = buf_min_size;
            continue;
        }
        if ((c != '#') && (c != '.') && (c != '@')) {
            fprintf(stderr, 
                "Unknown character \"%c\" in the data file \"%s\"\n at line %zu, column %zu\n", 
                    c, f, line_number+1, current_line_length+1);
            exit(EXIT_FAILURE);
        }
        if (c == '@') {
            if (player_already_found) {
                fprintf(stderr, "More than one player position found\n");
                exit(EXIT_FAILURE); 
            }
            new_world.x_pos = current_line_length;
            new_world.y_pos = line_number;
            c = '.';
            player_already_found = true;
        }
        if (buf_size <= current_line_length) {
            buf = realloc(buf, (buf_size + buf_min_size) * sizeof(char));
            buf_size += buf_min_size;
        }
        buf[current_line_length++] = c;
    }

    free(buf);

    if (!player_already_found) {
        fprintf(stderr, "No player was found on the map\n");
        exit(EXIT_FAILURE);
    }

    new_world.height = line_number;
    
    if (!feof(fp)) {
        perror("Error while reading file");
        exit(EXIT_FAILURE);
    }

    fclose(fp);

    #if DEBUG
        puts("The map has been loaded sucessfully.");
    #endif

    return new_world;
}

world_t new_world(size_t height, size_t width, size_t walks, size_t steps) {
    srand(time(NULL));

    typedef struct { int y; int x; } coordinate_t;

    world_t new_world;
    new_world.height = height;
    new_world.width = width;

    char** new_map = NULL;
    for(size_t i = 0; i < height; ++i) {
        new_map = realloc(new_map, sizeof(char*) * i + 1);
        new_map[i] = malloc(sizeof(char) * width);
        for(size_t j = 0; j < width; ++j) {
            new_map[i][j] = '#';
        }
    }
    int counter = 0;
    coordinate_t* free_cells = malloc(sizeof(coordinate_t) * walks * steps); // too much, but I don't care...
    for(int i = 0; i < walks; ++i) {
        int y, x;
        y = rand() % height;
        x = rand() % width;
        for(int j = 0; j < steps; ++j) {
            movement_t direction = rand() % 4;
            switch (direction) {
                case UP:
                    if ((y - 1) > 1) {
                        new_map[--y][x] = '.';
                        free_cells[counter].y = y;
                        free_cells[counter].x = x;
                        counter++;
                    }
                    break;
                case RIGHT:
                    if ((x + 1) < width - 1) {
                        new_map[y][++x] = '.';
                        free_cells[counter].y = y;
                        free_cells[counter].x = x;
                        counter++;
                    }
                    break;
                case LEFT:
                    if ((x - 1) > 1) {
                        new_map[y][--x] = '.';
                        free_cells[counter].y = y;
                        free_cells[counter].x = x;
                        counter++;
                    }
                    break;
                case DOWN:
                    if ((y + 1) < height - 1) {                     
                        new_map[++y][x] = '.';
                        free_cells[counter].y = y;
                        free_cells[counter].x = x;
                        counter++;
                    }
                    break;
                default:
                    break;
            }
        }
    }
    new_world.map = new_map;
    coordinate_t player = free_cells[rand() % counter];
    new_world.y_pos = player.y;
    new_world.x_pos = player.x;
    free(free_cells);
    return new_world;
}

void draw_world(world_t* world, int row, int col) {
    size_t new_height, new_width;
    new_height = row > world->height ? world->height : row;
    new_width = col > world->width ? world->width : col;
    wresize(stdscr, new_height, new_width);
    size_t player_pos_x, player_pos_y;
    size_t dist_to_right_edge = world->width - world->x_pos;
    size_t right_col;
    row = row;
    if (dist_to_right_edge < col / 2) {
        right_col = world->width;
    }
    else if (world->x_pos < col / 2) { // the same as dist to left edge
        right_col = col;
    }
    else {
        right_col = world->x_pos + col / 2;
    }
    size_t dist_to_bottom_edge = world->height - world->y_pos;
    size_t bottom_row;
    if (dist_to_bottom_edge < row / 2) {
        bottom_row = world->height;
    }
    else if (world->y_pos < row / 2) { // the same as dist to upper edge
        bottom_row = row;
    }
    else {
        bottom_row = world->y_pos + row / 2;
    }
    size_t left_col = right_col - (size_t)fmin(col, world->width);
    size_t upper_row = bottom_row - (size_t)fmin(row, world->height);
    char* str = malloc(sizeof(char) * 
        (right_col - left_col) * (bottom_row - upper_row));
    int offset = 0;
    for (size_t i = upper_row; i < bottom_row; ++i) {
        for(size_t j = left_col; j < right_col; ++j) {
            if ((i == world->y_pos) && (j == world->x_pos)) {
                str[offset + j - left_col] = '@';    
            }
            else {
                str[offset + j - left_col] = world->map[i][j];
            }
        }
        offset+=(right_col - left_col);
    }
    printw(str);
    free(str);
}

void move_character(movement_t mvt, world_t* world) {
    switch (mvt) {
        case UP:
            if (world->y_pos - 1 < 0) {
                break;
            }
            if (world->map[world->y_pos - 1][world->x_pos] != '#') {
                world->y_pos--;
            }
            break;
        case DOWN:
            if (world->y_pos + 1 >= world->height) {
                break;
            }
            if (world->map[world->y_pos + 1][world->x_pos] != '#') {
                world->y_pos++;
            }
            break;
        case RIGHT:
            if (world->x_pos + 1 >= world->width) {
                break;
            }
            if (world->map[world->y_pos][world->x_pos + 1] != '#') {
                world->x_pos++;
            }
            break;
        case LEFT:
            if (world->x_pos - 1 < 0) {
                break;
            }
            if (world->map[world->y_pos][world->x_pos - 1] != '#') {
                world->x_pos--;
            }
            break;
        default:
            break;
    }
}

int main(int argc, char* argv[]) {
    if (argc > 2) {
        usage();
        exit(EXIT_FAILURE);
    }
    if ((argc == 2) && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
        usage();
        exit(EXIT_SUCCESS);
    }

    world_t world;

    if (argc == 2) {
        world = load_world(argv[1]);
    }
    else {
        world = new_world(height, width, walks, steps);
    }

	initscr();
	keypad(stdscr, TRUE);
	noecho();
	curs_set(0);

    int row, col;
    int c;

	while (1) {
		clear();
        getmaxyx(stdscr, row, col);
        draw_world(&world, row, col);
		refresh();
		c = getch();
        switch(c) {
            case KEY_UP:
            case 'w':
            case 'k':
                move_character(UP, &world);
                break;
            case KEY_DOWN:
            case 's':
            case 'j':
                move_character(DOWN, &world);
                break;
            case KEY_RIGHT:
            case 'd':
            case 'l':
                move_character(RIGHT, &world);
                break;
            case KEY_LEFT:
            case 'a':
            case 'h':
                move_character(LEFT, &world);
                break;
            default:
                break;
        }
	}
	endwin();
}
