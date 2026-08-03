#!/usr/bin/env python2
# -*- coding: utf-8 -*-
import csv

tm_file = "./results/HW3_test2_time.dat"
tv_file = "./results/HW3_test2_freq.dat"

srow = []
xrow = []
i = 0

with open(tv_file) as csv_file:
    csv_reader = csv.reader(csv_file, delimiter=',')
    for row in csv_reader:
        srow.append(row[0])
        # xrow.append(row[3])


with open(tm_file) as csv_file:
    csv_reader = csv.reader(csv_file, delimiter=',')
    for row in csv_reader:
        if(int(row[0])!=0):
            print srow[i],
            # print ',',
            # print xrow[i],
            print ',',
            print row[0]
        i += 1
        
        
        