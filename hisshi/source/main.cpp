#include "search.h"
#include "thread.h"
#include "tt.h"
#include "usi.h"
#include "misc.h"


int main(int argc, char* argv[])
{

	CommandLine::init(argc,argv);
	USI::init(Options);
	Bitboards::init();
	Position::init();
	Search::init();

	size_t thread_num = Options.count("Threads") ? (size_t)Options["Threads"] : 1;
	Threads.set(thread_num);

	Eval::init();


	USI::loop(argc, argv);


	Threads.set(0);

	return 0;
}
