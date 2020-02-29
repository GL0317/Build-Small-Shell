File: README.txt
Author: Gerson Lindor Jr.
Date: February 26, 2020

To compile the program:

gcc -g -std=gnu99 smallsh.c -o smallsh

To run test, p3testscript, (use one of the options below):

p3testscript 2>&1
p3testscript 2>&1 | more
p3testscript > mytestresults 2>&1 

