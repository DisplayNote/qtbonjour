// The tests run under googletest's gmock_main. Socket notifiers and the
// register's worker thread need a QCoreApplication, so a global test
// environment owns one for the whole run.

#include <gtest/gtest.h>

#include <QCoreApplication>

#include <memory>

namespace {

class QtApplicationEnvironment : public ::testing::Environment
{
public:
    void SetUp() override { m_app = std::make_unique<QCoreApplication>(m_argc, m_argv); }

    void TearDown() override { m_app.reset(); }

private:
    int m_argc = 1;
    char m_arg0[20] = "qtbonjourtests";
    char * m_argv[2] = {m_arg0, nullptr};
    std::unique_ptr<QCoreApplication> m_app;
};

const auto * const environment =
    ::testing::AddGlobalTestEnvironment(new QtApplicationEnvironment);

} // namespace
