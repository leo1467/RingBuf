#!/usr/bin/env python3

import re
with open("seq") as f:
    for lineno, line in enumerate(f, 1):
        nums = list(map(int, re.findall(r"\[(\d+)\]", line)))
        if len(nums) == 3:
            if len(set(nums)) != 1:
                print(f"{line}")