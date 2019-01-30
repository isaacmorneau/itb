#!/usr/bin/env python3

import sys, re, secrets

simplestring = re.compile('#define ([a-zA-Z_]+) "(.*)"')

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("usage: ./itb_obfs.py /path/to/strings.h /path/to/output.h")
        sys.exit(1)
    with open(sys.argv[1], "r") as f, open(sys.argv[2], "w") as o:
        for line in f:
            cleanline = line.strip()
            if simplestring.match(cleanline):
                parts = simplestring.search(cleanline)
                varname = parts.group(1)
                original = parts.group(2)
                #magic to parse escapes
                #thanks jerub https://stackoverflow.com/a/4020824
                string = bytes(original, "utf-8").decode("unicode_escape").encode()
                encarray = list(secrets.token_bytes(len(string)))
                newarray = []
                for i in range(len(string)):
                    newarray.append(encarray[i] ^ string[i])

                newarray.append(0)
                encarray.append(0)
                o.write("""//#define {1} "{0}"
#define {1}_ENC {{{3},{4}}}
#define {1}_LEN {2}
""".format(original, varname, len(newarray), ','.join(hex(e) for e in newarray), ','.join(hex(e) for e in encarray)))
            else:
                o.write(line)
