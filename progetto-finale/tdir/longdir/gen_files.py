import random as rm
import os

path = os.getcwd()

def conv_bytes(size):
    if size < 1024:          return str(size) + "B"
    elif size < 1024 * 1024: return "{:4.2f} KB".format(size / 1024)
    else:                    return "{:4.2f} MB".format(size / 1204 / 1024)

def gen_file(name, values):
    size = 1024 * rm.randint(1, 64) # 1kb ~> 64kb
    with open(name, "wb") as f:
        for i in range(0, size):
            f.write(rm.choice(values))
    return size

for i in range(0, 64):
    name = path + "/file{:02d}.txt".format(i)
    values = [chr(rm.randint(0, 127)).encode('utf-8') for j in range(0, 8 * rm.randint(8, 16))]
    size = gen_file(name, values)
    print("{0}: {1}".format(name, conv_bytes(size)))
