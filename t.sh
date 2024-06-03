#!/bin/bash

# Basic Bash Script
# This script demonstrates basic bash commands, including alias and unalias.

# Print a welcome message
echo "Welcome to the basic bash script!"

# Define some aliases
alias ll='ls -la'
alias gs='echo a'
alias gp='echo b'

# Print a message indicating that aliases have been set
echo "Aliases set: ll, gs, gp"

# Demonstrate the use of aliases
echo "Listing files with 'll' alias:"
ll

# Checking git status with 'gs' alias (assuming you're in a git repository)
echo "Checking git status with 'gs' alias:"
gs

# Pulling latest changes with 'gp' alias (assuming you're in a git repository)
echo "Pulling latest changes with 'gp' alias:"
gp

# Remove aliases
unalias ll
unalias gs
unalias gp

# Print a message indicating that aliases have been removed
echo "Aliases removed: ll, gs, gp"

# Print a goodbye message
echo "Goodbye from the basic bash script!"