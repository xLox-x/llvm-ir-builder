int start = 1;
int end = 10;
int result = 0;

int main() {
    int index = start;
    while(index <= end) {
        result = result +index;
        index = index + 1;
    }
    return result;
}