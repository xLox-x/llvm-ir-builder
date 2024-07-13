int start = 1;
int end = 10;
int result = 0;

int printf(const char *format, ...);

int sum(int x, int y) {
    printf("result:%d\n", result);
    return x + y;
}

int main() {
    return sum(start, end);
}