/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_GCAPI_h
#define js_GCAPI_h

#include "mozilla/NullPtr.h"

#include "js/HeapAPI.h"
#include "js/RootingAPI.h"
#include "js/Value.h"

namespace JS {

#define GCREASONS(D)                            \
    /* Reasons internal to the JS engine */     \
    D(API)                                      \
    D(MAYBEGC)                                  \
    D(DESTROY_RUNTIME)                          \
    D(DESTROY_CONTEXT)                          \
    D(LAST_DITCH)                               \
    D(TOO_MUCH_MALLOC)                          \
    D(ALLOC_TRIGGER)                            \
    D(DEBUG_GC)                                 \
    D(DEBUG_MODE_GC)                            \
    D(TRANSPLANT)                               \
    D(RESET)                                    \
    D(OUT_OF_NURSERY)                           \
    D(EVICT_NURSERY)                            \
    D(FULL_STORE_BUFFER)                        \
                                                \
    /* These are reserved for future use. */    \
    D(RESERVED0)                                \
    D(RESERVED1)                                \
    D(RESERVED2)                                \
    D(RESERVED3)                                \
    D(RESERVED4)                                \
    D(RESERVED5)                                \
    D(RESERVED6)                                \
    D(RESERVED7)                                \
    D(RESERVED8)                                \
    D(RESERVED9)                                \
    D(RESERVED10)                               \
    D(RESERVED11)                               \
    D(RESERVED12)                               \
    D(RESERVED13)                               \
    D(RESERVED14)                               \
    D(RESERVED15)                               \
    D(RESERVED16)                               \
    D(RESERVED17)                               \
    D(RESERVED18)                               \
    D(RESERVED19)                               \
                                                \
    /* Reasons from Firefox */                  \
    D(DOM_WINDOW_UTILS)                         \
    D(COMPONENT_UTILS)                          \
    D(MEM_PRESSURE)                             \
    D(CC_WAITING)                               \
    D(CC_FORCED)                                \
    D(LOAD_END)                                 \
    D(POST_COMPARTMENT)                         \
    D(PAGE_HIDE)                                \
    D(NSJSCONTEXT_DESTROY)                      \
    D(SET_NEW_DOCUMENT)                         \
    D(SET_DOC_SHELL)                            \
    D(DOM_UTILS)                                \
    D(DOM_IPC)                                  \
    D(DOM_WORKER)                               \
    D(INTER_SLICE_GC)                           \
    D(REFRESH_FRAME)                            \
    D(FULL_GC_TIMER)                            \
    D(SHUTDOWN_CC)                              \
    D(FINISH_LARGE_EVALUTE)

namespace gcreason {

/* GCReasons will end up looking like JSGC_MAYBEGC */
enum Reason {
#define MAKE_REASON(name) name,
    GCREASONS(MAKE_REASON)
#undef MAKE_REASON
    NO_REASON,
    NUM_REASONS,

    /*
     * For telemetry, we want to keep a fixed max bucket size over time so we
     * don't have to switch histograms. 100 is conservative; as of this writing
     * there are 26. But the cost of extra buckets seems to be low while the
     * cost of switching histograms is high.
     */
    NUM_TELEMETRY_REASONS = 100
};

} /* namespace gcreason */

extern JS_FRIEND_API(void)
PrepareZoneForGC(Zone *zone);

extern JS_FRIEND_API(void)
PrepareForFullGC(JSRuntime *rt);

extern JS_FRIEND_API(void)
PrepareForIncrementalGC(JSRuntime *rt);

extern JS_FRIEND_API(bool)
IsGCScheduled(JSRuntime *rt);

extern JS_FRIEND_API(void)
SkipZoneForGC(Zone *zone);

/*
 * When triggering a GC using one of the functions below, it is first necessary
 * to select the compartments to be collected. To do this, you can call
 * PrepareZoneForGC on each compartment, or you can call PrepareForFullGC
 * to select all compartments. Failing to select any compartment is an error.
 */

extern JS_FRIEND_API(void)
GCForReason(JSRuntime *rt, gcreason::Reason reason);

extern JS_FRIEND_API(void)
ShrinkingGC(JSRuntime *rt, gcreason::Reason reason);

extern JS_FRIEND_API(void)
ShrinkGCBuffers(JSRuntime *rt);

extern JS_FRIEND_API(void)
IncrementalGC(JSRuntime *rt, gcreason::Reason reason, int64_t millis = 0);

extern JS_FRIEND_API(void)
FinishIncrementalGC(JSRuntime *rt, gcreason::Reason reason);

enum GCProgress {
    /*
     * During non-incremental GC, the GC is bracketed by JSGC_CYCLE_BEGIN/END
     * callbacks. During an incremental GC, the sequence of callbacks is as
     * follows:
     *   JSGC_CYCLE_BEGIN, JSGC_SLICE_END  (first slice)
     *   JSGC_SLICE_BEGIN, JSGC_SLICE_END  (second slice)
     *   ...
     *   JSGC_SLICE_BEGIN, JSGC_CYCLE_END  (last slice)
     */

    GC_CYCLE_BEGIN,
    GC_SLICE_BEGIN,
    GC_SLICE_END,
    GC_CYCLE_END
};

struct JS_FRIEND_API(GCDescription) {
    bool isCompartment_;

    GCDescription(bool isCompartment)
      : isCompartment_(isCompartment) {}

