#include <iostream>
#include <string>

int main(int argc, char **argv) {
    std::string student_id = argc > 1 ? argv[1] : "unknown_id";
    std::string name = argc > 2 ? argv[2] : "unknown_name";
    std::cout << "Hello gem5\n";
    std::cout << "student_id=" << student_id << "\n";
    std::cout << "name=" << name << "\n";
    return 0;
}
