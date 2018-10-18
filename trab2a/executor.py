from re import findall
from subprocess import call
from collections import defaultdict, OrderedDict


IS_IN_CLUSTER = True
SIZES=[2, 4, 8]
MATH_FUNCTIONS=4


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
        "mrexec all rm -rf stack.h",
        "mrexec all rm -rf stack.c",
        "mrexec all rm -rf math_function.h",
        "mrexec all rm -rf math_function.c",
        "mrexec all rm -rf main",
        "mrcp all ./main.c main.c",
        "mrcp all ./stack.c stack.c",
        "mrcp all ./stack.h stack.h",
        "mrcp all ./math_function.c math_function.c",
        "mrcp all ./math_function.h math_function.h",
        "mrexec all mpicc -o main main.c math_function.c stack.c -std=c11 -lm"
    ]
    if not IS_IN_CLUSTER:
        commands = ["mpicc main.c math_function.c stack.c -o main"]
    run_commands(commands)


def execute():
    for execution in SIZES:
        for func in range(MATH_FUNCTIONS):
            command = "echo 'mpirun -np {} main 0 1 {}' >> output.txt".format(execution, func)
            run_commands([command])
            command = "mpirun -np {} -hostfile host_file main 0 1 {} >> output.txt".format(execution, func)
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
        if "Time-executor" in line or "Result:" in line:
            continue
        elif "Time-start-program" in line:
            tag = "start"
            typo = "Time-start-program"
        elif "Time-calculator" in line:
            tag = "calc"
            typo = "Time-calculator"
        elif "Time-network" in line:
            tag = "net"
            typo = "Time-network"
        elif "Time-diff-end-end" in line:
            tag = "end"
            typo = "Time-diff-end-end"
        elif "Time-coordinator" in line:
            tag = "coordinator"
            typo = "Time-coordinator"
        elif "Time" in line:
            tag = "total"
            typo = "Time"

        if tag:
            node, time = get_node_time(line, typo)
            key = "{},{},{}".format(nodes, function, node)
            data[key][tag] = time
        elif "mpirun -np" in line:
            nodes, function = build_head(line)

    print("nodes,function,node,start,calc,net,end,total")
    for node, values in OrderedDict(sorted(data.items())).items():
        if "calc" in values:
            print("{},{},{},{},{},{}".format(
                node, values["start"], values["calc"], 
                values["net"], values["end"], values["total"]
            ))
        else:
            print("{},{},{},{},{},{}".format(
                node, values["start"], 0, values["coordinator"], 
                values["end"], values["total"]
            ))


clean()
build()
execute()
generate_metrics()