#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <immintrin.h> // Para instruções SIMD
#include <errno.h>

// Constantes do labirinto
#define MAX_SIZE 1000
#define WALL '#'
#define START 'S'
#define END 'E'
#define PATH '.'
#define EMPTY ' '

// Constantes de otimização
#define CACHE_LINE 64
#define BITS_PER_WORD (sizeof(unsigned long) * 8)
#define BIT_WORD_SIZE(n) (((n) + BITS_PER_WORD - 1) / BITS_PER_WORD)
#define ALIGN __attribute__((aligned(CACHE_LINE)))

// Estruturas de dados otimizadas
typedef struct {
    short x, y;
} Point;

typedef struct {
    Point pos;
    unsigned short g, h;
    int parent;
} ALIGN Node;

typedef struct {
    double solve_time;
    double total_time;
} TimingInfo;

// Estado global do solucionador
typedef struct {
    Node ALIGN open_heap[MAX_SIZE];
    Node ALIGN closed_list[MAX_SIZE];
    unsigned char ALIGN maze_data[MAX_SIZE * MAX_SIZE];
    unsigned long ALIGN closed_bits[BIT_WORD_SIZE(MAX_SIZE * MAX_SIZE)];
    unsigned long ALIGN open_bits[BIT_WORD_SIZE(MAX_SIZE * MAX_SIZE)];
    Point ALIGN path[MAX_SIZE];
    int width, height;
    int open_count, closed_count;
    int path_size;
} MazeSolver;

// Lookup tables pré-calculadas
static const char ALIGN dx[] = {0, 1, 0, -1};
static const char ALIGN dy[] = {-1, 0, 1, 0};

// Funções inline para manipulação de bits
static inline int get_bit_index(const MazeSolver* solver, int x, int y) {
    return y * solver->width + x;
}

static inline void set_bit(unsigned long* bits, int idx) {
    __builtin_prefetch(bits + (idx / BITS_PER_WORD));
    bits[idx / BITS_PER_WORD] |= 1UL << (idx % BITS_PER_WORD);
}

static inline void clear_bit(unsigned long* bits, int idx) {
    bits[idx / BITS_PER_WORD] &= ~(1UL << (idx % BITS_PER_WORD));
}

static inline int test_bit(const unsigned long* bits, int idx) {
    return (bits[idx / BITS_PER_WORD] >> (idx % BITS_PER_WORD)) & 1;
}

// Funções da heap binária
static inline unsigned int calc_f(const Node* node) {
    return node->g + node->h;
}

static inline void heapify_up(MazeSolver* solver, int idx) {
    Node temp = solver->open_heap[idx];
    unsigned int f_temp = calc_f(&temp);
    
    while (idx > 0) {
        int parent = (idx - 1) >> 1;
        __builtin_prefetch(&solver->open_heap[parent]);
        if (calc_f(&solver->open_heap[parent]) <= f_temp) break;
        solver->open_heap[idx] = solver->open_heap[parent];
        idx = parent;
    }
    solver->open_heap[idx] = temp;
}

static inline void heapify_down(MazeSolver* solver, int idx) {
    Node temp = solver->open_heap[idx];
    unsigned int f_temp = calc_f(&temp);
    int half = solver->open_count >> 1;
    
    while (idx < half) {
        int smallest = idx;
        int left = (idx << 1) + 1;
        int right = left + 1;
        
        __builtin_prefetch(&solver->open_heap[left]);
        __builtin_prefetch(&solver->open_heap[right]);
        
        if (left < solver->open_count && calc_f(&solver->open_heap[left]) < f_temp)
            smallest = left;
            
        if (right < solver->open_count) {
            unsigned int f_right = calc_f(&solver->open_heap[right]);
            if (f_right < (smallest == idx ? f_temp : calc_f(&solver->open_heap[left])))
                smallest = right;
        }
        
        if (smallest == idx) break;
        solver->open_heap[idx] = solver->open_heap[smallest];
        idx = smallest;
    }
    solver->open_heap[idx] = temp;
}

