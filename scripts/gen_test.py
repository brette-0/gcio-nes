from enum import Enum
from sys import argv
import json

inputs : int = 0

class vector2:
    x : int = 0
    y : int = 0

    def __init__(self, members : dict[str, int]) -> None:
        outliers : list[str] = [member for member in members if member not in {"x", "y"}]
        if any(member not in {"x", "y"} for member in members):
            raise ValueError(f"unexpected members: {outliers}")
        
        for member in ("x", "y"):
            if member not in members:
                raise ValueError(f"missing member: '{member}'") 
            

        self.x = members["x"]
        self.y = members["y"]

class Commands(Enum):
    INPUT = 0

def __main__(args : list[str]) -> None:
    def get_vector() -> vector2:
        nonlocal command

        if command[2] is not dict:
            raise TypeError(f"{command[2]} not a struct for vector2")
                
        return vector2(command[2])

    def input_command():
        nonlocal command
        nonlocal test_bytecode

        match command[1]:
            case "lstick":
                stick_vector : vector2 = get_vector()

                test_bytecode[3] = stick_vector.x
                test_bytecode[4] = stick_vector.y

                pass

            case "cstick":
                stick_vector : vector2 = get_vector()

                test_bytecode[5] = stick_vector.x
                test_bytecode[6] = stick_vector.y
                pass

            case "ltrigger":
                pass

            case "rtrigger":
                pass

            case "a":
                pass

            case "b":
                pass

            case "x":
                pass

            case "y":
                pass
            
            case "z":
                pass

            case "start":
                pass

            case "up":
                pass

            case "down":
                pass

            case "left":
                pass

            case "right":
                pass

            case _:
                raise ValueError(f"{command[1]} is not a valid input command")

        

    for arg in args:
        test_bytecode : bytearray = bytearray()

        with open(arg, "rb") as f:
            test_src : list[str] = json.load(f)
        
        for src in test_src:
            command : list[str] = src.split(' ')
            match command[0]:
                case "input":
                    test_bytecode.append(Commands.INPUT.value)
                    input_command()

                case _:
                    pass

            print(src)
        
        


        with open(f"{arg}_bytecode", "wb") as f:
            f.write(test_bytecode)
        print(arg)

    return


if __name__ == "__main__":
    __main__(argv)