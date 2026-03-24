#include "blocked_queue.h"

#include <iostream>
#include <memory>
#include <algorithm>

using namespace std;

int main() {

    cout << "test" << endl;
    {
        auto arr = std::make_unique<int[]>(20);
        for (int i = 0; i < 20; ++i) {
            arr[i] = i;
        }

        std::for_each(arr.get(), arr.get() + 20, [](int item){
            std::cout << item << endl;
        });
    }

    return 0;
}