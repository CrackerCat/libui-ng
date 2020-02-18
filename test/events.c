// 18 may 2019
#include "test.h"

// TODO test the number of calls to handlers made

struct handler {
	uiEvent *e;
	int id;
	bool validID;
	const char *name;

	bool gotRun;
	void *gotSender;
	void *gotArgs;

	bool wantRun;
	void *wantSender;
	void *wantArgs;
	bool wantBlocked;

	int *runCount;
};

static void handler(void *sender, void *args, void *data)
{
	struct handler *h = (struct handler *) data;

	h->gotRun = true;
	h->gotSender = sender;
	h->gotArgs = args;
	(*(h->runCount))++;
}

static void *allocHandlersFull(const char *file, long line, size_t n)
{
	struct handler *h;

	h = (struct handler *) malloc(n * sizeof (struct handler));
	if (h == NULL)
		TestFatalfFull(file, line, "memory exhausted allocating handlers");
	memset(h, 0, n * sizeof (struct handler));
	return h;
}

#define allocHandlers(n) allocHandlersFull(__FILE__, __LINE__, n)

static void resetGot(struct handler *h, int *runCount)
{
	h->gotRun = false;
	h->gotSender = NULL;
	h->gotArgs = NULL;
	h->runCount = runCount;
}

static void registerHandler(struct handler *h, uiEvent *e, void *sender, void *args)
{
	h->wantSender = sender;
	h->wantArgs = args;
	h->e = e;
	h->id = uiEventAddHandler(h->e, handler, h->wantSender, h);
	h->validID = true;
}

static void unregisterHandler(struct handler *h)
{
	if (!h->validID)		// not registered; do nothing (likely a deferred call)
		return;
	uiEventDeleteHandler(h->e, h->id);
	h->e = NULL;
	h->validID = false;
}

static void wantRun(struct handler *h)
{
	h->wantRun = true;
	h->wantBlocked = false;
}

static void wantNotRun(struct handler *h)
{
	h->wantRun = false;
	h->wantBlocked = false;
}

static void wantBlocked(struct handler *h)
{
	h->wantRun = false;
	h->wantBlocked = true;
}

static void runFull(const char *file, long line, uiEvent *e, void *sender, void *args, struct handler *handlers, int n, int wantRunCount)
{
	int i;
	int gotRunCount;
	struct handler *h;
	bool gotBlocked;

	gotRunCount = 0;
	for (i = 0; i < n; i++)
		resetGot(handlers + i, &gotRunCount);

	uiEventFire(e, sender, args);

	h = handlers;
	for (i = 0; i < n; i++) {
		if (!h->gotRun && h->wantRun)
			TestErrorfFull(file, line, "%s not run; should have been", h->name);
		else if (h->gotRun && !h->wantRun)
			TestErrorfFull(file, line, "%s run; should not have been", h->name);
		if (h->gotRun && h->wantRun) {
			// only check these if it was correctly run, to reduce noise if the above failed
			if (h->gotSender != h->wantSender)
				TestErrorfFull(file, line, "incorrect sender seen by %s:" diff("%p"),
					h->name, h->gotSender, h->wantSender);
			if (h->gotArgs != h->wantArgs)
				TestErrorfFull(file, line, "incorrect args seen by %s:" diff("%p"),
					h->name, h->gotArgs, h->wantArgs);
		}
		if (h->validID) {
			// the following call will fail if the ID isn't valid
			gotBlocked = uiEventHandlerBlocked(e, h->id);
			if (!gotBlocked && h->wantBlocked)
				TestErrorfFull(file, line, "%s not blocked; should have been", h->name);
			else if (gotBlocked && !h->wantBlocked)
				TestErrorfFull(file, line, "%s blocked; should not have been", h->name);
		}
		h++;
	}
	if (gotRunCount != wantRunCount)
		TestErrorfFull(file, line, "incorrect number of handler runs:" diff("%d"),
			gotRunCount, wantRunCount);
}

#define run(e, sender, args, handlers, n, wantRunCount) runFull(__FILE__, __LINE__, e, sender, args, handlers, n, wantRunCount)

