#! /bin/bash

rg -g "*.cpp" -g "*.h" --files | xargs clang-format -i
