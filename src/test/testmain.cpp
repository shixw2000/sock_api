extern int testRb();
extern void testTimer();
extern int testPoll(int argc, char* argv[]);

static int test(int argc, char* argv[]) {
    int ret = 0;

    #ifdef __TEST__
        ret = testRb();
    #else
        ret = testPoll(argc, argv);
    #endif

    return ret;
}

int main(int argc, char* argv[]) {
    int ret = 0;

    ret = test(argc, argv);

    return ret;
}