struct baseParams {
	bool global;
	void *sender;
	void *args;
};

#define testImpl(f, g, s, a) \
	{ \
		struct baseParams p; \
		p.global = g; \
		p.sender = s; \
		p.args = a; \
		f(&p); \
	}

/*
static void runArgsSubtests(struct baseParams *p)
Te_st(xxxx_Args)
testImpl(xxxxx, true, NULL, (&p))
Te_st(xxxx_NoArgs)
testImpl(xxxxx, true, NULL, NULL)

static void runGlobalSubtests(struct baseParams *p)
Te_st(xxxx_Global_Args)
testImpl(xxxx, true, NULL, (&p))
Te_st(xxxx_Global_NoArgs)
testImpl(xxxxx, true, NULL, NULL)
Te_st(xxxx_Nonglobal_Args)
testImpl(xxxxx, false, (&p), (&p))
Te_st(xxxx_Nonglobal_NoArgs)
testImpl(xxxxx, false, (&p), NULL)
*/

static void deferEventFree(void *data)
{
	uiEventFree((uiEvent *) data);
}

static void deferUnregisterHandler(void *data)
{
	unregisterHandler((struct handler *) data);
}

static void basicEventFunctionalityImpl(struct baseParams *p)
{
	uiEvent *e;
	uiEventOptions opts;
	struct handler *h;

	memset(&opts, 0, sizeof (uiEventOptions));
	opts.Size = sizeof (uiEventOptions);
	opts.Global = p->global;
	e = uiNewEvent(&opts);
	TestDefer(deferEventFree, e);

	h = allocHandlers(1);
	TestDefer(free, h);
	h[0].name = "handler";
	TestDefer(deferUnregisterHandler, h + 0);

	registerHandler(h + 0, e, p->sender, p->args);
	wantRun(h + 0);
	run(e, p->sender, p->args,
		h, 1, 1);
}

Test(BasicEventFunctionality_Global_Args)
testImpl(basicEventFunctionalityImpl, true, NULL, (&p))
Test(BasicEventFunctionality_Global_NoArgs)
testImpl(basicEventFunctionalityImpl, true, NULL, NULL)
Test(BasicEventFunctionality_Nonglobal_Args)
testImpl(basicEventFunctionalityImpl, false, (&p), (&p))
Test(BasicEventFunctionality_Nonglobal_NoArgs)
testImpl(basicEventFunctionalityImpl, false, (&p), NULL)

#if 0
// TODO CONTINUE HERE

