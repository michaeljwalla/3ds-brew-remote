#include <iostream>
#include "server.h"

using std::cout, std::endl;
int main() {
    cout << "Hello, world!" << endl;
    run_client({"127.0.0.1"});
}