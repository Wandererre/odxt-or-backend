from os import write, read

in_file = "./results/HW2_test1_freq.dat"

counter = 0
max_len = 0

with open(in_file) as file:
    for line in file:
        if(int(line) > max_len):
            max_len = int(line)
        counter += 1

print "Number of kws: " + str(counter)
print "Max freq: " + str(max_len)