static void addDeleteEventHandlersImpl(testingT *t, void *data)
{
	struct baseParams *p = (struct baseParams *) data;
	uiEvent *e;
	uiEventOptions opts;
	struct handler *h;

	memset(&opts, 0, sizeof (uiEventOptions));
	opts.Size = sizeof (uiEventOptions);
	opts.Global = p->global;
	e = uiNewEvent(&opts);
	TestDefer(deferEventFree, e);

	h = allocHandlers(6);
	TestDefer(free, h);
	h[0].name = "handler 1";
	h[1].name = "handler 2";
	h[2].name = "handler 3";
	h[3].name = "new handler 1";
	h[4].name = "new handler 2";
	h[5].name = "new handler 3";
	TestDefer(deferUnregisterHandler, h + 5);
	TestDefer(deferUnregisterHandler, h + 4);
	TestDefer(deferUnregisterHandler, h + 3);
	TestDefer(deferUnregisterHandler, h + 2);
	TestDefer(deferUnregisterHandler, h + 1);
	TestDefer(deferUnregisterHandler, h + 0);

	testingTLogf(t, "*** initial handlers");
	registerHandler(h + 0, e, p->sender, p->args);
	registerHandler(h + 1, e, p->sender, p->args);
	registerHandler(h + 2, e, p->sender, p->args);
	wantRun(h + 0);
	wantRun(h + 1);
	wantRun(h + 2);
	wantNotRun(h + 3);
	wantNotRun(h + 4);
	wantNotRun(h + 5);
	run(e, p->sender, p->args,
		h, 6, 3);

	testingTLogf(t, "*** deleting a handler from the middle");
	unregisterHandler(h + 1);
	wantRun(h + 0);
	wantNotRun(h + 1);
	wantRun(h + 2);
	wantNotRun(h + 3);
	wantNotRun(h + 4);
	wantNotRun(h + 5);
	run(e, p->sender, p->args,
		h, 6, 2);

	testingTLogf(t, "*** adding handler after deleting a handler from the middle");
	registerHandler(h + 3, e, p->sender, p->args);
	wantRun(h + 0);
	wantNotRun(h + 1);
	wantRun(h + 2);
	wantRun(h + 3);
	wantNotRun(h + 4);
	wantNotRun(h + 5);
	run(e, p-> sender, p->args,
		h, 6, 3);

	testingTLogf(t, "*** deleting first handler added and adding another");
	unregisterHandler(h + 0);
	registerHandler(h + 4, e, p->sender, p->args);
	wantNotRun(h + 0);
	wantNotRun(h + 1);
	wantRun(h + 2);
	wantRun(h + 3);
	wantRun(h + 4);
	wantNotRun(h + 5);
	run(e, p->sender, p->args,
		h, 6, 3);

	testingTLogf(t, "*** deleting most recently added handler and adding another");
	unregisterHandler(h + 4);
	registerHandler(h + 5, e, p->sender, p->args);
	wantNotRun(h + 0);
	wantNotRun(h + 1);
	wantRun(h + 2);
	wantRun(h + 3);
	wantNotRun(h + 4);
	wantRun(h + 5);
	run(e, p->sender, p->args,
		h, 6, 3);

	testingTLogf(t, "*** deleting all handlers");
	unregisterHandler(h + 2);
	unregisterHandler(h + 3);
	unregisterHandler(h + 5);
	wantNotRun(h + 0);
	wantNotRun(h + 1);
	wantNotRun(h + 2);
	wantNotRun(h + 3);
	wantNotRun(h  +4);
	wantNotRun(h + 5);
	run(e, p->sender, p->args,
		h, 6, 0);

	testingTLogf(t, "*** adding handler after deleting all handlers");
	registerHandler(h + 0, e, p->sender, p->args);
	wantRun(h + 0);
	wantNotRun(h + 1);
	wantNotRun(h + 2);
	wantNotRun(h + 3);
	wantNotRun(h + 4);
	wantNotRun(h + 5);
	run(e, p->sender, p->args,
		h, 6, 1);
}

testingTest(AddDeleteEventHandlers)
{
	struct baseParams p;

	memset(&p, 0, sizeof (struct baseParams));
	p.impl = addDeleteEventHandlersImpl;
	runGlobalSubtests(t, &p);
}

