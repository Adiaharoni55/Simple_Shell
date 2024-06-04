#!/bin/bash
echo "warm up:"
echo 1 2 3 4
echo "1 2 3 4"
echo "1" "2" "3" "4"
echo "12 34 56 78 9"
echo "1 2 3 4" 5 6 7
echo should be now "4 ERR:"
echo 1 2 3 4 5
echo "1 2 3 4" 5 6 7 8
echo "1" "2" "3" "4" "5"
echo "1" "2" "3" "4" 5
echo "if you failed those go to make sandwiches"
echo lets continue
alias g1 = 'ls'
g1
alias g1 = 'echo "did you override the ls ?????"'
g1
alias g2 = 'echo 1 2 3 4'
alias g3 = 'echo "1" "2" "3" "4"'
alias g4 = 'echo "1 2 3 4 5"'
alias g5 = 'echo "1 2 3 4" 5 6 7'
alias g6='echo "1 2 3 4 5" 6 7 8 9'
alias g7 = 'echo "1" "2" "3" "4" 5'
alias g8 = 'echo "1 2 3 4" 5 6 7'
alias g9 = 'echo'
g2
g3
g4
g5
g9 1 2 3 4
g9 should print ERR now
g6
g7
g8
g9 "YOU FAILED"
g9 stam "last wasnt ERR" "but now ERR"
g9 "1" "2" "3" "4"
g9 "relax omer kidding"
g9 now ERR
g9 "1" "2" "3" "4" 5
alias
alias alias = 'ls'
echo should print ls now
alias
unalias alias
unalias g1
echo should print "alias" "2-9"
alias

#c

#abc
echo "print 4 ERR"
alias ll ls
alias ll 'ls'
alias ll = ls
alias ll=ls
echo "should be 44 cmd, 8 alias, 58 script lines, do exit_shell and should print 25 quotes"
