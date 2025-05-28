import random
import os
from typing import List, Tuple

def generate_maze(rows: int, columns: int) -> List[List[str]]:
    """
    Gera um labirinto usando o algoritmo de busca em profundidade com backtracking.
    
    Args:
        rows (int): Número de linhas do labirinto
        columns (int): Número de colunas do labirinto
        
    Returns:
        List[List[str]]: Lista 2D representando o labirinto
    """
    # Garante que as dimensões sejam ímpares para uma geração adequada
    if rows % 2 == 0:
        rows += 1
    if columns % 2 == 0:
        columns += 1

    # Inicializa o labirinto com paredes
    maze = [['#'] * columns for _ in range(rows)]

    def is_in_bounds(x: int, y: int) -> bool:
        """Verifica se as coordenadas estão dentro dos limites do labirinto."""
        return 0 <= x < rows and 0 <= y < columns

    # Começa da posição (1,1)
    stack = [(1, 1)]
    maze[1][1] = ' '

    while stack:
        current_x, current_y = stack[-1]
        # Direções possíveis: direita, baixo, esquerda, cima
        directions = [(0, 2), (2, 0), (0, -2), (-2, 0)]
        random.shuffle(directions)
        carved = False

        for dx, dy in directions:
            new_x, new_y = current_x + dx, current_y + dy
            if is_in_bounds(new_x, new_y) and maze[new_x][new_y] == '#':
                # Abre o caminho
                maze[current_x + dx // 2][current_y + dy // 2] = ' '
                maze[new_x][new_y] = ' '
                stack.append((new_x, new_y))
                carved = True
                break

        if not carved:
            stack.pop()

    # Define os pontos de início e fim
    maze[1][1] = 'S'
    maze[rows - 2][columns - 2] = 'E'
    
    return maze

def save_maze(maze: List[List[str]], suffix: str) -> str:
    """
    Salva o labirinto em um arquivo com o sufixo fornecido.
    
    Args:
        maze (List[List[str]]): O labirinto a ser salvo
        suffix (str): Sufixo para o nome do arquivo
        
    Returns:
        str: O nome do arquivo onde o labirinto foi salvo
    """
    maze_str = '\n'.join(''.join(row) for row in maze)
    filename = f"maze_{suffix}.txt"
    
    # Cria o diretório mazes se não existir
    os.makedirs("mazes", exist_ok=True)
    filepath = os.path.join("mazes", filename)
    
    with open(filepath, "w", encoding="utf-8") as f:
        f.write(maze_str)
    
    return filepath

def main():
    """Função principal para lidar com entrada do usuário e geração do labirinto."""
    try:
        rows = int(input("Digite o número de linhas: "))
        columns = int(input("Digite o número de colunas: "))
        suffix = input("Digite o sufixo para o arquivo (ex: '19' para 'maze_19.txt'): ")
        
        maze = generate_maze(rows, columns)
        filepath = save_maze(maze, suffix)
        
        print(f"Labirinto gerado e salvo como '{filepath}'.")
        
    except ValueError:
        print("Erro: Por favor, insira números válidos para as dimensões do labirinto.")
    except Exception as e:
        print(f"Erro ao gerar o labirinto: {str(e)}")

if __name__ == "__main__":
    main()
