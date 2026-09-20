.. default-domain:: c
.. highlight:: c

***************
Handling Events
***************

Events are sent to a view when it has received user input,
must be drawn, or in other situations that may need to be handled such as resizing.

Events are sent to the event handler as a :union:`PuglEvent` union.
The ``type`` field defines the type of the event and which field of the union is active.
The application must handle at least :enumerator:`PUGL_CONFIGURE <PuglEventType.PUGL_CONFIGURE>`
and :enumerator:`PUGL_EXPOSE <PuglEventType.PUGL_EXPOSE>` to draw anything,
but there are many other :enum:`event types <PuglEventType>`.

For example, a basic event handler might look something like this:

.. code-block:: c

   static PuglStatus
   onEvent(PuglView* view, const PuglEvent* event)
   {
     MyApp* app = (MyApp*)puglGetHandle(view);

     switch (event->type) {
     case PUGL_REALIZE:
       return setupGraphics(app);
     case PUGL_UNREALIZE:
       return teardownGraphics(app);
     case PUGL_CONFIGURE:
       return resize(app, event->configure.width, event->configure.height);
     case PUGL_EXPOSE:
       return draw(app, view);
     case PUGL_CLOSE:
       return quit(app);
     case PUGL_BUTTON_PRESS:
        return onButtonPress(app, view, event->button);
     default:
       break;
     }

     return PUGL_SUCCESS;
   }

Using the Graphics Context
==========================

Drawing
-------

Note that Pugl uses a different drawing model than many libraries,
particularly those designed for game-style main loops like `SDL <https://libsdl.org/>`_ and `GLFW <https://www.glfw.org/>`_.

In that style of code, drawing is performed imperatively in the main loop,
but with Pugl, the application must draw only while handling an expose event.
This is because Pugl supports event-driven applications that only draw the damaged region when necessary,
and handles exposure internally to provide optimized and consistent behavior across platforms.

Cairo Context
-------------

A Cairo context is created for each :struct:`PuglExposeEvent`,
and only exists during the handling of that event.
Null is returned by :func:`puglGetContext` at any other time.

OpenGL Context
--------------

The OpenGL context is only active while handling realization (:enumerator:`PUGL_REALIZE`, :enumerator:`PUGL_UNREALIZE`) and exposure (:enumerator:`PUGL_EXPOSE`),
and can only be used for rendering during exposure.

Note that the OpenGL context is *not* active while handling configuration.
When a :enumerator:`PUGL_CONFIGURE` is received,
the application should record it and prepare accordingly,
but only apply it to OpenGL when the view is next exposed.

Vulkan Context
--------------

With Vulkan, the graphics context is managed by the application rather than Pugl.
However, drawing must still only be performed during an expose.



Raw Pointer Contacts
====================

Touch and pen input is represented by :struct:`PuglPointerEvent`.
Each active contact has a non-zero :type:`PuglPointerId` that remains stable
from :enumerator:`PUGL_POINTER_DOWN <PuglEventType.PUGL_POINTER_DOWN>` until
:enumerator:`PUGL_POINTER_UP <PuglEventType.PUGL_POINTER_UP>` or
:enumerator:`PUGL_POINTER_CANCEL <PuglEventType.PUGL_POINTER_CANCEL>`.

Backends send one event per changed contact, so no allocation is required to
represent multi-pointer input.  Gesture interpretation such as tap, drag,
pinch, or rotation is intentionally left to higher-level toolkits.

A cancelled contact must not be treated as a normal release.  Applications
should discard any transient interaction state associated with that pointer ID.
