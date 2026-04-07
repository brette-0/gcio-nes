import math

def __main__():
    gen_square_table()
    gen_atan_table()
    gen_reciprocal_table()
    return

def gen_square_table():
    out : str = ""
    for line in range(0, 16):
        for word in range(0, 16):
            out += f"0x{(((line << 4) | word) * ((line << 4) | word)):04x}, "
        out = out[:-1] + '\n'
    out = out[:-1]

    with open("gen/square_table", "w") as f:
        f.write(out)

def gen_atan_table():
    out : str = ""
    for line in range(0, 8):
        for byte in range(0, 16):
            out += f"0x{int(round(math.atan((line << 4) | byte) * 256 / (2 * math.pi))):02x}, "
        out = out[:-1] + '\n'
    out = out[:-1]

    with open("gen/atan_table", "w") as f:
        f.write(out)


if __name__ == "__main__":
    __main__()