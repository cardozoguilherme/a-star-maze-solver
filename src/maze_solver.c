#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>

// Constantes para representação do labirinto
#define MAX_DIM 1000  // Dimensão máxima do labirinto (largura ou altura)
#define WALL '#'      // Parede
#define START 'S'     // Ponto de início
#define END 'E'       // Ponto de chegada
#define PATH '.'      // Caminho encontrado
#define EMPTY ' '     // Espaço livre

// Estrutura para representar uma posição no labirinto
typedef struct {
    short x, y;      // Coordenadas x,y (short para economia de memória)
} Point;

// Estrutura para um nó no algoritmo A*
typedef struct {
    Point pos;                // Posição atual no labirinto
    unsigned short g, h;      // g = custo atual, h = heurística até o objetivo
    int parent;              // Índice do nó pai no array closed
} Node;

// Estrutura principal do labirinto e dados do algoritmo
typedef struct {
    Node  *heap;             // Heap binário de nós a serem explorados
    Node  *closed;           // Array de nós já explorados
    Point *path;             // Caminho encontrado (do fim ao início)
    char  *map;              // Matriz do labirinto
    char  *visited;          // Matriz de células já visitadas
    char  *open;             // Matriz de células no heap
    int   *node_index;       // Mapeia posições aos seus índices no heap
    int   width, height;     // Dimensões do labirinto
    int   heap_count;        // Quantidade de nós no heap
    int   closed_count;      // Quantidade de nós explorados
    int   path_size;         // Tamanho do caminho encontrado
} Maze;

// Estrutura para medição de tempo
typedef struct {
    double solve_time;       // Tempo do algoritmo A*
    double total_time;       // Tempo total incluindo I/O
} Timing;

// Vetores para movimentação nos 4 vizinhos (Norte, Leste, Sul, Oeste)
static const char dx[] = {0, 1, 0, -1};
static const char dy[] = {-1, 0, 1, 0};

/**
 * Obtém o tempo atual em milissegundos
 * Usado para medir performance do algoritmo
 */
static inline double get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

/**
 * Calcula o valor f(n) = g(n) + h(n) para o algoritmo A*
 * g(n) = custo do caminho até o nó
 * h(n) = estimativa do custo até o objetivo
 */
static inline unsigned int calc_f(const Node* node) {
    return node->g + node->h;
}

/**
 * Reorganiza o heap para cima após inserção
 * Mantém a propriedade de heap mínimo baseado em f(n)
 */
static void heapify_up(Maze* maze, int idx) {
    Node temp = maze->heap[idx];
    int pos_idx = temp.pos.y * maze->width + temp.pos.x;
    unsigned int f_temp = calc_f(&temp);
    
    while (idx > 0) {
        int parent = (idx - 1) >> 1;  // Otimização: divisão por 2 usando shift
        if (calc_f(&maze->heap[parent]) <= f_temp) break;
        
        // Move o pai para baixo
        maze->heap[idx] = maze->heap[parent];
        maze->node_index[maze->heap[idx].pos.y * maze->width + maze->heap[idx].pos.x] = idx;
        idx = parent;
    }
    
    // Coloca o nó na posição final
    maze->heap[idx] = temp;
    maze->node_index[pos_idx] = idx;
}

/**
 * Reorganiza o heap para baixo após remoção
 * Mantém a propriedade de heap mínimo baseado em f(n)
 */
static void heapify_down(Maze* maze, int idx) {
    Node temp = maze->heap[idx];
    int pos_idx = temp.pos.y * maze->width + temp.pos.x;
    unsigned int f_temp = calc_f(&temp);
    int half = maze->heap_count >> 1;  // Otimização: divisão por 2 usando shift

    while (idx < half) {
        int smallest = idx;
        int l = (idx << 1) + 1;  // Otimização: 2*idx + 1
        int r = l + 1;
        
        // Encontra o menor entre pai e filhos
        if (l < maze->heap_count && calc_f(&maze->heap[l]) < f_temp)
            smallest = l;
        if (r < maze->heap_count && calc_f(&maze->heap[r]) < calc_f(&maze->heap[smallest]))
            smallest = r;
            
        if (smallest == idx) break;
        
        // Move o menor filho para cima
        maze->heap[idx] = maze->heap[smallest];
        maze->node_index[maze->heap[idx].pos.y * maze->width + maze->heap[idx].pos.x] = idx;
        idx = smallest;
    }
    
    // Coloca o nó na posição final
    maze->heap[idx] = temp;
    maze->node_index[pos_idx] = idx;
}

/**
 * Adiciona um novo nó ao heap
 * Atualiza todas as estruturas de controle necessárias
 */
static void add_node(Maze* maze, Node node) {
    int idx = maze->heap_count++;
    int pos_idx = node.pos.y * maze->width + node.pos.x;
    maze->heap[idx] = node;
    maze->node_index[pos_idx] = idx;
    maze->open[pos_idx] = 1;
    heapify_up(maze, idx);
}

