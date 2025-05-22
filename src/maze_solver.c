#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>

// Constantes do labirinto
#define MAX_SIZE 1000
#define WALL '#'
#define START 'S'
#define END 'E'
#define PATH '.'
#define EMPTY ' '

// Estruturas de dados simplificadas
typedef struct {
    short x, y;
} Point;

typedef struct {
    Point pos;
    unsigned short g, h;
    int parent;
} Node;

typedef struct {
    Node heap[MAX_SIZE];
    Node closed[MAX_SIZE];
    char map[MAX_SIZE * MAX_SIZE];
    char visited[MAX_SIZE * MAX_SIZE];
    char open[MAX_SIZE * MAX_SIZE];
    Point path[MAX_SIZE];
    int width, height;
    int heap_count, closed_count;
    int path_size;
} Maze;

typedef struct {
    double solve_time;
    double total_time;
} Timing;

// Direções de movimento (N, L, S, O)
static const char dx[] = {0, 1, 0, -1};
static const char dy[] = {-1, 0, 1, 0};

// Funções heap
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
        int left = 2 * idx + 1;
        int right = left + 1;
        
        if (left < maze->heap_count && calc_f(&maze->heap[left]) < f_temp)
            smallest = left;
            
        if (right < maze->heap_count) {
            unsigned int f_right = calc_f(&maze->heap[right]);
            if (f_right < (smallest == idx ? f_temp : calc_f(&maze->heap[left])))
                smallest = right;
        }
        
        if (smallest == idx) break;
        maze->heap[idx] = maze->heap[smallest];
        idx = smallest;
    }
    maze->heap[idx] = temp;
}

// Funções auxiliares
static void add_node(Maze* maze, Node node) {
    maze->heap[maze->heap_count] = node;
    maze->open[node.pos.y * maze->width + node.pos.x] = 1;
    heapify_up(maze, maze->heap_count++);
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

// Obter timestamp em milissegundos
static double get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000.0) + (tv.tv_usec / 1000.0);
}

// Funções de I/O
static int save_maze(const char* filename, const Maze* maze) {
    FILE* f = fopen(filename, "w");
    if (!f) {
        fprintf(stderr, "Erro ao abrir %s para escrita: %s\n", filename, strerror(errno));
        return 0;
    }

    char* line = malloc(maze->width + 2);
    if (!line) {
        fprintf(stderr, "Erro ao alocar memória\n");
        fclose(f);
        return 0;
    }

    for (int y = 0; y < maze->height; y++) {
        for (int x = 0; x < maze->width; x++) {
            char c = maze->map[y * maze->width + x];
            line[x] = (c == WALL || c == START || c == END || c == PATH || c == EMPTY) ? c : EMPTY;
        }
        line[maze->width] = '\n';
        line[maze->width + 1] = '\0';
        
        if (fputs(line, f) == EOF) {
            fprintf(stderr, "Erro ao escrever em %s\n", filename);
            free(line);
            fclose(f);
            return 0;
        }
    }

    free(line);
    fclose(f);
    return 1;
}

static int save_path_json(const char* filename, const Maze* maze) {
    FILE* f = fopen(filename, "w");
    if (!f) {
        fprintf(stderr, "Erro ao abrir %s para escrita: %s\n", filename, strerror(errno));
        return 0;
    }

    fprintf(f, "{\n  \"path\": [\n");
    for (int i = maze->path_size - 1; i >= 0; i--) {
        fprintf(f, "    {\"x\": %d, \"y\": %d}%s\n",
            maze->path[i].x, maze->path[i].y,
            i > 0 ? "," : "");
    }
    fprintf(f, "  ]\n}\n");

    fclose(f);
    return 1;
}

static char* read_maze_file(const char* filename, size_t* size) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Erro ao abrir arquivo %s: %s\n", filename, strerror(errno));
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* data = malloc(*size + 1);
    if (!data) {
        fprintf(stderr, "Erro ao alocar memória\n");
        fclose(f);
        return NULL;
    }

    size_t read_bytes = fread(data, 1, *size, f);
    fclose(f);

    if (read_bytes != *size) {
        fprintf(stderr, "Erro ao ler arquivo %s\n", filename);
        free(data);
        return NULL;
    }

    data[*size] = '\0';
    return data;
}

// Inicialização do labirinto
static int init_maze(Maze* maze, const char* data) {
    maze->width = 0;
    maze->height = 0;
    
    // Calcular dimensões
    const char* ptr = data;
    while (*ptr && *ptr != '\n') {
        maze->width++;
        ptr++;
    }
    
    ptr = data;
    int line_width = 0;
    while (*ptr) {
        if (*ptr == '\n') {
            if (line_width != maze->width && line_width != 0) {
                fprintf(stderr, "Erro: Largura inconsistente\n");
                return 0;
            }
            maze->height++;
            line_width = 0;
        } else {
            line_width++;
        }
        ptr++;
    }
    if (line_width > 0) maze->height++;
    
    if (maze->width > MAX_SIZE || maze->height > MAX_SIZE) {
        fprintf(stderr, "Labirinto muito grande (máximo: %dx%d)\n", MAX_SIZE, MAX_SIZE);
        return 0;
    }
    
    // Limpar estado
    memset(maze->visited, 0, maze->width * maze->height);
    memset(maze->open, 0, maze->width * maze->height);
    maze->heap_count = maze->closed_count = maze->path_size = 0;
    
    return 1;
}

