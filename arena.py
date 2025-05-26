import os
import subprocess
import sys
import tempfile

def solve_maze(labyrinth: str, is_file: bool = False) -> float:
    """
    Recebe um labirinto (como string ou caminho do arquivo) e retorna o tempo total em milissegundos.
    
    Args:
        labyrinth: String contendo o labirinto ou caminho do arquivo
        is_file: True se labyrinth é um caminho de arquivo, False se é uma string do labirinto
    """
    # Garante que o executável existe
    if not os.path.exists("maze_solver"):
        raise RuntimeError("maze_solver não encontrado. Execute 'make' primeiro.")
    
    if is_file:
        # Se for um arquivo, usa ele diretamente
        maze_path = labyrinth
        temp_path = None
    else:
        # Se for string, cria arquivo temporário
        with tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.txt') as temp:
            temp.write(labyrinth)
            temp_path = temp.name
            maze_path = temp_path
    
    try:
        # Executa o solver
        result = subprocess.run(
            ["./maze_solver", maze_path],
            capture_output=True,
            text=True,
            check=True
        )
        
        # Extrai o tempo de resolução da saída
        for line in result.stdout.split('\n'):
            if "Resolução A*:" in line:
                time_str = line.split(':')[1].strip()
                time_ms = float(time_str.replace('ms', '').strip())
                return time_ms
                
        raise RuntimeError("Não foi possível encontrar o tempo de resolução na saída")
        
    except subprocess.CalledProcessError as e:
        raise RuntimeError(f"Erro ao executar maze_solver: {e.stderr}")
    finally:
        # Limpa o arquivo temporário se foi criado
        if temp_path:
            try:
                os.unlink(temp_path)
            except:
                pass

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Uso:")
        print("  Com arquivo: python arena.py arquivo.txt")
        print("  Com string: python arena.py \"##########\\n#S       #\\n...\"")
        sys.exit(1)
        
    try:
        input_maze = sys.argv[1]
        
        # Verifica se é um arquivo que existe
        if os.path.isfile(input_maze):
            time_ms = solve_maze(input_maze, is_file=True)
        else:
            # Considera como string do labirinto
            maze_str = input_maze.replace('\\n', '\n')
            time_ms = solve_maze(maze_str, is_file=False)
            
        print(f"Tempo de resolução: {time_ms} ms")
    except Exception as e:
        print(f"Erro: {e}", file=sys.stderr)
        sys.exit(1) 