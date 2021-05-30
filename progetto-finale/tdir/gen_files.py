import random as rm
import os
import sys

numFiles  = int([arg.split("=", 1)[1] for arg in sys.argv if arg.startswith("-numFiles=")][0]) 
minSizeKb = int([arg.split("=", 1)[1] for arg in sys.argv if arg.startswith("-minSizeKb=")][0]) 
maxSizeKb = int([arg.split("=", 1)[1] for arg in sys.argv if arg.startswith("-maxSizeKb=")][0]) 
outDir    = [arg.split("=", 1)[1] for arg in sys.argv if arg.startswith("-outDir=")][0]

os.mkdir(outDir)
path = os.path.join(os.getcwd(), outDir)

def conv_bytes(size):
    if size < 1024:          return str(size) + "B"
    elif size < 1024 * 1024: return "{:4.2f} KB".format(size / 1024)
    else:                    return "{:4.2f} MB".format(size / 1204 / 1024)

def gen_file(name, values):
    size = 1024 * rm.randint(minSizeKb, maxSizeKb)
    with open(name, "wb") as f:
        for i in range(0, size):
            f.write(rm.choice(values))
    return size

for i in range(0, numFiles):
    name = path + "/file{:02d}.txt".format(i)
    values = [chr(rm.randint(0, 127)).encode('utf-8') for j in range(0, 8 * rm.randint(8, 16))]
    size = gen_file(name, values)
    print("{0}: {1}".format(name, conv_bytes(size)))
