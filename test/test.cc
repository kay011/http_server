#include <gtest/gtest.h>

TEST(DemoTest, Test1) { EXPECT_EQ(2, 2); }
int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);  //将命令行参数传递给gtest
  return RUN_ALL_TESTS();  // RUN_ALL_TESTS()运行所有测试案例
}