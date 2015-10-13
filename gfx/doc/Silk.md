Silk Architecture Overview
=================

#Architecture
Our current architecture is to align three components to hardware vsync timers:

1. Compositor
2. RefreshDriver / Painting
3. Input Events

The flow of our rendering engine is as follows:

1. Hardware Vsync event occurs on an OS specific *Hardware Vsync Thread* on a per display/output-device basis.
2. The *Hardware Vsync Thread* attached to the display notifies any observers listening to it, which are generally going to be Widgets or VsyncParents (for cross-process vsync).
3. Each Widget can listen to either another widget (generally a toplevel widget) for vsync notifications, or directly to a **VsyncSource**.
4. A **VsyncSource** may either be a direct display getting notifications from the *Hardware Vsync Thread*, or it may be a **VsyncChild** (via the PVSync protocol) that is listening to cross-process Vsync notifications from the main/compositor thread.
5. The Widget, in turn, will notify any of the things that are listening to *it* -- these are currently RefreshDrivers, Compositors, or other Widgets.
6. When it receives a vsync event, the **Compositor** dispatches input events on the *Compositor Thread*, then composites. Input events are only dispatched on the *Compositor Thread* on b2g.
7. When it receives a vsync event, The **RefreshDriver** paints on the *Main Thread*.

<b>NOTE: This figure is obsolete!</b>  The implementation is broken into the following sections and will reference this figure. Note that **Objects** are bold fonts while *Threads* are italicized.

<img src="silkArchitecture.png" width="900px" height="630px" />

#Hardware Vsync
Hardware vsync events from (1), occur on a specific **VsyncSource** Object.
Each **VsyncSource** object has an associated UUID.  These UUIDs are generated during each runtime, with the exception of a single well-known "global display" ID.
The **VsyncSource** object is responsible for enabling / disabling vsync on a per connected display basis.
For example, if two monitors are connected, two **VsyncSource** should be created, each listening to vsync events for their respective displays. (NOTE: currently, no multi-monitor support exists on any platform, though all the functionality is in place for this to work properly.)
The "global display" ID is the default display that each widget should listen to, unless it knows about a better display.
Each platform will have to implement a specific **VsyncSource** object to hook and listen to vsync events.

OS X creates one *Hardware Vsync Thread* and uses DisplayLink to listen to vsync events.  We do not currently support multiple displays, so we use one global **CVDisplayLinkRef** that works across all active displays.

On Windows, we have to create a new platform *thread* that waits for DwmFlush(), which works across all active displays.
Once the thread wakes up from DwmFlush(), the actual vsync timestamp is retrieved from DwmGetCompositionTimingInfo(), which is the timestamp that is actually passed into the compositor and refresh driver.

On Firefox OS, the **VsyncSource** is implemented using **HwcComposer2D**.

When a vsync occurs on a **VsyncSource**, the *Hardware Vsync Thread* callback notifies all of its observers (generally Widgets) with the vsync's timestamp.

The Widgets then notify all of their own observers, generally other Widgets, RefreshDrivers, or Compositors.

A **VsyncManager** object manages **VsyncSources**, keeping a list of nsIDs to associated **VsyncSources**.  **VsyncSources** may be added or removed to the **VsyncManager** as they are added/removed from the system.

The **VsyncManager** object is accessible from **gfxPlatform**, and is instantiated only on the parent process when **gfxPlatform** is created.  The **VsyncManager** is destroyed when **gfxPlatform** is destroyed.  There is only one **VsyncManager** object throughout the entire lifetime of Firefox.

# Compositor
The **Compositor** is notified of a vsync event through a **CompositorVsyncObserver**.  The **CompositorVsyncObserver** posts a task to the *Compositor Thread* to tell the **Compositor** to do its work, to ensure that the work does not happen on the *Vsync Thread*.
The **CompositorVsyncObserver** listens to vsync events as needed and stops listening to vsync when composites are no longer scheduled or required.
Every **CompositorParent** is has a unique **CompositorVsyncObserver**.
Each **CompositorParent** is associated with one widget and is created when a new platform window or **nsBaseWidget** is created.
The **CompositorParent**, **CompositorVsyncObserver**, and **nsBaseWidget** all have the same lifetimes, which are created and destroyed together.

