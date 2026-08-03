#!/usr/bin/env python2
# -*- coding: utf-8 -*-
import csv

db_file = "widrxdb.csv"

query = ["00000006","0000001a"]

srow = []

drow = {}
rrow = []

with open(db_file) as csv_file:
    csv_reader = csv.reader(csv_file, delimiter=',')
    for row in csv_reader:
        srow.append(row[:-1])
    
for r in srow:
    drow[r[0]] = r[1:]

for q in query:
    rrow.append(drow[q])


sirow = set(rrow[0])
for sr in rrow:
    sirow = sirow & (set(sr))

print "Set len:" + str(len(sirow))
print sirow