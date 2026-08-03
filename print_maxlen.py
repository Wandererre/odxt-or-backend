import csv
from os import write

# in_file = "wiki_MDB_22k.csv"
in_file = "widxrdb.csv"

counter = 0
kw_id_counter = 0
max_len = 0

with open(in_file) as csv_file:
    csv_reader = csv.reader(csv_file,delimiter=',')
    for row in csv_reader:
        kw_id_counter += len(row)
        if(len(row) > max_len):
            max_len = len(row)
        counter += 1

print "Number of keywords: " + str(counter)
print "Number of max ids: " + str(max_len)
print "Number of kw-id pairs: " + str(kw_id_counter)