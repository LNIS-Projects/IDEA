import sys


with open(sys.argv[1]) as a:
    with open(sys.argv[2]) as b:
        c = [int(i.strip()) for i in a.readlines()]
        d = [int(i.strip()) for i in b.readlines()]
        z = max(max(c[4:]), max(d[4:])) + 2
        print(z)

        x = list([
            list([0 for _ in range(0, z)])
            for _ in range(0, z)])

        for i, j in zip(c[4:], d[4:]):
            x[i + 1][j + 1] += 1

        for i in x:
            print("\t".join([str(j) for j in i]))
