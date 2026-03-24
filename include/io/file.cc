#include <iostream>
#include <fstream>
#include <string>

int main() {

    std::string filename = "test.txt";
    std::ofstream fs("test.txt");

    if (!fs.is_open()) {
        perror("open error");
        exit(1);
    }

    fs << "hello";
    fs.close();    

    return 0;
}