// Funções de manipulação da lista aberta
static inline void add_to_heap(MazeSolver* solver, Node node) {
    __builtin_prefetch(&solver->open_heap[solver->open_count]);
    solver->open_heap[solver->open_count] = node;
    set_bit(solver->open_bits, get_bit_index(solver, node.pos.x, node.pos.y));
    heapify_up(solver, solver->open_count++);
}

static inline Node pop_min_node(MazeSolver* solver) {
    Node min = solver->open_heap[0];
    clear_bit(solver->open_bits, get_bit_index(solver, min.pos.x, min.pos.y));
    solver->open_heap[0] = solver->open_heap[--solver->open_count];
    if (solver->open_count > 0) heapify_down(solver, 0);
    return min;
}

// Funções de verificação de estado
static inline int is_closed(const MazeSolver* solver, short x, short y) {
    int idx = get_bit_index(solver, x, y);
    __builtin_prefetch(&solver->closed_bits[idx / BITS_PER_WORD]);
    return test_bit(solver->closed_bits, idx);
}

static inline int is_open(const MazeSolver* solver, short x, short y) {
    int idx = get_bit_index(solver, x, y);
    __builtin_prefetch(&solver->open_bits[idx / BITS_PER_WORD]);
    return test_bit(solver->open_bits, idx);
}

// Função para calcular distância Manhattan
static inline unsigned short manhattan(short x1, short y1, short x2, short y2) {
    return (unsigned short)(abs(x1 - x2) + abs(y1 - y2));
}

// Funções de I/O
static int save_maze_file(const char* filename, const MazeSolver* solver) {
    FILE* f = fopen(filename, "w"); // Mudando para modo texto em vez de binário
    if (!f) {
        fprintf(stderr, "Erro ao abrir %s para escrita: %s\n", filename, strerror(errno));
        return 0;
    }

    // Alocando buffer para uma linha
    char* line = malloc(solver->width + 2);
    if (!line) {
        fprintf(stderr, "Erro ao alocar memória para buffer de linha\n");
        fclose(f);
        return 0;
    }

    // Escrevendo linha por linha
    for (int y = 0; y < solver->height; y++) {
        // Copiando e sanitizando cada caractere
        for (int x = 0; x < solver->width; x++) {
            char c = solver->maze_data[y * solver->width + x];
            // Garantindo que apenas caracteres válidos sejam escritos
            switch (c) {
                case WALL:
                case START:
                case END:
                case PATH:
                case EMPTY:
                    line[x] = c;
                    break;
                default:
                    line[x] = EMPTY; // Caractere inválido, substituindo por espaço
            }
        }
        line[solver->width] = '\n';
        line[solver->width + 1] = '\0';
        
        if (fputs(line, f) == EOF) {
            fprintf(stderr, "Erro ao escrever linha %d em %s\n", y, filename);
            free(line);
            fclose(f);
            return 0;
        }
    }

    free(line);
    fclose(f);
    return 1;
}

static int save_json_file(const char* filename, const MazeSolver* solver) {
    FILE* f = fopen(filename, "wb");
    if (!f) {
        fprintf(stderr, "Erro ao abrir %s para escrita: %s\n", filename, strerror(errno));
        return 0;
    }

    fprintf(f, "{\n  \"path\": [\n");
    for (int i = solver->path_size - 1; i >= 0; i--) {
        fprintf(f, "    {\"x\": %d, \"y\": %d}%s\n",
            solver->path[i].x, solver->path[i].y,
            i > 0 ? "," : "");
    }
    fprintf(f, "  ]\n}\n");

    fclose(f);
    return 1;
}

static char* read_maze_from_file(const char* filename, size_t* size) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Erro ao abrir arquivo %s: %s\n", filename, strerror(errno));
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* maze = malloc(*size + 1);
    if (!maze) {
        fprintf(stderr, "Erro ao alocar memória para o labirinto\n");
        fclose(f);
        return NULL;
    }

    size_t read = fread(maze, 1, *size, f);
    fclose(f);

    if (read != *size) {
        fprintf(stderr, "Erro ao ler arquivo %s: %s\n", filename, strerror(errno));
        free(maze);
        return NULL;
    }

    maze[*size] = '\0';
    return maze;
}