/**
 * Remove e retorna o nó com menor f(n) do heap
 * Este é o próximo nó a ser explorado pelo A*
 */
static Node pop_min_node(Maze* maze) {
    Node min = maze->heap[0];
    int pos_idx = min.pos.y * maze->width + min.pos.x;
    maze->open[pos_idx] = 0;
    maze->node_index[pos_idx] = -1;  // Marca como removido
    
    if (--maze->heap_count > 0) {
        maze->heap[0] = maze->heap[maze->heap_count];
        maze->node_index[maze->heap[0].pos.y * maze->width + maze->heap[0].pos.x] = 0;
        heapify_down(maze, 0);
    }
    return min;
}

/**
 * Calcula a distância Manhattan entre dois pontos
 * Esta é a heurística h(n) usada pelo A*
 * É admissível pois nunca superestima o custo real
 */
static inline unsigned short manhattan_distance(short x1, short y1, short x2, short y2) {
    return (unsigned short)(abs(x1 - x2) + abs(y1 - y2));
}

/**
 * Lê o arquivo do labirinto com tratamento de erros
 * Retorna o conteúdo do arquivo como string
 */
static char* read_maze_file(const char* filename, size_t* size) {
    FILE* f = fopen(filename, "rb");
    if (!f) { perror("fopen"); return NULL; }
    
    if (fseek(f, 0, SEEK_END) != 0) {
        perror("fseek");
        fclose(f);
        return NULL;
    }
    
    long fsize = ftell(f);
    if (fsize == -1) {
        perror("ftell");
        fclose(f);
        return NULL;
    }
    *size = (size_t)fsize;
    
    if (fseek(f, 0, SEEK_SET) != 0) {
        perror("fseek");
        fclose(f);
        return NULL;
    }
    
    char* data = malloc(*size + 1);
    if (!data) {
        perror("malloc");
        fclose(f);
        return NULL;
    }
    
    size_t bytes_read = fread(data, 1, *size, f);
    if (bytes_read != *size) {
        perror("fread");
        free(data);
        fclose(f);
        return NULL;
    }
    
    data[*size] = '\0';
    fclose(f);
    return data;
}

/**
 * Inicializa a estrutura do labirinto
 * Calcula dimensões e verifica consistência
 */
static int init_maze(Maze* maze, const char* data) {
    // Calcula a largura real do labirinto (sem contar o \n)
    maze->width = 0;
    const char* p = data;
    while (*p && *p != '\n') { 
        if (*p != '\r') maze->width++; // Ignora \r em arquivos Windows
        p++; 
    }
    
    int w = maze->width, h = 0, line = 0;
    p = data;
    while (*p) {
        if (*p == '\n') { 
            if (line && line != w) { 
                fprintf(stderr, "Largura inconsistente\n"); 
                return 0; 
            } 
            line = 0; 
            h++; 
        }
        else if (*p != '\r') line++; // Ignora \r em arquivos Windows
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

/**
 * Implementação principal do algoritmo A*
 * Encontra o menor caminho do ponto S ao ponto E
 */
static int solve_maze_internal(Maze* maze, const char* data, Point* start, Point* end, double* solve_time) {
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
    
    // Inicialização de node_index para -1 (não está no heap)
    memset(maze->node_index, -1, sizeof(int) * maze->width * maze->height);
    
    // Início da medição do tempo do A*
    double t_start = get_time_ms();
    
    // iniciar A*
    Node sn = {*start, 0, manhattan_distance(start->x,start->y,end->x,end->y), -1};
    add_node(maze, sn);
    
    while (maze->heap_count) {
        Node cur = pop_min_node(maze);
        int cur_pos = cur.pos.y*maze->width + cur.pos.x;
        
        if (cur.pos.x==end->x && cur.pos.y==end->y) {
            // reconstruir caminho
            int idx = cur.parent;
            maze->path[maze->path_size++] = cur.pos;
            while (idx != -1) {
                maze->path[maze->path_size++] = maze->closed[idx].pos;
                idx = maze->closed[idx].parent;
            }
            *solve_time = get_time_ms() - t_start;
            return 1;
        }
        
        // mover para fechado
        maze->closed[maze->closed_count] = cur;
        maze->visited[cur_pos] = 1;
        int cidx = maze->closed_count++;
        
        // Expandir nós vizinhos
        for (int d=0; d<4; d++) {
            int nx = cur.pos.x+dx[d], ny = cur.pos.y+dy[d];
            if (nx<0||nx>=maze->width||ny<0||ny>=maze->height) continue;
            
            int off = ny*maze->width+nx;
            if (maze->map[off]==WALL || maze->visited[off]) continue;
            
            unsigned short ng = cur.g+1;
            
            if (maze->open[off]) {
                int heap_idx = maze->node_index[off];
                if (heap_idx != -1 && ng < maze->heap[heap_idx].g) {
                    maze->heap[heap_idx].g = ng;
                    maze->heap[heap_idx].parent = cidx;
                    heapify_up(maze, heap_idx);
                }
            } else {
                Node nn = {{nx,ny}, ng, manhattan_distance(nx,ny,end->x,end->y), cidx};
                add_node(maze, nn);
            }
        }
    }
    *solve_time = get_time_ms() - t_start;
    return 0;
}

/**
 * Salva o labirinto resolvido em arquivo
 * Marca o caminho encontrado com '.'
 */
static int save_maze(const char* fname, const Maze* maze) {
    FILE* f = fopen(fname, "w"); 
    if (!f) { 
        perror("fopen"); 
        return 0; 
    }
    
    int w = maze->width, h = maze->height;
    char* buf = malloc(w + 2);
    if (!buf) { 
        perror("malloc"); 
        fclose(f); 
        return 0; 
    }
    
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            char c = maze->map[y*w + x];
            buf[x] = (c==WALL||c==START||c==END||c==PATH||c==EMPTY)?c:EMPTY;
        }
        buf[w] = '\n';
        buf[w+1] = '\0';
        if (fputs(buf, f) == EOF) {
            perror("fputs");
            free(buf);
            fclose(f);
            return 0;
        }
    }
    
    free(buf);
    fclose(f);
    return 1;
}