static void eventSendersHonoredImpl(testingT *t, void *data)
{
	struct baseParams *p = (struct baseParams *) data;
	uiEvent *e;
	uiEventOptions opts;
	struct handler *h;
	void *sender1, *sender2, *sender3;

	memset(&opts, 0, sizeof (uiEventOptions));
	opts.Size = sizeof (uiEventOptions);
	opts.Global = false;
	e = uiNewEvent(&opts);
	TestDefer(deferEventFree, e);

	h = allocHandlers(4);
	TestDefer(free, h);
	h[0].name = "sender 1 handler 1";
	h[1].name = "sender 2 handler";
	h[2].name = "sender 3 handler";
	h[3].name = "sender 1 handler 2";
	TestDefer(deferUnregisterHandler, h + 3);
	TestDefer(deferUnregisterHandler, h + 2);
	TestDefer(deferUnregisterHandler, h + 1);
	TestDefer(deferUnregisterHandler, h + 0);

	// dynamically allocate these so we don't run the risk of upsetting an optimizer somewhere, since we don't touch this memory
	sender1 = malloc(16);
	if (sender1 == NULL)
		testingTFatalf(t, "memory exhausted allocating sender 1");
	memset(sender1, 5, 16);
	TestDefer(free, sender1);
	sender2 = malloc(32);
	if (sender2 == NULL)
		testingTFatalf(t, "memory exhausted allocating sender 2");
	memset(sender2, 10, 32);
	TestDefer(free, sender2);
	sender3 = malloc(64);
	if (sender3 == NULL)
		testingTFatalf(t, "memory exhausted allocating sender 3");
	memset(sender3, 15, 64);
	TestDefer(free, sender3);

	registerHandler(h + 0, e, sender1, p->args);
	registerHandler(h + 1, e, sender2, p->args);
	registerHandler(h + 2, e, sender3, p->args);
	registerHandler(h + 3, e, sender1, p->args);

	testingTLogf(t, "*** sender 1");
	wantRun(h + 0);
	wantNotRun(h + 1);
	wantNotRun(h + 2);
	wantRun(h + 3);
	run(e, sender1, p->args,
		h, 4, 2);

	testingTLogf(t, "*** sender 2");
	wantNotRun(h + 0);
	wantRun(h + 1);
	wantNotRun(h + 2);
	wantNotRun(h + 3);
	run(e, sender2, p->args,
		h, 4, 1);

	testingTLogf(t, "*** sender 3");
	wantNotRun(h + 0);
	wantNotRun(h + 1);
	wantRun(h + 2);
	wantNotRun(h + 3);
	run(e, sender3, p->args,
		h, 4, 1);

	testingTLogf(t, "*** an entirely different sender");
	wantNotRun(h + 0);
	wantNotRun(h + 1);
	wantNotRun(h + 2);
	wantNotRun(h + 3);
	run(e, p, p->args,
		h, 4, 0);

	testingTLogf(t, "*** deleting one of sender 1's handlers doesn't affect the other");
	unregisterHandler(h + 3);
	wantRun(h + 0);
	wantNotRun(h + 1);
	wantNotRun(h + 2);
	wantNotRun(h + 3);
	run(e, sender1, p->args,
		h, 4, 1);

	testingTLogf(t, "*** after registering a handler with the above entirely different sender, it will work");
	registerHandler(h + 3, e, p, p->args);
	wantNotRun(h + 0);
	wantNotRun(h + 1);
	wantNotRun(h + 2);
	wantRun(h + 3);
	run(e, p, p->args,
		h, 4, 1);
}

testingTest(EventSendersHonored)
{
	struct baseParams p;

	memset(&p, 0, sizeof (struct baseParams));
	p.impl = eventSendersHonoredImpl;
	runArgsSubtests(t, &p);
}