// Função auxiliar para obter timestamp em milissegundos
static inline double get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000.0) + (tv.tv_usec / 1000.0);
}

// Inicialização do solucionador
static int init_solver(MazeSolver* solver, const char* maze_str) {
    solver->width = 0;
    solver->height = 0;
    
    // Calcular largura (primeira linha)
    const char* ptr = maze_str;
    while (*ptr && *ptr != '\n') {
        solver->width++;
        ptr++;
    }
    
    // Calcular altura e verificar consistência da largura
    ptr = maze_str;
    int line_width = 0;
    while (*ptr) {
        if (*ptr == '\n') {
            if (line_width != solver->width && line_width != 0) {
                fprintf(stderr, "Erro: Largura inconsistente no labirinto\n");
                return 0;
            }
            solver->height++;
            line_width = 0;
        } else {
            line_width++;
        }
        ptr++;
    }
    if (line_width > 0) {
        solver->height++; // Para última linha sem \n
    }
    
    // Verificar dimensões
    if (solver->width > MAX_SIZE || solver->height > MAX_SIZE) {
        fprintf(stderr, "Labirinto muito grande (máximo: %dx%d)\n", MAX_SIZE, MAX_SIZE);
        return 0;
    }
    
    // Limpar estado
    memset(solver->closed_bits, 0, BIT_WORD_SIZE(solver->width * solver->height) * sizeof(unsigned long));
    memset(solver->open_bits, 0, BIT_WORD_SIZE(solver->width * solver->height) * sizeof(unsigned long));
    solver->open_count = solver->closed_count = solver->path_size = 0;
    
    return 1;
}

// Função principal de resolução
static int solve_maze_internal(MazeSolver* solver, const char* maze_str, Point* start, Point* end) {
    // Copiar e processar labirinto
    const char* ptr = maze_str;
    int found_start = 0, found_end = 0;
    Point first_start = {-1, -1}, first_end = {-1, -1};
    
    for (int y = 0; y < solver->height; y++) {
        for (int x = 0; x < solver->width; x++) {
            char c = *ptr++;
            if (c == '\n') {
                // Ajustar o ponteiro para o próximo caractere após \n
                x--;
                continue;
            }
            
            solver->maze_data[y * solver->width + x] = c;
            
            if (c == START) {
                if (!found_start) {
                    first_start.x = x;
                    first_start.y = y;
                    found_start = 1;
                } else {
                    // Converter outros pontos de início em espaços vazios
                    solver->maze_data[y * solver->width + x] = EMPTY;
                }
            } else if (c == END) {
                if (!found_end) {
                    first_end.x = x;
                    first_end.y = y;
                    found_end = 1;
                } else {
                    // Converter outros pontos de fim em espaços vazios
                    solver->maze_data[y * solver->width + x] = EMPTY;
                }
            }
        }
        // Pular qualquer caractere restante até o próximo \n
        while (*ptr && *ptr != '\n') ptr++;
        if (*ptr == '\n') ptr++;
    }
    
    if (!found_start || !found_end) {
        fprintf(stderr, "Erro: Início ou fim não encontrado no labirinto\n");
        return 0;
    }
    
    *start = first_start;
    *end = first_end;
    
    // Adicionar nó inicial
    Node start_node = {
        .pos = *start,
        .g = 0,
        .h = manhattan(start->x, start->y, end->x, end->y),
        .parent = -1
    };
    add_to_heap(solver, start_node);
    
    // Loop principal do A*
    while (solver->open_count > 0) {
        Node current = pop_min_node(solver);
        
        if (current.pos.x == end->x && current.pos.y == end->y) {
            // Reconstruir caminho
            int parent_idx = current.parent;
            solver->path[solver->path_size++] = current.pos;
            
            while (parent_idx != -1) {
                solver->path[solver->path_size] = solver->closed_list[parent_idx].pos;
                parent_idx = solver->closed_list[parent_idx].parent;
                solver->path_size++;
            }
            return 1;
        }
        
        solver->closed_list[solver->closed_count] = current;
        set_bit(solver->closed_bits, get_bit_index(solver, current.pos.x, current.pos.y));
        int closed_idx = solver->closed_count++;
        
        // Verificar vizinhos
        for (int i = 0; i < 4; i++) {
            const short nx = current.pos.x + dx[i];
            const short ny = current.pos.y + dy[i];
            
            if (nx >= 0 && nx < solver->width && ny >= 0 && ny < solver->height) {
                const int idx = ny * solver->width + nx;
                if (solver->maze_data[idx] != WALL && !is_closed(solver, nx, ny)) {
                    const unsigned short new_g = current.g + 1;
                    if (is_open(solver, nx, ny)) {
                        // Atualizar nó existente se necessário
                        for (int j = 0; j < solver->open_count; j++) {
                            if (solver->open_heap[j].pos.x == nx && 
                                solver->open_heap[j].pos.y == ny) {
                                if (new_g < solver->open_heap[j].g) {
                                    solver->open_heap[j].g = new_g;
                                    solver->open_heap[j].parent = closed_idx;
                                    heapify_up(solver, j);
                                }
                                break;
                            }
                        }
                    } else {
                        // Adicionar novo nó
                        Node new_node = {
                            .pos = {nx, ny},
                            .g = new_g,
                            .h = manhattan(nx, ny, end->x, end->y),
                            .parent = closed_idx
                        };
                        add_to_heap(solver, new_node);
                    }
                }
            }
        }
    }
    
    return 0; // Caminho não encontrado
}

