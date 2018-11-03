from re import findall
from subprocess import call
from collections import defaultdict, OrderedDict

IS_IN_CLUSTER = True
SIZES=[2, 4, 8]
run = 10
MATH_FUNCTIONS=2


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
    from os.path import dirname, realpath
    current_path = dirname(realpath(__file__))
    commands = [
        "mrexec all rm -rf ./parallel-mpi/",
        "mrexec all git clone https://github.com/lzmths/parallel-mpi.git",
        "mrexec all mpicc -o {}/main {}/main.c {}/math_function.c {}/stack.c -std=c11 -lm".format(current_path),
        "mpicc -o main main.c math_function.c stack.c -std=c11 -lm"
    ]
    if not IS_IN_CLUSTER:
        commands = ["mpicc -o main main.c math_function.c stack.c -std=c11 -lm"]
    run_commands(commands)


def execute():
    for execution in SIZES:
        for func in range(MATH_FUNCTIONS):
            command = "echo 'mpirun -np {} main 0 1 {}' >> output.txt".format(execution, func)
            run_commands([command])
            command = "mpirun -np {} -hostfile ../../host_file ./main 0 1 {} >> output.txt".format(execution, func)
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

def resize():
    exp = 4
    for j in range(exp):
        for i in range(run - 1):
            SIZES.insert(j * run, 2 ** (j + 1))

def generate_metrics():
    arquive = open('output.txt')
    nodes, function = build_head(next(arquive))
    counts = defaultdict(int)
    counts["{}-{}".format(nodes, function)] += 1
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
            if tag not in data[key]:
                data[key][tag] = 0.0
            data[key][tag] += time
        elif "mpirun -np" in line:
            nodes, function = build_head(line)
            counts["{}-{}".format(nodes, function)] += 1

    print("nodes,function,node,start,calc,net,end,total,count")
    for node, values in OrderedDict(sorted(data.items())).items():
        count = counts["{}-{}".format(*node.split(",")[0:2])]
        if "calc" in values:
            print("{},{},{},{},{},{},{}".format(
                node, values["start"]/count, values["calc"]/count, 
                values["net"]/count, values["end"]/count, 
                values["total"]/count,count
            ))
        else:
            print("{},{},{},{},{},{},{}".format(
                node, values["start"]/count, 0, values["coordinator"]/count, 
                values["end"]/count, values["total"]/count,count
            ))

clean()
build()
resize()
execute()
generate_metrics()
