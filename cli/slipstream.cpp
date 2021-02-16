#include "lt/slipstream/cli.h"

using namespace lt::slipstream;

int main(int argc, char* argv[])
{
    return cli::main<PlainText>(argc, argv);
}