// Função principal que resolve o labirinto
TimingInfo solveMaze(const char* maze_str) {
    TimingInfo timing = {0, 0};
    MazeSolver solver = {0};
    Point start = {0, 0}, end = {0, 0};
    
    double start_solve = get_time_ms();
    
    // Inicializar solver
    if (!init_solver(&solver, maze_str)) {
        return timing;
    }
    
    // Resolver labirinto
    int found = solve_maze_internal(&solver, maze_str, &start, &end);
    if (!found) {
        fprintf(stderr, "Nenhum caminho encontrado ou labirinto inválido!\n");
        timing.solve_time = get_time_ms() - start_solve;
        return timing;
    }
    
    // Marcar caminho
    for (int i = 0; i < solver.path_size; i++) {
        const int idx = solver.path[i].y * solver.width + solver.path[i].x;
        const char current = solver.maze_data[idx];
        if (current != START && current != END) {
            solver.maze_data[idx] = PATH;
        }
    }
    
    // Registrar tempo de resolução
    timing.solve_time = get_time_ms() - start_solve;
    
    // Salvar resultados
    fprintf(stderr, "Salvando output.txt (dimensões: %dx%d)...\n", 
            solver.width - 1, solver.height);
    if (!save_maze_file("output.txt", &solver)) {
        return timing;
    }
    
    fprintf(stderr, "Salvando path.json...\n");
    if (!save_json_file("path.json", &solver)) {
        return timing;
    }
    
    fprintf(stderr, "Arquivos salvos com sucesso!\n");
    
    return timing;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <arquivo_labirinto.txt>\n", argv[0]);
        return 1;
    }
    
    double start_total = get_time_ms();
    
    size_t maze_size;
    char* maze = read_maze_from_file(argv[1], &maze_size);
    if (!maze) {
        return 1;
    }
    
    TimingInfo timing = solveMaze(maze);
    timing.total_time = get_time_ms() - start_total;
    
    printf("Tempo de resolução do labirinto: %f ms\n", timing.solve_time);
    printf("Tempo total de execução: %f ms\n", timing.total_time);
    printf("Tempo gasto em I/O e outras operações: %f ms\n", 
           timing.total_time - timing.solve_time);
    
    free(maze);
    return 0;
} 