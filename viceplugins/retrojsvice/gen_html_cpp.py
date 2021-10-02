#!/usr/bin/env python3

import os
import re

print('#include "../src/html.hpp"')
print()
print('namespace retrojsvice {')

for filename in os.listdir("html"):
    if not filename.endswith(".html"):
        continue

    name = "".join(x.capitalize() for x in filename[:-5].split("_"))

    with open("html/" + filename) as fp:
        code = fp.read()

    code = re.sub(r'%-([a-zA-Z0-9]+)-%', r')DELIM" << data.\1 << R"DELIM(', code)

    print()
    print('void write{}HTML(ostream& out, const {}HTMLData& data) {{'.format(name, name))
    print('    out << R"DELIM(' + code + ')DELIM";')
    print('}')

print()
print('}')
