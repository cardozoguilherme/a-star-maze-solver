# A* Maze Solver

Este projeto implementa um solucionador de labirinto usando o algoritmo A* (A-Star). O programa encontra o menor caminho entre dois pontos em um labirinto 2D.

## Formato do Labirinto

O labirinto deve ser fornecido como um arquivo de texto com as seguintes características:
- Linhas separadas por `\n`
- Caracteres válidos:
  - `#`: parede
  - ` ` (espaço): caminho livre
  - `S`: ponto de início
  - `E`: ponto de chegada

Exemplo:
```
##########
#S       #
# ###### #
#      # #
# #### # #
# #    # #
# # #### #
#      #E#
##########
```

## Labirintos de Exemplo

O projeto inclui vários labirintos de exemplo na pasta `mazes/`:
- `maze_1.txt` até `maze_9.txt`: Labirintos de diferentes tamanhos e complexidades

## Compilação

```bash
make clean && make && ./maze_solver
```

## Uso

```bash
./maze_solver mazes/<maze_name>.txt
```

## Saída

O programa gera dois arquivos:
1. `output.txt`: Labirinto com o caminho marcado com `.`
2. `path.json`: Coordenadas do caminho em formato JSON

Exemplo de saída em `output.txt`:
```
##########
#S.......#
# ######.#
#      #.#
# #### #.#
# #    #.#
# # ####.#
#      #E#
##########
```

Exemplo de saída em `path.json`:
```json
{
  "path": [
    {"x": 1, "y": 1},
    {"x": 2, "y": 1},
    {"x": 3, "y": 1},
    ...
  ]
}
```

O programa também imprime o tempo de execução em milissegundos.

## Estrutura do Projeto

```
.
├── Makefile
├── README.md
├── mazes/
│   ├── maze_1.txt
│   ├── maze_2.txt
│   └── ...
├── src/
│   └── maze_solver.c
└── .gitignore
```
