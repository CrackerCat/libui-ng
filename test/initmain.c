// 10 april 2019
#include "test.h"

// TODO test the number of calls to queued functions made

#define errInvalidOptions "options parameter to uiInit() must be NULL"
#define errAlreadyInitialized "libui already initialized"

TestNoInit(Init)
{
	uiInitError err;

	if (uiInit(NULL, NULL))
		TestErrorf("uiInit() with NULL error succeeded; expected failure");

	memset(&err, 0, sizeof (uiInitError));

	err.Size = 2;
	if (uiInit(NULL, &err))
		TestErrorf("uiInit() with error with invalid size succeeded; expected failure");

	err.Size = sizeof (uiInitError);

	if (uiInit(&err, &err))
		TestErrorf("uiInit() with non-NULL options succeeded; expected failure");
	if (strcmp(err.Message, errInvalidOptions) != 0)
		TestErrorf("uiInit() with non-NULL options returned bad error message:" diff("%s"),
			err.Message, errInvalidOptions);
}

#if 0

testingTest(InitAfterInitialized)
{
	uiInitError err;

	memset(&err, 0, sizeof (uiInitError));
	err.Size = sizeof (uiInitError);
	if (uiInit(NULL, &err))
		testingTErrorf(t, "uiInit() after a previous successful call succeeded; expected failure");
	if (strcmp(err.Message, errAlreadyInitialized) != 0)
		testingTErrorf(t, "uiInit() after a previous successful call returned bad error message:" diff("%s"),
			err.Message, errAlreadyInitialized);
}

struct testParams {
	uint32_t flag;
	timerSysError err;
};

/*
TODO if I remove the uiQuit() from this test on Windows, I will occasionally get
=== RUN   TestQueueMain_DifferentThread
    ../test/initmain.c:161: uiMain() timed out (5s)
--- FAIL: TestQueueMain_DifferentThread (4.9989539s)
*/
static void queued(void *data)
{
	struct testParams *p = (struct testParams *) data;

	p->flag = 1;
	uiQuit();
}

testingTest(QueueMain)
{
	struct testParams p;

	memset(&p, 0, sizeof (struct testParams));
	p.flag = 0;
	uiQueueMain(queued, &p);
	timeout_uiMain(t, 5 * timerSecond);
	if (p.flag != 1)
		testingTErrorf(t, "uiQueueMain() didn't set flag properly:" diff("%d"),
			p.flag, 1);
}

#define queueOp(name, expr) \
	static void name(void *data) \
	{ \
		struct testParams *p = (struct testParams *) data; \
		p->flag = p->flag expr; \
	}
queueOp(sub2, - 2)
queueOp(div3, / 3)
queueOp(mul8, * 8)

static void done(void *data)
{
	uiQuit();
}

static const struct {
	uint32_t result;
	const char *order;
} orders[] = {
	{ 8, "sub2 -> div3 -> mul8" },			// the one we want
	{ 13, "sub2 -> mul8 -> div3" },
	{ 0, "div3 -> sub2 -> mul8" },
	{ 14, "div3 -> mul8 -> sub2" },
	{ 18, "mul8 -> sub2 -> div3" },
	{ 16, "mul8 -> div3 -> sub2" },
};

static void queueOrder(struct testParams *p)
{
	p->flag = 7;
	uiQueueMain(sub2, p);
	uiQueueMain(div3, p);
	uiQueueMain(mul8, p);
	uiQueueMain(done, NULL);
}

static void checkOrderFull(testingT *t, const char *file, long line, uint32_t flag)
{
	int i;

	if (flag == orders[0].result)
		return;
	for (i = 1; i < 6; i++)
		if (flag == orders[i].result) {
			testingTErrorfFull(t, file, line, "wrong order:" diff("%" PRIu32 " (%s)"),
				flag, orders[i].order,
				orders[0].result, orders[0].order);
			return;
		}
	testingTErrorfFull(t, file, line, "wrong result:" diff("%" PRIu32 " (%s)"),
		flag, "unknown order",
		orders[0].result, orders[0].order);
}

#define checkOrder(t, flag) checkOrderFull(t, __FILE__, __LINE__, flag)

testingTest(QueueMain_Sequence)
{
	struct testParams p;

	memset(&p, 0, sizeof (struct testParams));
	queueOrder(&p);
	timeout_uiMain(t, 5 * timerSecond);
	checkOrder(t, p.flag);
}

static void queueThread(void *data)
{
	struct testParams *p = (struct testParams *) data;

	p->err = timerSleep(1250 * timerMillisecond);
	uiQueueMain(queued, p);
}

testingTest(QueueMain_DifferentThread)
{
	threadThread *thread;
	threadSysError err;
	struct testParams p;

	memset(&p, 0, sizeof (struct testParams));
	p.flag = 0;
	err = threadNewThread(queueThread, &p, &thread);
	if (err != 0)
		testingTFatalf(t, "error creating thread: " threadSysErrorFmt, threadSysErrorFmtArg(err));
	timeout_uiMain(t, 5 * timerSecond);
	err = threadThreadWaitAndFree(thread);
	if (err != 0)
		testingTFatalf(t, "error waiting for thread to finish: " threadSysErrorFmt, threadSysErrorFmtArg(err));
	if (p.err != 0)
		testingTErrorf(t, "error sleeping in thread to ensure a high likelihood the uiQueueMain() is run after uiMain() starts: " timerSysErrorFmt, timerSysErrorFmtArg(p.err));
	if (p.flag != 1)
		testingTErrorf(t, "uiQueueMain() didn't set flag properly:" diff("%d"),
			p.flag, 1);
}

static void queueOrderThread(void *data)
{
	struct testParams *p = (struct testParams *) data;

	p->err = timerSleep(1250 * timerMillisecond);
	queueOrder(p);
}

testingTest(QueueMain_DifferentThreadSequence)
{
	threadThread *thread;
	threadSysError err;
	struct testParams p;

	memset(&p, 0, sizeof (struct testParams));
	p.flag = 1;		// make sure it's initialized just in case
	err = threadNewThread(queueOrderThread, &p, &thread);
	if (err != 0)
		testingTFatalf(t, "error creating thread: " threadSysErrorFmt, threadSysErrorFmtArg(err));
	timeout_uiMain(t, 5 * timerSecond);
	err = threadThreadWaitAndFree(thread);
	if (err != 0)
		testingTFatalf(t, "error waiting for thread to finish: " threadSysErrorFmt, threadSysErrorFmtArg(err));
	if (p.err != 0)
		testingTErrorf(t, "error sleeping in thread to ensure a high likelihood the uiQueueMain() is run after uiMain() starts: " timerSysErrorFmt, timerSysErrorFmtArg(p.err));
	checkOrder(t, p.flag);
}

//#if 0
static void timer(void *data)
{
	int *n = (int *) data;

	// TODO return stop if n == 5, continue otherwise
	*n++;
}

testingTest(Timer)
{
}
#endif