### Multiple Displays

NOTE: Not implemented yet.
When multiple displays are present on a system, multiple **VsyncSource**s should exist, one for each display.
The choice of which display to listen to should be made on a per-toplevel-widget basis.  Generally, the Widget should listen to vsync events from the display that contains the biggest portion of the Widget's area.
When a toplevel widget moves, all that needs to happen is it needs to change the **VsyncSource** that it's listening to.  All the associated **RefreshDrivers**, **Compositors**, etc. that are being shown within that **Widget** do not need to be aware of the change.

### GeckoTouchDispatcher
The **GeckoTouchDispatcher** is a singleton that resamples touch events to smooth out jank while tracking a user's finger.
Because input and composite are linked together, the **CompositorVsyncObserver** has a reference to the **GeckoTouchDispatcher** and vice versa.

###Input Events
One large goal of Silk is to align touch events with vsync events.
On Firefox OS, touchscreens often have different touch scan rates than the display refreshes.
A Flame device has a touch refresh rate of 75 HZ, while a Nexus 4 has a touch refresh rate of 100 HZ, while the device's display refresh rate is 60HZ.
When a vsync event occurs, we resample touch events, and then dispatch the resampled touch event to APZ.
Touch events on Firefox OS occur on a *Touch Input Thread* whereas they are processed by APZ on the *APZ Controller Thread*.
We use [Google Android's touch resampling](http://www.masonchang.com/blog/2014/8/25/androids-touch-resampling-algorithm) algorithm to resample touch events.

Currently, we have a strict ordering between Composites and touch events.
When a touch event occurs on the *Touch Input Thread*, we store the touch event in a queue.
When a vsync event occurs, the **Compositor** is notified of a vsync event, which notifies the **GeckoTouchDispatcher**.
The **GeckoTouchDispatcher** processes the touch event first on the *APZ Controller Thread*, which is the same as the *Compositor Thread* on b2g, then the **Compositor** finishes compositing.
We require this strict ordering because if a vsync notification is dispatched to both the **Compositor** and **GeckoTouchDispatcher** at the same time, a race condition occurs between processing the touch event and therefore position versus compositing.
In practice, this creates very janky scrolling.
As of this writing, we have not analyzed input events on desktop platforms.

One slight quirk is that input events can start a composite, for example during a scroll and after the **Compositor** is no longer listening to vsync events.
In these cases, we notify the **Compositor** to observe vsync so that it dispatches touch events.
If touch events were not dispatched, and since the **Compositor** is not listening to vsync events, the touch events would never be dispatched.
The **GeckoTouchDispatcher** handles this case by always forcing the **Compositor** to listen to vsync events while touch events are occurring.

### Widget, Compositor, GeckoTouchDispatcher Shutdown Procedure
When the [nsBaseWidget shuts down](http://hg.mozilla.org/mozilla-central/file/0df249a0e4d3/widget/nsBaseWidget.cpp#l182) - It calls nsBaseWidget::DestroyCompositor on the *Gecko Main Thread*.
During nsBaseWidget::DestroyCompositor, it first destroys the CompositorChild.
CompositorChild sends a sync IPC call to CompositorParent::RecvStop, which calls [CompositorParent::Destroy](http://hg.mozilla.org/mozilla-central/file/ab0490972e1e/gfx/layers/ipc/CompositorParent.cpp#l509).
During this time, the *main thread* is blocked on the parent process.
CompositorParent::RecvStop runs on the *Compositor thread* and cleans up some resources, including setting the **CompositorVsyncObserver** to nullptr.
CompositorParent::RecvStop also explicitly keeps the CompositorParent alive and posts another task to run CompositorParent::DeferredDestroy on the Compositor loop so that all ipdl code can finish executing.
The **CompositorVsyncObserver** also unobserves from vsync and cancels any pending composite tasks.
Once CompositorParent::RecvStop finishes, the *main thread* in the parent process continues shutting down the nsBaseWidget.

At the same time, the *Compositor thread* is executing tasks until CompositorParent::DeferredDestroy runs, which flushes the compositor message loop.
Now we have two tasks as both the nsBaseWidget releases a reference to the Compositor on the *main thread* during destruction and the CompositorParent::DeferredDestroy releases a reference to the CompositorParent on the *Compositor Thread*.
Finally, the CompositorParent itself is destroyed on the *main thread* once both references are gone due to explicit [main thread destruction](http://hg.mozilla.org/mozilla-central/file/50b95032152c/gfx/layers/ipc/CompositorParent.h#l148).

With the **CompositorVsyncObserver**, any accesses to the widget after nsBaseWidget::DestroyCompositor executes are invalid.
Any accesses to the compositor between the time the nsBaseWidget::DestroyCompositor runs and the CompositorVsyncObserver's destructor runs aren't safe yet a hardware vsync event could occur between these times.
Since any tasks posted on the Compositor loop after CompositorParent::DeferredDestroy is posted are invalid, we make sure that no vsync tasks can be posted once CompositorParent::RecvStop executes and DeferredDestroy is posted on the Compositor thread.
When the sync call to CompositorParent::RecvStop executes, we explicitly set the CompositorVsyncObserver to null to prevent vsync notifications from occurring.
If vsync notifications were allowed to occur, since the **CompositorVsyncObserver**'s vsync notification executes on the *hardware vsync thread*, it would post a task to the Compositor loop and may execute after CompositorParent::DeferredDestroy.
Thus, we explicitly shut down vsync events in the **CompositorVsyncObserver** during nsBaseWidget::Shutdown to prevent any vsync tasks from executing after CompositorParent::DeferredDestroy.

The **CompositorVsyncObserver** has a race to be destroyed either during CompositorParent shutdown or from the **GeckoTouchDispatcher** which is destroyed on the main thread with [ClearOnShutdown](http://hg.mozilla.org/mozilla-central/file/21567e9a6e40/xpcom/base/ClearOnShutdown.h#l15).

# Refresh Driver
The Refresh Driver is ticked from a [single active timer](http://hg.mozilla.org/mozilla-central/file/ab0490972e1e/layout/base/nsRefreshDriver.cpp#l11).
The assumption is that there are multiple **RefreshDrivers** connected to a single **RefreshTimer**.
There are two **RefreshTimers**: an active and an inactive **RefreshTimer**.
Each Tab has its own **RefreshDriver**, which connects to one of the global **RefreshTimers**.
The **RefreshTimers** execute on the *Main Thread* and tick their connected **RefreshDrivers**.
We do not want to break this model of multiple **RefreshDrivers** per a set of two global **RefreshTimers**.
Each **RefreshDriver** switches between the active and inactive **RefreshTimer**.

Instead, we create a new **RefreshTimer**, the **WidgetVsyncRefreshTimer** which ticks based on vsync messages.
We replace the current active timer with a **WidgetVsyncRefreshTimer**.
All tabs will then tick based on this new active timer.
We create a **WidgetVsyncRefreshTimer** per **RefreshDriver**, because it refers to a specific screen.

When Firefox starts, we initially create a new **VsyncRefreshTimer** in the Chrome process.
The **WidgetVsyncRefreshTimer** will listen to vsync notifications from its widget.

With Content processes, the start up process is more complicated.
We send vsync IPC messages via the use of the PBackground thread on the parent process, which allows us to send messages from the Parent process' without waiting on the *main thread*.
This sends messages from the Parent::*PBackground Thread* to the Child::*Main Thread*.
The *main thread* receiving IPC messages on the content process is acceptable because **RefreshDrivers** must execute on the *main thread*.
However, there is some amount of time required to setup the IPC connection upon process creation.  This is done the first time a particular display is requested in nsBaseWidget.

The **VsyncParent** listens to vsync events through the **VsyncSource** on the parent side and sends vsync IPC messages to the **VsyncChild**.
The **VsyncChild** is itself a **VsyncSource**, and notifies its observers when it receives a notifiation from the remote process.

During the shutdown process of the content process, ActorDestroy is called on the **VsyncChild** and **VsyncParent** due to the normal PBackground shutdown process.
Once ActorDestroy is called, no IPC messages should be sent across the channel.
After ActorDestroy is called, the IPDL machinery will delete the **VsyncParent/Child** pair.
The **VsyncParent**, due to being a **VsyncObserver**, is ref counted.

Thus the overall flow during normal execution is:

1. [parent] VsyncSource receives a hardware vsync notification from the underlying OS.
2. [parent] VsyncSource notifies its observers: Widgets and VsyncParents
3. [parent] VsyncParent psots a task to the PBackground thread to send a vsync IPC message to its VsyncChild
4. [child] VsyncChild receives a notification that vsync has occurred (on the main thread)
5. [child] VsyncChild notifies all of its observers (in this case, generally exclusively Widgets)
6. [child] Normal Widget vsync processing occurs (it gets dispatched to Widgets, Compositors, and RefreshDrivers)

### Compressing Vsync Messages

Vsync messages occur quite often and the *main thread* can be busy for long periods of time due to JavaScript.
Consistently sending vsync messages to the refresh driver timer can flood the *main thread* with refresh driver ticks, causing even more delays.
To avoid this problem, we compress vsync messages on both the parent and child processes.

On the parent process, newer vsync messages update a vsync timestamp but do not actually queue any tasks on the *main thread*.
Once the parent process' *main thread* executes the refresh driver tick, it uses the most updated vsync timestamp to tick the refresh driver.
After the refresh driver has ticked, one single vsync message is queued for another refresh driver tick task.
On the content process, the IPDL **compress** keyword automatically compresses IPC messages.

### Multiple Monitors

In order to have multiple monitor support for the **RefreshDrivers**, we have multiple active **RefreshTimers**.
Each **RefreshTimer** is associated with a specific **Display** via an id and tick when it's respective **Display** vsync occurs.
We have **N RefreshTimers**, where N is the number of connected displays.
Each **RefreshTimer** still has multiple **RefreshDrivers**.

When a tab or window changes monitors, the **nsIWidget** receives a display changed notification.
Based on which display the window is on, the window switches to the correct **VsyncSource**.
Each **TabParent** should also send a notification to their child.
Each **TabChild**, given the display ID, switches to the correct **RefreshTimer** associated with the display ID.
When each display vsync occurs, it sends one IPC message to notify vsync.
The vsync message contains a display ID, to tick the appropriate **RefreshTimer** on the content process.
There is still only one **VsyncParent/VsyncChild** pair, just each vsync notification will include a display ID, which maps to the correct **RefreshTimer**.

# Object Lifetime

* CompositorVsyncObserver - Lives and dies the same time as the CompositorParent.
* VsyncManager - Lives as long as the gfxPlatform on the chrome process, which is the lifetime of Firefox.
* VsyncParent/VsyncChild - Lives as long as the content process
* RefreshTimer - Lives as long as the process

# Threads

All **VsyncObservers** are notified on the *Hardware Vsync Thread*. It is the responsibility of the **VsyncObservers** to post tasks to their respective correct thread. For example, the **CompositorVsyncObserver** will be notified on the *Hardware Vsync Thread*, and post a task to the *Compositor Thread* to do the actual composition.

1. Compositor Thread - Nothing changes
2. Main Thread - PVsyncChild receives IPC messages on the main thread. We also enable/disable vsync on the main thread.
3. PBackground Thread - Creates a connection from the PBackground thread on the parent process to the main thread in the content process.
4. Hardware Vsync Thread - Every platform is different, but we always have the concept of a hardware vsync thread. Sometimes this is actually created by the host OS. On Windows, we have to create a separate platform thread that blocks on DwmFlush().
