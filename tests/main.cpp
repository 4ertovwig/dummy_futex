#include <gtest/gtest.h>

void sigchild_handler(int)
{
	//Catch SIGCHLD from interprocess tests
}

int main(int argc, char* argv[]) {
	::signal(SIGCHLD, sigchild_handler);
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