    jschar *formatMessage(JSRuntime *rt) const;
    jschar *formatJSON(JSRuntime *rt, uint64_t timestamp) const;
};

typedef void
(* GCSliceCallback)(JSRuntime *rt, GCProgress progress, const GCDescription &desc);

extern JS_FRIEND_API(GCSliceCallback)
SetGCSliceCallback(JSRuntime *rt, GCSliceCallback callback);

/*
 * Signals a good place to do an incremental slice, because the browser is
 * drawing a frame.
 */
extern JS_FRIEND_API(void)
NotifyDidPaint(JSRuntime *rt);

extern JS_FRIEND_API(bool)
IsIncrementalGCEnabled(JSRuntime *rt);

JS_FRIEND_API(bool)
IsIncrementalGCInProgress(JSRuntime *rt);

extern JS_FRIEND_API(void)
DisableIncrementalGC(JSRuntime *rt);

extern JS_FRIEND_API(void)
DisableGenerationalGC(JSRuntime *rt);

extern JS_FRIEND_API(void)
EnableGenerationalGC(JSRuntime *rt);

extern JS_FRIEND_API(bool)
IsIncrementalBarrierNeeded(JSRuntime *rt);

extern JS_FRIEND_API(bool)
IsIncrementalBarrierNeeded(JSContext *cx);

extern JS_FRIEND_API(void)
IncrementalReferenceBarrier(void *ptr, JSGCTraceKind kind);

extern JS_FRIEND_API(void)
IncrementalValueBarrier(const Value &v);

extern JS_FRIEND_API(void)
IncrementalObjectBarrier(JSObject *obj);

extern JS_FRIEND_API(void)
PokeGC(JSRuntime *rt);

/* Was the most recent GC run incrementally? */
extern JS_FRIEND_API(bool)
WasIncrementalGC(JSRuntime *rt);

extern JS_FRIEND_API(size_t)
GetGCNumber();

class AutoAssertNoGC {
#ifdef DEBUG
    size_t gcNumber;

  public:
    AutoAssertNoGC();
    ~AutoAssertNoGC();
#endif
};

class JS_PUBLIC_API(ObjectPtr)
{
    Heap<JSObject *> value;

  public:
    ObjectPtr() : value(nullptr) {}

    ObjectPtr(JSObject *obj) : value(obj) {}

    /* Always call finalize before the destructor. */
    ~ObjectPtr() { JS_ASSERT(!value); }

    void finalize(JSRuntime *rt) {
        if (IsIncrementalBarrierNeeded(rt))
            IncrementalObjectBarrier(value);
        value = nullptr;
    }

    void init(JSObject *obj) { value = obj; }

    JSObject *get() const { return value; }

    void writeBarrierPre(JSRuntime *rt) {
        IncrementalObjectBarrier(value);
    }

    bool isAboutToBeFinalized();

    ObjectPtr &operator=(JSObject *obj) {
        IncrementalObjectBarrier(value);
        value = obj;
        return *this;
    }

    void trace(JSTracer *trc, const char *name);

    JSObject &operator*() const { return *value; }
    JSObject *operator->() const { return value; }
    operator JSObject *() const { return value; }
};

/*
 * Unsets the gray bit for anything reachable from |thing|. |kind| should not be
 * JSTRACE_SHAPE. |thing| should be non-null.
 */
extern JS_FRIEND_API(bool)
UnmarkGrayGCThingRecursively(void *thing, JSGCTraceKind kind);

/*
 * This should be called when an object that is marked gray is exposed to the JS
 * engine (by handing it to running JS code or writing it into live JS
 * data). During incremental GC, since the gray bits haven't been computed yet,
 * we conservatively mark the object black.
 */
static JS_ALWAYS_INLINE void
ExposeGCThingToActiveJS(void *thing, JSGCTraceKind kind)
{
    JS_ASSERT(kind != JSTRACE_SHAPE);

    shadow::Runtime *rt = js::gc::GetGCThingRuntime(thing);
#ifdef JSGC_GENERATIONAL
    /*
     * GC things residing in the nursery cannot be gray: they have no mark bits.
     * All live objects in the nursery are moved to tenured at the beginning of
     * each GC slice, so the gray marker never sees nursery things.
     */
    if (js::gc::IsInsideNursery(rt, thing))
        return;
#endif
    if (IsIncrementalBarrierNeededOnGCThing(rt, thing, kind))
        IncrementalReferenceBarrier(thing, kind);
    else if (GCThingIsMarkedGray(thing))
        UnmarkGrayGCThingRecursively(thing, kind);
}

static JS_ALWAYS_INLINE void
ExposeValueToActiveJS(const Value &v)
{
    if (v.isMarkable())
        ExposeGCThingToActiveJS(v.toGCThing(), v.gcKind());
}

static JS_ALWAYS_INLINE void
ExposeObjectToActiveJS(JSObject *obj)
{
    ExposeGCThingToActiveJS(obj, JSTRACE_OBJECT);
}

/*
 * If a GC is currently marking, mark the object black.
 */
static JS_ALWAYS_INLINE void
MarkGCThingAsLive(JSRuntime *rt_, void *thing, JSGCTraceKind kind)
{
    shadow::Runtime *rt = shadow::Runtime::asShadowRuntime(rt_);
#ifdef JSGC_GENERATIONAL
    /*
     * Any object in the nursery will not be freed during any GC running at that time.
     */
    if (js::gc::IsInsideNursery(rt, thing))
        return;
#endif
    if (IsIncrementalBarrierNeededOnGCThing(rt, thing, kind))
        IncrementalReferenceBarrier(thing, kind);
}

static JS_ALWAYS_INLINE void
MarkStringAsLive(Zone *zone, JSString *string)
{
    JSRuntime *rt = JS::shadow::Zone::asShadowZone(zone)->runtimeFromMainThread();
    MarkGCThingAsLive(rt, string, JSTRACE_STRING);
}

} /* namespace JS */

#endif /* js_GCAPI_h */