static void eventBlocksHonoredImpl(testingT *t, void *data)
{
	struct baseParams *p = (struct baseParams *) data;
	uiEvent *e;
	uiEventOptions opts;
	struct handler *h;

	memset(&opts, 0, sizeof (uiEventOptions));
	opts.Size = sizeof (uiEventOptions);
	opts.Global = p->global;
	e = uiNewEvent(&opts);
	TestDefer(deferEventFree, e);

	h = allocHandlers(3);
	TestDefer(free, h);
	h[0].name = "handler 1";
	h[1].name = "handler 2";
	h[2].name = "handler 3";
	TestDefer(deferUnregisterHandler, h + 2);
	TestDefer(deferUnregisterHandler, h + 1);
	TestDefer(deferUnregisterHandler, h + 0);

	testingTLogf(t, "*** initial handlers are unblocked");
	registerHandler(h + 0, e, p->sender, p->args);
	registerHandler(h + 1, e, p->sender, p->args);
	registerHandler(h + 2, e, p->sender, p->args);
	wantRun(h + 0);
	wantRun(h + 1);
	wantRun(h + 2);
	run(e, p->sender, p->args,
		h, 3, 3);

	testingTLogf(t, "*** blocking handler 2 omits it");
	uiEventSetHandlerBlocked(e, h[1].id, true);
	wantRun(h + 0);
	wantBlocked(h + 1);
	wantRun(h + 2);
	run(e, p->sender, p->args,
		h, 3, 2);

	testingTLogf(t, "*** blocking handler 3 omits both 2 and 3");
	uiEventSetHandlerBlocked(e, h[2].id, true);
	wantRun(h + 0);
	wantBlocked(h + 1);
	wantBlocked(h + 2);
	run(e, p->sender, p->args,
		h, 3, 1);

	testingTLogf(t, "*** unblocking handler 2 omits only 3");
	uiEventSetHandlerBlocked(e, h[1].id, false);
	wantRun(h + 0);
	wantRun(h + 1);
	wantBlocked(h + 2);
	run(e, p->sender, p->args,
		h, 3, 2);

	testingTLogf(t, "*** blocking an already blocked handler is a no-op");
	uiEventSetHandlerBlocked(e, h[2].id, true);
	wantRun(h + 0);
	wantRun(h + 1);
	wantBlocked(h + 2);
	run(e, p->sender, p->args,
		h, 3, 2);

	testingTLogf(t, "*** unblocking an already unblocked handler is a no-op");
	uiEventSetHandlerBlocked(e, h[1].id, false);
	wantRun(h + 0);
	wantRun(h + 1);
	wantBlocked(h + 2);
	run(e, p->sender, p->args,
		h, 3, 2);

	testingTLogf(t, "*** blocking everything omits everything");
	uiEventSetHandlerBlocked(e, h[0].id, true);
	uiEventSetHandlerBlocked(e, h[1].id, true);
	uiEventSetHandlerBlocked(e, h[2].id, true);
	wantBlocked(h + 0);
	wantBlocked(h + 1);
	wantBlocked(h + 2);
	run(e, p->sender, p->args,
		h, 3, 0);

	testingTLogf(t, "*** unblocking everything omits nothing");
	uiEventSetHandlerBlocked(e, h[0].id, false);
	uiEventSetHandlerBlocked(e, h[1].id, false);
	uiEventSetHandlerBlocked(e, h[2].id, false);
	wantRun(h + 0);
	wantRun(h + 1);
	wantRun(h + 2);
	run(e, p->sender, p->args,
		h, 3, 3);

	testingTLogf(t, "*** blocking something again works");
	uiEventSetHandlerBlocked(e, h[2].id, true);
	wantRun(h + 0);
	wantRun(h + 1);
	wantBlocked(h + 2);
	run(e, p->sender, p->args,
		h, 3, 2);

	testingTLogf(t, "*** deleting a blocked handler and adding a new one doesn't keep the new one blocked");
	unregisterHandler(h + 2);
	registerHandler(h + 2, e, p->sender, p->args);
	wantRun(h + 0);
	wantRun(h + 1);
	wantRun(h + 2);
	run(e, p->sender, p->args,
		h, 3, 3);

	testingTLogf(t, "*** adding a new handler while one is blocked doesn't affect the blocked one");
	unregisterHandler(h + 2);
	uiEventSetHandlerBlocked(e, h[1].id, true);
	registerHandler(h + 2, e, p->sender, p->args);
	wantRun(h + 0);
	wantBlocked(h + 1);
	wantRun(h + 2);
	run(e, p->sender, p->args,
		h, 3, 2);
}

testingTest(EventBlocksHonored)
{
	struct baseParams p;

	memset(&p, 0, sizeof (struct baseParams));
	p.impl = eventBlocksHonoredImpl;
	runGlobalSubtests(t, &p);
}

