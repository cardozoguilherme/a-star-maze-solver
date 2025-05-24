#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>

// Constantes do labirinto
#define MAX_DIM 1000
#define WALL '#'
#define START 'S'
#define END 'E'
#define PATH '.'
#define EMPTY ' '

// Estruturas de dados usando alocação dinâmica
typedef struct {
    short x, y;
} Point;

typedef struct {
    Point pos;
    unsigned short g, h;
    int parent;
} Node;

typedef struct {
    Node  *heap;
    Node  *closed;
    Point *path;
    char  *map;
    char  *visited;
    char  *open;
    int   width, height;
    int   heap_count, closed_count;
    int   path_size;
} Maze;

typedef struct {
    double solve_time;
    double total_time;
} Timing;

// Movimentação (N, L, S, O)
static const char dx[] = {0, 1, 0, -1};
static const char dy[] = {-1, 0, 1, 0};

static double get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

static unsigned int calc_f(const Node* node) {
    return node->g + node->h;
}

static void heapify_up(Maze* maze, int idx) {
    Node temp = maze->heap[idx];
    unsigned int f_temp = calc_f(&temp);
    while (idx > 0) {
        int parent = (idx - 1) / 2;
        if (calc_f(&maze->heap[parent]) <= f_temp) break;
        maze->heap[idx] = maze->heap[parent];
        idx = parent;
    }
    maze->heap[idx] = temp;
}

static void heapify_down(Maze* maze, int idx) {
    Node temp = maze->heap[idx];
    unsigned int f_temp = calc_f(&temp);
    int half = maze->heap_count / 2;

    while (idx < half) {
        int smallest = idx;
        int l = 2*idx + 1;
        int r = l + 1;
        if (l < maze->heap_count && calc_f(&maze->heap[l]) < f_temp)
            smallest = l;
        if (r < maze->heap_count && calc_f(&maze->heap[r]) < calc_f(&maze->heap[smallest]))
            smallest = r;
        if (smallest == idx) break;
        maze->heap[idx] = maze->heap[smallest];
        idx = smallest;
    }
    maze->heap[idx] = temp;
}

static void add_node(Maze* maze, Node node) {
    int idx = maze->heap_count++;
    maze->heap[idx] = node;
    maze->open[node.pos.y * maze->width + node.pos.x] = 1;
    heapify_up(maze, idx);
}

static Node pop_min_node(Maze* maze) {
    Node min = maze->heap[0];
    maze->open[min.pos.y * maze->width + min.pos.x] = 0;
    maze->heap[0] = maze->heap[--maze->heap_count];
    if (maze->heap_count > 0) heapify_down(maze, 0);
    return min;
}

static unsigned short manhattan_distance(short x1, short y1, short x2, short y2) {
    return abs(x1 - x2) + abs(y1 - y2);
}

static char* read_maze_file(const char* filename, size_t* size) {
    FILE* f = fopen(filename, "rb");
    if (!f) { perror("fopen"); return NULL; }
    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* data = malloc(*size + 1);
    if (!data) { perror("malloc"); fclose(f); return NULL; }
    fread(data, 1, *size, f);
    data[*size] = '\0';
    fclose(f);
    return data;
}

static int init_maze(Maze* maze, const char* data) {
    maze->width = 0;
    const char* p = data;
    while (*p && *p != '\n') { maze->width++; p++; }
    int w = maze->width, h = 0, line = 0;
    p = data;
    while (*p) {
        if (*p == '\n') { if (line && line != w) { fprintf(stderr, "Largura inconsistente\n"); return 0; } line = 0; h++; }
        else line++;
        p++;
    }
    if (line) h++;
    maze->height = h;
    if (w > MAX_DIM || h > MAX_DIM) {
        fprintf(stderr, "Labirinto muito grande (max %dx%d)\n", MAX_DIM, MAX_DIM);
        return 0;
    }
    // reset counters
    maze->heap_count = maze->closed_count = maze->path_size = 0;
    return 1;
}

