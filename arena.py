import os
import subprocess
import sys
import tempfile

def solve_maze(labyrinth: str) -> float:
    """
    Recebe um labirinto como string (com \n separando as linhas)
    e retorna o tempo total em milissegundos para resolvê-lo.
    Você deve gerar um arquivo chamado output.txt para que ele possa ser
    auditado.
    """
    # Garante que o executável existe
    if not os.path.exists("maze_solver"):
        raise RuntimeError("maze_solver não encontrado. Execute 'make' primeiro.")
    
    # Cria um arquivo temporário para o labirinto
    with tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.txt') as temp:
        temp.write(labyrinth)
        temp_path = temp.name
    
    try:
        # Executa o solver
        result = subprocess.run(
            ["./maze_solver", temp_path],
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
        # Limpa o arquivo temporário
        try:
            os.unlink(temp_path)
        except:
            pass

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Uso: python arena.py <arquivo_labirinto>")
        sys.exit(1)
        
    try:
        with open(sys.argv[1], 'r') as f:
            maze = f.read()
        time_ms = solve_maze(maze)
        print(f"Tempo de resolução: {time_ms} ms")
    except Exception as e:
        print(f"Erro: {e}", file=sys.stderr)
        sys.exit(1) 