static void eventBlocksHonoredWithDifferentSendersImpl(testingT *t, void *data)
{
	struct baseParams *p = (struct baseParams *) data;
	uiEvent *e;
	uiEventOptions opts;
	struct handler *h;
	void *sender1, *sender2;

	memset(&opts, 0, sizeof (uiEventOptions));
	opts.Size = sizeof (uiEventOptions);
	opts.Global = false;
	e = uiNewEvent(&opts);
	TestDefer(deferEventFree, e);

	h = allocHandlers(4);
	TestDefer(free, h);
	h[0].name = "sender 1 handler 1";
	h[1].name = "sender 2 handler 1";
	h[2].name = "sender 2 handler 2";
	h[3].name = "sender 1 handler 2";
	TestDefer(deferUnregisterHandler, h + 3);
	TestDefer(deferUnregisterHandler, h + 2);
	TestDefer(deferUnregisterHandler, h + 1);
	TestDefer(deferUnregisterHandler, h + 0);

	// dynamically allocate these so we don't run the risk of upsetting an optimizer somewhere, since we don't touch this memory
	sender1 = malloc(16);
	if (sender1 == NULL)
		testingTFatalf(t, "memory exhausted allocating sender 1");
	memset(sender1, 5, 16);
	TestDefer(free, sender1);
	sender2 = malloc(32);
	if (sender2 == NULL)
		testingTFatalf(t, "memory exhausted allocating sender 2");
	memset(sender2, 10, 32);
	TestDefer(free, sender2);

	registerHandler(h + 0, e, sender1, p->args);
	registerHandler(h + 1, e, sender2, p->args);
	registerHandler(h + 2, e, sender2, p->args);
	registerHandler(h + 3, e, sender1, p->args);

	testingTLogf(t, "*** sender 1 with nothing blocked");
	wantRun(h + 0);
	wantNotRun(h + 1);
	wantNotRun(h + 2);
	wantRun(h + 3);
	run(e, sender1, p->args,
		h, 4, 2);

	testingTLogf(t, "*** sender 2 with nothing blocked");
	wantNotRun(h + 0);
	wantRun(h + 1);
	wantRun(h + 2);
	wantNotRun(h + 3);
	run(e, sender2, p->args,
		h, 4, 2);

	testingTLogf(t, "*** an entirely different sender with nothing blocked");
	wantNotRun(h + 0);
	wantNotRun(h + 1);
	wantNotRun(h + 2);
	wantNotRun(h + 3);
	run(e, p, p->args,
		h, 4, 0);

	testingTLogf(t, "*** blocking one of sender 1's handlers only runs the other");
	uiEventSetHandlerBlocked(e, h[3].id, true);
	wantRun(h + 0);
	wantNotRun(h + 1);
	wantNotRun(h + 2);
	wantBlocked(h + 3);
	run(e, sender1, p->args,
		h, 4, 1);

	testingTLogf(t, "*** blocking one of sender 1's handlers doesn't affect sender 2");
	wantNotRun(h + 0);
	wantRun(h + 1);
	wantRun(h + 2);
	wantBlocked(h + 3);
	run(e, sender2, p->args,
		h, 4, 2);

	testingTLogf(t, "*** blocking one of sender 1's handlers doesn't affect the above entirely different sender");
	wantNotRun(h + 0);
	wantNotRun(h + 1);
	wantNotRun(h + 2);
	wantBlocked(h + 3);
	run(e, p, p->args,
		h, 4, 0);

	testingTLogf(t, "*** blocking one of sender 2's handlers only runs the other");
	uiEventSetHandlerBlocked(e, h[2].id, true);
	wantNotRun(h + 0);
	wantRun(h + 1);
	wantBlocked(h + 2);
	wantBlocked(h + 3);
	run(e, sender2, p->args,
		h, 4, 1);

	testingTLogf(t, "*** blocking one of sender 2's handlers doesn't affect sender 1");
	wantRun(h + 0);
	wantNotRun(h + 1);
	wantBlocked(h + 2);
	wantBlocked(h + 3);
	run(e, sender1, p->args,
		h, 4, 1);

	testingTLogf(t, "*** blocking one of sender 2's handlers doesn't affect the above entirely different sender");
	wantNotRun(h + 0);
	wantNotRun(h + 1);
	wantBlocked(h + 2);
	wantBlocked(h + 3);
	run(e, p, p->args,
		h, 4, 0);

	testingTLogf(t, "*** deleting the blocked sender 2 handler only runs the other");
	unregisterHandler(h + 2);
	wantNotRun(h + 0);
	wantRun(h + 1);
	wantNotRun(h + 2);
	wantBlocked(h + 3);
	run(e, sender2, p->args,
		h, 4, 1);

	testingTLogf(t, "*** deleting the blocked sender 2 handler doesn't affect sender 1");
	wantRun(h + 0);
	wantNotRun(h + 1);
	wantNotRun(h + 2);
	wantBlocked(h + 3);
	run(e, sender1, p->args,
		h, 4, 1);

	testingTLogf(t, "*** deleting the blocked sender 2 handler doesn't affect the above entirely different sender");
	wantNotRun(h + 0);
	wantNotRun(h + 1);
	wantNotRun(h + 2);
	wantBlocked(h + 3);
	run(e, p, p->args,
		h, 4, 0);

	testingTLogf(t, "*** adding a new sender 1 handler doesn't affect the existing blocked one");
	h[2].name = "sender 1 handler 3";
	registerHandler(h + 2, e, sender1, p->args);
	wantRun(h + 0);
	wantNotRun(h + 1);
	wantRun(h + 2);
	wantBlocked(h + 3);
	run(e, sender1, p->args,
		h, 4, 2);

	testingTLogf(t, "*** adding a new sender 1 handler doesn't affect sender 2");
	wantNotRun(h + 0);
	wantRun(h + 1);
	wantNotRun(h + 2);
	wantBlocked(h + 3);
	run(e, sender2, p->args,
		h, 4, 1);

	testingTLogf(t, "*** adding a new sender 1 handler doesn't affect the above entirely different handler");
	wantNotRun(h + 0);
	wantNotRun(h + 1);
	wantNotRun(h + 2);
	wantBlocked(h + 3);
	run(e, p, p->args,
		h, 4, 0);
}

