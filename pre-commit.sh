#!/bin/sh

# Pre-commit hook for ted development
# To install:
#   ln -s ../../pre-commit.sh .git/hooks/pre-commit

# Check for trailing whitespace
git grep -In ' $' && { echo 'Fix trailing whitespace!'; exit 1; }
git grep -In '\w'"$(printf '\t')"'$' && { echo 'Fix trailing whitespace!'; exit 1; }
exit 0
