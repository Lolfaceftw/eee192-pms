import argparse
import binascii
import time

parser = argparse.ArgumentParser()

parser.add_argument("-o", "--output", type=str, default="putty.log")
parser.add_argument("-t", "--time", type=int, default=1.01)
args = vars(parser.parse_args())
TIME_DELAY = args["time"]
filename = args["output"]
def byte_to_string(line: bytes) -> str:
    return line.replace(starting_bits, b'\n' + starting_bits).decode("utf-8")

# ! The commented portions assume data is represented in integers.
# def string_to_hex(string: str, data_num: int) -> str:
#     first_part = string[0:2]
#     second_part = string[2:4]
#     #print(f"Data {data_num} Hex: {string} | {first_part} | {second_part}")
#     return f"{int(first_part, 16)}.{int(second_part, 16)}"
def clear_line(*args, **kwargs) -> None:
    """ Pretty much just clears the line so it doesn't leave previous characters """
    print(" "*60, end="\r")

while True:
    with open(filename, 'rb') as f:
        content = f.read()
    hex_content = binascii.hexlify(content)
    starting_bits = b'424d'
    
    last_line_b = hex_content.split(starting_bits)[::-1][0]
    last_line_str = byte_to_string(last_line_b)
    # !The last line does not include 0x424d
    # We choose the particles under atmospheric conditions per the datasheet.
    data_1 = last_line_str[16:20]
    data_2 = last_line_str[20:24]
    data_3 = last_line_str[24:28]
    with open("convert.log", "a") as f:
        # print(f"Hex: {last_line_str}")
        # print(f"Hex: {last_line_str}", file=f)
        try:
            # print(f"PM1.0: {string_to_hex(data_1, 1)} | PM2.5: {string_to_hex(data_2, 2)} | PM10: {string_to_hex(data_3, 3)} || Unit: ug/m3")
            # print(f"PM1.0: {string_to_hex(data_1, 1)} | PM2.5: {string_to_hex(data_2, 2)} | PM10: {string_to_hex(data_3, 3)} || Unit: ug/m3", file=f)
            # print("-"*10, "No decimal", "-"*10)
            clear_line()
            print(f"PM1.0: {int(data_1, 16)} | PM 2.5: {int(data_2, 16)} | PM 10: {int(data_3, 16)} || Unit: ug/m3", end="\r")
            print(f"PM1.0: {int(data_1, 16)} | PM 2.5: {int(data_2, 16)} | PM 10: {int(data_3, 16)} || Unit: ug/m3", file=f)
        except:
            print("Error! Bits are short... looping again...", end="\r")
            print("Error! Bits are short... looping again...", file=f)
            #time.sleep(TIME_DELAY)
            continue
    time.sleep(TIME_DELAY)