testingTest(EventBlocksHonoredWithDifferentSenders)
{
	struct baseParams p;

	memset(&p, 0, sizeof (struct baseParams));
	p.impl = eventBlocksHonoredWithDifferentSendersImpl;
	runGlobalSubtests(t, &p);
}

static void eventInvalidateSenderImpl(testingT *t, void *data)
{
	struct baseParams *p = (struct baseParams *) data;
	uiEvent *e;
	uiEventOptions opts;
	struct handler *h;
	void *sender1, *sender2;

	memset(&opts, 0, sizeof (uiEventOptions));
	opts.Size = sizeof (uiEventOptions);
	opts.Global = false;
	e = uiNewEvent(&opts);
	TestDefer(deferEventFree, e);

	h = allocHandlers(4);
	TestDefer(free, h);
	h[0].name = "sender 1 handler 1";
	h[1].name = "sender 2 handler 1";
	h[2].name = "sender 2 handler 2";
	h[3].name = "sender 1 handler 2";
	TestDefer(deferUnregisterHandler, h + 3);
	TestDefer(deferUnregisterHandler, h + 2);
	TestDefer(deferUnregisterHandler, h + 1);
	TestDefer(deferUnregisterHandler, h + 0);

	// dynamically allocate these so we don't run the risk of upsetting an optimizer somewhere, since we don't touch this memory
	sender1 = malloc(16);
	if (sender1 == NULL)
		testingTFatalf(t, "memory exhausted allocating sender 1");
	memset(sender1, 5, 16);
	TestDefer(free, sender1);
	sender2 = malloc(32);
	if (sender2 == NULL)
		testingTFatalf(t, "memory exhausted allocating sender 2");
	memset(sender2, 10, 32);
	TestDefer(free, sender2);

	registerHandler(h + 0, e, sender1, p->args);
	registerHandler(h + 1, e, sender2, p->args);
	registerHandler(h + 2, e, sender2, p->args);
	registerHandler(h + 3, e, sender1, p->args);

	testingTLogf(t, "*** sender 1 initial state");
	wantRun(h + 0);
	wantNotRun(h + 1);
	wantNotRun(h + 2);
	wantRun(h + 3);
	run(e, sender1, p->args,
		h, 4, 2);

	testingTLogf(t, "*** invalidating sender 1 disables it");
	uiEventInvalidateSender(e, sender1);
	wantNotRun(h + 0);
	wantNotRun(h + 1);
	wantNotRun(h + 2);
	wantNotRun(h + 3);
	run(e, sender1, p->args,
		h, 4, 0);

	testingTLogf(t, "*** unblocking one of sender 1's handlers does nothing");
	uiEventSetHandlerBlocked(e, h[3].id, false);
	wantNotRun(h + 0);
	wantNotRun(h + 1);
	wantNotRun(h + 2);
	wantNotRun(h + 3);
	run(e, sender1, p->args,
		h, 4, 0);

	testingTLogf(t, "*** blocking one of sender 1's handlers saves the flag setting, but does not otherwise have any effect");
	uiEventSetHandlerBlocked(e, h[3].id, true);
	wantNotRun(h + 0);
	wantNotRun(h + 1);
	wantNotRun(h + 2);
	wantBlocked(h + 3);
	run(e, sender1, p->args,
		h, 4, 0);

	testingTLogf(t, "*** and unblocking it again only affects the flag, nothing else");
	uiEventSetHandlerBlocked(e, h[3].id, false);
	wantNotRun(h + 0);
	wantNotRun(h + 1);
	wantNotRun(h + 2);
	wantNotRun(h + 3);
	run(e, sender1, p->args,
		h, 4, 0);

	testingTLogf(t, "*** sender 1 being invalidated has no effect on sender 2");
	wantNotRun(h + 0);
	wantRun(h + 1);
	wantRun(h + 2);
	wantNotRun(h + 3);
	run(e, sender2, p->args,
		h, 4, 2);

	testingTLogf(t, "*** sender 1 being invalidated has no effect on an entirely different sender");
	wantNotRun(h + 0);
	wantNotRun(h + 1);
	wantNotRun(h + 2);
	wantNotRun(h + 3);
	run(e, p, p->args,
		h, 4, 0);

	testingTLogf(t, "*** deleting an unblocked sender 1 handler and then adding another one does not block the new one");
	unregisterHandler(h + 3);
	registerHandler(h + 3, e, sender1, p->args);
	wantNotRun(h + 0);
	wantNotRun(h + 1);
	wantNotRun(h + 2);
	wantRun(h + 3);
	run(e, sender1, p->args,
		h, 4, 1);

	testingTLogf(t, "*** sender 2 initial state");
	wantNotRun(h + 0);
	wantRun(h + 1);
	wantRun(h + 2);
	wantNotRun(h + 3);
	run(e, sender2, p->args,
		h, 4, 2);

	testingTLogf(t, "*** invalidating sender 2 disables it");
	uiEventInvalidateSender(e, sender2);
	wantNotRun(h + 0);
	wantNotRun(h + 1);
	wantNotRun(h + 2);
	wantNotRun(h + 3);
	run(e, sender2, p->args,
		h, 4, 0);

	testingTLogf(t, "*** blocking one of sender 2's handlers saves the flag setting, but does not otherwise have any effect");
	uiEventSetHandlerBlocked(e, h[2].id, true);
	wantNotRun(h + 0);
	wantNotRun(h + 1);
	wantBlocked(h + 2);
	wantNotRun(h + 3);
	run(e, sender2, p->args,
		h, 4, 0);

	testingTLogf(t, "*** sender 2 being invalidated has no effect on sender 1");
	wantNotRun(h + 0);
	wantNotRun(h + 1);
	wantBlocked(h + 2);
	wantRun(h + 3);
	run(e, sender1, p->args,
		h, 4, 1);

	testingTLogf(t, "*** sender 2 being invalidated has no effect on the above entirely different sender");
	wantNotRun(h + 0);
	wantNotRun(h + 1);
	wantBlocked(h + 2);
	wantNotRun(h + 3);
	run(e, p, p->args,
		h, 4, 0);

	testingTLogf(t, "*** deleting a blocked sender 2 handler and then adding another one does not block the new one");
	unregisterHandler(h + 2);
	registerHandler(h + 2, e, sender2, p->args);
	wantNotRun(h + 0);
	wantNotRun(h + 1);
	wantRun(h + 2);
	wantNotRun(h + 3);
	run(e, sender2, p->args,
		h, 4, 1);
}

testingTest(EventInvalidateSender)
{
	struct baseParams p;

	memset(&p, 0, sizeof (struct baseParams));
	p.impl = eventInvalidateSenderImpl;
	runArgsSubtests(t, &p);
}

#endif
