#include <iostream>
#include "server.h"

using std::cout, std::endl, std::cin, std::string;
int main() {
    cout << "Input IP [127.0.0.1]: ";
    std::string in;
    std::getline(cin, in);

    if (in.empty()) in = "127.0.0.1";
    
    run_client( { in } );
}