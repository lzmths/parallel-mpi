from collections import defaultdict, OrderedDict
from re import findall
from subprocess import call


def run_commands(commands):
    command = " && ".join(commands)
    print(command)
    return call(command, shell=True)


def clean():
    commands = [
        "rm output.txt",
        "touch output.txt"
    ]
    run_commands(commands)


def build():
    commands = [
        "mrexec all rm -rf main.c",
        "mrexec all rm -rf main",
        "mrcp all ./main.c main.c",
        "mrcp all ./math_function.c math_function.c",
        "mrcp all ./math_function.h math_function.h",
        "mrexec all mpicc -o main main.c -std=c11 -lm"
    ]
    run_commands(commands)


def execute():
    for execution in [1, 2, 4, 8]:
        for func in range(3):
            command = "echo 'mpirun -np {} main 0 1 {}' >> output.txt".format(execution, func)
            run_commands([command])
            command = "mpirun -np {} -host host_file main 0 1 {} >> output.txt".format(execution, func)
            run_commands([command])


def get_node_time(line, split_base):
    line = line.replace("{} (".format(split_base), "")
    line = line.replace("):", "")
    node, time = line.split(" ")
    return int(node), float(time)


def build_head(head):
    values = findall(r'\d+', head)
    nodes, _, _, function = [int(value) for value in values]
    return nodes, function

def generate_metrics():
    arquive = open('output.txt')
    nodes, function = build_head(next(arquive))
    data = defaultdict(dict)
    for line in arquive:
        line = line.strip()
        tag = None
        if "Time-around-quad" in line:
            tag = "quad"
            typo = "Time-around-quad"
        elif "Time-total" in line:
            tag = "total"
            typo = "Time-total"

        if tag:
            node, time = get_node_time(line, typo)
            key = "{}-{}-{}".format(nodes, function, node)
            data[key][tag] = time
        elif "mpirun -np" in line:
            nodes, function = build_head(line)

    print("node,quad,total")
    for node, values in OrderedDict(sorted(data.items())).items():
        print("{},{},{}".format(node, values["quad"], values["total"]))

clean()
build()
execute()
generate_metrics()