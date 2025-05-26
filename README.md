# A* Maze Solver

Este projeto implementa um solucionador de labirinto usando o algoritmo A* (A-Star). O programa encontra o menor caminho entre dois pontos em um labirinto 2D.

## Estrutura do Projeto

```
.
├── Makefile
├── README.md
├── arena.py         # Interface Python para o solver
├── mazes/          # Labirintos de exemplo
│   ├── maze_1.txt
│   ├── maze_2.txt
│   └── ...
└── src/
    └── maze_solver.c  # Implementação do A* em C
```

## Formato do Labirinto

O labirinto deve ser fornecido como um arquivo de texto ou string com as seguintes características:
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

## Compilação

```bash
make clean && make
```

## Uso

### Via Python

Existem duas formas de usar o programa via Python:

1. Passando um arquivo de labirinto:
```bash
python/python3 arena.py mazes/maze_1.txt
```

2. Passando o labirinto diretamente como string:
```bash
python/python3 arena.py "##########
#S       #
# ###### #
#      # #
# #### # #
# #    # #
# # #### #
#      #E#
##########"
```

Em ambos os casos, o programa retornará o tempo de execução em milissegundos.

### Via C (Uso Direto)

```bash
./maze_solver mazes/<maze_name>.txt
```

## Saída

O programa gera dois arquivos:
1. `output.txt`: Labirinto com o caminho marcado com `.`
2. `path.json`: Coordenadas do caminho em formato JSON

## Implementação

O solver utiliza o algoritmo A* com as seguintes otimizações:
- Heap binário para seleção eficiente do próximo nó
- Mapeamento direto de posições para índices no heap
- Alocação eficiente de memória
- Heurística de distância Manhattan

## Performance

O tempo retornado é apenas o tempo de execução do algoritmo A*, excluindo operações de I/O.