// Função principal de resolução
static int solve_maze_internal(Maze* maze, const char* data, Point* start, Point* end) {
    // Carregar labirinto
    const char* ptr = data;
    int found_start = 0, found_end = 0;
    Point first_start = {-1, -1}, first_end = {-1, -1};
    
    for (int y = 0; y < maze->height; y++) {
        for (int x = 0; x < maze->width; x++) {
            char c = *ptr++;
            if (c == '\n') {
                x--;
                continue;
            }
            
            maze->map[y * maze->width + x] = c;
            
            if (c == START && !found_start) {
                first_start.x = x;
                first_start.y = y;
                found_start = 1;
            } else if (c == START) {
                maze->map[y * maze->width + x] = EMPTY;
            } else if (c == END && !found_end) {
                first_end.x = x;
                first_end.y = y;
                found_end = 1;
            } else if (c == END) {
                maze->map[y * maze->width + x] = EMPTY;
            }
        }
        while (*ptr && *ptr != '\n') ptr++;
        if (*ptr == '\n') ptr++;
    }
    
    if (!found_start || !found_end) {
        fprintf(stderr, "Erro: Início ou fim não encontrado\n");
        return 0;
    }
    
    *start = first_start;
    *end = first_end;
    
    // Adicionar nó inicial
    Node start_node = {
        .pos = *start,
        .g = 0,
        .h = manhattan_distance(start->x, start->y, end->x, end->y),
        .parent = -1
    };
    add_node(maze, start_node);
    
    // Algoritmo A*
    while (maze->heap_count > 0) {
        Node current = pop_min_node(maze);
        
        // Verificar se chegou ao destino
        if (current.pos.x == end->x && current.pos.y == end->y) {
            // Reconstruir caminho
            int parent_index = current.parent;
            maze->path[maze->path_size++] = current.pos;
            
            while (parent_index != -1) {
                maze->path[maze->path_size++] = maze->closed[parent_index].pos;
                parent_index = maze->closed[parent_index].parent;
            }
            return 1;
        }
        
        // Marcar como visitado
        maze->closed[maze->closed_count] = current;
        maze->visited[current.pos.y * maze->width + current.pos.x] = 1;
        int closed_index = maze->closed_count++;
        
        // Explorar vizinhos
        for (int i = 0; i < 4; i++) {
            short nx = current.pos.x + dx[i];
            short ny = current.pos.y + dy[i];
            
            if (nx >= 0 && nx < maze->width && ny >= 0 && ny < maze->height) {
                int index = ny * maze->width + nx;
                if (maze->map[index] != WALL && !maze->visited[index]) {
                    unsigned short new_g = current.g + 1;
                    if (maze->open[index]) {
                        // Atualizar nó existente se necessário
                        for (int j = 0; j < maze->heap_count; j++) {
                            if (maze->heap[j].pos.x == nx && maze->heap[j].pos.y == ny) {
                                if (new_g < maze->heap[j].g) {
                                    maze->heap[j].g = new_g;
                                    maze->heap[j].parent = closed_index;
                                    heapify_up(maze, j);
                                }
                                break;
                            }
                        }
                    } else {
                        // Adicionar novo nó
                        Node new_node = {
                            .pos = {nx, ny},
                            .g = new_g,
                            .h = manhattan_distance(nx, ny, end->x, end->y),
                            .parent = closed_index
                        };
                        add_node(maze, new_node);
                    }
                }
            }
        }
    }
    
    return 0; // Caminho não encontrado
}

// Função principal que resolve o labirinto
Timing solve_maze(const char* data) {
    Timing timing = {0, 0};
    Maze maze = {0};
    Point start = {0, 0}, end = {0, 0};
    
    double start_time = get_time_ms();
    
    // Inicializar labirinto
    if (!init_maze(&maze, data)) {
        return timing;
    }
    
    // Resolver labirinto
    int found = solve_maze_internal(&maze, data, &start, &end);
    if (!found) {
        fprintf(stderr, "Nenhum caminho encontrado!\n");
        timing.solve_time = get_time_ms() - start_time;
        return timing;
    }
    
    // Marcar caminho no mapa
    for (int i = 0; i < maze.path_size; i++) {
        int idx = maze.path[i].y * maze.width + maze.path[i].x;
        char current = maze.map[idx];
        if (current != START && current != END) {
            maze.map[idx] = PATH;
        }
    }
    
    timing.solve_time = get_time_ms() - start_time;
    
    // Salvar resultados
    fprintf(stderr, "Salvando output.txt (dimensões: %dx%d)...\n", maze.width - 1, maze.height);
    save_maze("output.txt", &maze);
    fprintf(stderr, "Salvando path.json...\n");
    save_path_json("path.json", &maze);
    
    return timing;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <arquivo_labirinto.txt>\n", argv[0]);
        return 1;
    }
    
    double start_total = get_time_ms();
    
    size_t size;
    char* data = read_maze_file(argv[1], &size);
    if (!data) {
        return 1;
    }
    
    Timing timing = solve_maze(data);
    timing.total_time = get_time_ms() - start_total;
    
    printf("Tempo de resolução do labirinto: %f ms\n", timing.solve_time);
    printf("Tempo total: %f ms\n", timing.total_time);
    printf("Tempo I/O: %f ms\n", timing.total_time - timing.solve_time);
    
    free(data);
    return 0;
}