static int solve_maze_internal(Maze* maze, const char* data, Point* start, Point* end) {
    // carregar mapa e localizar S e E
    const char* p = data;
    int found_s = 0, found_e = 0;
    for (int y = 0; y < maze->height; y++) {
        for (int x = 0; x < maze->width; x++) {
            char c = *p++;
            if (c == '\n') { x--; continue; }
            maze->map[y*maze->width + x] = c;
            if (c == START && !found_s) { start->x = x; start->y = y; found_s = 1; }
            if (c == END   && !found_e) {   end->x = x;   end->y = y;   found_e = 1; }
        }
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
    }
    if (!found_s || !found_e) { fprintf(stderr, "Start/End não encontrado\n"); return 0; }
    // iniciar A*
    Node sn = {*start, 0, manhattan_distance(start->x,start->y,end->x,end->y), -1};
    add_node(maze, sn);
    while (maze->heap_count) {
        Node cur = pop_min_node(maze);
        if (cur.pos.x==end->x && cur.pos.y==end->y) {
            // reconstruir caminho
            int idx = cur.parent;
            maze->path[maze->path_size++] = cur.pos;
            while (idx != -1) {
                maze->path[maze->path_size++] = maze->closed[idx].pos;
                idx = maze->closed[idx].parent;
            }
            return 1;
        }
        // mover para fechado
        maze->closed[maze->closed_count] = cur;
        maze->visited[cur.pos.y*maze->width+cur.pos.x] = 1;
        int cidx = maze->closed_count++;
        for (int d=0; d<4; d++) {
            int nx = cur.pos.x+dx[d], ny = cur.pos.y+dy[d];
            if (nx<0||nx>=maze->width||ny<0||ny>=maze->height) continue;
            int off = ny*maze->width+nx;
            if (maze->map[off]==WALL || maze->visited[off]) continue;
            unsigned short ng = cur.g+1;
            if (maze->open[off]) {
                for (int i=0;i<maze->heap_count;i++){
                    if (maze->heap[i].pos.x==nx && maze->heap[i].pos.y==ny && ng<maze->heap[i].g) {
                        maze->heap[i].g=ng;
                        maze->heap[i].parent=cidx;
                        heapify_up(maze,i);
                    }
                }
            } else {
                Node nn = {{nx,ny}, ng, manhattan_distance(nx,ny,end->x,end->y), cidx};
                add_node(maze, nn);
            }
        }
    }
    return 0;
}

static int save_maze(const char* fname, const Maze* maze) {
    FILE* f = fopen(fname, "w"); if (!f){ perror("fopen"); return 0; }
    int w=maze->width, h=maze->height;
    char* buf = malloc(w+2);
    for (int y=0;y<h;y++){
        for(int x=0;x<w;x++){
            char c = maze->map[y*w+x];
            buf[x] = (c==WALL||c==START||c==END||c==PATH||c==EMPTY)?c:EMPTY;
        }
        buf[w]='\n'; buf[w+1]='\0';
        fputs(buf,f);
    }
    free(buf);
    fclose(f);
    return 1;
}

static int save_path_json(const char* fname, const Maze* maze) {
    FILE* f = fopen(fname, "w"); if (!f){ perror("fopen"); return 0; }
    fprintf(f,"{\n  \"path\": [\n");
    for(int i=maze->path_size-1;i>=0;i--) {
        fprintf(f,"    {\"x\":%d, \"y\":%d}%s\n",
            maze->path[i].x, maze->path[i].y, i?",":"");
    }
    fprintf(f,"  ]\n}\n");
    fclose(f);
    return 1;
}

Timing solve_maze(const char* data) {
    Timing t={0,0};
    Maze maze={0};
    double t0 = get_time_ms();
    if (!init_maze(&maze,data)) return t;
    int cap = maze.width * maze.height;
    // alocar dinâmico
    maze.map     = malloc(cap);
    maze.visited = calloc(cap,1);
    maze.open    = calloc(cap,1);
    maze.heap    = malloc(sizeof(Node)*cap);
    maze.closed  = malloc(sizeof(Node)*cap);
    maze.path    = malloc(sizeof(Point)*cap);
    if (!maze.map||!maze.visited||!maze.open||!maze.heap||!maze.closed||!maze.path) {
        fprintf(stderr,"Erro malloc\n"); exit(1);
    }
    Point s,e;
    if (!solve_maze_internal(&maze,data,&s,&e)){
        fprintf(stderr,"Sem caminho\n");
    } else {
        for(int i=0;i<maze.path_size;i++){
            int idx = maze.path[i].y*maze.width + maze.path[i].x;
            char c = maze.map[idx];
            if (c!=START && c!=END) maze.map[idx] = PATH;
        }
        fprintf(stderr,"Salvando output.txt (%dx%d)\n",maze.width,maze.height);
        save_maze("output.txt",&maze);
        save_path_json("path.json",&maze);
    }
    t.solve_time = get_time_ms()-t0;
    t.total_time = get_time_ms()-t0;
    // liberar
    free(maze.map);
    free(maze.visited);
    free(maze.open);
    free(maze.heap);
    free(maze.closed);
    free(maze.path);
    return t;
}

int main(int argc, char* argv[]) {
    if(argc!=2) { printf("Uso: %s <arquivo>\n",argv[0]); return 1; }
    size_t sz;
    char* data = read_maze_file(argv[1], &sz);
    if (!data) return 1;
    Timing t = solve_maze(data);
    printf("Tempo resolução: %f ms\nTotal: %f ms\n", t.solve_time, t.total_time);
    free(data);
    return 0;
}