/**
 * Salva o caminho encontrado em formato JSON
 * Útil para visualização ou processamento posterior
 */
static int save_path_json(const char* fname, const Maze* maze) {
    FILE* f = fopen(fname, "w"); 
    if (!f) { 
        perror("fopen"); 
        return 0; 
    }
    
    if (fprintf(f, "{\n  \"path\": [\n") < 0) {
        perror("fprintf");
        fclose(f);
        return 0;
    }
    
    for (int i = maze->path_size-1; i >= 0; i--) {
        if (fprintf(f, "    {\"x\":%d, \"y\":%d}%s\n",
            maze->path[i].x, maze->path[i].y, i?",":"") < 0) {
            perror("fprintf");
            fclose(f);
            return 0;
        }
    }
    
    if (fprintf(f, "  ]\n}\n") < 0) {
        perror("fprintf");
        fclose(f);
        return 0;
    }
    
    fclose(f);
    return 1;
}

/**
 * Função principal de resolução
 * Coordena todo o processo e mede os tempos
 */
Timing solve_maze(const char* data) {
    Timing t = {0, 0};
    double t0 = get_time_ms();
    
    Maze maze = {0};
    if (!init_maze(&maze,data)) return t;
    int cap = maze.width * maze.height;
    
    // Alocação única para reduzir chamadas malloc
    char* all_memory = calloc(cap * (sizeof(char)*3 + sizeof(int)), 1);
    if (!all_memory) {
        fprintf(stderr,"Erro malloc\n"); 
        return t;
    }
    
    maze.map = all_memory;
    maze.visited = all_memory + cap;
    maze.open = all_memory + (cap * 2);
    maze.node_index = (int*)(all_memory + (cap * 3));
    
    // Essas estruturas ainda precisam de alocações separadas
    maze.heap = malloc(sizeof(Node)*cap);
    maze.closed = malloc(sizeof(Node)*cap);
    maze.path = malloc(sizeof(Point)*cap);
    
    if (!maze.heap || !maze.closed || !maze.path) {
        fprintf(stderr,"Erro malloc\n"); 
        free(all_memory);
        free(maze.heap);
        free(maze.closed);
        free(maze.path);
        return t;
    }
    
    Point s = {0, 0}, e = {0, 0};  // Inicialização explícita
    if (!solve_maze_internal(&maze, data, &s, &e, &t.solve_time)) {
        fprintf(stderr,"Sem caminho\n");
    } else {
        for(int i=0;i<maze.path_size;i++){
            int idx = maze.path[i].y*maze.width + maze.path[i].x;
            char c = maze.map[idx];
            if (c!=START && c!=END) maze.map[idx] = PATH;
        }
        fprintf(stderr,"Salvando output.txt (%dx%d)\n",maze.width,maze.height);
        if (!save_maze("output.txt",&maze) || !save_path_json("path.json",&maze)) {
            fprintf(stderr,"Erro ao salvar arquivos de saída\n");
        }
    }
    
    t.total_time = get_time_ms() - t0;
    
    free(all_memory);
    free(maze.heap);
    free(maze.closed);
    free(maze.path);
    return t;
}

/**
 * Função main
 * Processa argumentos e apresenta resultados
 */
int main(int argc, char* argv[]) {
    if(argc!=2) { 
        fprintf(stderr, "Uso: %s <arquivo>\n",argv[0]); 
        return 1; 
    }
    
    size_t sz;
    char* data = read_maze_file(argv[1], &sz);
    if (!data) return 1;
    
    Timing t = solve_maze(data);
    
    // Imprime apenas o tempo de resolução do A* para stdout
    printf("Resolução A*: %f ms\n", t.solve_time);
    
    // Informações adicionais vão para stderr
    fprintf(stderr, "Total (com I/O): %f ms\n", t.total_time);
    fprintf(stderr, "Overhead I/O: %f ms\n", t.total_time - t.solve_time);
    
    free(data);
    return 0;
}
