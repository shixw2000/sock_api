
extern int testRb();
extern void testTimer();
extern int testPoll(int argc, char* argv[]);

static int test(int argc, char* argv[]) {
    int ret = 0;
    
    (void)argc;
    (void)argv;

#ifdef __TEST__
    ret = testRb();
#elif defined( __TEST_POLL__)
    ret = testPoll(argc, argv);
#endif

    return ret;
}

int main(int argc, char* argv[]) {
    int ret = 0;

    ret = test(argc, argv);

    return ret;
}

