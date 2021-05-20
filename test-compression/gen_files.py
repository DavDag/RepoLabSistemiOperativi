import random as rm

path = "//wsl$/Ubuntu/home/davdag/sis-op/test-compression/testdir"

def gen_file(name, values):
    global path
    size = 256*256*(rm.randint(1, 512)) # 1 ~> 512 Mb
    with open(path + "/" + name, "wb") as f:
        for i in range(0, size):
            f.write(rm.choice(values))

for i in range(0, 32):
    name = "test_" + str(i) + ".txt"
    values = [chr(rm.randint(32, 128)).encode('utf-8') for j in range(0, 8 * rm.randint(8, 16))]
    gen_file(name, values)

