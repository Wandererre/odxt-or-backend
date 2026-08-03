import os

bucket_sizes = [5, 10]
hamming_weights = [2, 3, 4, 5, 6, 7, 8, 9, 10]
isOptimized = [0, 1]
# bucket_sizes = [5]
# hamming_weights = [2]
# isOptimized = [0]
num_queries = 50


def read_stats(filename:str):
    lines = []
    with open(filename) as file:
        lines = file.readlines()
    lines = [elem[0:len(elem)-2] for elem in lines]
    return lines

def custom_read(filename:str):
    lines = []
    with open(filename) as file:
        lines = file.readlines()
    return lines


def func(opt:int, bs:int, hw:int, to_setup:bool):
    dir_name = f"{opt}_{bs}_{hw}"
    if(to_setup):
        os.system(f"make clean_all")
    else:
        os.system(f"make clean")
    os.system(f"make single_thread")
    os.system(f"./sse_sing_thread_out {dir_name} 97545 {bs} {opt} {hw} {num_queries}")
    os.system(f"make clean")

    # exp = custom_read(f"./test_vectors/{dir_name}/metaquery_result.csv")
    # act = custom_read(f"./results/{dir_name}/res_id.csv")
    # if act != exp:
    #     print(f"Error in {dir_name}")
    #     exit(1)

    # s1 = read_stats(f"./test_vectors/{dir_name}/stats.csv")
    s1 = read_stats(f"./results/{dir_name}/res_stats.csv")
    # for elem in s2:
    #     s1.append(elem)
    # s1.append(0); s1.append(0)
    return s1


results = []

for opt in isOptimized:
    for bs in bucket_sizes:
        for hw in hamming_weights:
            r = [opt, bs, hw]
            for elem in func(opt, bs, hw, hw==hamming_weights[0]):
                r.append(elem)
            results.append(r)
        

indexes = ["Scheme", "Bucket Size", "Hamming Weight", "Total Setup Time (microseconds)", "Total Query Time (microseconds)", "Avg Update Time", "Avg #mkw updates per kw update", "Avg Query Time", "Avg Precision", "Avg #mkws per query", "Avg #ODXT search routines per query"]

with open('results.csv', 'w') as file:
    file.write(','.join(indexes) + '\n')
    for row in results:
        file.write(','.join(map(str, row)) + '\n')

print("Done")