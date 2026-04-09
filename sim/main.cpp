#include <iostream>
#include <string>
#include <vector>

extern "C" {
    #include "../src/main.h"
}

void ExitOnComplete(std::vector<std::string> args);
void ExitOnRequest();
void RunTest(std::string arg);

int main(int argc, char** argv){
    std::vector<std::string> args(argv, argv + argc);
    std::cout << "gcio sim Brette Allen (2026)\n";

    if (argc > 1){
        ExitOnComplete(args);
    } else {
        ExitOnRequest();
    }
    return 0;
}

void ExitOnRequest() {
    while (true) {

    }
}

void ExitOnComplete(std::vector<std::string> args){
    for (const auto& arg : args){
        RunTest(arg);
    }
    
}

void RunTest(const std::string arg){

}